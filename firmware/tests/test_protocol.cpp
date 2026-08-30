#include "protocol.h"
#include "test_support.h"

#include <cstring>

using testsupport::check;
using testsupport::checkNear;

static void testBareCommandsParse() {
    struct Case { const char* line; Command expect; };
    const Case cases[] = {
        {"ARM", Command::Arm},     {"DISARM", Command::Disarm},
        {"PRESS", Command::Press}, {"VENT", Command::Vent},
        {"ABORT", Command::Abort}, {"CLEAR", Command::Clear},
    };
    for (const auto& c : cases) {
        ParsedLine p = parseLine(c.line);
        check(p.kind == ParsedLine::Kind::SmCommand, "bare command parses as SmCommand");
        check(p.smCmd == c.expect, "bare command maps to the right Command");
    }
}

static void testCaseAndWhitespaceInsensitive() {
    check(parseLine("arm").smCmd == Command::Arm, "lowercase arm parses");
    check(parseLine("  Arm  ").smCmd == Command::Arm, "padded Arm parses");
    check(parseLine("ARM\r").smCmd == Command::Arm, "trailing cr is stripped (windows terminals)");
    check(parseLine("set\t10").kind == ParsedLine::Kind::SmCommand, "tab separator works");
}

static void testSetParsesArgument() {
    ParsedLine p = parseLine("SET 10.5");
    check(p.kind == ParsedLine::Kind::SmCommand, "SET with arg parses");
    check(p.smCmd == Command::Set, "SET maps to Command::Set");
    check(p.hasArg, "SET carries its argument");
    checkNear(p.arg, 10.5f, 1e-6f, "SET argument value survives parsing");
}

static void testSetBadArguments() {
    check(parseLine("SET").kind == ParsedLine::Kind::BadArg, "SET with no arg is err arg");
    check(parseLine("SET pants").kind == ParsedLine::Kind::BadArg, "SET pants is err arg");
    check(parseLine("SET 10 junk").kind == ParsedLine::Kind::BadArg, "trailing junk after the number is err arg");
}

static void testRateAndStatus() {
    ParsedLine r = parseLine("RATE 20");
    check(r.kind == ParsedLine::Kind::Rate, "RATE parses");
    checkNear(r.arg, 20.0f, 1e-6f, "RATE argument survives");
    check(parseLine("RATE").kind == ParsedLine::Kind::BadArg, "RATE with no arg is err arg");
    check(parseLine("STATUS").kind == ParsedLine::Kind::Status, "STATUS parses");
}

static void testInjectParsesFaultNames() {
    ParsedLine p = parseLine("INJECT overpressure");
    check(p.kind == ParsedLine::Kind::Inject, "INJECT with a real fault name parses");
    check(p.fault == Fault::Overpressure, "INJECT maps the fault name");

    check(parseLine("INJECT LOOP_LOW").fault == Fault::LoopLow, "INJECT is case-insensitive too");
    check(parseLine("INJECT gremlins").kind == ParsedLine::Kind::BadArg, "unknown fault name is err arg");
    check(parseLine("INJECT").kind == ParsedLine::Kind::BadArg, "INJECT with no name is err arg");
}

static void testUnknownAndEmpty() {
    check(parseLine("FROBNICATE").kind == ParsedLine::Kind::Unknown, "unknown command is Unknown");
    check(parseLine("").kind == ParsedLine::Kind::Empty, "empty line is Empty");
    check(parseLine("   \r").kind == ParsedLine::Kind::Empty, "whitespace-only line is Empty");
}

static void testOverlongLineRejected() {
    char big[200];
    std::memset(big, 'A', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    check(parseLine(big).kind == ParsedLine::Kind::Unknown, "absurdly long line is Unknown, not a crash");
}

static void testResultText() {
    check(std::strcmp(cmdResultText(CmdResult::Ok), "ok") == 0, "Ok renders as ok");
    check(std::strcmp(cmdResultText(CmdResult::ErrState), "err state") == 0, "ErrState renders as err state");
    check(std::strcmp(cmdResultText(CmdResult::ErrRange), "err range") == 0, "ErrRange renders as err range");
    check(std::strcmp(cmdResultText(CmdResult::ErrFault), "err fault") == 0, "ErrFault renders as err fault");
}

static void testTelemetryFormat() {
    TelemetrySnapshot snap;
    snap.tMs = 123456;
    snap.state = State::Hold;
    snap.psi = 10.48f;
    snap.setpoint = 10.5f;
    snap.hasSetpoint = true;
    snap.degC = 21.3f;
    snap.flowLpm = 1.24f;
    snap.pumpDuty = 1.0f;
    snap.valvePos = 0.62f;

    char buf[200];
    int n = formatTelemetry(buf, sizeof(buf), snap);
    check(n > 0, "telemetry formats into a reasonable buffer");
    check(buf[0] == '{', "telemetry line starts with { (the framing rule)");
    check(buf[n - 1] == '\n', "telemetry line ends with newline");
    check(std::strstr(buf, "\"t\":123456") != nullptr, "t field present");
    check(std::strstr(buf, "\"state\":\"HOLD\"") != nullptr, "state field present and uppercase");
    check(std::strstr(buf, "\"psi\":10.48") != nullptr, "psi rounds to two decimals");
    check(std::strstr(buf, "\"setpoint\":10.50") != nullptr, "setpoint present when set");
    check(std::strstr(buf, "\"faults\":[]") != nullptr, "healthy board reports empty faults array");
}

static void testTelemetryNullsAndFaults() {
    TelemetrySnapshot snap;
    snap.state = State::Abort;
    snap.hasSetpoint = false;
    snap.tcValid = false;

    FaultFlags f;
    f.latch(Fault::Overpressure);
    f.latch(Fault::AbortCmd);
    snap.faultMask = f.mask();

    char buf[200];
    int n = formatTelemetry(buf, sizeof(buf), snap);
    check(n > 0, "telemetry with faults formats");
    check(std::strstr(buf, "\"setpoint\":null") != nullptr, "no setpoint renders as null");
    check(std::strstr(buf, "\"degC\":null") != nullptr, "faulted thermocouple renders as null");
    check(std::strstr(buf, "\"faults\":[\"overpressure\",\"abort_cmd\"]") != nullptr,
          "latched faults appear by name in latch order");
}

static void testTelemetryTinyBufferFails() {
    TelemetrySnapshot snap;
    char buf[20];
    check(formatTelemetry(buf, sizeof(buf), snap) == 0, "too-small buffer returns 0, not truncated json");
}

void runProtocolTests() {
    testsupport::section("protocol");
    testBareCommandsParse();
    testCaseAndWhitespaceInsensitive();
    testSetParsesArgument();
    testSetBadArguments();
    testRateAndStatus();
    testInjectParsesFaultNames();
    testUnknownAndEmpty();
    testOverlongLineRejected();
    testResultText();
    testTelemetryFormat();
    testTelemetryNullsAndFaults();
    testTelemetryTinyBufferFails();
}
