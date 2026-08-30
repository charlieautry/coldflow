#pragma once
#include <cstdint>

// Resolver-to-digital decoder, pure math half. A resolver outputs two
// windings amplitude-modulated by the shaft angle: sin channel = sin(theta) *
// excitation, cos channel = cos(theta) * excitation. The sim Pico generates
// exactly that with PWM + RC filters; this class turns synchronized ADC
// samples of (excitation, sin, cos) back into an angle.
//
// How: multiply each channel by the sign of the excitation (synchronous /
// envelope demodulation, which rescues the sign that a plain rectifier would
// throw away), low-pass the products into two envelopes, then atan2 the
// envelopes. atan2 IS the quadrant logic: sin and cos signs together pick the
// quadrant, which a bare arctangent of the ratio cannot do.

struct ResolverConfig {
    float midpoint = 0.5f;      // normalized ADC value of the waveform center
    float filterAlpha = 0.02f;  // per-sample IIR coefficient for the envelopes
    float minAmplitude = 0.05f; // envelope magnitude below this = no resolver
                                // connected, angle is meaningless
};

class ResolverDecoder {
public:
    explicit ResolverDecoder(const ResolverConfig& cfg) : cfg_(cfg) {}

    // feed one synchronized sample triple, all normalized 0..1
    void sample(float exc, float sinCh, float cosCh);

    float angleDeg() const;  // decoded shaft angle, [0, 360)
    float amplitude() const; // envelope magnitude, for signal health checks
    bool valid() const { return amplitude() >= cfg_.minAmplitude; }

    void reset();

private:
    ResolverConfig cfg_;
    float sinEnv_ = 0.0f;
    float cosEnv_ = 0.0f;
};
