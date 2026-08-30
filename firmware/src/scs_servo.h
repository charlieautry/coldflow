#pragma once
#include <cstdint>

#include "hardware/uart.h"

// Minimal driver for the Feetech SCS0009 serial bus servo that actuates the
// needle valve (FCV-101). Write-only on purpose: we command positions and
// never wait on replies, because the control loop should not block on a servo
// that got unplugged. Half duplex bus: pico TX bridged to RX through 1 kOhm,
// the servo's data line hangs on the RX side (see docs/wiring.md).
//
// Packet format (Feetech SCS protocol):
//   0xFF 0xFF <id> <len> <instr> <params...> <checksum>
//   len = number of params + 2, checksum = ~(id + len + instr + params) & 0xFF
// SCS series registers are big endian. Goal position lives at 0x2A, followed
// by goal time (0x2C) and goal speed (0x2E), so one 6-byte write sets all
// three.

class ScsServo {
public:
    void init(uart_inst_t* uart, uint32_t baud, uint8_t txPin, uint8_t rxPin);

    // position 0..1023 maps onto the servo's ~300 degree travel.
    // speed 0 = as fast as it can, which is what a control valve wants.
    void setPosition(uint8_t id, uint16_t position, uint16_t speed = 0);

    // torque off, lets the valve be turned by hand (handy on the bench)
    void torqueEnable(uint8_t id, bool on);

private:
    void writeRegisters(uint8_t id, uint8_t addr, const uint8_t* data, uint8_t len);
    uart_inst_t* uart_ = nullptr;
};
