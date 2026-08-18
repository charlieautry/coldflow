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

class StateMachine {
    public:

        CmdResult handleCommand(Command cmd, float arg, const Inputs& in, uint32_t nowMs);

        // rows are conditional so tick() needs to be called every loop
        void tick(const Inputs& in, uint32_t nowMs);

        State state() const { return state_; }
        Outputs outputs() const { return outputs_; }
        float setpoint() const { return setpoint_; }
        bool hasSetpoint() const { return hasSetpoint_; }
        bool faultLatched() const { return faultLatched_; }

    private:

        void enter(State next); // this sets outputs per the spec, and resets dwell if needed
        bool anyRedline(const Inputs& in) const;

        State state_ = State::Safe; // needs to power on, again per the spec
        Outputs outputs_{};
        float setpoint_ = 0.0f;
        bool hasSetpoint_ = false;
        bool faultLatched_ = false;
        uint32_t dwellStartMs_ = 0;
        bool dwellArmed_ = false;
};