#pragma once
#include <cstdint>

#include "faults.h"
#include "hal.h"
#include "pid.h"
#include "protocol.h"
#include "sensors.h"
#include "state_machine.h"

// The glue module: owns one of everything and runs the show. main.cpp on the
// Pico shrinks to "make a PicoHal, make a Rig, feed it lines, call tick()".
// Because the only hardware access is through Hal, this entire class runs
// off-target, which is where the interesting tests live (abort paths,
// telemetry cadence, fault injection) without a single wire connected.

struct RigConfig {
    SensorCalibration cal;
    PidConfig pid;              // placeholder tuning, see the ctor of RigConfig
    float abortPsi = 21.0f;     // redline: comfortably above 15 psi operating,
                                // comfortably below the 29 psi transducer ceiling
    float defaultRateHz = 10.0f;
    float maxRateHz = 50.0f;

    RigConfig() {
        // starting-point gains for the servo valve loop. these are Phase 2
        // placeholders to be retuned against the real plant; they only need
        // to be sane enough that the simulated-plant tests close the loop.
        pid.kp = 0.08f;
        pid.ki = 0.05f;
        pid.kd = 0.01f;
        pid.outMin = 0.0f;
        pid.outMax = 1.0f;
    }
};

class Rig {
public:
    Rig(Hal& hal, const RigConfig& cfg);

    // one incoming command line (without needing the newline). writes exactly
    // one response line back through the hal, unless the line was blank.
    void handleLine(const char* line);

    // the control step: read sensors, update faults, tick the state machine,
    // run the PID if it's engaged, drive outputs, stream telemetry. call at a
    // fixed rate (100 Hz on the pico).
    void tick();

    // read-only views for tests and debugging
    State state() const { return sm_.state(); }
    const FaultFlags& faults() const { return faults_; }
    float valveCommand() const { return valveCmd_; }
    float pumpCommand() const { return pumpCmd_; }
    float rateHz() const { return rateHz_; }
    float lastPsi() const { return lastPsi_; }

private:
    Inputs buildInputs() const;
    void applyOutputs();
    void emitTelemetry(uint32_t nowMs);
    void forceAbortFromFault();

    Hal& hal_;
    RigConfig cfg_;
    StateMachine sm_;
    Pid pid_;
    FaultFlags faults_;

    float rateHz_;
    uint32_t lastEmitMs_ = 0;
    uint32_t lastTickMs_ = 0;
    uint32_t lastFreshMs_ = 0;

    // latest converted sensor picture, refreshed every tick
    float lastPsi_ = 0.0f;
    bool loopFaultLive_ = false;
    bool staleLive_ = false;
    float lastDegC_ = 0.0f;
    bool tcValid_ = true;
    float lastFlowLpm_ = 0.0f;

    // latest commanded outputs, mirrored into telemetry
    float pumpCmd_ = 0.0f;
    float valveCmd_ = 0.0f;

    State prevState_ = State::Safe;
};
