#pragma once
#include "hal.h"
#include "scs_servo.h"

// The real-hardware side of the Hal interface. Everything SDK-flavored lives
// here and in scs_servo.*; nothing in core/ knows this file exists.
//
// Pin map (matches docs/wiring.md, change it there first):
//   GP26/ADC0  PT-101 loop sense (150 ohm to AGND)
//   GP27/ADC1  resolver sin channel
//   GP28/ADC2  resolver cos channel
//   GP2        pump MOSFET PWM, 20 kHz
//   GP3        vent solenoid MOSFET (energized = closed, valve is NO)
//   GP4        flow sensor pulse input (1 kOhm external pulldown, RP2350-E9)
//   GP5        resolver excitation sign (digital, from the sim pico)
//   GP8/GP9    UART1 to the SCS0009 servo bus, half duplex, 1 Mbaud
//   GP16/17/18 SPI0 MISO / CS / SCK to the MAX31855
//   GP25       onboard LED heartbeat

struct PicoHalConfig {
    // servo counts at the valve's mechanical ends, through the 2:1 coupling.
    // PLACEHOLDERS until the coupling is assembled: find the real closed seat
    // by hand (torque off, close gently, read position), then set these.
    uint16_t servoClosedCounts = 200;
    uint16_t servoOpenCounts = 814;
    uint8_t servoId = 1;

    // 16x oversampling knocks the pico's noisy 12-bit adc down to something
    // the derivative term can live with
    uint8_t adcOversample = 16;
};

class PicoHal : public Hal {
public:
    explicit PicoHal(const PicoHalConfig& cfg) : cfg_(cfg) {}

    void init(); // claims every peripheral; call once before the first tick

    uint32_t millis() override;
    RawSensors readSensors() override;
    void setPumpDuty(float duty01) override;
    void setVentEnergized(bool energized) override;
    void setValvePos(float pos01) override;
    void writeLine(const char* line) override;

    // resolver bolt-on raw access, sampled by the fast loop in main.cpp
    float readResolverSin();  // normalized 0..1
    float readResolverCos();
    bool readExcitationSign();

    void setHeartbeatLed(bool on);

private:
    uint16_t readAdcOversampled(uint8_t channel);
    uint32_t readMax31855();

    PicoHalConfig cfg_;
    ScsServo servo_;
    float lastValveCmd_ = -1.0f; // don't spam the servo bus with repeats
};
