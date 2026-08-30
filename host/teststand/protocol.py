"""Wire protocol helpers, matching docs/firmware-spec.md exactly.

Two framing rules carry the whole protocol:
- telemetry lines are json objects, so they always start with ``{``
- command responses are ``ok`` or ``err <reason>`` and never start with ``{``

Everything in this module is pure string work so it tests without a board.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field


class CommandError(Exception):
    """A command came back ``err <reason>``.

    ``reason`` is the single lowercase word from the spec: state, range, arg,
    unknown, or fault.
    """

    def __init__(self, reason: str, command: str = ""):
        self.reason = reason
        self.command = command
        super().__init__(f"{command!r} rejected: err {reason}")


@dataclass
class Telemetry:
    """One parsed telemetry frame. Field names mirror the json keys."""

    t_ms: int
    state: str
    psi: float
    setpoint: float | None
    deg_c: float | None
    flow_lpm: float
    pump: float
    valve: float
    faults: list[str] = field(default_factory=list)

    @classmethod
    def from_line(cls, line: str) -> "Telemetry":
        d = json.loads(line)
        return cls(
            t_ms=int(d["t"]),
            state=str(d["state"]),
            psi=float(d["psi"]),
            setpoint=None if d["setpoint"] is None else float(d["setpoint"]),
            deg_c=None if d["degC"] is None else float(d["degC"]),
            flow_lpm=float(d["flow_lpm"]),
            pump=float(d["pump"]),
            valve=float(d["valve"]),
            faults=list(d["faults"]),
        )


def is_telemetry(line: str) -> bool:
    """The one-character sort that keeps the driver simple."""
    return line.startswith("{")


def parse_response(line: str) -> str | None:
    """Return None for ``ok``, the reason word for ``err <reason>``.

    Raises ValueError for anything that is neither, because a garbled response
    line means the framing broke and pretending otherwise hides real bugs.
    """
    if line == "ok":
        return None
    if line.startswith("err "):
        return line[4:].strip()
    raise ValueError(f"unparseable response line: {line!r}")
