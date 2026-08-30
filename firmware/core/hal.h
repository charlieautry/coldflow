#pragma once
#include <cstdint>

// The hardware abstraction boundary. Everything the control logic wants from
// the physical world goes through this one interface, so core/ never includes
// an SDK header and the whole brain runs off-target against a FakeHal.
// src/pico_hal.* implements it with real ADC/PWM/SPI/UART calls.

// one tick's worth of raw sensor data, exactly as the hardware produced it.
// conversion to engineering units happens in core/sensors.*, on our side of
// the boundary, so the math is testable.
struct RawSensors {
    uint16_t pressureCounts = 0; // 12-bit ADC counts off the 150 ohm sense resistor
    bool pressureFresh = false;  // true if the ADC produced a new sample this tick
    uint32_t tcRaw = 0;          // 32-bit word clocked out of the MAX31855
    uint32_t flowPeriodUs = 0;   // period between hall pulses, 0 = none in the last second
};

class Hal {
public:
    virtual ~Hal() = default;

    virtual uint32_t millis() = 0;
    virtual RawSensors readSensors() = 0;

    virtual void setPumpDuty(float duty01) = 0;        // 0 = off, 1 = full send
    virtual void setVentEnergized(bool energized) = 0; // NO solenoid: energized = CLOSED
    virtual void setValvePos(float pos01) = 0;         // needle valve, 0 = closed, 1 = open

    // one complete line (already newline-terminated where it matters)
    virtual void writeLine(const char* line) = 0;
};
