# Quick Links

Reference docs for the coldflow rig project, grouped by what they're needed for.

## pytest / Python test harness

- pytest docs (main): https://docs.pytest.org/en/stable/
- How-to guides (fixtures, parametrize, markers): https://docs.pytest.org/en/stable/how-to/index.html
- Fixtures deep-dive: https://docs.pytest.org/en/stable/how-to/fixtures.html
- Good integration practices (test layout, conftest, import modes): https://docs.pytest.org/en/stable/explanation/goodpractices.html
- pyserial API: https://pyserial.readthedocs.io/en/latest/pyserial_api.html
- Python 3 stdlib (csv, json, argparse, dataclasses): https://docs.python.org/3/library/index.html

## C++ / firmware

- Pico C SDK API docs: https://www.raspberrypi.com/documentation/pico-sdk/
- Getting started with Pico (toolchain setup, first build): https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf
- pico-examples (canonical usage of every SDK API): https://github.com/raspberrypi/pico-examples
- RP2350 datasheet (ADC, PWM, PIO chapters; E9 erratum in appendix): https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf
- Pico 2 pinout: https://datasheets.raspberrypi.com/pico/pico-2-pinout.pdf
- cppreference (language + stdlib): https://en.cppreference.com/
- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- CMake tutorial: https://cmake.org/cmake/help/latest/guide/tutorial/
- Catch2 (off-target unit tests): https://github.com/catchorg/Catch2/tree/devel/docs

## Git

- Pro Git book (free, full text): https://git-scm.com/book/en/v2
- Command reference: https://git-scm.com/docs
- GitHub flow (branches + PRs): https://docs.github.com/en/get-started/using-github/github-flow

## Sensors, wiring, electronics

- NI field wiring and noise guide (shielding, grounding, floating vs grounded sources): https://www.ni.com/en/support/documentation/supplemental/06/field-wiring-and-noise-considerations-for-analog-signals.html
- MAX31855 datasheet (thermocouple SPI interface): https://www.analog.com/media/en/technical-documentation/data-sheets/MAX31855.pdf
- Adafruit MAX31855 guide (wiring + gotchas): https://learn.adafruit.com/thermocouple
- 4-20 mA current loop basics: https://en.wikipedia.org/wiki/Current_loop

## Fluid system / P&ID

- P&ID overview and ISA symbology: https://en.wikipedia.org/wiki/Piping_and_instrumentation_diagram
- NPT thread reference: https://en.wikipedia.org/wiki/National_pipe_thread

## Reference projects (patterns to study, not copy)

- ANU Rocketry control-panel (sequences-as-data, valve naming): https://github.com/ANU-Rocketry/control-panel
- PSAS liquid engine test stand: https://github.com/psas/liquid-engine-test-stand
- Golioth: automated hardware testing using pytest: https://blog.golioth.io/automated-hardware-testing-using-pytest/
- Arduino Due pytest HIL example: https://gitlab.com/ci-cd-examples/arduino-due-pytest-hil-example
- Synnax docs (professional test-ops software concepts): https://docs.synnaxlabs.com/
