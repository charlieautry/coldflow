# Firmware Spec

This doc is the contract between the firmware and literally everything else in the project.
The pytest suite asserts against what's written here, the Python driver parses what's written
here, and future me debugs against what's written here. If the code and this doc disagree,
one of them is wrong, and honestly it's probably the code.

## State transition table

Quick reading guide before the table dumps on you:

- The output columns (Pump / Vent / Valve) are what gets commanded the moment we *enter* the
  next state.
- The vent solenoid SV-201 is normally open, which means "open" = de-energized (the safe,
  boring default) and "closed" = actively energized. Yes this feels backwards. No I will not
  be taking questions.
- The Response column only applies to rows triggered by a command. Some rows fire on their
  own when a pressure condition is met, and those don't respond to anything, they just show
  up as a state change in the telemetry stream. Those are marked "auto".

| Current State | Event                                                                                 | Guard                                                                                                 | Next State | Pump | Vent   | Valve  | Response  |
| ------------- | ------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- | ---------- | ---- | ------ | ------ | --------- |
| IDLE          | ARM command received                                                                  | no fault is latched, chamber's pressure is below 1 psi, and the PT-101 loop current is in valid range | ARMED      | off  | open   | closed | ok        |
| ARMED         | PRESS command received                                                                | no fault is latched, psi is still below 1, and a valid setpoint (5-15 psi) has been received via SET  | PRESSURIZE | on   | closed | PID    | ok        |
| ARMED         | DISARM command received                                                               | none                                                                                                  | IDLE       | off  | open   | closed | ok        |
| PRESSURIZE    | chamber pressure within 0.5 psi of setpoint, held continuously for 2 s              | no fault is latched                                                                                   | HOLD       | on   | closed | PID    | auto      |
| PRESSURIZE    | VENT command received                                                                 | none, venting is always allowed                                                                       | VENT       | off  | open   | open   | ok        |
| HOLD          | VENT command received                                                                 | none, venting is always allowed                                                                       | VENT       | off  | open   | open   | ok        |
| VENT          | chamber pressure below 0.5 psi, held continuously for 2 s                             | none                                                                                                  | SAFE       | off  | open   | closed | auto      |
| *any state* | ABORT command received, or redline tripped (psi >= 21, PT-101 loop fault, stale read) | none, abort is unconditional                                                                          | ABORT      | off  | open   | open   | ok / auto |
| ABORT         | chamber pressure below 0.5 psi, held continuously for 2 s                             | pressure reading must be fresh, a stale read keeps us parked in ABORT (fault stays latched)          | SAFE       | off  | open   | closed | auto      |

Any command that has no row for the current state gets rejected with `err` and changes
nothing. No transition, no outputs, nothing. Sending `PRESS` while IDLE doesn't do anything
except get told no.

Some quick notes:

- HOLD has no exit row for pressure drifting out of band, on purpose. HOLD and PRESSURIZE
  command identical outputs, so a drop-back row would just flap the state name in telemetry
  without touching a single actuator. Sagging pressure is the PID's job to fix and the
  redline monitor's job to notice.
- ABORT never rolls into SAFE on a stale reading. SAFE is the gate to CLEAR and re-arming,
  and a frozen sensor doesn't get to hand out that permission. ABORT's outputs are already
  the safe configuration, so waiting there costs nothing, and the mechanical gauge is the
  tiebreaker if the transducer never comes back.

## Serial protocol

### Framing

The whole protocol is newline-delimited ASCII over USB serial (115200 baud). One command per
line, terminated by `\n`. Commands are case-insensitive, arguments are space-separated.
Nothing fancy, and that's on purpose, because I want to be able to debug this thing from a
bare serial monitor at 11pm.

Two rules that everything else hangs off of:

- Every command gets exactly one response line back: `ok` or `err <reason>`. Responses never
  start with `{`.
- Telemetry lines are JSON objects, so they *always* start with `{`. That means the host can
  sort the interleaved stream by looking at one character, which keeps the Python driver
  from needing anything clever.

Error reasons are single lowercase words: `state` (command doesn't apply to the current
state), `range` (argument or precondition out of its valid range), `arg` (couldn't parse the
argument), `unknown` (never heard of that command), `fault` (refused because a fault is
latched).

### Command reference

**`ARM`**
Valid in: IDLE
Guards per the transition table (no latched fault, psi < 1, loop current valid). Enters ARMED.
Errors: `err state`, `err fault`, `err range` (chamber not empty or loop invalid).
Example: `ARM` returns `ok`

**`DISARM`**
Valid in: ARMED
Stand down, back to IDLE, all outputs safe. Never refused while ARMED.
Errors: `err state`.
Example: `DISARM` returns `ok`

