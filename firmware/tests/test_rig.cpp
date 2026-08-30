#include "rig.h"
#include "test_support.h"

#include <cstring>
#include <string>
#include <vector>

using testsupport::check;
using testsupport::checkNear;

namespace {

// Fake hardware: the test scripts the sensors and captures every output and
// serial line. std::vector is fine here, this file never runs on the pico.
class FakeHal : public Hal {
public:
    uint32_t now = 0;
    RawSensors raw;
    float pumpDuty = -1.0f;
    bool ventEnergized = false;
    float valvePos = -1.0f;
    std::vector<std::string> lines;

    FakeHal() {
        raw.pressureFresh = true;
        setPsi(0.0f);
        raw.tcRaw = 100u << 18; // healthy 25.0 C
    }

    // what would the ADC read at this chamber pressure
    void setPsi(float psi) {
        const SensorCalibration cal;
        const float ma = 4.0f + (psi / cal.fullScalePsi) * 16.0f;
        const float volts = ma * cal.senseOhms / 1000.0f;
        raw.pressureCounts = static_cast<uint16_t>((volts / cal.adcVref) * cal.adcMax + 0.5f);
    }

    uint32_t millis() override { return now; }
    RawSensors readSensors() override { return raw; }
    void setPumpDuty(float d) override { pumpDuty = d; }
    void setVentEnergized(bool e) override { ventEnergized = e; }
    void setValvePos(float p) override { valvePos = p; }
    void writeLine(const char* line) override { lines.emplace_back(line); }

    // run the rig for a stretch of simulated time at 100 Hz
    void run(Rig& rig, uint32_t ms) {
        for (uint32_t i = 0; i < ms; i += 10) {
            now += 10;
            rig.tick();
        }
    }

    std::string lastLine() const { return lines.empty() ? "" : lines.back(); }
    void clearLines() { lines.clear(); }
};

// boot the rig into IDLE with telemetry muted, the start every test wants
void bootToIdle(FakeHal& hal, Rig& rig) {
    rig.handleLine("RATE 0");
    rig.handleLine("CLEAR");
    hal.clearLines();
}

}  // namespace

static void testBootIsSafeAndClearReachesIdle() {
    FakeHal hal;
    Rig rig(hal, RigConfig{});
    check(rig.state() == State::Safe, "board powers up in SAFE");
    check(!hal.ventEnergized, "vent is de-energized (open) at boot");
    checkNear(hal.pumpDuty, 0.0f, 1e-6f, "pump is off at boot");

    rig.handleLine("CLEAR");
    check(hal.lastLine() == "ok", "CLEAR from SAFE with an empty chamber answers ok");
    check(rig.state() == State::Idle, "CLEAR lands in IDLE");
}

static void testFullHappyPath() {
    FakeHal hal;
    Rig rig(hal, RigConfig{});
    bootToIdle(hal, rig);

    rig.handleLine("ARM");
    check(hal.lastLine() == "ok", "ARM answers ok");

    rig.handleLine("SET 10");
    check(hal.lastLine() == "ok", "SET 10 answers ok");

    rig.handleLine("PRESS");
    check(hal.lastLine() == "ok", "PRESS answers ok");
    check(rig.state() == State::Pressurize, "PRESS enters PRESSURIZE");
    checkNear(hal.pumpDuty, 1.0f, 1e-6f, "pump runs in PRESSURIZE");
    check(hal.ventEnergized, "vent solenoid energizes (closes) in PRESSURIZE");

    // pressure arrives on target and dwells: auto transition to HOLD
    hal.setPsi(10.0f);
    hal.run(rig, 2500);
    check(rig.state() == State::Hold, "in-band pressure for 2 s reaches HOLD");

    // operator ends the run: VENT, chamber empties, auto to SAFE
    rig.handleLine("VENT");
    check(hal.lastLine() == "ok", "VENT answers ok");
    checkNear(hal.pumpDuty, 0.0f, 1e-6f, "pump stops in VENT");
    check(!hal.ventEnergized, "vent de-energizes (opens) in VENT");

    hal.setPsi(0.0f);
    hal.run(rig, 2500);
    check(rig.state() == State::Safe, "empty chamber for 2 s reaches SAFE");
}

static void testStatusEmitsTelemetryThenOk() {
    FakeHal hal;
    Rig rig(hal, RigConfig{});
    bootToIdle(hal, rig);

    rig.handleLine("STATUS");
    check(hal.lines.size() == 2, "STATUS produces exactly two lines");
    check(!hal.lines[0].empty() && hal.lines[0][0] == '{', "first line is telemetry json");
    check(hal.lines[1] == "ok", "second line is the ok response");
    check(hal.lines[0].find("\"state\":\"IDLE\"") != std::string::npos, "telemetry reports the state");
}

static void testTelemetryCadenceFollowsRate() {
    FakeHal hal;
    Rig rig(hal, RigConfig{});
    bootToIdle(hal, rig);

    rig.handleLine("RATE 10");
    hal.clearLines();
    hal.run(rig, 1000);
    // 10 Hz for one second = 10 frames, give or take the tick phase
    check(hal.lines.size() >= 9 && hal.lines.size() <= 11, "RATE 10 streams about 10 frames per second");

    rig.handleLine("RATE 0");
    hal.clearLines();
    hal.run(rig, 1000);
    check(hal.lines.empty(), "RATE 0 silences the stream");

    rig.handleLine("RATE 99");
    check(hal.lastLine() == "err range", "RATE above 50 answers err range");
}

