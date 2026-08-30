#pragma once
#include <cstdint>
#include "state_machine.h"
#include "faults.h"

// Serial protocol, text side only: one incoming line -> a parsed command, and
// one telemetry snapshot -> a json line. No hardware, no allocation, so the
// exact bytes on the wire get unit tested off-target. The framing rules live
// in docs/firmware-spec.md: responses are "ok"/"err <reason>", telemetry
// lines always start with '{'.

struct ParsedLine {
    enum class Kind : uint8_t {
        Empty,     // blank line, ignore silently
        SmCommand, // one of the state machine commands below
        Status,    // STATUS: emit one telemetry line now
        Rate,      // RATE <hz>: set the streaming rate
        Inject,    // INJECT <fault>: the test hook
        BadArg,    // recognized command, unparseable/missing argument -> err arg
        Unknown    // never heard of it -> err unknown
    };

    Kind kind = Kind::Empty;
    Command smCmd = Command::Arm; // valid when kind == SmCommand
    float arg = 0.0f;             // SET psi / RATE hz, valid when hasArg
    bool hasArg = false;
    Fault fault = Fault::Overpressure; // valid when kind == Inject
};

// commands are case-insensitive, whitespace-tolerant, cr-tolerant.
// lines longer than 63 chars are treated as Unknown (nothing legal is close).
ParsedLine parseLine(const char* line);

// the response line (without newline) for a state machine command result
const char* cmdResultText(CmdResult r);

// uppercase state name exactly as telemetry reports it
const char* stateName(State s);

// everything one telemetry line needs, gathered by the caller
struct TelemetrySnapshot {
    uint32_t tMs = 0;
    State state = State::Safe;
    float psi = 0.0f;
    float setpoint = 0.0f;
    bool hasSetpoint = false; // false -> "setpoint": null
    float degC = 0.0f;
    bool tcValid = true;      // false -> "degC": null
    float flowLpm = 0.0f;
    float pumpDuty = 0.0f;    // commanded, 0..1
    float valvePos = 0.0f;    // commanded, 0..1
    uint8_t faultMask = 0;    // FaultFlags::mask()
};

// writes one json object + '\n' into buf, returns chars written (excluding
// the terminator), or 0 if the buffer is too small. 160 bytes is plenty.
int formatTelemetry(char* buf, int bufLen, const TelemetrySnapshot& snap);
