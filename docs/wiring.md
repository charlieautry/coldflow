# Wiring Guide

How every wire on the rig hooks up. I wrote this for future me, standing at the bench
with a multimeter, not remembering which pin does what. The pin map here is final. If
a pin ever changes, this file changes in the same commit.

## Power tree

Three power domains, one shared ground point (see the grounding section).

```
SHNITPWR 12 V 3 A adjustable supply (set to 12.0 V)
 |
 +--> pump P-101 (+) ............. low-side switched, MOSFET channel A
 +--> vent solenoid SV-201 (+) ... low-side switched, MOSFET channel B
 +--> PT-101 loop (+) ............ 4-20 mA loop, returns through 150 ohm sense resistor
 +--> MP1584 buck input
       |
       +--> 5.0 V out --> GREDIA flow sensor (red wire), nothing else

USB wall adapter --> 4-port hub --> main Pico 2 (flight computer)
                               --> sim Pico 2 (resolver simulator)
```

Rules:

- The Picos run from USB only. They never touch the 12 V rail.
- Set the buck to 5.0 V with the flow sensor disconnected, then connect it.
- 12 V return, sense resistor bottom, main Pico GND, and sim Pico GND all land on
  the single star point.

## Main Pico 2 pinout

| Pin | Direction | Signal | Notes |
|-----|-----------|--------|-------|
| GP26 / ADC0 | in (analog) | PT-101 loop sense | Top of the 150 ohm sense resistor. 0.6 V at 4 mA, 3.0 V at 20 mA. Below 0.525 V (~3.5 mA) means broken loop. |
| GP27 / ADC1 | in (analog) | Resolver sin | From sim Pico GP2 through 1 kOhm + 100 nF RC filter. |
| GP28 / ADC2 | in (analog) | Resolver cos | From sim Pico GP4 through 1 kOhm + 100 nF RC filter. |
| GP2 | out (PWM) | Pump MOSFET gate input | 20 kHz PWM. 1 kOhm pulldown at the module input. |
| GP3 | out (digital) | Vent solenoid MOSFET | Plain on/off. High = coil energized = valve CLOSED (the valve is normally open). 1 kOhm pulldown at the module input. |
| GP4 | in (digital) | Flow sensor pulse | Yellow wire from the GREDIA sensor. 1 kOhm pulldown on this pin. Measure period between pulses, not counts per window. |
| GP5 | in (digital) | Resolver excitation sign | Square wave from sim Pico GP0. |
| GP8 / UART1 TX | out | Servo bus TX | Feetech SCS0009, 1 Mbaud, half duplex. |
| GP9 / UART1 RX | in | Servo bus RX | TX bridged to RX through 1 kOhm. The servo data line connects on the RX side. |
| GP16 / SPI0 MISO | in | MAX31855 DO | Thermocouple data. |
| GP17 / SPI0 CS | out | MAX31855 CS | Active low. |
| GP18 / SPI0 SCK | out | MAX31855 CLK | |
| GP25 | out | Onboard LED | Heartbeat blink. If it stops, the firmware is wedged. |
| 3V3 (OUT) | power | MAX31855 VCC | The breakout runs at 3.3 V. |
| GND | power | Star point | One wire from a Pico GND pin to the star point. |
| USB | power/data | Serial + 5 V | Through the 4-port hub. |

### Servo bus half-duplex bridge

The SCS0009 has a single data line. The Pico UART has separate TX and RX, so:

```
GP8 (TX) ----[1 kOhm]----+---- GP9 (RX)
                         |
                         +---- servo data line
```

The 1 kOhm lets the servo pull the line during its reply without fighting the TX pin.

## Resolver sim Pico pinout

| Pin | Direction | Signal | Notes |
|-----|-----------|--------|-------|
| GP0 | out (digital) | Excitation sign square wave | Straight wire to main Pico GP5. |
| GP2 | out (PWM) | Sin channel | Through RC (1 kOhm series, 100 nF to ground) to main Pico GP27. |
| GP4 | out (PWM) | Cos channel | Through RC (1 kOhm series, 100 nF to ground) to main Pico GP28. |
| GND | power | Star point | Common ground with the main Pico, at the star point. Without this the ADC readings are garbage. |
| USB | power/data | Serial + 5 V | Through the same hub. |

## The 4-20 mA loop, step by step

PT-101 is a 2-wire loop device. The transducer regulates the current through itself
to represent pressure: 4 mA = 0 psi, 20 mA = full scale (29 psi). That same current
flows through the sense resistor, and I read the voltage it makes.

```
+12 V ----> transducer (+) wire
            transducer (-) wire ----+----> GP26 / ADC0
                                    |
                                 [150 ohm]   <- measure the real value!
                                    |
                                star point ground
```

