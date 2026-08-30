#pragma once

// PID pressure controller. Pure math, no hardware, no allocation, so it
// unit-tests off-target exactly like the state machine does.
//
// Conventions:
// - update() returns the actuator command already clamped to [outMin, outMax].
//   For this rig that's needle valve position 0..1 (or pump duty 0..1 if I
//   fall back to pump-side control).
// - ki is per second and kd is in seconds, so the gains keep their meaning
//   if the loop rate changes.
// - derivative is taken on the measurement, not the error, so yanking the
//   setpoint doesn't spike the output (no "derivative kick").

struct PidConfig {
    float kp = 0.0f;
    float ki = 0.0f;               // integral gain, 1/s
    float kd = 0.0f;               // derivative gain, s
    float outMin = 0.0f;
    float outMax = 1.0f;
    float derivFilterTauS = 0.05f; // first order low-pass on the derivative,
                                   // because raw ADC derivative is pure noise
};

class Pid {
public:
    explicit Pid(const PidConfig& cfg) : cfg_(cfg) {}

    // one control step, dtS = seconds since the previous call
    float update(float setpoint, float measured, float dtS);

    // forget integral and derivative history, keep the tuning.
    // call on every entry to PRESSURIZE so an old run's windup can't kick.
    void reset();

    void setConfig(const PidConfig& cfg) { cfg_ = cfg; }
    const PidConfig& config() const { return cfg_; }

    float output() const { return output_; }
    float integral() const { return integral_; }

private:
    PidConfig cfg_;
    float integral_ = 0.0f;
    float prevMeasured_ = 0.0f;
    float filteredDeriv_ = 0.0f; // filtered d(measured)/dt, sign flipped into error terms
    float output_ = 0.0f;
    bool primed_ = false;        // first update has no derivative history yet
};
