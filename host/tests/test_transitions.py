"""The state transition table from docs/firmware-spec.md, asserted over the
wire. The parametrized rejection list below IS the "any command with no row
gets err state" rule, written out so a spec change breaks a named test."""

import pytest

from teststand import CommandError


def test_the_full_happy_path_in_order(stand):
    """IDLE -> ARMED -> PRESSURIZE -> HOLD -> VENT -> SAFE, every stop
    checked. This is the one test to read first to learn the rig."""
    assert stand.status().state == "IDLE"

    stand.arm()
    assert stand.status().state == "ARMED"

    stand.set_pressure(10)
    stand.press()
    assert stand.status().state == "PRESSURIZE"

    stand.set_rate(20)
    tel = stand.wait_for_state("HOLD", timeout_s=90)
    assert abs(tel.psi - 10) <= 0.5, "HOLD is only declared inside the band"

    stand.vent()
    tel = stand.wait_for_state("SAFE", timeout_s=30)
    assert tel.psi < 0.5, "SAFE is only declared once the chamber is empty"


def test_disarm_backs_out_of_armed(stand):
    stand.arm()
    stand.disarm()
    assert stand.status().state == "IDLE"


# every (state, command) pair with no row in the transition table
REJECTIONS = [
    ("IDLE", "PRESS", "state"),
    ("IDLE", "VENT", "state"),
    ("IDLE", "DISARM", "state"),
    ("IDLE", "CLEAR", "state"),
    ("ARMED", "ARM", "state"),
    ("ARMED", "VENT", "state"),
    ("ARMED", "CLEAR", "state"),
    ("PRESSURIZE", "ARM", "state"),
    ("PRESSURIZE", "PRESS", "state"),
    ("VENT", "PRESS", "state"),
    ("VENT", "SET 10", "state"),
    ("SAFE", "ARM", "state"),
    ("SAFE", "SET 10", "state"),
]


@pytest.mark.parametrize(
    "origin,command,reason", REJECTIONS, ids=[f"{s}-{c.split()[0]}" for s, c, _ in REJECTIONS]
)
def test_commands_with_no_table_row_are_rejected(drive, origin, command, reason):
    """A rejected command answers err state and changes nothing."""
    stand = drive(origin)
    with pytest.raises(CommandError) as excinfo:
        stand.command(command)
    assert excinfo.value.reason == reason
    assert stand.status().state == origin, "a rejection must not move the state machine"


@pytest.mark.parametrize("psi", [4.9, 15.1, 0, -3])
def test_setpoint_outside_5_to_15_is_range_error(stand, psi):
    stand.arm()
    with pytest.raises(CommandError) as excinfo:
        stand.set_pressure(psi)
    assert excinfo.value.reason == "range"


@pytest.mark.parametrize("psi", [5, 15])
def test_setpoint_boundaries_are_inclusive(stand, psi):
    stand.arm()
    stand.set_pressure(psi)  # not raising is the assertion


def test_press_without_a_setpoint_is_refused(stand):
    """Pressurizing toward a target that doesn't exist is how you meet the
    relief valve, so the board says err range instead."""
    stand.arm()
    with pytest.raises(CommandError) as excinfo:
        stand.press()
    assert excinfo.value.reason == "range"
    assert stand.status().state == "ARMED"


def test_rate_outside_0_to_50_is_range_error(stand):
    with pytest.raises(CommandError) as excinfo:
        stand.set_rate(60)
    assert excinfo.value.reason == "range"


@pytest.mark.parametrize("line,reason", [
    ("SET pants", "arg"),
    ("INJECT gremlins", "arg"),
    ("RATE", "arg"),
    ("FROBNICATE", "unknown"),
])
def test_garbage_input_gets_the_right_reason(stand, line, reason):
    with pytest.raises(CommandError) as excinfo:
        stand.command(line)
    assert excinfo.value.reason == reason
