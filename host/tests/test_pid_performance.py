"""Closed-loop performance, judged from the logged CSV.

The flow is the whole architecture in one test: drive the rig through a
pressurize-hold-vent run, let the driver log every frame, then hand the CSV
to the review tooling and assert on settling time, overshoot, steady state
error, and redline margin. The same test grades the fake plant today and the
real water loop in phase 2 (expect the real numbers to need retuning, that's
what the bounds are for).
"""

import pytest

from teststand.review import (
    load_log,
    overshoot_psi,
    redline_proximity,
    settling_time_s,
    steady_state_error_psi,
)

pytestmark = pytest.mark.pid

# acceptance bounds, deliberately loose enough for an untuned first pass
MAX_SETTLING_S = 45.0
MAX_OVERSHOOT_PSI = 1.5
MAX_SS_ERROR_PSI = 0.3
MIN_REDLINE_MARGIN_PSI = 3.0


@pytest.mark.parametrize("setpoint", [6.0, 10.0, 14.0])
def test_reaches_and_holds_the_setpoint(make_stand, setpoint):
    stand = make_stand(f"pid_{setpoint:g}.csv")
    stand.set_rate(20)
    stand.arm()
    stand.set_pressure(setpoint)
    stand.press()

    tel = stand.wait_for_state("HOLD", timeout_s=120)
    hold_started = tel.t_ms
    stand.wait_until(
        lambda t: t.t_ms - hold_started >= 5000, timeout_s=60, desc="5 s of HOLD data"
    )
    stand.vent()
    stand.wait_for_state("SAFE", timeout_s=30)
    stand.close()  # flush the csv

    log = load_log(stand.log_path)

    settling = settling_time_s(log, band_psi=0.5)
    assert settling is not None, "the run never settled into the band at all"
    assert settling <= MAX_SETTLING_S, f"settled in {settling:.1f} s"

    overshoot = overshoot_psi(log)
    assert overshoot <= MAX_OVERSHOOT_PSI, f"overshot by {overshoot:.2f} psi"

    ss_error = steady_state_error_psi(log)
    assert ss_error <= MAX_SS_ERROR_PSI, f"steady state error {ss_error:.2f} psi"

    margin = redline_proximity(log)["margin_psi"]
    assert margin >= MIN_REDLINE_MARGIN_PSI, f"came within {margin:.2f} psi of the redline"


def test_retarget_mid_hold_tracks_the_new_setpoint(make_stand):
    """SET while holding retargets the running loop without a state change."""
    stand = make_stand("pid_retarget.csv")
    stand.set_rate(20)
    stand.arm()
    stand.set_pressure(8)
    stand.press()
    stand.wait_for_state("HOLD", timeout_s=120)

    stand.set_pressure(12)
    tel = stand.wait_until(
        lambda t: abs(t.psi - 12) <= 0.5, timeout_s=60, desc="pressure tracking 12 psi"
    )
    assert tel.state in ("HOLD", "PRESSURIZE"), "retargeting is not an excuse to abort"
    assert tel.setpoint == 12
