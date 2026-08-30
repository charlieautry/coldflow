#include "test_support.h"

// One entry point per test file. Keeping the list here (instead of static
// registration tricks) means a forgotten test shows up as a missing line in
// a five line file, which is the failure mode that bit me twice already.
void runStateMachineTests();
void runPidTests();
void runSensorTests();
void runProtocolTests();
void runFaultTests();
void runRigTests();
void runResolverTests();

int main() {
    runStateMachineTests();
    runPidTests();
    runSensorTests();
    runProtocolTests();
    runFaultTests();
    runRigTests();
    runResolverTests();

    const int failed = testsupport::failures();
    const int total = testsupport::checks();
    if (failed == 0) {
        std::printf("all %d checks passed\n", total);
    } else {
        std::printf("%d of %d checks FAILED\n", failed, total);
    }
    return failed ? 1 : 0;
}
