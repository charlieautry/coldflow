"""DAQ-style CSV logging.

Every telemetry frame the driver sees goes through one of these. The file
leads with commented metadata (what test, when, which port, what config), the
way a real data system stamps a session, then one row per frame with the host
timestamp alongside the board's own clock. Post-test tooling in ``review``
reads this format back.
"""

from __future__ import annotations

import csv
import time
from datetime import datetime, timezone
from pathlib import Path

from teststand.protocol import Telemetry

COLUMNS = [
    "host_time_s",  # seconds since the log opened, host clock
    "t_ms",         # board milliseconds since boot
    "state",
    "psi",
    "setpoint",
    "degC",
    "flow_lpm",
    "pump",
    "valve",
    "faults",       # |-separated fault names, empty when healthy
]


class CsvLogger:
    def __init__(self, path: str | Path, meta: dict | None = None):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._f = self.path.open("w", newline="", encoding="utf-8")
        self._start = time.monotonic()
        self.rows_written = 0

        header = {
            "log_format": "teststand v1",
            "started_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        }
        header.update(meta or {})
        for key, value in header.items():
            self._f.write(f"# {key}: {value}\n")

        self._writer = csv.writer(self._f)
        self._writer.writerow(COLUMNS)

    def log(self, tel: Telemetry) -> None:
        self._writer.writerow([
            f"{time.monotonic() - self._start:.3f}",
            tel.t_ms,
            tel.state,
            f"{tel.psi:.2f}",
            "" if tel.setpoint is None else f"{tel.setpoint:.2f}",
            "" if tel.deg_c is None else f"{tel.deg_c:.1f}",
            f"{tel.flow_lpm:.2f}",
            f"{tel.pump:.2f}",
            f"{tel.valve:.2f}",
            "|".join(tel.faults),
        ])
        self.rows_written += 1

    def close(self) -> None:
        if not self._f.closed:
            self._f.flush()
            self._f.close()

    def __enter__(self) -> "CsvLogger":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