Wiring order:

1. 12 V+ to the transducer + wire.
2. Transducer - wire to the top of the 150 ohm sense resistor.
3. Bottom of the resistor to the star point ground.
4. GP26 taps the top of the resistor (same node as the transducer - wire).

Numbers to know:

- 4 mA x 150 ohm = 0.60 V (0 psi), 20 mA x 150 ohm = 3.00 V (29 psi).
- The 21 psi software abort lands around 15.6 mA, about 2.33 V.
- Below ~3.5 mA (0.525 V) the loop is broken: cut wire, dead transducer, or no 12 V.
  Firmware treats that as a fault, not as negative pressure.
- Measure the actual resistor with the multimeter (it will not be exactly 150.0) and
  put the measured value in the firmware config. A 1% error in R is a 1% error in
  every pressure reading.

This loop is the only run that uses shielded twisted pair. Shield rules below.

## MOSFET modules (pump and solenoid)

D4184-class dual module, logic level, low-side switching. The load sits between +12 V
and the module output; the MOSFET connects the load's low side to ground when the
gate input goes high.

```
+12 V ---+---- load (+)          load = pump motor or solenoid coil
         |
        _|_  1N5819, band (cathode) UP toward +12 V
        /_\
         |
         +---- load (-) ---- module OUT-
                             module GND ---- star point
GPIO ---- module signal in ---+
                              |
                           [1 kOhm]
                              |
                             GND
```

- Flyback diode orientation: cathode (the banded end) to +12 V, anode to the
  MOSFET-side terminal of the load. One diode across the pump, one across the
  solenoid. Backwards, it is a dead short whenever the channel turns on.
- The pump diode freewheels every PWM cycle at 20 kHz, so it runs warm. That is
  normal. The solenoid diode only conducts at turn-off.
- Never energize the solenoid without its diode. The coil's inductive kick will kill
  the MOSFET.
- Each module signal input gets a 1 kOhm pulldown to ground (see the E9 rule below)
  so the loads stay OFF while the Pico is resetting or a wire is unplugged.
- Pump channel: PWM at 20 kHz or less. Solenoid channel: plain on/off, no PWM.

## Grounding and shielding rules

**Star point grounding.** The 12 V return, the sense resistor bottom, the main Pico
GND, and the sim Pico GND all meet at exactly one physical point. Why: the pump pulls
an amp of chopped current through its return wire. If the ADC's ground reference
shares any length of that wire, the pump current drops millivolts across it and those
millivolts add straight into the pressure reading. One meeting point means no shared
return wire, so no shared voltage drop.

**Shield grounded at one end only.** The transducer loop runs in shielded twisted
pair, shield connected to ground at the Pico end, cut flush and insulated at the
transducer end. Why: the shield's job is to be an electrostatic fence, and it only
needs one ground connection to do that. Ground both ends and the shield becomes a
second return path between two points at slightly different potentials, so current
flows down it, right next to my analog pair. One end grounded is a fence. Both ends
is an antenna loop.

**Twisted pair on the analog loop only.** The two loop wires twist around each other
so that any magnetic field (mostly from the pump PWM wiring) induces equal and
opposite voltages in each twist, which cancel. Power wires do not need this
treatment; keep them short and route them away from the analog run instead.

## The RP2350-E9 pulldown rule

The RP2350 has an erratum (E9) where a GPIO input can latch partway high if the
external pulldown is too weak. The fix is simple: any external pulldown on this board
must be 8.2 kOhm or less. The 10 kOhm resistors in the parts kit are NOT usable as
pulldowns. 1 kOhm is the standard pulldown for this whole build: MOSFET inputs, the
flow sensor pin, all of it. Pullups are unaffected; this is a pulldown-only rule.

## Wire color convention

| Color | Meaning |
|-------|---------|
| Red | +12 V rail |
| Orange | +5 V (buck output) |
| Black | Ground / star point returns |
| Blue | Gate drive (GPIO to MOSFET module inputs) |
| Yellow | Pulse and digital signals (flow sensor, excitation sign) |
| Green | SPI to the MAX31855 |
| White pair (shielded) | 4-20 mA analog loop, and only that |
| Thermocouple yellow | Type K positive leg |
| Thermocouple red | Type K NEGATIVE leg (yes, red is negative on thermocouples, do not "fix" it) |

The thermocouple probe is ungrounded-junction, so the tip is isolated from the sheath
and the ground rules above stay intact. Keep the MAX31855 board close to the probe
connector: the cold-junction compensation measures the temperature at the board, so
the board should live where the copper transition actually happens.
