"""Sequences as data: test procedures live in YAML files, this runs them.

A sequence file looks like:

    name: hold_10psi
    description: pressurize to 10 psi, hold, come home
    redlines:
      - {field: psi, max: 18}
    steps:
      - {command: CLEAR}
      - {command: ARM}
      - {command: SET 10}
      - {command: PRESS}
      - {wait_state: HOLD, timeout_s: 60}
      - {hold_s: 5}
      - {check: {field: psi, min: 9.5, max: 10.5}}
      - {command: VENT}
      - {wait_state: SAFE, timeout_s: 30}

Step types:
- ``command``: send it, expect ``ok`` (or set ``expect: "err state"`` to
  assert a rejection, which is how spec-compliance sequences are written)
- ``wait_state``: watch telemetry until the state shows up, or time out
- ``hold_s``: sit for N seconds of board time, still watching redlines
- ``check``: one telemetry frame, assert a field is inside [min, max] or a
  fault name is present (``fault: overpressure``)

Redlines declared in the file are watched during every wait and hold, and a
trip aborts the run on the spot.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

import yaml

from teststand.protocol import CommandError, Telemetry
from teststand.redline import Redline, RedlineMonitor


class SequenceError(Exception):
    pass


@dataclass
class StepResult:
    index: int
    step: dict
    ok: bool
    detail: str = ""


@dataclass
class SequenceResult:
    name: str
    passed: bool
    steps: list[StepResult] = field(default_factory=list)
    redlines_tripped: list[str] = field(default_factory=list)

    def summary(self) -> str:
        done = sum(1 for s in self.steps if s.ok)
        verdict = "PASS" if self.passed else "FAIL"
        lines = [f"{verdict}: {self.name} ({done}/{len(self.steps)} steps ok)"]
        for s in self.steps:
            mark = "ok " if s.ok else "FAIL"
            lines.append(f"  [{mark}] step {s.index}: {s.step} {('- ' + s.detail) if s.detail else ''}")
        if self.redlines_tripped:
            lines.append(f"  redlines tripped: {', '.join(self.redlines_tripped)}")
        return "\n".join(lines)


def load_sequence(path: str | Path) -> dict:
    with open(path, encoding="utf-8") as f:
        seq = yaml.safe_load(f)
    if not isinstance(seq, dict) or "steps" not in seq or not isinstance(seq["steps"], list):
        raise SequenceError(f"{path}: a sequence needs a 'steps' list")
    seq.setdefault("name", Path(path).stem)
    return seq


class SequenceRunner:
    def __init__(self, stand):
        self._stand = stand

    def run(self, seq: dict) -> SequenceResult:
        monitor = RedlineMonitor(
            self._stand, [Redline.from_config(c) for c in seq.get("redlines", [])]
        )
        result = SequenceResult(name=seq.get("name", "?"), passed=True)

        for i, step in enumerate(seq["steps"]):
            if monitor.abort_sent:
                result.steps.append(StepResult(i, step, False, "skipped, redline abort"))
                result.passed = False
                continue
            try:
                detail = self._run_step(step, monitor)
                result.steps.append(StepResult(i, step, True, detail))
            except (SequenceError, CommandError, TimeoutError) as e:
                result.steps.append(StepResult(i, step, False, str(e)))
                result.passed = False
                break  # a failed step invalidates everything after it

        if monitor.tripped:
            result.passed = False
            result.redlines_tripped = [r.name for r in monitor.tripped]
        return result

    # -- steps -------------------------------------------------------------

    def _run_step(self, step: dict, monitor: RedlineMonitor) -> str:
        if "command" in step:
            return self._step_command(step)
        if "wait_state" in step:
            return self._step_wait_state(step, monitor)
        if "hold_s" in step:
            return self._step_hold(step, monitor)
        if "check" in step:
            return self._step_check(step)
        raise SequenceError(f"unrecognized step: {step!r}")

    def _step_command(self, step: dict) -> str:
        expect = step.get("expect", "ok")
        try:
            self._stand.command(str(step["command"]))
            got = "ok"
        except CommandError as e:
            got = f"err {e.reason}"
        if got != expect:
            raise SequenceError(f"expected {expect!r}, got {got!r}")
        return got

    def _step_wait_state(self, step: dict, monitor: RedlineMonitor) -> str:
        target = str(step["wait_state"])
        timeout = float(step.get("timeout_s", 30.0))

        def done(tel: Telemetry) -> bool:
            monitor.observe(tel)
            return tel.state == target or monitor.abort_sent

        tel = self._stand.wait_until(done, timeout, desc=f"state {target}")
        if monitor.abort_sent and tel.state != target:
            raise SequenceError(f"redline abort while waiting for {target}")
        return f"reached {target} at t={tel.t_ms} ms"

    def _step_hold(self, step: dict, monitor: RedlineMonitor) -> str:
        hold_ms = int(float(step["hold_s"]) * 1000)
        start = self._stand.status().t_ms

        def done(tel: Telemetry) -> bool:
            monitor.observe(tel)
            return tel.t_ms - start >= hold_ms or monitor.abort_sent

        self._stand.wait_until(done, timeout_s=hold_ms / 1000.0 + 30.0, desc=f"{hold_ms} ms hold")
        if monitor.abort_sent:
            raise SequenceError("redline abort during hold")
        return f"held {hold_ms} ms"

    def _step_check(self, step: dict) -> str:
        cfg = step["check"]
        tel = self._stand.status()
        if "fault" in cfg:
            if cfg["fault"] not in tel.faults:
                raise SequenceError(f"fault {cfg['fault']!r} not latched (faults={tel.faults})")
            return f"fault {cfg['fault']} latched"
        value = getattr(tel, cfg["field"])
        if value is None:
            raise SequenceError(f"{cfg['field']} is null")
        lo, hi = cfg.get("min"), cfg.get("max")
        if lo is not None and value < float(lo):
            raise SequenceError(f"{cfg['field']}={value} below {lo}")
        if hi is not None and value > float(hi):
            raise SequenceError(f"{cfg['field']}={value} above {hi}")
        return f"{cfg['field']}={value} within [{lo}, {hi}]"
