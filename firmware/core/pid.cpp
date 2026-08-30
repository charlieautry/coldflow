#include "pid.h"

namespace {
    float clampf(float v, float lo, float hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
}

float Pid::update(float setpoint, float measured, float dtS) {
    // a zero or negative dt means the clock did something weird. don't divide
    // by it, just hold the last output
    if (dtS <= 0.0f) return output_;

    const float error = setpoint - measured;

    // derivative on measurement, then a first order filter so one noisy ADC
    // sample can't slam the valve. alpha folds dt in so the cutoff stays put
    // if the loop rate changes.
    float rawDeriv = 0.0f;
    if (primed_) {
        // minus sign: this equals d(error)/dt when the setpoint is constant
        rawDeriv = -(measured - prevMeasured_) / dtS;
    }
    const float alpha = dtS / (cfg_.derivFilterTauS + dtS);
    filteredDeriv_ += alpha * (rawDeriv - filteredDeriv_);
    prevMeasured_ = measured;
    primed_ = true;

    const float pd = cfg_.kp * error + cfg_.kd * filteredDeriv_;

    // conditional anti-windup: only let the integral grow if the output isn't
    // already pinned in the direction the error is pushing. without this, a
    // long pressurize ramp winds the integral way past full-open and the
    // controller overshoots for seconds while it unwinds.
    const float candidate = pd + integral_ + cfg_.ki * error * dtS;
    const bool windingHigh = candidate > cfg_.outMax && error > 0.0f;
    const bool windingLow = candidate < cfg_.outMin && error < 0.0f;
    if (!windingHigh && !windingLow) {
        integral_ += cfg_.ki * error * dtS;
    }

    output_ = clampf(pd + integral_, cfg_.outMin, cfg_.outMax);
    return output_;
}

void Pid::reset() {
    integral_ = 0.0f;
    filteredDeriv_ = 0.0f;
    prevMeasured_ = 0.0f;
    output_ = 0.0f;
    primed_ = false;
}
