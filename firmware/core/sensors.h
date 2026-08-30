#pragma once
#include <cstdint>

// Sensor conversion layer: raw hardware readings in, engineering units and
// fault flags out. Pure math so it unit-tests off-target. The hardware side
// (ADC reads, SPI transfers, the flow ISR) lives in src/ and just feeds
// numbers into these functions.

struct SensorCalibration {
    // 4-20 mA loop. senseOhms is THE calibration number for pressure: put the
    // multimeter-measured value of the actual resistor here, not 150.0 from
    // the bag label. a 1% resistor error is 0.3 psi of lie at full scale.
    float senseOhms = 150.0f;
    float adcVref = 3.3f;
    uint16_t adcMax = 4095;      // 12 bit
    float fullScalePsi = 29.0f;  // 0.2 MPa transducer: 4 mA = 0 psi, 20 mA = 29 psi
    float loopLowMa = 3.5f;      // below this = broken loop
    float loopHighMa = 21.0f;    // above this = short / over-range

    // GREDIA hall flow sensor: F(Hz) = 23 * Q(L/min) per the datasheet
    float flowHzPerLpm = 23.0f;

    // no fresh pressure sample for this long = stale fault
    uint32_t staleMs = 100;
};

struct PressureReading {
    float psi = 0.0f;    // clamped to [0, fullScalePsi]
    float loopMa = 0.0f; // raw loop current, useful for debugging the loop itself
    bool loopLow = false;
    bool loopHigh = false;
};

PressureReading convertPressure(uint16_t counts, const SensorCalibration& cal);

struct TcReading {
    float degC = 0.0f;
    bool open = false; // any MAX31855 fault: open circuit, short to gnd/vcc
};

// raw 32-bit word clocked out of the MAX31855, msb first
TcReading convertThermocouple(uint32_t raw);

// flow from the period between hall pulses (not counts per window, so slow
// flow still updates every pulse). periodUs == 0 means "no pulse in the last
// second" and reads as zero flow.
float flowLpmFromPeriodUs(uint32_t periodUs, const SensorCalibration& cal);

// true when the last fresh pressure sample is older than the stale window
bool isStale(uint32_t lastFreshMs, uint32_t nowMs, const SensorCalibration& cal);
