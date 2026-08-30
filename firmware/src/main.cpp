// Flight computer entry point. Everything interesting lives in core/ (tested
// off-target) and pico_hal.* (the only file that touches pins). This file is
// just the metronome: gather serial bytes into lines, run the control tick at
// 100 Hz, sample the resolver in the spare time, blink the LED so I can see
// the loop is alive from across the room.

#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"

#include "pico_hal.h"
#include "resolver.h"
#include "rig.h"

namespace {

constexpr uint32_t kTickPeriodMs = 10; // 100 Hz control loop
constexpr uint32_t kHeartbeatMs = 500;

// resolver decode is a bolt-on: ANGLE is handled here, not in core/protocol,
// so the core protocol stays exactly what docs/firmware-spec.md promises
bool handleResolverLine(const char* line, ResolverDecoder& dec, PicoHal& hal) {
    // accept "angle" in any case, tolerate trailing whitespace
    char word[8] = {0};
    int n = 0;
    for (const char* p = line; *p && *p != ' ' && *p != '\r' && n < 7; p++) {
        word[n++] = static_cast<char>((*p >= 'a' && *p <= 'z') ? *p - 32 : *p);
    }
    if (std::strcmp(word, "ANGLE") != 0) return false;

    char buf[96];
    snprintf(buf, sizeof(buf), "{\"angle_deg\":%.2f,\"amplitude\":%.3f,\"valid\":%s}",
             dec.angleDeg(), dec.amplitude(), dec.valid() ? "true" : "false");
    hal.writeLine(buf);
    hal.writeLine("ok");
    return true;
}

}  // namespace

int main() {
    PicoHal hal{PicoHalConfig{}};
    hal.init();

    Rig rig(hal, RigConfig{});
    ResolverDecoder resolver{ResolverConfig{}};

    char line[96];
    unsigned lineLen = 0;
    bool lineOverflow = false;

    uint32_t nextTickMs = hal.millis();
    uint32_t nextBlinkMs = 0;
    bool led = false;

    while (true) {
        // 1. drain incoming serial without blocking
        for (int c = getchar_timeout_us(0); c != PICO_ERROR_TIMEOUT; c = getchar_timeout_us(0)) {
            if (c == '\n') {
                line[lineLen] = '\0';
                if (lineOverflow) {
                    hal.writeLine("err unknown"); // an over-long line can't be legal
                } else if (!handleResolverLine(line, resolver, hal)) {
                    rig.handleLine(line);
                }
                lineLen = 0;
                lineOverflow = false;
            } else if (lineLen < sizeof(line) - 1) {
                line[lineLen++] = static_cast<char>(c);
            } else {
                lineOverflow = true;
            }
        }

        // 2. control tick on schedule
        const uint32_t now = hal.millis();
        if (static_cast<int32_t>(now - nextTickMs) >= 0) {
            nextTickMs += kTickPeriodMs;
            rig.tick();
        }

        // 3. resolver sampling soaks up the leftover loop time. plenty of
        // samples per carrier half-cycle at the sim's 500 Hz excitation.
        resolver.sample(hal.readExcitationSign() ? 1.0f : 0.0f,
                        hal.readResolverSin(), hal.readResolverCos());

        // 4. heartbeat
        if (static_cast<int32_t>(now - nextBlinkMs) >= 0) {
            nextBlinkMs = now + kHeartbeatMs;
            led = !led;
            hal.setHeartbeatLed(led);
        }
    }
}
