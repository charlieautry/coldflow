#include "state_machine.h"

CmdResult StateMachine::handleCommand(Command cmd, float arg, const Inputs& in, uint32_t nowMs) {
    return CmdResult::ErrState; //placeholder
}

void StateMachine::tick(const Inputs& in, uint32_t nowMs) {
    // placeholder
}

void StateMachine::enter(State next) {
    state_ = next;
}

bool StateMachine::anyRedline(const Inputs& in) const {
    return in.overpressure || in.loopFault;
}