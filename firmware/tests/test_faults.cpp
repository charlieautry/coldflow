#include "faults.h"
#include "test_support.h"

using testsupport::check;

static void testLatchAndClear() {
    FaultFlags f;
    check(!f.any(), "starts with no faults");

    f.latch(Fault::Overpressure);
    check(f.has(Fault::Overpressure), "latched fault reads back");
    check(f.any(), "any() sees the latched fault");

    f.clearAll();
    check(!f.any(), "clearAll drops everything, like the CLEAR command");
}

static void testTcOpenDoesNotAbort() {
    FaultFlags f;
    f.set(Fault::TcOpen, true);
    check(f.any(), "tc_open shows up as a fault");
    check(!f.anyAborting(), "tc_open alone never trips the redline abort");

    f.latch(Fault::LoopLow);
    check(f.anyAborting(), "any other fault still aborts with tc_open present");
}

static void testTcOpenSelfClears() {
    FaultFlags f;
    f.set(Fault::TcOpen, true);
    f.set(Fault::TcOpen, false); // sensor layer reports recovery
    check(!f.has(Fault::TcOpen), "set(false) clears tc_open without a CLEAR");
}

static void testNamesRoundTrip() {
    // the names are protocol surface (telemetry json + INJECT arguments), so
    // every fault must map to a name and back
    for (uint8_t i = 0; i < static_cast<uint8_t>(Fault::Count); i++) {
        Fault f = static_cast<Fault>(i);
        Fault parsed;
        check(FaultFlags::fromName(FaultFlags::name(f), parsed), "every fault name parses back");
        check(parsed == f, "parsed fault matches the original");
    }
}

static void testUnknownNameRejected() {
    Fault f;
    check(!FaultFlags::fromName("underpressure", f), "made-up fault name is rejected");
    check(!FaultFlags::fromName("", f), "empty fault name is rejected");
}

void runFaultTests() {
    testsupport::section("faults");
    testLatchAndClear();
    testTcOpenDoesNotAbort();
    testTcOpenSelfClears();
    testNamesRoundTrip();
    testUnknownNameRejected();
}
