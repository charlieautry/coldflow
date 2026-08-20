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
    sm.handleCommand(Command::Set, 10.0f, in, 0); // Set a valid setpoint
    CmdResult r = sm.handleCommand(Command::Press, 0.0f, in, 0);

    check(r == CmdResult::Ok, "Pressurize from Armed should succeed");
    check(sm.state() == State::Pressurize, "State should be Pressurize after Press command");
    check(sm.outputs().pumpOn, "Pump should be on after Press command");
    check(!sm.outputs().ventOpen, "Vent should be closed after Press command");
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

int main() {
    testArmFromIdle();
    testDisarmFromArmed();
    testPressurizeFromArmed();
    testInvalidPressurizeFromArmed();

    if (failures == 0) std::printf("all tests passed!\n");
    return failures ? 1 : 0;
}