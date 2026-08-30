#pragma once
#include <cstdint>
#include <cstring>

// Latched fault flags, matching the fault table in docs/firmware-spec.md.
// Header-only because it's a bitmask with names attached. The names here ARE
// the protocol: they go out in telemetry json and come in via INJECT, so the
// spec table, this file, and the pytest suite all have to agree on them.

enum class Fault : uint8_t {
    Overpressure = 0, // psi >= 21
    LoopLow,          // PT-101 loop current < 3.5 mA (broken loop)
    LoopHigh,         // PT-101 loop current > 21 mA (short / over-range)
    Stale,            // no fresh PT-101 sample for 100 ms
    TcOpen,           // MAX31855 reports open/short, non-latching, no abort
    AbortCmd,         // ABORT command received
    Count
};

class FaultFlags {
public:
    void latch(Fault f) { mask_ |= bit(f); }

    // for the self-clearing tc_open flag: sensor layer calls this every
    // update with the live fault state instead of latching
    void set(Fault f, bool on) {
        if (on) mask_ |= bit(f);
        else mask_ &= static_cast<uint8_t>(~bit(f));
    }

    void clearAll() { mask_ = 0; } // CLEAR command unlatches everything

    bool has(Fault f) const { return (mask_ & bit(f)) != 0; }
    bool any() const { return mask_ != 0; }

    // everything except tc_open trips the redline abort. temperature is a
    // nice-to-have channel, a flaky thermocouple doesn't end the test.
    bool anyAborting() const {
        return (mask_ & static_cast<uint8_t>(~bit(Fault::TcOpen))) != 0;
    }

    uint8_t mask() const { return mask_; }

    static const char* name(Fault f) {
        switch (f) {
            case Fault::Overpressure: return "overpressure";
            case Fault::LoopLow:      return "loop_low";
            case Fault::LoopHigh:     return "loop_high";
            case Fault::Stale:        return "stale";
            case Fault::TcOpen:       return "tc_open";
            case Fault::AbortCmd:     return "abort_cmd";
            default:                  return "?";
        }
    }

    // INJECT argument parsing. returns false on an unrecognized name.
    static bool fromName(const char* s, Fault& out) {
        for (uint8_t i = 0; i < static_cast<uint8_t>(Fault::Count); i++) {
            Fault f = static_cast<Fault>(i);
            if (std::strcmp(s, name(f)) == 0) {
                out = f;
                return true;
            }
        }
        return false;
    }

private:
    static uint8_t bit(Fault f) {
        return static_cast<uint8_t>(1u << static_cast<uint8_t>(f));
    }
    uint8_t mask_ = 0;
};
