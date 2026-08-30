"""The abort path, which is the part of this rig that actually matters.

Everything here leans on INJECT, the firmware's fault injection hook: it
latches a named fault exactly as if the real trigger had fired, which lets
the suite prove the abort plumbing without over-pressurizing anything."""

import pytest

from teststand import CommandError

ABORTING_FAULTS = ["overpressure", "loop_low", "loop_high", "stale"]


@pytest.mark.parametrize("fault", ABORTING_FAULTS)
def test_every_aborting_fault_actually_aborts(stand, fault):
    """Injecting any aborting fault lands in ABORT with safe outputs and the
    fault latched by name in telemetry."""
    stand.arm()
    stand.inject(fault)
    tel = stand.status()
    assert tel.state == "ABORT"
    assert fault in tel.faults
    assert tel.pump == 0.0, "abort means pump off, unconditionally"
    assert tel.valve == 1.0, "abort opens the needle valve"


@pytest.mark.parametrize("origin", ["IDLE", "ARMED", "PRESSURIZE", "HOLD", "VENT", "SAFE"])
def test_abort_command_works_from_every_state(drive, origin):
    """The spec's *any state* row, one state at a time."""
    stand = drive(origin)
    stand.abort()  # never refused
    tel = stand.status()
    assert tel.state == "ABORT"
    assert "abort_cmd" in tel.faults


def test_abort_reacts_within_the_latency_bound(stand):
    """Board time from injection to ABORT must be under 100 ms. On the real
    rig this is the number that decides whether 21 psi stays 21 psi."""
    stand.arm()
    stand.set_pressure(10)
    stand.press()
    t_before = stand.status().t_ms
    stand.inject("overpressure")
    tel = stand.status()
    assert tel.state == "ABORT"
    assert tel.t_ms - t_before <= 100, f"abort took {tel.t_ms - t_before} ms of board time"


def test_abort_recovers_to_safe_but_stays_latched(stand):
    """ABORT rolls into SAFE once the chamber is empty, but the fault stays
    latched (and visible) until an explicit CLEAR."""
    stand.arm()
    stand.inject("overpressure")
    tel = stand.wait_for_state("SAFE", timeout_s=30)
    assert "overpressure" in tel.faults, "reaching SAFE must not silently forgive the fault"

    stand.clear()
    tel = stand.status()
    assert tel.state == "IDLE"
    assert tel.faults == [], "CLEAR unlatches everything"
    stand.arm()  # and the board is genuinely reusable afterwards


def test_clear_is_refused_anywhere_but_safe(stand):
    """No pretending nothing happened while still in ABORT."""
    stand.abort()
    with pytest.raises(CommandError) as excinfo:
        stand.clear()
    # ABORT with an empty chamber rolls to SAFE quickly, so this races the
    # dwell on purpose: whichever side wins, CLEAR from ABORT itself is err
    assert excinfo.value.reason in ("state",)