static void testInjectOverpressureAborts() {
    FakeHal hal;
    Rig rig(hal, RigConfig{});
    bootToIdle(hal, rig);
    rig.handleLine("ARM");

    rig.handleLine("INJECT overpressure");
    check(hal.lastLine() == "ok", "INJECT answers ok");
    check(rig.state() == State::Abort, "injected overpressure aborts immediately");
    checkNear(hal.pumpDuty, 0.0f, 1e-6f, "abort turns the pump off");
    check(!hal.ventEnergized, "abort opens the vent");
    check(rig.faults().has(Fault::Overpressure), "the injected fault is latched");

    // the latch must survive into telemetry
    rig.handleLine("STATUS");
    check(hal.lines[hal.lines.size() - 2].find("\"faults\":[\"overpressure\"]") != std::string::npos,
          "telemetry reports the latched fault by name");
}

static void testRealOverpressureAbortsAndRecovers() {
    FakeHal hal;
    Rig rig(hal, RigConfig{});
    bootToIdle(hal, rig);
    rig.handleLine("ARM");
    rig.handleLine("SET 10");
    rig.handleLine("PRESS");

    hal.setPsi(22.0f); // past the 21 psi redline
    hal.run(rig, 20);
    check(rig.state() == State::Abort, "22 psi trips the redline abort within a tick or two");

    // chamber blows down through the open vent, rig parks itself in SAFE
    hal.setPsi(0.0f);
    hal.run(rig, 2500);
    check(rig.state() == State::Safe, "abort rolls into SAFE once the chamber is empty");
    check(rig.faults().has(Fault::Overpressure), "fault stays latched in SAFE until CLEAR");

    // re-arming without clearing is refused, then CLEAR resets the world
    rig.handleLine("CLEAR");
    check(hal.lastLine() == "ok", "CLEAR from SAFE answers ok");
    check(!rig.faults().any(), "CLEAR unlatches the faults");
    rig.handleLine("ARM");
    check(hal.lastLine() == "ok", "board re-arms after CLEAR");
}

static void testStaleSensorAbortsAndBlocksSafe() {
    FakeHal hal;
    Rig rig(hal, RigConfig{});
    bootToIdle(hal, rig);
    rig.handleLine("ARM");

    hal.raw.pressureFresh = false; // ADC stops producing samples
    hal.run(rig, 200);             // stale window is 100 ms
    check(rig.state() == State::Abort, "stale pressure data aborts");
    check(rig.faults().has(Fault::Stale), "stale fault latches");

    // chamber reads empty but the reading is stale: SAFE must stay locked out
    hal.setPsi(0.0f);
    hal.run(rig, 3000);
    check(rig.state() == State::Abort, "no SAFE on stale data, parked in ABORT");

    // sensor comes back, dwell runs, SAFE unlocks
    hal.raw.pressureFresh = true;
    hal.run(rig, 2500);
    check(rig.state() == State::Safe, "fresh data plus empty chamber finally reaches SAFE");
}

static void testPidDrivesValveTheRightDirection() {
    FakeHal hal;
    Rig rig(hal, RigConfig{});
    bootToIdle(hal, rig);
    rig.handleLine("ARM");
    rig.handleLine("SET 10");
    rig.handleLine("PRESS");

    // below setpoint: the valve should pinch down (toward closed) so the
    // pump can build pressure. the needle valve is the bleed path.
    hal.setPsi(5.0f);
    hal.run(rig, 500);
    const float valveLow = hal.valvePos;
    check(valveLow < 0.5f, "below setpoint the valve pinches toward closed");

    // above setpoint: the valve should open up and bleed pressure off
    hal.setPsi(14.0f);
    hal.run(rig, 3000);
    check(hal.valvePos > valveLow, "above setpoint the valve opens relative to the low case");
}

static void testProtocolErrorsComeBackAsLines() {
    FakeHal hal;
    Rig rig(hal, RigConfig{});
    bootToIdle(hal, rig);

    rig.handleLine("FROBNICATE");
    check(hal.lastLine() == "err unknown", "unknown command answers err unknown");

    rig.handleLine("SET pants");
    check(hal.lastLine() == "err arg", "unparseable argument answers err arg");

    rig.handleLine("PRESS");
    check(hal.lastLine() == "err state", "PRESS from IDLE answers err state");

    rig.handleLine("SET 40");
    check(hal.lastLine() == "err range", "SET outside 5-15 answers err range");
}

static void testAbortCommandLatchesAbortCmdFault() {
    FakeHal hal;
    Rig rig(hal, RigConfig{});
    bootToIdle(hal, rig);
    rig.handleLine("ARM");

    rig.handleLine("ABORT");
    check(hal.lastLine() == "ok", "ABORT always answers ok");
    check(rig.state() == State::Abort, "ABORT enters ABORT");
    check(rig.faults().has(Fault::AbortCmd), "abort_cmd fault latches");
}

void runRigTests() {
    testsupport::section("rig");
    testBootIsSafeAndClearReachesIdle();
    testFullHappyPath();
    testStatusEmitsTelemetryThenOk();
    testTelemetryCadenceFollowsRate();
    testInjectOverpressureAborts();
    testRealOverpressureAbortsAndRecovers();
    testStaleSensorAbortsAndBlocksSafe();
    testPidDrivesValveTheRightDirection();
    testProtocolErrorsComeBackAsLines();
    testAbortCommandLatchesAbortCmdFault();
}
