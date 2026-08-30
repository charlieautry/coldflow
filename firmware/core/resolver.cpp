#include "resolver.h"

#include <cmath>

void ResolverDecoder::sample(float exc, float sinCh, float cosCh) {
    // synchronous demodulation: the excitation's sign is the reference.
    // when excitation is in its negative half-cycle the windings are too,
    // so multiplying by the sign flips them back upright and the envelope
    // keeps its own sign (which quadrant detection depends on).
    const float ref = (exc >= cfg_.midpoint) ? 1.0f : -1.0f;
    const float s = ref * (sinCh - cfg_.midpoint);
    const float c = ref * (cosCh - cfg_.midpoint);

    // low-pass the demodulated products to strip carrier ripple
    sinEnv_ += cfg_.filterAlpha * (s - sinEnv_);
    cosEnv_ += cfg_.filterAlpha * (c - cosEnv_);
}

float ResolverDecoder::angleDeg() const {
    const float rad = std::atan2(sinEnv_, cosEnv_);
    float deg = rad * (180.0f / 3.14159265358979f);
    if (deg < 0.0f) deg += 360.0f;
    return deg;
}

float ResolverDecoder::amplitude() const {
    return std::sqrt(sinEnv_ * sinEnv_ + cosEnv_ * cosEnv_);
}

void ResolverDecoder::reset() {
    sinEnv_ = 0.0f;
    cosEnv_ = 0.0f;
}
