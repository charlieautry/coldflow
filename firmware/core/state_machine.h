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