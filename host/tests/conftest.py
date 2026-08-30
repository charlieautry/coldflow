"""Shared fixtures for the coldflow host suite.

The suite runs two ways with the same tests:

- default: against the FakeBoard simulation. No hardware, no wires, finishes
  in seconds. This is what CI runs.
- ``pytest --port COM5``: against the real rig over usb serial.

Keeping one suite for both is the point of the whole architecture: the fake
run proves the logic reads the spec correctly, the hardware run proves the
electrons agree.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from teststand import FakeBoard, TestStand

SEQUENCE_DIR = Path(__file__).resolve().parent.parent / "sequences"


def pytest_addoption(parser):
    parser.addoption(
        "--port",
        default=None,
        help="serial port of a real board (default: run against the simulated board)",
    )


@pytest.fixture(scope="session")
def port(request):
    """The --port option, or None meaning 'use the fake'."""
    return request.config.getoption("--port")


@pytest.fixture
def make_stand(port, tmp_path, request):
    """Factory fixture: build a TestStand that is reset to IDLE and logging
    to a per-test csv.

    A factory instead of a plain fixture because some tests (pid performance)
    want to close the stand mid-test to flush the log, then read it back.
    Everything created here still gets safed and closed at teardown.
    """
    stands: list[TestStand] = []

    def _make(log_name: str = "session.csv") -> TestStand:
        log_path = tmp_path / log_name
        meta = {"test": request.node.name, "port": port or "FAKE"}
        if port:
            stand = TestStand.open(port, log_path=log_path, log_meta=meta)
        else:
            stand = TestStand(FakeBoard(), log_path=log_path, log_meta=meta)
        stand.log_path = log_path  # so tests can find their own csv
        stands.append(stand)

        # reach IDLE from whatever the last test left behind: ABORT is legal
        # everywhere, SAFE follows once the chamber is empty, CLEAR resets
        stand.set_rate(0)
        stand.abort()
        stand.wait_for_state("SAFE", timeout_s=30)
        stand.clear()
        return stand

    yield _make

    for stand in stands:
        try:
            stand.abort()  # leave real hardware in the safe state, always
        except Exception:
            pass  # a dead port at teardown shouldn't mask the test's result
        stand.close()


@pytest.fixture
def stand(make_stand):
    """A TestStand resting in IDLE with telemetry streaming off."""
    return make_stand()


@pytest.fixture
def drive(stand):
    """Put the board into any state, going through the legal path to get
    there. Returns the stand for chaining."""

    def _drive(target: str) -> TestStand:
        if target == "IDLE":
            pass
        elif target == "ARMED":
            stand.arm()
        elif target in ("PRESSURIZE", "HOLD", "VENT"):
            stand.arm()
            stand.set_pressure(10)
            stand.press()
            if target == "HOLD":
                stand.set_rate(20)
                stand.wait_for_state("HOLD", timeout_s=90)
                stand.set_rate(0)
            elif target == "VENT":
                stand.vent()
        elif target == "ABORT":
            stand.abort()
        elif target == "SAFE":
            stand.abort()
            stand.wait_for_state("SAFE", timeout_s=30)
        else:
            raise ValueError(f"don't know how to reach {target}")
        return stand

    return _drive
