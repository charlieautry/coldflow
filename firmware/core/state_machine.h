#pragma once
#include <cstdint>

enum class State : uint8_t {
    Idle,
    Armed,
    Pressurize,
    Hold,
    Vent,
    Abort,
    Safe
};

enum class Command : uint8_t { 
    Arm,
    Disarm,
    Set,
    Press,
    Vent,
    Abort,
    Clear
};

enum class CmdResult : uint8_t {
    Ok,
    ErrState,
    ErrRange,
    ErrFault
};

// this is the sensors snapshots, class will latch them
struct Inputs {
    float psi = 0.0f;
    bool overpressure = false;
    bool loopFault = false;
    bool
};

struct Outputs {
    enum class ValveMode : uint8_t {
        Closed,
        Pid,
        Open
    };
    bool pumpOn = false;
    bool ventOpen = true; // i start true as thats the de-energized safe default mode
    ValveMode valve = ValveMode::Closed;
};