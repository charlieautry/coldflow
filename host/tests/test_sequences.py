"""The sequence runner: yaml in, judged run out."""

import pytest

from conftest import SEQUENCE_DIR
from teststand.sequence import SequenceError, SequenceRunner, load_sequence


@pytest.mark.parametrize("name", ["smoke.yaml", "hold_10psi.yaml", "abort_injection.yaml"])
def test_the_shipped_sequences_pass(stand, name):
    """Every sequence in host/sequences/ must run clean, because those files
    are the ones that get typed at the real rig."""
    seq = load_sequence(SEQUENCE_DIR / name)
    result = SequenceRunner(stand).run(seq)
    assert result.passed, result.summary()


def test_an_expect_mismatch_fails_the_run(stand):
    """expect: 'err state' asserts a rejection; getting ok instead is a
    failure, because a guard that stopped guarding is news."""
    seq = {
        "name": "expect_mismatch",
        "steps": [{"command": "ARM", "expect": "err state"}],  # ARM from IDLE is legal
    }
    result = SequenceRunner(stand).run(seq)
    assert not result.passed
    assert "expected 'err state'" in result.steps[0].detail


def test_a_failed_step_stops_the_sequence(stand):
    seq = {
        "name": "stops_early",
        "steps": [
            {"command": "PRESS"},   # err state from IDLE -> fails here
            {"command": "ARM"},     # must never run
        ],
    }
    result = SequenceRunner(stand).run(seq)
    assert not result.passed
    assert len(result.steps) == 1, "nothing after a failed step may execute"
    assert stand.status().state == "IDLE"


def test_a_redline_trip_aborts_mid_sequence(stand):
    """The redline monitor is watching during waits and holds, and its abort
    outranks whatever the sequence wanted to do next."""
    seq = {
        "name": "redline_demo",
        "redlines": [{"field": "psi", "max": 5}],  # guaranteed to trip on the way to 10
        "steps": [
            {"command": "RATE 20"},
            {"command": "ARM"},
            {"command": "SET 10"},
            {"command": "PRESS"},
            {"wait_state": "HOLD", "timeout_s": 90},
        ],
    }
    result = SequenceRunner(stand).run(seq)
    assert not result.passed
    assert "psi_max" in result.redlines_tripped
    tel = stand.status()
    assert tel.state in ("ABORT", "SAFE"), "the monitor's ABORT must actually reach the board"
    assert "abort_cmd" in tel.faults


def test_load_sequence_rejects_a_file_without_steps(tmp_path):
    bad = tmp_path / "bad.yaml"
    bad.write_text("name: nope\n")
    with pytest.raises(SequenceError):
        load_sequence(bad)
