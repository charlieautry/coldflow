"""Host-side driver and test tooling for the coldflow rig.

The layering, bottom up:

- ``protocol``  the wire format: telemetry parsing, response classification
- ``driver``    the TestStand class, one object per serial connection
- ``csvlog``    DAQ-style timestamped CSV logging of every telemetry frame
- ``fakeboard`` a simulated board + plant behind a pyserial-shaped interface,
                so the whole stack runs (and the suite passes) with no rig
- ``sequence``  sequences-as-data: YAML steps executed by a runner
- ``redline``   limit monitoring that can abort independently of the sequence
- ``review``    post-test CSV crunching: summaries, plots, pass/fail
"""

from teststand.driver import TestStand
from teststand.fakeboard import FakeBoard
from teststand.protocol import CommandError, Telemetry

__all__ = ["TestStand", "FakeBoard", "CommandError", "Telemetry"]
