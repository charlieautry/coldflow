#include "pico_hal.h"

#include <cmath>
#include <cstdio>

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

namespace {

// pins, in one place
constexpr uint kPinPumpPwm = 2;
constexpr uint kPinVent = 3;
constexpr uint kPinFlow = 4;
constexpr uint kPinExcSign = 5;
constexpr uint kPinServoTx = 8;
constexpr uint kPinServoRx = 9;
constexpr uint kPinTcMiso = 16;
constexpr uint kPinTcCs = 17;
constexpr uint kPinTcSck = 18;
constexpr uint kPinLed = 25;
constexpr uint kPinAdcPressure = 26; // ADC0
constexpr uint kPinAdcSin = 27;      // ADC1
constexpr uint kPinAdcCos = 28;      // ADC2

constexpr uint32_t kServoBaud = 1000000;
constexpr uint16_t kPumpPwmWrap = 7499; // 150 MHz / (7499 + 1) = 20 kHz

// flow pulse timing lives in ISR-land, so: volatile, written only by the
// ISR, read by the main loop with interrupts briefly held off. this is the
// whole "volatile and ISR discipline" story in three variables.
volatile uint32_t g_lastEdgeUs = 0;
volatile uint32_t g_flowPeriodUs = 0;
volatile bool g_flowSeenEdge = false;

void flowIsr(uint gpio, uint32_t events) {
    (void)events;
    if (gpio != kPinFlow) return;
    const uint32_t now = time_us_32();
    if (g_flowSeenEdge) {
        g_flowPeriodUs = now - g_lastEdgeUs;
    }
    g_lastEdgeUs = now;
    g_flowSeenEdge = true;
}

}  // namespace

void PicoHal::init() {
    stdio_init_all();

    // outputs first, driven to the safe state before anything else runs
    gpio_init(kPinVent);
    gpio_set_dir(kPinVent, GPIO_OUT);
    gpio_put(kPinVent, false); // de-energized = vent open = safe

    gpio_set_function(kPinPumpPwm, GPIO_FUNC_PWM);
    const uint slice = pwm_gpio_to_slice_num(kPinPumpPwm);
    pwm_set_wrap(slice, kPumpPwmWrap);
    pwm_set_gpio_level(kPinPumpPwm, 0); // pump off
    pwm_set_enabled(slice, true);

    gpio_init(kPinLed);
    gpio_set_dir(kPinLed, GPIO_OUT);

    // analog
    adc_init();
    adc_gpio_init(kPinAdcPressure);
    adc_gpio_init(kPinAdcSin);
    adc_gpio_init(kPinAdcCos);

    // flow pulse input. external 1 kOhm pulldown on the board (RP2350-E9:
    // internal pulldowns can stick, externals <= 8.2k are the fix)
    gpio_init(kPinFlow);
    gpio_set_dir(kPinFlow, GPIO_IN);
    gpio_set_irq_enabled_with_callback(kPinFlow, GPIO_IRQ_EDGE_RISE, true, &flowIsr);

    // excitation sign input from the resolver sim
    gpio_init(kPinExcSign);
    gpio_set_dir(kPinExcSign, GPIO_IN);

    // thermocouple spi (read-only device, MOSI unused)
    spi_init(spi0, 1 * 1000 * 1000);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(kPinTcMiso, GPIO_FUNC_SPI);
    gpio_set_function(kPinTcSck, GPIO_FUNC_SPI);
    gpio_init(kPinTcCs);
    gpio_set_dir(kPinTcCs, GPIO_OUT);
    gpio_put(kPinTcCs, true);

    // servo bus
    servo_.init(uart1, kServoBaud, kPinServoTx, kPinServoRx);
}

uint32_t PicoHal::millis() {
    return to_ms_since_boot(get_absolute_time());
}

uint16_t PicoHal::readAdcOversampled(uint8_t channel) {
    adc_select_input(channel);
    uint32_t sum = 0;
    for (uint8_t i = 0; i < cfg_.adcOversample; i++) {
        sum += adc_read();
    }
    return static_cast<uint16_t>(sum / cfg_.adcOversample);
}

uint32_t PicoHal::readMax31855() {
    uint8_t rx[4] = {0, 0, 0, 0};
    gpio_put(kPinTcCs, false);
    sleep_us(1); // t_CSS is 100 ns, one microsecond is free margin
    spi_read_blocking(spi0, 0x00, rx, sizeof(rx));
    gpio_put(kPinTcCs, true);
    return (static_cast<uint32_t>(rx[0]) << 24) | (static_cast<uint32_t>(rx[1]) << 16) |
           (static_cast<uint32_t>(rx[2]) << 8) | static_cast<uint32_t>(rx[3]);
}

RawSensors PicoHal::readSensors() {
    RawSensors raw;
    raw.pressureCounts = readAdcOversampled(0);
    raw.pressureFresh = true; // a synchronous adc read is fresh by definition;
                              // the stale fault exists for when this loop stops
    raw.tcRaw = readMax31855();

    // snapshot the ISR variables atomically
    const uint32_t save = save_and_disable_interrupts();
    const uint32_t lastEdge = g_lastEdgeUs;
    const uint32_t period = g_flowPeriodUs;
    const bool seen = g_flowSeenEdge;
    restore_interrupts(save);

    // no pulse for a second = report zero flow (spec: flow_lpm 0.0)
    const uint32_t now = time_us_32();
    if (!seen || period == 0 || (now - lastEdge) > 1000000u) {
        raw.flowPeriodUs = 0;
    } else {
        raw.flowPeriodUs = period;
    }
    return raw;
}

void PicoHal::setPumpDuty(float duty01) {
    if (duty01 < 0.0f) duty01 = 0.0f;
    if (duty01 > 1.0f) duty01 = 1.0f;
    pwm_set_gpio_level(kPinPumpPwm, static_cast<uint16_t>(duty01 * kPumpPwmWrap));
}

void PicoHal::setVentEnergized(bool energized) {
    gpio_put(kPinVent, energized);
}

void PicoHal::setValvePos(float pos01) {
    if (pos01 < 0.0f) pos01 = 0.0f;
    if (pos01 > 1.0f) pos01 = 1.0f;

    // only talk to the bus when the command actually moved: quarter-count
    // deadband keeps the 100 Hz loop from saturating the uart with repeats
    if (lastValveCmd_ >= 0.0f && std::fabs(pos01 - lastValveCmd_) < 0.0005f) return;
    lastValveCmd_ = pos01;

    const float span = static_cast<float>(cfg_.servoOpenCounts) - static_cast<float>(cfg_.servoClosedCounts);
    const uint16_t counts = static_cast<uint16_t>(
        static_cast<float>(cfg_.servoClosedCounts) + pos01 * span + 0.5f);
    servo_.setPosition(cfg_.servoId, counts);
}

void PicoHal::writeLine(const char* line) {
    // telemetry lines arrive with their newline already attached; command
    // responses don't. normalize here so the wire always sees one '\n'.
    fputs(line, stdout);
    const char* p = line;
    while (*p != '\0') p++;
    if (p == line || *(p - 1) != '\n') fputc('\n', stdout);
    fflush(stdout);
}

float PicoHal::readResolverSin() {
    return static_cast<float>(readAdcOversampled(1)) / 4095.0f;
}

float PicoHal::readResolverCos() {
    return static_cast<float>(readAdcOversampled(2)) / 4095.0f;
}

bool PicoHal::readExcitationSign() {
    return gpio_get(kPinExcSign);
}

void PicoHal::setHeartbeatLed(bool on) {
    gpio_put(kPinLed, on);
}
