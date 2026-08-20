#include "state_machine.h"
#include <cstdio>

namespace {
    int failures = 0;

    void check(bool cond, const char* what) {
        if (!cond) {
            std::printf("FAIL: %s\n", what);
            failures++;
        }
    }
}

// happy tests, paths that should succeed

void testArmFromIdle() {
    StateMachine sm;
    Inputs in;

    sm.handleCommand(Command::Clear, 0.0f, in, 0);
    CmdResult r = sm.handleCommand(Command::Arm, 0.0f, in, 0);

    check(r == CmdResult::Ok, "Arm from Idle should succeed");
    check(sm.state() == State::Armed, "State should be Armed after Arm command");
    check(!sm.outputs().pumpOn, "Pump should be off after Arm command");
    check(sm.outputs().ventOpen, "Vent should be open after Arm command");
}

void testDisarmFromArmed() {
    StateMachine sm;
    Inputs in;

    sm.handleCommand(Command::Clear, 0.0f, in, 0);
    sm.handleCommand(Command::Arm, 0.0f, in, 0);
    CmdResult r = sm.handleCommand(Command::Disarm, 0.0f, in, 0);

    check(r == CmdResult::Ok, "Disarm from Armed should succeed");
    check(sm.state() == State::Idle, "State should be Idle after Disarm command");
    check(!sm.outputs().pumpOn, "Pump should be off after Disarm command");
    check(sm.outputs().ventOpen, "Vent should be open after Disarm command");
}

void testPressurizeFromArmed() {
    StateMachine sm;
    Inputs in;

    sm.handleCommand(Command::Clear, 0.0f, in, 0);
    sm.handleCommand(Command::Arm, 0.0f, in, 0);
    check(sm.handleCommand(Command::Set, 10.0f, in, 0) == CmdResult::Ok, "precondition: SET 10 accepted");
    CmdResult r = sm.handleCommand(Command::Press, 0.0f, in, 0);

    check(r == CmdResult::Ok, "Pressurize from Armed should succeed");
    check(sm.state() == State::Pressurize, "State should be Pressurize after Press command");
    check(sm.outputs().pumpOn, "Pump should be on after Press command");
    check(!sm.outputs().ventOpen, "Vent should be closed after Press command");
}

void testPressurizeReachesHoldAfterDwell() {
    StateMachine sm;
    Inputs in;

    sm.handleCommand(Command::Clear, 0.0f, in, 0);
    sm.handleCommand(Command::Arm, 0.0f, in, 0);
    check(sm.handleCommand(Command::Set, 10.0f, in, 0) == CmdResult::Ok, "precondition: SET 10 accepted");
    check(sm.handleCommand(Command::Press, 0.0f, in, 0) == CmdResult::Ok, "precondition: PRESS accepted");

    in.psi = 10.0f;     // pressure arrives dead on the setpoint
    sm.tick(in, 1000);  // first in-band tick arms the dwell timer
    check(sm.state() == State::Pressurize, "still PRESSURIZE while the dwell runs");

    sm.tick(in, 2999);  // 1999 ms elapsed
    check(sm.state() == State::Pressurize, "still PRESSURIZE 1 ms before the dwell elapses");

    sm.tick(in, 3000);  // 2000 ms elapsed
    check(sm.state() == State::Hold, "HOLD after 2 s continuously in band");
    check(sm.outputs().pumpOn, "pump stays on in HOLD");
}

void testDwellRestsWhenBandExited() {
    StateMachine sm;
    Inputs in;

    sm.handleCommand(Command::Clear, 0.0f, in, 0);
    sm.handleCommand(Command::Arm, 0.0f, in, 0);
    check(sm.handleCommand(Command::Set, 10.0f, in, 0) == CmdResult::Ok, "precondition: SET 10 accepted");
    check(sm.handleCommand(Command::Press, 0.0f, in, 0) == CmdResult::Ok, "precondition: PRESS accepted");

    in.psi = 10.0f;
    sm.tick(in,0); // in band, dwell armed at 0 ms

    in.psi = 3.0f; // pressure drops out of band, dwell should reset
    sm.tick(in, 1000); // this tick should reset the dwell timer

    in.psi = 10.0f; // back in band, dwell should re-arm at 1000 ms
    sm.tick(in, 1100); // dwell armed at 1100 ms

    sm.tick(in,3000); // only 1900 ms elapsed since re-arming, should not yet transition to HOLD
    check(sm.state() == State::Pressurize, "still PRESSURIZE after leaving band and returning before dwell elapsed");

    sm.tick(in, 3100); // 2000 ms elapsed since re-arming, should now transition to HOLD
    check(sm.state() == State::Hold, "HOLD after 2 s continuously in band");
}

// unhappy tests, paths that try to do something invalid

void testInvalidPressurizeFromArmed() {
    StateMachine sm;
    Inputs in;

    sm.handleCommand(Command::Clear, 0.0f, in, 0);
    sm.handleCommand(Command::Arm, 0.0f, in, 0);
    // Do not set a valid setpoint
    CmdResult r = sm.handleCommand(Command::Press, 0.0f, in, 0);

    check(r == CmdResult::ErrRange, "Pressurize from Armed without setpoint should fail");
    check(sm.state() == State::Armed, "State should remain Armed after failed Press command");
}

void testSetRejectsOutOfRange() {
    StateMachine sm;
    Inputs in;

    sm.handleCommand(Command::Clear, 0.0f, in, 0);
    sm.handleCommand(Command::Arm, 0.0f, in, 0);
    CmdResult r = sm.handleCommand(Command::Set, 50.0f, in, 0);

    check(r == CmdResult::ErrRange, "SET 50 should be rejected with ErrRange");
    check(!sm.hasSetpoint(), "rejected SET should not store a setpoint");
    check(sm.state() == State::Armed, "state should not change on a rejected SET");
}

int main() {
    testArmFromIdle();
    testDisarmFromArmed();
    testPressurizeFromArmed();
    testPressurizeReachesHoldAfterDwell();
    testInvalidPressurizeFromArmed();
    testSetRejectsOutOfRange();
    testDwellRestsWhenBandExited();

    if (failures == 0) std::printf("all tests passed!\n");
    return failures ? 1 : 0;
}