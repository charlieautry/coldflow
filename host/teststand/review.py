"""Post-test data review: turn a session CSV back into judgment.

Reads the format csvlog writes, produces per-channel stats, a state
timeline, redline proximity, controller performance numbers (settling time,
overshoot, steady-state error), and optionally a plot. The pytest PID
performance tests import these functions and assert on their outputs, so the
review tooling is itself under test, which is exactly how it should be.
"""

from __future__ import annotations

import csv
from dataclasses import dataclass, field
from pathlib import Path

FIRMWARE_ABORT_PSI = 21.0


@dataclass
class LogData:
    meta: dict
    rows: list[dict] = field(default_factory=list)

    def column(self, name: str) -> list:
        return [r[name] for r in self.rows if r[name] is not None]


def load_log(path: str | Path) -> LogData:
    meta: dict = {}
    body: list[str] = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                key, _, value = line[1:].partition(":")
                meta[key.strip()] = value.strip()
            else:
                body.append(line)

    rows = []
    for raw in csv.DictReader(body):
        rows.append({
            "host_time_s": float(raw["host_time_s"]),
            "t_ms": int(raw["t_ms"]),
            "state": raw["state"],
            "psi": float(raw["psi"]),
            "setpoint": float(raw["setpoint"]) if raw["setpoint"] else None,
            "degC": float(raw["degC"]) if raw["degC"] else None,
            "flow_lpm": float(raw["flow_lpm"]),
            "pump": float(raw["pump"]),
            "valve": float(raw["valve"]),
            "faults": raw["faults"].split("|") if raw["faults"] else [],
        })
    return LogData(meta=meta, rows=rows)


# -- summaries -------------------------------------------------------------

def channel_stats(log: LogData, name: str) -> dict:
    values = log.column(name)
    if not values:
        return {"min": None, "max": None, "mean": None}
    return {"min": min(values), "max": max(values), "mean": sum(values) / len(values)}


def state_timeline(log: LogData) -> list[tuple[str, int, int]]:
    """[(state, entered_t_ms, left_t_ms)], in order."""
    spans = []
    for row in log.rows:
        if not spans or spans[-1][0] != row["state"]:
            if spans:
                spans[-1] = (spans[-1][0], spans[-1][1], row["t_ms"])
            spans.append((row["state"], row["t_ms"], row["t_ms"]))
        else:
            spans[-1] = (spans[-1][0], spans[-1][1], row["t_ms"])
    return spans


def redline_proximity(log: LogData, limit_psi: float = FIRMWARE_ABORT_PSI) -> dict:
    peak = channel_stats(log, "psi")["max"] or 0.0
    return {
        "limit_psi": limit_psi,
        "peak_psi": peak,
        "margin_psi": limit_psi - peak,
        "fraction_used": peak / limit_psi if limit_psi else None,
    }


def faults_seen(log: LogData) -> list[str]:
    seen: list[str] = []
    for row in log.rows:
        for f in row["faults"]:
            if f and f not in seen:
                seen.append(f)
    return seen


# -- controller performance ------------------------------------------------

def _closed_loop_rows(log: LogData) -> list[dict]:
    return [r for r in log.rows if r["state"] in ("PRESSURIZE", "HOLD") and r["setpoint"]]


def settling_time_s(log: LogData, band_psi: float = 0.5) -> float | None:
    """Board seconds from entering PRESSURIZE until psi enters the band and
    never leaves it again. None if it never settles."""
    rows = _closed_loop_rows(log)
    if not rows:
        return None
    start = rows[0]["t_ms"]
    settled_at = None
    for r in rows:
        inside = abs(r["psi"] - r["setpoint"]) <= band_psi
        if inside and settled_at is None:
            settled_at = r["t_ms"]
        elif not inside:
            settled_at = None  # left the band, that settle didn't count
    return None if settled_at is None else (settled_at - start) / 1000.0


def overshoot_psi(log: LogData) -> float | None:
    rows = _closed_loop_rows(log)
    if not rows:
        return None
    return max(0.0, max(r["psi"] - r["setpoint"] for r in rows))


def steady_state_error_psi(log: LogData, last_s: float = 3.0) -> float | None:
    """Mean |error| over the last N board-seconds of closed-loop data."""
    rows = _closed_loop_rows(log)
    if not rows:
        return None
    cutoff = rows[-1]["t_ms"] - last_s * 1000
    tail = [r for r in rows if r["t_ms"] >= cutoff]
    return sum(abs(r["psi"] - r["setpoint"]) for r in tail) / len(tail)


# -- the report ------------------------------------------------------------

def summarize(log: LogData) -> str:
    lines = [f"# review: {log.meta.get('test', log.meta.get('log_format', '?'))}"]
    lines.append(f"frames: {len(log.rows)}")
    if log.rows:
        dur = (log.rows[-1]["t_ms"] - log.rows[0]["t_ms"]) / 1000.0
        lines.append(f"duration: {dur:.1f} s (board clock)")
    for ch in ("psi", "flow_lpm", "degC", "pump", "valve"):
        s = channel_stats(log, ch)
        if s["min"] is not None:
            lines.append(f"{ch:9s} min {s['min']:7.2f}  max {s['max']:7.2f}  mean {s['mean']:7.2f}")
    prox = redline_proximity(log)
    lines.append(f"redline:  peak {prox['peak_psi']:.2f} psi of {prox['limit_psi']:.0f} "
                 f"({100 * prox['fraction_used']:.0f}% used, {prox['margin_psi']:.2f} psi margin)")
    lines.append("states:   " + " -> ".join(s for s, _, _ in state_timeline(log)))
    f = faults_seen(log)
    lines.append(f"faults:   {', '.join(f) if f else 'none'}")
    st = settling_time_s(log)
    if st is not None:
        lines.append(f"settling: {st:.1f} s   overshoot: {overshoot_psi(log):.2f} psi   "
                     f"ss error: {steady_state_error_psi(log):.2f} psi")
    return "\n".join(lines)


def plot(log: LogData, out_path: str | Path) -> Path:
    """Pressure + setpoint on top, actuators below, saved as a png."""
    import matplotlib
    matplotlib.use("Agg")  # file output only, no display needed
    import matplotlib.pyplot as plt

    t = [r["t_ms"] / 1000.0 for r in log.rows]
    fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True, figsize=(10, 7))

    ax1.plot(t, [r["psi"] for r in log.rows], label="psi")
    ax1.plot(t, [r["setpoint"] for r in log.rows], linestyle="--", label="setpoint")
    ax1.axhline(FIRMWARE_ABORT_PSI, color="red", linewidth=0.8, label=f"abort {FIRMWARE_ABORT_PSI:.0f} psi")
    ax1.set_ylabel("psi")
    ax1.legend(loc="best")
    ax1.set_title(log.meta.get("test", "coldflow run"))

    ax2.plot(t, [r["pump"] for r in log.rows], label="pump")
    ax2.plot(t, [r["valve"] for r in log.rows], label="valve")
    ax2.set_ylabel("command 0..1")
    ax2.set_xlabel("board time [s]")
    ax2.legend(loc="best")

    # shade state changes so aborts are impossible to miss
    for name, start, end in state_timeline(log):
        if name == "ABORT":
            ax1.axvspan(start / 1000.0, end / 1000.0, color="red", alpha=0.15)

    out = Path(out_path)
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    plt.close(fig)
    return out
