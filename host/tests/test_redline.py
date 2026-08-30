"""Unit tests for the redline primitives, no board involved at all."""

import pytest

from teststand.protocol import Telemetry
from teststand.redline import Redline, RedlineMonitor


class AbortCounter:
    """The one method RedlineMonitor needs from a stand."""

    def __init__(self):
        self.aborts = 0

    def abort(self):
        self.aborts += 1


def frame(psi=0.0, faults=(), state="HOLD"):
    return Telemetry(
        t_ms=0, state=state, psi=psi, setpoint=10.0, deg_c=21.0,
        flow_lpm=0.0, pump=1.0, valve=0.5, faults=list(faults),
    )


def test_maximum_trips_only_above_the_limit():
    r = Redline.maximum("psi", 18.0)
    assert not r.violated(frame(psi=18.0)), "the limit itself is still legal"
    assert r.violated(frame(psi=18.1))


def test_minimum_trips_only_below_the_limit():
    r = Redline.minimum("psi", 1.0)
    assert r.violated(frame(psi=0.5))
    assert not r.violated(frame(psi=1.0))


def test_forbid_fault_watches_the_fault_list():
    r = Redline.forbid_fault("loop_low")
    assert r.violated(frame(faults=["loop_low"]))
    assert not r.violated(frame(faults=["tc_open"]))


def test_monitor_aborts_exactly_once():
    stand = AbortCounter()
    monitor = RedlineMonitor(stand, [Redline.maximum("psi", 18.0)])

    assert monitor.observe(frame(psi=10.0)) == []
    assert stand.aborts == 0

    monitor.observe(frame(psi=19.0))
    monitor.observe(frame(psi=20.0))  # still violated, but the abort already went out
    assert stand.aborts == 1
    assert not monitor.clean
    assert [r.name for r in monitor.tripped] == ["psi_max"]


def test_from_config_builds_the_three_kinds():
    assert Redline.from_config({"field": "psi", "max": 18}).name == "psi_max"
    assert Redline.from_config({"field": "psi", "min": 1}).name == "psi_min"
    assert Redline.from_config({"fault": "stale"}).name == "fault_stale"
    with pytest.raises(ValueError):
        Redline.from_config({"field": "psi"})  # no limit given
