"""A simulated board behind a pyserial-shaped interface.

This is the firmware spec re-implemented in Python on top of a toy plant, so
the entire host stack (driver, sequences, redlines, the pytest suite) runs
with zero hardware. It is deliberately written FROM THE SPEC, not from the
C++: when a suite that passes against this fake also passes against the real
board, that's evidence both implementations read the spec the same way.

Time is simulated and only advances while the host is interacting (each
command costs a few fake milliseconds, and a blocked readline() advances time
until telemetry is due). A full pressurize-hold-vent run takes wall-clock
milliseconds, which is what makes the PID performance tests cheap to run.
"""

from __future__ import annotations

import json
from collections import deque

ABORTING_FAULTS = ("overpressure", "loop_low", "loop_high", "stale", "abort_cmd")
ALL_FAULTS = ABORTING_FAULTS + ("tc_open",)

# spec constants
SETPOINT_MIN, SETPOINT_MAX = 5.0, 15.0
ARM_MAX_PSI = 1.0
BAND_PSI = 0.5
EMPTY_PSI = 0.5
DWELL_MS = 2000
ABORT_PSI = 21.0
RATE_MAX = 50.0


class _Plant:
    """First order fake physics, tuned to feel like the real rig's numbers:
    pump dead-heads around 30 psi (its relief), the needle valve is the bleed
    path, the vent empties the chamber in well under a second."""

    def __init__(self):
        self.psi = 0.0

    def step(self, dt_s: float, pump: float, valve_open: float, vent_open: bool) -> None:
        inflow = 14.0 * pump * max(0.0, 1.0 - self.psi / 30.0)  # relief tapers it off
        out_valve = 3.0 * valve_open * self.psi
        out_vent = 8.0 * self.psi if vent_open else 0.0
        self.psi = max(0.0, self.psi + (inflow - out_valve - out_vent) * dt_s)


