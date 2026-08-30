#include "sensors.h"

namespace {
    float clampf(float v, float lo, float hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
}

PressureReading convertPressure(uint16_t counts, const SensorCalibration& cal) {
    PressureReading r;

    // counts -> volts at the top of the sense resistor -> loop current
    const float volts = (static_cast<float>(counts) / static_cast<float>(cal.adcMax)) * cal.adcVref;
    r.loopMa = (volts / cal.senseOhms) * 1000.0f;

    // fault flags first, then the conversion. a broken loop still produces a
    // psi number (clamped to zero) but the flags are what the state machine
    // acts on, so a dead sensor can never read as "chamber empty, go ahead".
    r.loopLow = r.loopMa < cal.loopLowMa;
    r.loopHigh = r.loopMa > cal.loopHighMa;

    // 4 mA = 0 psi, 20 mA = full scale, linear in between
    const float span = 16.0f; // mA from zero to full scale
    r.psi = clampf(((r.loopMa - 4.0f) / span) * cal.fullScalePsi, 0.0f, cal.fullScalePsi);
    return r;
}

TcReading convertThermocouple(uint32_t raw) {
    TcReading r;

    // MAX31855 word, msb first:
    //   D31..D18 = thermocouple temp, signed 14 bit, 0.25 C per lsb
    //   D16      = fault summary bit
    //   D2..D0   = SCV (short to vcc), SCG (short to gnd), OC (open circuit)
    r.open = (raw & 0x00010007u) != 0;

    // arithmetic shift on a signed copy keeps the sign of sub-zero temps
    const int32_t signedRaw = static_cast<int32_t>(raw);
    r.degC = static_cast<float>(signedRaw >> 18) * 0.25f;
    return r;
}

float flowLpmFromPeriodUs(uint32_t periodUs, const SensorCalibration& cal) {
    if (periodUs == 0) return 0.0f; // no pulse seen recently = no flow
    const float hz = 1.0e6f / static_cast<float>(periodUs);
    return hz / cal.flowHzPerLpm;
}

bool isStale(uint32_t lastFreshMs, uint32_t nowMs, const SensorCalibration& cal) {
    // unsigned subtraction survives the (never on this rig) millis wraparound
    return (nowMs - lastFreshMs) > cal.staleMs;
}
