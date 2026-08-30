// Resolver simulator, the whole second pico. Pretends to be a resolver on a
// spinning (or parked) shaft: two PWM+RC channels carry sin/cos windings
// amplitude-modulated by the excitation carrier, one digital pin carries the
// carrier's sign for the decoder's synchronous demodulation.
//
//   GP0  excitation sign, 500 Hz square  -> main pico GP5
//   GP2  sin winding PWM -> RC (1k + 100 nF) -> main pico GP27/ADC1
//   GP4  cos winding PWM -> RC (1k + 100 nF) -> main pico GP28/ADC2
//
// Serial commands (usb, newline terminated, same ok/err framing as the
// flight computer so the pytest plumbing is shared):
//   ANGLE <deg>   park the fake shaft at an angle       -> ok
//   SPIN <deg/s>  rotate continuously                    -> ok
//   STOP          freeze wherever it is                  -> ok
//   STATUS        one json line with the current angle   -> then ok

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

namespace {

constexpr uint kPinExcSign = 0;
constexpr uint kPinSinPwm = 2;
constexpr uint kPinCosPwm = 4;

constexpr uint16_t kPwmWrap = 1023;        // ~146 kHz pwm, far above the RC cutoff
constexpr uint32_t kUpdatePeriodUs = 100;  // waveform update rate, 10 kHz
constexpr uint32_t kCarrierHalfPeriodUs = 1000; // 500 Hz excitation
constexpr float kAmplitude = 0.4f;         // envelope swing around the 0.5 midpoint
constexpr float kPi = 3.14159265358979f;

// shared between the timer callback and the command loop
volatile float g_angleDeg = 0.0f;
volatile float g_spinDegPerSec = 0.0f;

bool waveformUpdate(repeating_timer_t* t) {
    (void)t;
    static uint32_t usAccum = 0;
    static bool carrierHigh = true;

    // advance the fake shaft
    if (g_spinDegPerSec != 0.0f) {
        float a = g_angleDeg + g_spinDegPerSec * (static_cast<float>(kUpdatePeriodUs) * 1e-6f);
        while (a >= 360.0f) a -= 360.0f;
        while (a < 0.0f) a += 360.0f;
        g_angleDeg = a;
    }

    // flip the carrier every half period
    usAccum += kUpdatePeriodUs;
    if (usAccum >= kCarrierHalfPeriodUs) {
        usAccum = 0;
        carrierHigh = !carrierHigh;
        gpio_put(kPinExcSign, carrierHigh);
    }

    const float carrier = carrierHigh ? 1.0f : -1.0f;
    const float rad = g_angleDeg * kPi / 180.0f;
    const float dutySin = 0.5f + kAmplitude * std::sin(rad) * carrier;
    const float dutyCos = 0.5f + kAmplitude * std::cos(rad) * carrier;
    pwm_set_gpio_level(kPinSinPwm, static_cast<uint16_t>(dutySin * kPwmWrap));
    pwm_set_gpio_level(kPinCosPwm, static_cast<uint16_t>(dutyCos * kPwmWrap));
    return true; // keep repeating
}

void setupPwmPin(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    const uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_wrap(slice, kPwmWrap);
    pwm_set_gpio_level(pin, kPwmWrap / 2);
    pwm_set_enabled(slice, true);
}

// same tolerant float parsing the flight computer uses
bool parseFloatArg(const char* s, float* out) {
    while (*s == ' ') s++;
    if (*s == '\0') return false;
    char* end = nullptr;
    float v = std::strtof(s, &end);
    if (end == s) return false;
    while (*end == ' ' || *end == '\r') end++;
    if (*end != '\0') return false;
    *out = v;
    return true;
}

void handleLine(char* line) {
    // uppercase the command word in place
    char* p = line;
    while (*p == ' ') p++;
    char* word = p;
    while (*p && *p != ' ' && *p != '\r') {
        *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
        p++;
    }
    const char* args = p;
    if (*p) { *p = '\0'; args = p + 1; }

    if (*word == '\0') return;

    float v = 0.0f;
    if (std::strcmp(word, "ANGLE") == 0 && parseFloatArg(args, &v)) {
        while (v >= 360.0f) v -= 360.0f;
        while (v < 0.0f) v += 360.0f;
        g_spinDegPerSec = 0.0f;
        g_angleDeg = v;
        printf("ok\n");
    } else if (std::strcmp(word, "SPIN") == 0 && parseFloatArg(args, &v)) {
        g_spinDegPerSec = v;
        printf("ok\n");
    } else if (std::strcmp(word, "STOP") == 0) {
        g_spinDegPerSec = 0.0f;
        printf("ok\n");
    } else if (std::strcmp(word, "STATUS") == 0) {
        printf("{\"angle_deg\":%.2f,\"spin_dps\":%.2f}\n", static_cast<double>(g_angleDeg),
               static_cast<double>(g_spinDegPerSec));
        printf("ok\n");
    } else if (std::strcmp(word, "ANGLE") == 0 || std::strcmp(word, "SPIN") == 0) {
        printf("err arg\n");
    } else {
        printf("err unknown\n");
    }
    fflush(stdout);
}

}  // namespace

int main() {
    stdio_init_all();

    gpio_init(kPinExcSign);
    gpio_set_dir(kPinExcSign, GPIO_OUT);
    setupPwmPin(kPinSinPwm);
    setupPwmPin(kPinCosPwm);

    repeating_timer_t timer;
    add_repeating_timer_us(-static_cast<int32_t>(kUpdatePeriodUs), waveformUpdate, nullptr, &timer);

    char line[64];
    unsigned len = 0;
    while (true) {
        const int c = getchar_timeout_us(1000);
        if (c == PICO_ERROR_TIMEOUT) continue;
        if (c == '\n') {
            line[len] = '\0';
            handleLine(line);
            len = 0;
        } else if (len < sizeof(line) - 1) {
            line[len++] = static_cast<char>(c);
        }
    }
}
