#include "resolver.h"
#include "test_support.h"

#include <cmath>

using testsupport::check;
using testsupport::checkNear;

namespace {

constexpr float kPi = 3.14159265358979f;

// feed the decoder n samples of a synthetic resolver signal at a fixed shaft
// angle: square-wave excitation, sin/cos channels modulated by the angle,
// everything centered on 0.5 like the real RC-filtered PWM will be
void feedAngle(ResolverDecoder& dec, float angleDeg, int n, float amp = 0.4f) {
    const float rad = angleDeg * kPi / 180.0f;
    const int samplesPerHalfCycle = 8; // carrier period = 16 samples
    for (int i = 0; i < n; i++) {
        const float carrier = ((i / samplesPerHalfCycle) % 2 == 0) ? 1.0f : -1.0f;
        const float exc = 0.5f + 0.4f * carrier;
        const float s = 0.5f + amp * std::sin(rad) * carrier;
        const float c = 0.5f + amp * std::cos(rad) * carrier;
        dec.sample(exc, s, c);
    }
}

// smallest difference between two angles, respecting the 360 wrap
float angularError(float a, float b) {
    float d = std::fabs(a - b);
    if (d > 180.0f) d = 360.0f - d;
    return d;
}

}  // namespace

static void testDecodesSweptAngles() {
    // sweep the full circle, one decoder per angle. every quadrant gets hit,
    // which is the whole point of using atan2 over a plain arctangent.
    for (int a = 0; a < 360; a += 15) {
        ResolverDecoder dec{ResolverConfig{}};
        feedAngle(dec, static_cast<float>(a), 2000);
        check(dec.valid(), "decoder reports valid signal during the sweep");
        check(angularError(dec.angleDeg(), static_cast<float>(a)) < 1.0f,
              "decoded angle within 1 degree across the sweep");
    }
}

static void testTracksAngleChange() {
    ResolverDecoder dec{ResolverConfig{}};
    feedAngle(dec, 30.0f, 2000);
    checkNear(angularError(dec.angleDeg(), 30.0f), 0.0f, 1.0f, "settles at 30 degrees");

    feedAngle(dec, 200.0f, 4000); // let the envelope filter re-settle
    checkNear(angularError(dec.angleDeg(), 200.0f), 0.0f, 1.0f, "tracks a move to 200 degrees");
}

static void testFlatlinedInputIsInvalid() {
    // resolver unplugged: channels sit at the midpoint, no modulation
    ResolverDecoder dec{ResolverConfig{}};
    for (int i = 0; i < 2000; i++) dec.sample(0.5f, 0.5f, 0.5f);
    check(!dec.valid(), "flat mid-rail inputs decode as invalid, not a random angle");
}

static void testWeakSignalStillDecodesButFlagsMarginal() {
    // half the nominal amplitude still clears the validity floor and decodes
    ResolverDecoder dec{ResolverConfig{}};
    feedAngle(dec, 120.0f, 3000, 0.2f);
    check(dec.valid(), "half amplitude is still a usable signal");
    check(angularError(dec.angleDeg(), 120.0f) < 1.5f, "angle decode degrades gracefully at low amplitude");
}

static void testResetForgetsEnvelope() {
    ResolverDecoder dec{ResolverConfig{}};
    feedAngle(dec, 90.0f, 2000);
    dec.reset();
    check(!dec.valid(), "reset drops the envelopes back to no-signal");
}

void runResolverTests() {
    testsupport::section("resolver");
    testDecodesSweptAngles();
    testTracksAngleChange();
    testFlatlinedInputIsInvalid();
    testWeakSignalStillDecodesButFlagsMarginal();
    testResetForgetsEnvelope();
}
