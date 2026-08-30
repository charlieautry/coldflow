#include "pid.h"
#include "test_support.h"

using testsupport::check;
using testsupport::checkNear;

// helper: run n identical updates, return the last output
static float runSteps(Pid& pid, int n, float setpoint, float measured, float dtS) {
    float out = 0.0f;
    for (int i = 0; i < n; i++) out = pid.update(setpoint, measured, dtS);
    return out;
}

static void testProportionalOnly() {
    PidConfig cfg;
    cfg.kp = 0.2f;
    cfg.outMax = 10.0f;
    Pid pid(cfg);

    float out = pid.update(10.0f, 8.0f, 0.01f); // error = 2
    checkNear(out, 0.4f, 1e-5f, "P-only output is kp * error");

    out = pid.update(10.0f, 10.0f, 0.01f); // error = 0
    checkNear(out, 0.0f, 1e-5f, "P-only output is zero at zero error");
}

static void testOutputClampsToLimits() {
    PidConfig cfg;
    cfg.kp = 100.0f; // silly gain so one step saturates
    cfg.outMin = 0.0f;
    cfg.outMax = 1.0f;
    Pid pid(cfg);

    checkNear(pid.update(15.0f, 0.0f, 0.01f), 1.0f, 1e-6f, "huge positive error clamps to outMax");
    checkNear(pid.update(0.0f, 15.0f, 0.01f), 0.0f, 1e-6f, "huge negative error clamps to outMin");
}

static void testIntegralAccumulates() {
    PidConfig cfg;
    cfg.ki = 1.0f;
    cfg.outMax = 10.0f;
    Pid pid(cfg);

    runSteps(pid, 3, 1.0f, 0.0f, 0.1f); // error 1 held for 0.3 s total
    checkNear(pid.integral(), 0.3f, 1e-5f, "integral is ki * error * elapsed time");
}

static void testAntiWindupHoldsIntegralAtRail() {
    PidConfig cfg;
    cfg.ki = 1.0f;
    cfg.outMin = 0.0f;
    cfg.outMax = 1.0f;
    Pid pid(cfg);

    // 10 simulated seconds pinned at the rail with error = 5. naive windup
    // would leave integral = 50; conditional integration stops near 1.
    runSteps(pid, 1000, 5.0f, 0.0f, 0.01f);
    check(pid.integral() <= cfg.outMax + 0.1f, "integral does not wind past the output rail");

    // the moment the error reverses, the output should come off the rail
    // almost immediately instead of burning off 49 units of windup first
    float out = runSteps(pid, 5, 0.0f, 5.0f, 0.01f); // error now -5
    check(out < cfg.outMax, "output leaves the rail promptly after the error reverses");
}

static void testNoDerivativeKickOnSetpointStep() {
    PidConfig cfg;
    cfg.kd = 1.0f;
    cfg.outMin = -10.0f;
    cfg.outMax = 10.0f;
    Pid pid(cfg);

    // steady measurement, then yank the setpoint. derivative-on-measurement
    // means the D term shouldn't even notice.
    pid.update(5.0f, 5.0f, 0.01f);
    float before = pid.update(5.0f, 5.0f, 0.01f);
    float after = pid.update(12.0f, 5.0f, 0.01f); // setpoint step of +7
    checkNear(after - before, 0.0f, 1e-5f, "setpoint step produces no derivative kick");
}

static void testDerivativeOpposesRisingMeasurement() {
    PidConfig cfg;
    cfg.kd = 1.0f;
    cfg.derivFilterTauS = 0.0f; // unfiltered so the sign check is exact
    cfg.outMin = -10.0f;
    cfg.outMax = 10.0f;
    Pid pid(cfg);

    pid.update(5.0f, 1.0f, 0.01f);
    float out = pid.update(5.0f, 2.0f, 0.01f); // measurement rising fast
    check(out < 0.0f, "rising measurement drives the derivative term negative");
}

static void testResetClearsHistory() {
    PidConfig cfg;
    cfg.ki = 1.0f;
    cfg.kd = 1.0f;
    cfg.outMax = 10.0f;
    Pid pid(cfg);

    runSteps(pid, 50, 2.0f, 0.0f, 0.01f);
    check(pid.integral() > 0.0f, "precondition: integral is nonzero before reset");

    pid.reset();
    checkNear(pid.integral(), 0.0f, 1e-6f, "reset clears the integral");
    checkNear(pid.output(), 0.0f, 1e-6f, "reset clears the held output");
}

static void testBadDtHoldsLastOutput() {
    PidConfig cfg;
    cfg.kp = 1.0f;
    cfg.outMax = 10.0f;
    Pid pid(cfg);

    float out = pid.update(4.0f, 2.0f, 0.01f);
    checkNear(pid.update(9.0f, 0.0f, 0.0f), out, 1e-6f, "dt = 0 holds the last output");
    checkNear(pid.update(9.0f, 0.0f, -1.0f), out, 1e-6f, "negative dt holds the last output");
}

static void testConvergesOnToyPlant() {
    // crude first order plant: pressure rises with valve-restricted pump flow
    // and leaks back out. not physical, just enough dynamics to prove the
    // loop closes and settles where it's told.
    PidConfig cfg;
    cfg.kp = 0.5f;
    cfg.ki = 0.8f;
    cfg.kd = 0.02f;
    Pid pid(cfg);

    const float dt = 0.01f;
    float psi = 0.0f;
    for (int i = 0; i < 4000; i++) { // 40 simulated seconds
        float u = pid.update(10.0f, psi, dt);
        psi += (u * 20.0f - psi) * 0.5f * dt; // plant: gain 20, tau 2 s
    }
    checkNear(psi, 10.0f, 0.2f, "closed loop settles at the setpoint on a toy plant");
}

void runPidTests() {
    testsupport::section("pid");
    testProportionalOnly();
    testOutputClampsToLimits();
    testIntegralAccumulates();
    testAntiWindupHoldsIntegralAtRail();
    testNoDerivativeKickOnSetpointStep();
    testDerivativeOpposesRisingMeasurement();
    testResetClearsHistory();
    testBadDtHoldsLastOutput();
    testConvergesOnToyPlant();
}
