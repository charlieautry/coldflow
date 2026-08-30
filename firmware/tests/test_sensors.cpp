#include "sensors.h"
#include "test_support.h"

using testsupport::check;
using testsupport::checkNear;

// helper: what count value would the 12-bit ADC report for this voltage
static uint16_t countsForVolts(float volts, const SensorCalibration& cal) {
    return static_cast<uint16_t>((volts / cal.adcVref) * static_cast<float>(cal.adcMax) + 0.5f);
}

static void testPressureAtLoopEndpoints() {
    SensorCalibration cal;

    // 4 mA through 150 ohms = 0.6 V = zero pressure
    PressureReading lo = convertPressure(countsForVolts(0.6f, cal), cal);
    checkNear(lo.psi, 0.0f, 0.05f, "4 mA converts to 0 psi");
    checkNear(lo.loopMa, 4.0f, 0.05f, "0.6 V across 150 ohm reads as 4 mA");
    check(!lo.loopLow && !lo.loopHigh, "4 mA is a healthy loop");

    // 20 mA = 3.0 V = full scale 29 psi
    PressureReading hi = convertPressure(countsForVolts(3.0f, cal), cal);
    checkNear(hi.psi, 29.0f, 0.05f, "20 mA converts to full scale 29 psi");
    check(!hi.loopLow && !hi.loopHigh, "20 mA is a healthy loop");
}

static void testPressureMidScale() {
    SensorCalibration cal;
    // 12 mA is halfway through the 4-20 span, so half of 29 psi
    PressureReading mid = convertPressure(countsForVolts(1.8f, cal), cal);
    checkNear(mid.psi, 14.5f, 0.1f, "12 mA converts to mid scale 14.5 psi");
}

static void testBrokenLoopFlagsLow() {
    SensorCalibration cal;
    // wire fell off: no current, ADC reads near zero
    PressureReading r = convertPressure(0, cal);
    check(r.loopLow, "0 mA flags loop_low");
    checkNear(r.psi, 0.0f, 0.01f, "broken loop psi clamps to 0, never negative");
}

static void testOverRangeFlagsHigh() {
    SensorCalibration cal;
    // shorted / over-driven loop: ADC pinned at the rail = 22 mA
    PressureReading r = convertPressure(cal.adcMax, cal);
    check(r.loopHigh, "pinned ADC flags loop_high");
    checkNear(r.psi, cal.fullScalePsi, 0.01f, "over-range psi clamps to full scale");
}

static void testMeasuredResistorChangesCalibration() {
    // the whole reason senseOhms is a config field: same counts, different
    // measured resistor, different (more honest) pressure
    SensorCalibration nominal;
    SensorCalibration measured;
    measured.senseOhms = 151.5f; // what the multimeter actually said
    uint16_t counts = countsForVolts(1.8f, nominal);
    float psiNominal = convertPressure(counts, nominal).psi;
    float psiMeasured = convertPressure(counts, measured).psi;
    check(psiMeasured < psiNominal, "larger measured resistance reads lower current, lower psi");
}

static void testThermocoupleConversion() {
    // MAX31855 word: temp lives in D31..D18 at 0.25 C per lsb
    TcReading room = convertThermocouple(static_cast<uint32_t>(100 << 18)); // 100 * 0.25
    checkNear(room.degC, 25.0f, 0.001f, "raw 100 counts converts to 25.00 C");
    check(!room.open, "healthy word has no fault flags");

    // negative temps arrive as a signed 14 bit field
    TcReading cold = convertThermocouple(static_cast<uint32_t>(-4) << 18); // -4 * 0.25
    checkNear(cold.degC, -1.0f, 0.001f, "sign extends for sub-zero temps");
}

static void testThermocoupleFaultBits() {
    // open circuit: fault summary bit D16 plus OC bit D0
    TcReading oc = convertThermocouple((100u << 18) | (1u << 16) | 1u);
    check(oc.open, "open circuit bit flags tc_open");

    TcReading scg = convertThermocouple((100u << 18) | (1u << 16) | 2u);
    check(scg.open, "short-to-ground bit flags tc_open");
}

static void testFlowFromPeriod() {
    SensorCalibration cal;
    // GREDIA curve F = 23 * Q, so 1 L/min = 23 Hz = 43478 us between pulses
    checkNear(flowLpmFromPeriodUs(43478, cal), 1.0f, 0.01f, "43478 us period is 1 L/min");
    checkNear(flowLpmFromPeriodUs(21739, cal), 2.0f, 0.01f, "half the period is twice the flow");
    checkNear(flowLpmFromPeriodUs(0, cal), 0.0f, 1e-6f, "no recent pulse reads as zero flow");
}

static void testStaleBoundary() {
    SensorCalibration cal; // staleMs = 100
    check(!isStale(1000, 1100, cal), "exactly 100 ms old is still fresh");
    check(isStale(1000, 1101, cal), "101 ms old is stale");
    check(!isStale(1000, 1000, cal), "brand new sample is fresh");
}

void runSensorTests() {
    testsupport::section("sensors");
    testPressureAtLoopEndpoints();
    testPressureMidScale();
    testBrokenLoopFlagsLow();
    testOverRangeFlagsHigh();
    testMeasuredResistorChangesCalibration();
    testThermocoupleConversion();
    testThermocoupleFaultBits();
    testFlowFromPeriod();
    testStaleBoundary();
}
