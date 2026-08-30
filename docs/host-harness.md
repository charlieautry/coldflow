# Host Harness Reference

The Python side of the rig, bottom of the stack to the top. Everything lives
in `host/teststand/`; the pytest suite that exercises it lives in
`host/tests/`.

## The stack

```
pytest suite          asserts the spec over the wire
sequence runner       executes YAML procedures, watched by redlines
redline monitor       aborts independently of whatever else is happening
TestStand driver      speaks the serial protocol, logs every frame
csv logger            DAQ-style session files
                                       |
                serial.Serial  or  FakeBoard
                (the real rig)     (spec + toy plant, no hardware)
```

The same suite runs against both bottoms. `pytest` alone uses the fake and
finishes in about a second; `pytest --port COM5` points everything at the
real board instead. That split is deliberate: the fake run proves the logic
reads the spec correctly, the hardware run proves the wiring agrees.

## TestStand driver

One object per serial connection. The verbs map one-to-one onto the spec's
commands:

```python
from teststand import TestStand

with TestStand.open("COM5", log_path="data/run1.csv") as stand:
    stand.clear()
    stand.arm()
    stand.set_pressure(10)
    stand.press()
    tel = stand.wait_for_state("HOLD", timeout_s=60)
    print(tel.psi, tel.valve)
    stand.vent()
    stand.wait_for_state("SAFE")
```

Rules the driver enforces so nothing upstream has to think about them:

- every telemetry frame that passes through, for any reason, lands in the
  ring buffer (`stand.history`), `stand.latest`, and the CSV log
- `ok` returns, `err <reason>` raises `CommandError` with the reason word
- `wait_until`/`wait_for_state` ride the telemetry stream when it's on and
  fall back to STATUS polling when it's off

## CSV session format

Commented metadata header, then one row per frame:

```
# log_format: teststand v1
# started_utc: 2026-08-30T19:00:12+00:00
# test: hold_10psi
# port: COM5
host_time_s,t_ms,state,psi,setpoint,degC,flow_lpm,pump,valve,faults
0.000,1204,IDLE,0.02,,21.5,0.00,0.00,0.00,
```

`host_time_s` is the laptop clock, `t_ms` is the board clock, and having both
is the point: disagreement between them is how you spot dropped frames.
Faults are `|`-separated names, empty when healthy.

## Sequences as data

A test procedure is a YAML file, not a script:

```yaml
name: hold_10psi
redlines:
  - {field: psi, max: 18}
steps:
  - {command: ABORT}                      # preamble: reach IDLE from anywhere
  - {wait_state: SAFE, timeout_s: 30}
  - {command: CLEAR}
  - {command: ARM}
  - {command: SET 10}
  - {command: PRESS}
  - {wait_state: HOLD, timeout_s: 90}
  - {hold_s: 5}
  - {check: {field: psi, min: 9.5, max: 10.5}}
  - {command: VENT}
  - {wait_state: SAFE, timeout_s: 30}
```

Step types:

| step | meaning |
| --- | --- |
| `command` | send it, expect `ok` (or `expect: "err state"` to assert a rejection) |
| `wait_state` | watch telemetry for a state, fail on timeout |
| `hold_s` | dwell for N board-seconds, redlines still watching |
| `check` | one frame, assert a field is in `[min, max]` or a `fault` name is latched |

Run one with:

```
python -m teststand run sequences/hold_10psi.yaml --port COM5
python -m teststand run sequences/hold_10psi.yaml --fake     # no hardware
```

Exit code is 0 on pass, 1 on fail, and a per-step transcript prints either
way. Every run writes a CSV under `data/` unless `--log` says otherwise.

## Redlines

Declared per sequence (or built in code from `Redline.maximum`, `.minimum`,
`.forbid_fault`). The monitor sees every telemetry frame during waits and
holds, and the first violation sends `ABORT` immediately, without consulting
the sequence. The firmware's own hard redlines (21 psi, loop faults, stale
data) sit underneath this layer; host redlines should always be tighter.

## Data review

```
python -m teststand review data/hold_10psi_20260830_120000.csv --plot out.png
```

prints per-channel min/max/mean, the state timeline, redline proximity (peak
psi against the 21 psi abort), any faults seen, and the controller numbers
(settling time into the 0.5 psi band, overshoot, steady-state error). The
same functions are imported by `test_pid_performance.py`, so the grading
tools are themselves under test.
