"""Driver-level behavior: framing, error mapping, logging.

These tests are about the TestStand class itself, not the rig's logic. If
they fail against the real board but pass against the fake, the wire framing
is what broke.
"""

import pytest

from teststand import CommandError


def test_status_returns_a_parsed_frame(stand):
    """STATUS produces one telemetry frame with typed fields."""
    tel = stand.status()
    assert tel.state == "IDLE"
    assert isinstance(tel.psi, float)
    assert isinstance(tel.faults, list)
    assert tel.setpoint is None, "no setpoint should be stored right after CLEAR"


def test_rejected_command_raises_with_the_reason_word(stand):
    """err <reason> comes back as CommandError.reason, not a bare failure."""
    stand.arm()
    with pytest.raises(CommandError) as excinfo:
        stand.set_pressure(40)  # spec range is 5-15
    assert excinfo.value.reason == "range"


def test_unknown_command_raises_err_unknown(stand):
    with pytest.raises(CommandError) as excinfo:
        stand.command("FROBNICATE")
    assert excinfo.value.reason == "unknown"


def test_commands_survive_an_active_telemetry_stream(stand):
    """Responses and telemetry interleave on one line stream; the driver must
    route each to the right place without dropping either."""
    stand.set_rate(20)
    stand.arm()
    stand.disarm()  # both acked correctly means no response line got eaten
    tel = stand.status()
    assert tel.state == "IDLE"
    assert len(stand.history) >= 1, "streamed frames should land in the ring buffer"


def test_every_frame_lands_in_the_csv_log(stand):
    """The log is the data product; frames observed must equal rows written."""
    stand.status()
    stand.status()
    frames_seen = len(stand.history)
    stand.close()

    text = stand.log_path.read_text()
    header_lines = [l for l in text.splitlines() if l.startswith("#")]
    assert any("log_format" in l for l in header_lines), "session header is stamped"
    assert any("test:" in l for l in header_lines), "test name is stamped"
    data_rows = [l for l in text.splitlines() if l and not l.startswith("#")]
    assert len(data_rows) - 1 == frames_seen  # minus the column header row
