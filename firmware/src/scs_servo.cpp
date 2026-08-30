#include "scs_servo.h"

#include "hardware/gpio.h"

namespace {
    constexpr uint8_t kInstrWrite = 0x03;
    constexpr uint8_t kRegGoalPosition = 0x2A; // pos(2) time(2) speed(2), big endian
    constexpr uint8_t kRegTorqueEnable = 0x28;
}

void ScsServo::init(uart_inst_t* uart, uint32_t baud, uint8_t txPin, uint8_t rxPin) {
    uart_ = uart;
    uart_init(uart_, baud);
    gpio_set_function(txPin, GPIO_FUNC_UART);
    gpio_set_function(rxPin, GPIO_FUNC_UART);
}

void ScsServo::setPosition(uint8_t id, uint16_t position, uint16_t speed) {
    if (position > 1023) position = 1023;
    const uint8_t data[6] = {
        static_cast<uint8_t>(position >> 8), static_cast<uint8_t>(position & 0xFF),
        0x00, 0x00, // goal time 0: position mode, no fixed duration
        static_cast<uint8_t>(speed >> 8), static_cast<uint8_t>(speed & 0xFF),
    };
    writeRegisters(id, kRegGoalPosition, data, sizeof(data));
}

void ScsServo::torqueEnable(uint8_t id, bool on) {
    const uint8_t data[1] = { static_cast<uint8_t>(on ? 1 : 0) };
    writeRegisters(id, kRegTorqueEnable, data, sizeof(data));
}

void ScsServo::writeRegisters(uint8_t id, uint8_t addr, const uint8_t* data, uint8_t len) {
    if (uart_ == nullptr) return;

    uint8_t packet[16];
    uint8_t n = 0;
    packet[n++] = 0xFF;
    packet[n++] = 0xFF;
    packet[n++] = id;
    packet[n++] = static_cast<uint8_t>(len + 3); // addr + params + checksum... per
                                                 // protocol: instr + addr + data + 2
    packet[n++] = kInstrWrite;
    packet[n++] = addr;

    uint8_t sum = static_cast<uint8_t>(id + packet[3] + kInstrWrite + addr);
    for (uint8_t i = 0; i < len; i++) {
        packet[n++] = data[i];
        sum = static_cast<uint8_t>(sum + data[i]);
    }
    packet[n++] = static_cast<uint8_t>(~sum);

    uart_write_blocking(uart_, packet, n);
    // the servo answers on the shared line; we don't read it, and the 1 kOhm
    // bridge keeps its reply from fighting our idle TX
}
