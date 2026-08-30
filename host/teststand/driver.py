"""The TestStand driver: one object per serial connection to the board.

Wraps anything pyserial-shaped (a real ``serial.Serial`` or a ``FakeBoard``)
and speaks the protocol from docs/firmware-spec.md. Telemetry frames that
arrive interleaved with command responses are never lost: every frame passes
through ``_handle_telemetry`` and lands in the ring buffer and the CSV log
regardless of what the caller was doing at the time.
"""

from __future__ import annotations

import time
from collections import deque
from pathlib import Path

from teststand.csvlog import CsvLogger
from teststand.protocol import CommandError, Telemetry, is_telemetry, parse_response


class TestStand:
    def __init__(self, ser, log_path: str | Path | None = None, log_meta: dict | None = None):
        """``ser`` needs write()/readline()/close() and a ``timeout`` attribute."""
        self._ser = ser
        self._logger = CsvLogger(log_path, log_meta) if log_path else None
        self.latest: Telemetry | None = None
        self.history: deque[Telemetry] = deque(maxlen=20000)

    @classmethod
    def open(cls, port: str, baud: int = 115200, timeout: float = 1.0, **kwargs) -> "TestStand":
        import serial  # imported here so the fake path never needs pyserial

        return cls(serial.Serial(port, baud, timeout=timeout), **kwargs)

    # -- plumbing ----------------------------------------------------------

    def _read_line(self) -> str | None:
        raw = self._ser.readline()
        if not raw:
            return None  # serial timeout, nothing arrived
        return raw.decode("ascii", errors="replace").strip()

    def _handle_telemetry(self, line: str) -> Telemetry:
        tel = Telemetry.from_line(line)
        self.latest = tel
        self.history.append(tel)
        if self._logger:
            self._logger.log(tel)
        return tel

    # -- the protocol ------------------------------------------------------

    def command(self, line: str, timeout_s: float = 2.0) -> None:
        """Send one command, absorb telemetry until its response arrives.

        Returns on ``ok``, raises CommandError on ``err``, TimeoutError if the
        board never answers (which on this rig means the usb cable, not the
        firmware, since every command is acked).
        """
        self._ser.write((line + "\n").encode("ascii"))
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            raw = self._read_line()
            if raw is None or raw == "":
                continue
            if is_telemetry(raw):
                self._handle_telemetry(raw)
                continue
            reason = parse_response(raw)
            if reason is None:
                return
            raise CommandError(reason, line)
        raise TimeoutError(f"no response to {line!r} within {timeout_s} s")

    # -- the verbs, one per spec command -----------------------------------

    def arm(self) -> None:
        self.command("ARM")

    def disarm(self) -> None:
        self.command("DISARM")

    def set_pressure(self, psi: float) -> None:
        self.command(f"SET {psi:g}")

    def press(self) -> None:
        self.command("PRESS")

    def vent(self) -> None:
        self.command("VENT")

    def abort(self) -> None:
        self.command("ABORT")

    def clear(self) -> None:
        self.command("CLEAR")

    def inject(self, fault: str) -> None:
        self.command(f"INJECT {fault}")

    def set_rate(self, hz: float) -> None:
        self.command(f"RATE {hz:g}")

    def status(self) -> Telemetry:
        """One fresh frame on demand, whatever the streaming rate is."""
        before = self.latest
        self.command("STATUS")
        if self.latest is None or self.latest is before:
            raise TimeoutError("STATUS acked but no telemetry frame arrived")
        return self.latest

    # -- waiting on the stream ---------------------------------------------

    def wait_until(self, predicate, timeout_s: float = 30.0, desc: str = "condition") -> Telemetry:
        """Watch telemetry until ``predicate(frame)`` is true.

        Rides the streaming rate when it's on; falls back to STATUS polling
        when the stream is off or quiet, so it works at RATE 0 too.
        """
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            raw = self._read_line()
            if raw is None or raw == "":
                # stream is quiet, nudge a frame out
                tel = self.status()
            elif is_telemetry(raw):
                tel = self._handle_telemetry(raw)
            else:
                continue  # a stray response line, not ours to judge here
            if predicate(tel):
                return tel
        raise TimeoutError(f"timed out after {timeout_s} s waiting for {desc}")

    def wait_for_state(self, state: str, timeout_s: float = 30.0) -> Telemetry:
        return self.wait_until(lambda t: t.state == state, timeout_s, desc=f"state {state}")

    # -- lifecycle ---------------------------------------------------------

    def close(self) -> None:
        if self._logger:
            self._logger.close()
        self._ser.close()

    def __enter__(self) -> "TestStand":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