class FakeBoard:
    """Duck-typed like serial.Serial as far as TestStand cares."""

    def __init__(self):
        self.timeout = 1.0  # seconds of *simulated* quiet before readline gives up

        self._out: deque[bytes] = deque()
        self._partial = b""

        self.t_ms = 0
        self.state = "SAFE"
        self.faults: list[str] = []  # latch order preserved, like the firmware
        self.setpoint: float | None = None
        self.rate_hz = 10.0
        self._next_emit_ms = 0
        self._dwell_start_ms: int | None = None

        # outputs
        self.pump = 0.0
        self.valve = 0.0  # 0 closed, 1 open
        self.vent_open = True

        # pid mirror of the firmware's placeholder tuning
        self._kp, self._ki, self._kd = 0.08, 0.05, 0.01
        self._integ = 0.0
        self._prev_psi = 0.0

        self.plant = _Plant()

    # -- pyserial face -----------------------------------------------------

    def write(self, data: bytes) -> int:
        self._partial += data
        while b"\n" in self._partial:
            line, self._partial = self._partial.split(b"\n", 1)
            self._advance(5)  # command handling isn't free on real hardware either
            self._handle(line.decode("ascii", errors="replace").strip())
        return len(data)

    def readline(self) -> bytes:
        sim_deadline = self.t_ms + int(self.timeout * 1000)
        while not self._out:
            if self.t_ms >= sim_deadline:
                return b""
            self._advance(10)
        return self._out.popleft()

    @property
    def in_waiting(self) -> int:
        return sum(len(b) for b in self._out)

    def close(self) -> None:
        pass

    def reset_input_buffer(self) -> None:
        self._out.clear()

    # -- the fake firmware -------------------------------------------------

    def _respond(self, text: str) -> None:
        self._out.append((text + "\n").encode("ascii"))

    def _latch(self, fault: str) -> None:
        if fault not in self.faults:
            self.faults.append(fault)

    def _enter(self, state: str) -> None:
        self.state = state
        self._dwell_start_ms = None
        if state in ("IDLE", "ARMED", "SAFE"):
            self.pump, self.vent_open, self.valve = 0.0, True, 0.0
        elif state in ("PRESSURIZE", "HOLD"):
            self.pump, self.vent_open = 1.0, False
            # valve stays wherever the pid put it
        elif state in ("VENT", "ABORT"):
            self.pump, self.vent_open, self.valve = 0.0, True, 1.0

    def _abort(self, fault: str) -> None:
        self._latch(fault)
        self._enter("ABORT")

    def _handle(self, line: str) -> None:
        if not line:
            return
        parts = line.upper().split()
        word, args = parts[0], parts[1:]

        if word == "ABORT":
            self._abort("abort_cmd")
            self._respond("ok")
        elif word == "ARM":
            if self.state != "IDLE":
                self._respond("err state")
            elif self.faults and any(f in ABORTING_FAULTS for f in self.faults):
                self._respond("err fault")
            elif self.plant.psi >= ARM_MAX_PSI:
                self._respond("err range")
            else:
                self._enter("ARMED")
                self._respond("ok")
        elif word == "DISARM":
            if self.state != "ARMED":
                self._respond("err state")
            else:
                self._enter("IDLE")
                self._respond("ok")
        elif word == "SET":
            value = self._float_arg(args)
            if value is None:
                self._respond("err arg")
            elif self.state not in ("IDLE", "ARMED", "PRESSURIZE", "HOLD"):
                self._respond("err state")
            elif not SETPOINT_MIN <= value <= SETPOINT_MAX:
                self._respond("err range")
            else:
                self.setpoint = value
                self._respond("ok")
        elif word == "PRESS":
            if self.state != "ARMED":
                self._respond("err state")
            elif self.faults and any(f in ABORTING_FAULTS for f in self.faults):
                self._respond("err fault")
            elif self.setpoint is None or self.plant.psi >= ARM_MAX_PSI:
                self._respond("err range")
            else:
                self._integ = 0.0  # fresh loop, no leftover windup
                self._enter("PRESSURIZE")
                self._respond("ok")
        elif word == "VENT":
            if self.state not in ("PRESSURIZE", "HOLD"):
                self._respond("err state")
            else:
                self._enter("VENT")
                self._respond("ok")
        elif word == "CLEAR":
            if self.state != "SAFE":
                self._respond("err state")
            elif self.plant.psi >= ARM_MAX_PSI:
                self._respond("err range")
            else:
                self.faults.clear()
                self.setpoint = None
                self._enter("IDLE")
                self._respond("ok")
        elif word == "STATUS":
            self._emit()
            self._respond("ok")
        elif word == "RATE":
            value = self._float_arg(args)
            if value is None:
                self._respond("err arg")
            elif not 0.0 <= value <= RATE_MAX:
                self._respond("err range")
            else:
                self.rate_hz = value
                self._respond("ok")
        elif word == "INJECT":
            name = args[0].lower() if args else ""
            if name not in ALL_FAULTS:
                self._respond("err arg")
            elif name == "tc_open":
                self._latch(name)  # self-clears in a real board; close enough here
                self._respond("ok")
            else:
                self._abort(name)
                self._respond("ok")
        else:
            self._respond("err unknown")

    @staticmethod
    def _float_arg(args: list[str]) -> float | None:
        if len(args) != 1:
            return None
        try:
            return float(args[0])
        except ValueError:
            return None

    # -- simulated time ----------------------------------------------------

    def _advance(self, ms: int) -> None:
        for _ in range(0, ms, 10):
            self.t_ms += 10
            dt = 0.010

            # pid runs while the state machine says the valve is its problem
            if self.state in ("PRESSURIZE", "HOLD") and self.setpoint is not None:
                error = self.setpoint - self.plant.psi
                deriv = -(self.plant.psi - self._prev_psi) / dt
                candidate = (self._kp * error + self._integ + self._ki * error * dt
                             + self._kd * deriv)
                if not ((candidate > 1.0 and error > 0) or (candidate < 0.0 and error < 0)):
                    self._integ += self._ki * error * dt
                u = min(1.0, max(0.0, self._kp * error + self._integ + self._kd * deriv))
                self.valve = 1.0 - u  # reverse acting, same as the firmware
            self._prev_psi = self.plant.psi

            self.plant.step(dt, self.pump, self.valve, self.vent_open)

            # redlines
            if self.state != "ABORT":
                if self.plant.psi >= ABORT_PSI:
                    self._abort("overpressure")

            # dwell-gated auto rows
            if self.state == "PRESSURIZE" and self.setpoint is not None:
                in_band = abs(self.plant.psi - self.setpoint) <= BAND_PSI
                self._dwell("HOLD", in_band)
            elif self.state == "VENT":
                self._dwell("SAFE", self.plant.psi < EMPTY_PSI)
            elif self.state == "ABORT":
                # an INJECTed stale is a latched fault, not a live condition, so
                # it does not park us here; the firmware behaves the same way
                self._dwell("SAFE", self.plant.psi < EMPTY_PSI)

            # telemetry stream
            if self.rate_hz > 0:
                period = int(1000 / self.rate_hz)
                if self.t_ms >= self._next_emit_ms:
                    self._emit()
                    self._next_emit_ms = self.t_ms + period

    def _dwell(self, next_state: str, condition: bool) -> None:
        if not condition:
            self._dwell_start_ms = None
            return
        if self._dwell_start_ms is None:
            self._dwell_start_ms = self.t_ms
        elif self.t_ms - self._dwell_start_ms >= DWELL_MS:
            self._enter(next_state)

    def _emit(self) -> None:
        frame = {
            "t": self.t_ms,
            "state": self.state,
            "psi": round(self.plant.psi, 2),
            "setpoint": self.setpoint,
            "degC": 21.5,
            "flow_lpm": round(1.3 * self.valve * self.pump, 2),
            "pump": self.pump,
            "valve": round(self.valve, 2),
            "faults": list(self.faults),
        }
        self._out.append((json.dumps(frame) + "\n").encode("ascii"))
