#pragma once
#include <cstdio>
#include <cmath>

// Tiny shared harness for the off-target tests. No framework on purpose: the
// whole thing is a failure counter and a couple of helpers, so a reader can
// see exactly what "passed" means without learning a library first.
//
// Each test_*.cpp exposes one runXxxTests() entry point and test_main.cpp
// calls them in order. Add a new file, add one line to test_main.cpp.

namespace testsupport {

inline int& failures() {
    static int count = 0;
    return count;
}

inline int& checks() {
    static int count = 0;
    return count;
}

inline void check(bool cond, const char* what) {
    checks()++;
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        failures()++;
    }
}

// float compare with an absolute tolerance, prints both sides on failure so
// the number that went wrong is right there in the log
inline void checkNear(float actual, float expected, float tol, const char* what) {
    checks()++;
    if (std::fabs(actual - expected) > tol) {
        std::printf("FAIL: %s (got %.6f, expected %.6f +/- %.6f)\n", what, actual, expected, tol);
        failures()++;
    }
}

inline void section(const char* name) {
    std::printf("--- %s\n", name);
}

}  // namespace testsupport
