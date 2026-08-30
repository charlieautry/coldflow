"""Redline monitoring: limits that abort the test no matter what the
sequence thinks it's doing.

The split of duties mirrors how real stands work. The sequence runner is the
choreographer, the redline monitor is the safety officer, and the safety
officer does not ask permission: the first violated redline sends ABORT
through the driver immediately and the sequence finds out afterwards.

(The firmware has its own hard redlines underneath this, 21 psi and the loop
faults. These host-side redlines are the configurable, per-test layer that
should always be tighter than the firmware's.)
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

from teststand.protocol import Telemetry


@dataclass
class Redline:
    name: str
    violated: Callable[[Telemetry], bool]
    describe: str = ""

    @classmethod
    def maximum(cls, field: str, limit: float) -> "Redline":
        return cls(
            name=f"{field}_max",
            violated=lambda t: getattr(t, field) is not None and getattr(t, field) > limit,
            describe=f"{field} > {limit}",
        )

    @classmethod
    def minimum(cls, field: str, limit: float) -> "Redline":
        return cls(
            name=f"{field}_min",
            violated=lambda t: getattr(t, field) is not None and getattr(t, field) < limit,
            describe=f"{field} < {limit}",
        )

    @classmethod
    def forbid_fault(cls, fault: str) -> "Redline":
        return cls(
            name=f"fault_{fault}",
            violated=lambda t: fault in t.faults,
            describe=f"fault {fault} latched",
        )

    @classmethod
    def from_config(cls, cfg: dict) -> "Redline":
        """Build one redline from a sequence file entry, e.g.
        ``{field: psi, max: 18}`` or ``{fault: overpressure}``."""
        if "fault" in cfg:
            return cls.forbid_fault(cfg["fault"])
        field = cfg["field"]
        if "max" in cfg:
            return cls.maximum(field, float(cfg["max"]))
        if "min" in cfg:
            return cls.minimum(field, float(cfg["min"]))
        raise ValueError(f"redline entry needs 'max', 'min', or 'fault': {cfg!r}")


class RedlineMonitor:
    """Feed every telemetry frame through observe(). The first violation
    commands an abort through the stand and latches what tripped."""

    def __init__(self, stand, redlines: list[Redline]):
        self._stand = stand
        self.redlines = list(redlines)
        self.tripped: list[Redline] = []
        self.abort_sent = False

    def observe(self, tel: Telemetry) -> list[Redline]:
        hits = [r for r in self.redlines if r.violated(tel)]
        for r in hits:
            if r not in self.tripped:
                self.tripped.append(r)
        if hits and not self.abort_sent:
            self.abort_sent = True
            self._stand.abort()  # no discussion
        return hits

    @property
    def clean(self) -> bool:
        return not self.tripped
