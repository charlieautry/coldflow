#include "state_machine.h"

// constants, i use namespace bc it only needs to be visible in this file only
namespace {
    constexpr float kMaxArmPsi = 1.0f; // chamber empty threshold for ARM, PRESS, CLEAR
    constexpr float kMinSetpointPsi = 5.0f; // min setpoint for SET
    constexpr float kMaxSetpointPsi = 15.0f; // max setpoint for SET
    constexpr float kBandPsi = 0.5f; // PRESS to HOLD band
    constexpr float kEmptyPsi = 0.5f; // VENT or ABORT to SAFE threshold
    constexpr uint32_t kDwellMs = 2000;
}

CmdResult StateMachine::handleCommand(Command cmd, float arg, const Inputs& in, uint32_t nowMs) {
    (void)nowMs;

    // abort goes before anything so its never refused on accident
    if (cmd == Command::Abort) {
        faultLatched_ = true;
        enter(State::Abort);
        return CmdResult::Ok;
    }

    // i handle everything else with a switch statement
    switch (cmd) {
        case Command::Arm:
            if (state_ != State::Idle) return CmdResult::ErrState;
            if (faultLatched_) return CmdResult::ErrFault;
            if (in.psi >= kMaxArmPsi || in.loopFault) return CmdResult::ErrRange;
            enter(State::Armed);
            return CmdResult::Ok;
        
        case Command::Disarm:
            if (state_ != State::Armed) return CmdResult::ErrState;
            enter(State::Idle);
            return CmdResult::Ok;

        case Command::Set:
            if (state_ != State::Idle && state_ != State::Armed && state_ != State::Pressurize && state_ != State::Hold) return CmdResult::ErrState;

            if (arg < kMinSetpointPsi || arg > kMaxSetpointPsi) return CmdResult::ErrRange;

            setpoint_ = arg;
            hasSetpoint_ = true;
            return CmdResult::Ok; // no enter() bc SET changes data not the state

        case Command::Press:
            if (state_ != State::Armed) return CmdResult::ErrState;
            if (faultLatched_) return CmdResult::ErrFault;
            if (in.psi >= kMaxArmPsi || !hasSetpoint_) return CmdResult::ErrRange;
            
            enter(State::Pressurize);
            return CmdResult::Ok;

        case Command::Vent:
            if (state_ != State::Pressurize && state_ != State::Hold ) return CmdResult::ErrState;

            enter(State::Vent);
            return CmdResult::Ok;

        case Command::Clear:
            if (state_ != State::Safe) return CmdResult::ErrState;
            if (in.psi >= kMaxArmPsi) return CmdResult::ErrRange;

            faultLatched_ = false;
            hasSetpoint_ = false;
            setpoint_ = 0.0f;

            enter(State::Idle);
            return CmdResult::Ok;

        case Command::Abort: // alr handled above
            return CmdResult::Ok;
    }
    
    return CmdResult::ErrState; //keep for compile
}

void StateMachine::tick(const Inputs& in, uint32_t nowMs) {
    // placeholder

    //redline check first from any state, then a switch on state for the conditional rows

    if (anyRedline(in) && state_ != State::Abort) {
        faultLatched_ = true;
        enter(State::Abort);
        return;
    }

    switch (state_) {
        case State::Pressurize:
            if (in.psi >= setpoint_ - kBandPsi && in.psi <= setpoint_ + kBandPsi) {
                if (!dwellArmed_) {
                    dwellArmed_ = true;
                    dwellStartMs_ = nowMs;
                } else if (nowMs - dwellStartMs_ >= kDwellMs) {
                    enter(State::Hold);
                }
            } else {
                dwellArmed_ = false; //so tha band holds continuously
            }
            break;

        case State::Hold:
            break;

        case State::Vent:
            if (in.psi <= kEmptyPsi) {
                if (!dwellArmed_) {
                    dwellArmed_ = true;
                    dwellStartMs_ = nowMs;
                } else if (nowMs - dwellStartMs_ >= kDwellMs) {
                    enter(State::Safe);
                }
            }
            break;

        case State::Abort:
            if (!in.stale && in.psi <= kEmptyPsi) {
                if (!dwellArmed_) {
                    dwellArmed_ = true;
                    dwellStartMs_ = nowMs;
                } else if (nowMs - dwellStartMs_ >= kDwellMs) {
                    enter(State::Safe);
                }
            } else {
                dwellArmed_ = false;
            }
            break;

        default:
            break; // for Idle, Armed, Safe, do nothing
    }
}

void StateMachine::enter(State next) {
    state_ = next;
}

bool StateMachine::anyRedline(const Inputs& in) const {
    return in.overpressure || in.loopFault;
}