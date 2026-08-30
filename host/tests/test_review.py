"""The post-test review tooling, tested on a synthetic run whose numbers are
easy to check by hand. The tools grade the real data, so the tools get tested
hardest."""

import pytest

from teststand.csvlog import CsvLogger
from teststand.protocol import Telemetry
from teststand.review import (
    channel_stats,
    faults_seen,
    load_log,
    overshoot_psi,
    plot,
    redline_proximity,
    settling_time_s,
    state_timeline,
    steady_state_error_psi,
    summarize,
)


def _frame(t_ms, state, psi, setpoint=10.0, faults=()):
    return Telemetry(
        t_ms=t_ms, state=state, psi=psi, setpoint=setpoint, deg_c=21.0,
        flow_lpm=1.0, pump=1.0, valve=0.5, faults=list(faults),
    )


@pytest.fixture
def synthetic_log(tmp_path):
    """A hand-checkable run: ramp up, overshoot by 0.4, settle, vent."""
    path = tmp_path / "run.csv"
    with CsvLogger(path, meta={"test": "synthetic"}) as log:
        log.log(_frame(0, "PRESSURIZE", 0.0))
        log.log(_frame(1000, "PRESSURIZE", 5.0))
        log.log(_frame(2000, "PRESSURIZE", 9.6))   # first frame inside the 0.5 band
        log.log(_frame(3000, "PRESSURIZE", 10.4))  # the overshoot peak
        log.log(_frame(4000, "HOLD", 10.0))
        log.log(_frame(5000, "VENT", 2.0, setpoint=None))
        log.log(_frame(6000, "SAFE", 0.0, setpoint=None))
    return load_log(path)


def test_load_log_round_trips_metadata_and_types(synthetic_log):
    assert synthetic_log.meta["test"] == "synthetic"
    assert len(synthetic_log.rows) == 7
    row = synthetic_log.rows[0]
    assert isinstance(row["psi"], float) and isinstance(row["t_ms"], int)
    assert synthetic_log.rows[-1]["setpoint"] is None, "empty csv cell reads back as None"


def test_channel_stats_are_min_max_mean(synthetic_log):
    stats = channel_stats(synthetic_log, "psi")
    assert stats["min"] == 0.0
    assert stats["max"] == 10.4
    assert stats["mean"] == pytest.approx(sum([0, 5, 9.6, 10.4, 10, 2, 0]) / 7)


def test_state_timeline_preserves_order(synthetic_log):
    names = [s for s, _, _ in state_timeline(synthetic_log)]
    assert names == ["PRESSURIZE", "HOLD", "VENT", "SAFE"]


def test_redline_proximity_measures_the_margin(synthetic_log):
    prox = redline_proximity(synthetic_log)
    assert prox["peak_psi"] == 10.4
    assert prox["margin_psi"] == pytest.approx(21.0 - 10.4)


def test_settling_time_is_first_entry_into_band_that_sticks(synthetic_log):
    # in band at t=2000 and never leaves: 2.0 s after closed loop began at t=0
    assert settling_time_s(synthetic_log, band_psi=0.5) == pytest.approx(2.0)


def test_overshoot_is_peak_above_setpoint(synthetic_log):
    assert overshoot_psi(synthetic_log) == pytest.approx(0.4)


def test_steady_state_error_uses_the_closed_loop_tail(synthetic_log):
    # closed loop ends at t=4000, so a 2 s tail is rows t=2000, 3000, 4000
    expected = (0.4 + 0.4 + 0.0) / 3
    assert steady_state_error_psi(synthetic_log, last_s=2.0) == pytest.approx(expected)


def test_faults_seen_collects_names_once(tmp_path):
    path = tmp_path / "faulty.csv"
    with CsvLogger(path) as log:
        log.log(_frame(0, "ABORT", 22.0, faults=["overpressure"]))
        log.log(_frame(100, "ABORT", 20.0, faults=["overpressure", "abort_cmd"]))
    assert faults_seen(load_log(path)) == ["overpressure", "abort_cmd"]


def test_summarize_mentions_the_things_a_reviewer_scans_for(synthetic_log):
    text = summarize(synthetic_log)
    assert "psi" in text and "redline" in text and "PRESSURIZE" in text


def test_plot_writes_a_png(synthetic_log, tmp_path):
    out = plot(synthetic_log, tmp_path / "run.png")
    assert out.exists() and out.stat().st_size > 0