**`SET <psi>`**
Valid in: IDLE, ARMED, PRESSURIZE, HOLD
Stores the pressure target. If we're already in PRESSURIZE or HOLD, the running PID
retargets immediately.
Errors: `err range` if psi is outside 5-15, `err arg`, `err state`.
Example: `SET 10.5` returns `ok`

**`PRESS`**
Valid in: ARMED
Starts closed-loop pressurization toward the stored setpoint. Refused if no setpoint has
been stored since boot or since the last `CLEAR`, because pressurizing toward a target that
doesn't exist is how you meet the relief valve.
Errors: `err state`, `err fault`, `err range` (no setpoint stored).
Example: `PRESS` returns `ok`

**`VENT`**
Valid in: PRESSURIZE, HOLD
Ends the run on purpose: pump off, vent open, valve open. Enters VENT, then rolls into SAFE
on its own once the chamber is empty.
Errors: `err state`.
Example: `VENT` returns `ok`

**`ABORT`**
Valid in: all states
The big red button. Pump off, vent open, valve open, latch the `abort_cmd` fault, enter
ABORT. Never refused, and it jumps the line ahead of any other pending command.
Example: `ABORT` returns `ok`

**`CLEAR`**
Valid in: SAFE
Unlatches all faults, throws away the stored setpoint, and returns to IDLE. Refused while
pressure is still >= 1 psi, you don't get to pretend nothing happened until the chamber
agrees.
Errors: `err state`, `err range` (chamber not empty).
Example: `CLEAR` returns `ok`

**`STATUS`**
Valid in: all states
Emits one telemetry line right now (regardless of the streaming rate), then responds.
Example: `STATUS` returns `{...}` then `ok`

**`RATE <hz>`**
Valid in: all states
Sets the telemetry streaming rate, 0-50 Hz, where 0 turns streaming off. Boot default is
10 Hz.
Errors: `err range`, `err arg`.
Example: `RATE 20` returns `ok`

**`INJECT <fault>`**
Valid in: all states
The test hook. Latches the named fault exactly as if its real trigger had fired, including
any redline abort that fault would cause. This is how the pytest suite proves the abort path
works without ever actually over-pressurizing my chamber. Argument is a fault name from the
fault table below.
Errors: `err arg` (unrecognized fault name).
Example: `INJECT overpressure` returns `ok`, then the board aborts and telemetry shows ABORT

### Telemetry

One JSON object per line, streamed at whatever `RATE` is set to. Looks like this:

```json
{"t": 123456, "state": "HOLD", "psi": 10.48, "setpoint": 10.5, "degC": 21.3,
 "flow_lpm": 1.24, "pump": 1.0, "valve": 0.62, "faults": []}
```

Field by field:

- `t`: milliseconds since boot (uint32, wraps after ~49.7 days, which this rig will never see)
- `state`: current state name, uppercase
- `psi`: chamber pressure from PT-101, converted using the *measured* sense-resistor value,
  not the number printed on the bag
- `setpoint`: stored target psi, `null` if none set
- `degC`: chamber temperature from TT-101, `null` if the thermocouple is faulted
- `flow_lpm`: flow from the period between hall pulses, `0.0` if no pulse in the last second
- `pump`: commanded pump duty, 0.0 to 1.0
- `valve`: commanded needle-valve position, 0.0 (closed) to 1.0 (open)
- `faults`: array of latched fault names, empty when everything is healthy

State transitions that happen without a command (the "auto" rows, redline aborts) only show
up as the `state` field changing between telemetry lines, so anything watching the board
needs to actually watch.

### Faults

| Fault            | Trigger                                          | Latches | Causes abort | Cleared by                            |
| ---------------- | ------------------------------------------------ | ------- | ------------ | ------------------------------------- |
| `overpressure` | psi >= 21                                        | yes     | yes          | `CLEAR`                             |
| `loop_low`     | PT-101 loop current < 3.5 mA (broken loop)       | yes     | yes          | `CLEAR`                             |
| `loop_high`    | PT-101 loop current > 21 mA (short / over-range) | yes     | yes          | `CLEAR`                             |
| `stale`        | no fresh PT-101 sample for 100 ms                | yes     | yes          | `CLEAR`                             |
| `tc_open`      | MAX31855 reports open/shorted thermocouple       | no      | no           | self-clears when the reading recovers |
| `abort_cmd`    | `ABORT` command received                       | yes     | (is one)     | `CLEAR`                             |

Any fault with "causes abort = yes" trips the redline row of the transition table the moment
it latches, from any state, including IDLE and ARMED. The one exception to the panic is
`tc_open`: temperature is a nice-to-have channel on this rig, so a flaky thermocouple gets
reported and shrugged at rather than ending the test.
