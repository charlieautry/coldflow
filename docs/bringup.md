# Bring-Up Procedure

First power-on, in order. Each stage has three parts: what to do, what to measure
(multimeter points and expected readings), and a gate. If the gate fails, stop and
fix it, because every later stage assumes the earlier ones passed.

## Safety, read this first

- **Never energize the solenoid without its flyback diode installed.** The inductive
  kick at turn-off will destroy the MOSFET. Confirm the diode is there and the band
  faces +12 V before the coil ever sees power.
- **Compressed air in PVC stores energy. Water does not (much).** Air compresses, so
  a burst with a big air headspace throws fragments; water barely compresses, so a
  burst with water mostly just leaks. Keep the air headspace in the chamber small,
  use water as the working fluid, and keep any air-only testing at 5 psi or below.
- **NPT joints get PTFE tape, 2 to 3 wraps**, wound clockwise when looking at the
  open end of the thread so it does not unwind during assembly. G1/4 threads do NOT
  seal on the thread; those seal on the washer face, no tape.
- **Do not exceed 15 psi manually, ever.** Operating band is 5 to 15 psi, software
  abort is 21 psi, transducer full scale is 29 psi, pump relief is 30 to 40 psi.
  The glycerin gauge is the truth reference; believe it over the transducer.

## Stage 1: bench power, no loads

**What to do:** Disconnect pump, solenoid, transducer, and flow sensor. Power the
12 V supply and dial it in. Power the USB hub and plug in both Picos.

**What to measure:**
- Supply terminals: 12.0 V, plus or minus 0.1.
- Star point to +12 V rail: the same 12.0 V (confirms the return path).
- MP1584 buck output: adjust the trimpot to 5.00 V, sensor still disconnected.
- Both Picos enumerate over USB, and the heartbeat LED blinks on the main one.

**Do not continue unless:** 12 V and 5 V are both correct and stable, and both Picos
show up as serial ports.

## Stage 2: MOSFET channels, multimeter only

**What to do:** Clip a 1 kOhm resistor from each module output (OUT-) up to +12 V as
a dummy load. Leave the real loads disconnected. Drive the inputs by hand: first
nothing (input floating), then a jumper to 3.3 V.

**What to measure:**
- Input floating: OUT- reads about 12 V (channel off, held off by the 1 kOhm
  pulldown).
- Input jumped to 3.3 V: OUT- reads under 0.1 V (channel on).
- Repeat for both channels.

**Do not continue unless:** both channels switch cleanly and both sit OFF with the
input floating. If a channel is on while floating, the pulldown is missing or too
big (the kit 10 kOhm does not count, see the E9 note in wiring.md).

## Stage 3: MOSFET channels with real loads

**What to do:** Verify both 1N5819 flyback diodes are installed, band toward +12 V.
Connect the pump to channel A and the solenoid to channel B. From the firmware or a
REPL, PWM GP2 at 20 kHz and sweep duty; toggle GP3 high and low.

**What to measure:**
- Pump spins up with duty (dry-running a diaphragm pump for a few seconds is fine).
- Solenoid clicks on GP3 high; voltage across the coil reads about 12 V energized.
- Touch test after a minute of pump PWM: the pump-side diode runs warm. Expected.

**Do not continue unless:** the pump responds to duty, the solenoid audibly opens
and closes, and nothing gets alarmingly hot (warm diode fine, hot MOSFET not fine).

## Stage 4: 4-20 mA loop, dry

**What to do:** Measure the actual sense resistor with the multimeter first and
write the measured value into the firmware config (it should be near 150 ohm but
not exactly). Then wire the loop per wiring.md and power the 12 V rail. The chamber
is open to air, so the transducer sees 0 psig.

**What to measure:**
- GP26 node (top of the sense resistor) to star ground: 0.60 V at 0 psi with a true
  150 ohm (in general, 0.004 x measured R).
- Pull one loop wire: the node drops to about 0 V and the firmware should flag a
  broken-loop fault (threshold 3.5 mA, which is 0.525 V).

**Do not continue unless:** 0 psi reads 4 mA within a couple percent, and the
broken-loop fault fires when the loop is opened.

## Stage 5: thermocouple

**What to do:** Connect the MAX31855 on SPI0 (SCK GP18, MISO GP16, CS GP17, VCC
3.3 V) with the Type K probe (yellow = +, red = -). Read temperature over serial.

**What to measure:**
- Ambient: 20 to 25 C, steady within about half a degree.
- Pinch the probe tip: reading climbs toward 30 C within seconds.
- Probe tip in ice water: 0 to 2 C.

**Do not continue unless:** all three points read sane. A reading stuck at a weird
constant or a fault bit usually means swapped legs or an open probe.

## Stage 6: flow sensor spin test

**What to do:** Power the sensor from the 5 V buck (red = 5 V, black = star ground,
yellow = signal). Fit the 1 kOhm pulldown on the input side. Before landing yellow
on GP4, check its high level. Then connect GP4 and blow through the sensor body in
the arrow direction.

**What to measure:**
- Yellow wire high level while blowing: must be at or below 3.3 V before it ever
  touches the Pico pin.
- While blowing: a pulse train on GP4; the firmware period measurement shows a few
  Hz up to a few tens of Hz. For scale, full pump flow of 1.3 L/min should read
  about 30 Hz (F = 23 x Q).

**Do not continue unless:** pulses count while blowing, zero pulses at rest, and the
signal level is Pico-safe.

## Stage 7: servo bus

**What to do:** Wire the half-duplex bridge (GP8 TX through 1 kOhm to GP9 RX, servo
data line on the RX side), servo powered, 1 Mbaud. Leave the servo mechanically
DISCONNECTED from the needle valve coupling. Ping the servo ID, then command small
moves.

**What to measure:**
- Ping gets a reply (this proves the 1 kOhm bridge and the baud rate).
- Small position commands move the horn; position reads back within a few counts.

**Do not continue unless:** commands and replies both work. Only then couple the
2:1 gear to the needle valve, and set soft travel limits before the first coupled
move so the servo cannot drive the valve into its hard stop.

## Stage 8: dry plumbing pressure test, low-pressure air

**What to do:** Full plumbing assembled, no water. Close FCV-101 (the needle valve),
leave the solenoid unpowered, and cap or clamp the vent path for this test. Bring
the chamber to about 3 psi with air (hand pump, or a brief pump run on air). Brush
soapy water on every joint: bulkheads, NPT fittings, the G1/4 adapter, barbs.

**What to measure:**
- Glycerin gauge: pump to 3 psi, never above 5 psi on air (stored energy, see the
  safety section).
- Watch the gauge for 10 minutes: less than 0.5 psi drop.
- No soap bubbles anywhere.

**Do not continue unless:** the system holds 3 psi of air for 10 minutes bubble
free. Fix leaks now, while everything is dry.

## Stage 9: first water fill

**What to do:** Solenoid unpowered, which means the vent is OPEN (it is a normally
open valve). Fill the reservoir with distilled water only. Prime the pump with 1 to
2 second blips until water moves, then let everything sit full for a few minutes.

**What to measure:**
- Visual: no drips at any joint at zero pressure.
- The sight glass shows a water level.
- PT-101 node still reads about 0.60 V (0 psi, vent open).

**Do not continue unless:** the floor is dry, water is visible in the sight glass,
and the transducer still agrees there is no pressure.

## Stage 10: first pump run, vent open

**What to do:** Vent stays open (solenoid unpowered). Run the pump continuously at
moderate duty for 30 to 60 seconds. Water should circulate: chamber, out the vent
path, back to the reservoir.

**What to measure:**
- Flow sensor: a steady frequency, up to about 30 Hz near full flow.
- Glycerin gauge and PT-101: near 0, no more than about 1 psi, since the vent is
  open.
- Listen: the pump smooths out once the air is purged.

**Do not continue unless:** stable circulation, air burped out, still no leaks, and
pressure stays near zero with the vent open.

## Stage 11: first closed-vent pressurization, 5 psi

**What to do:** Keep a hand near the power switch. Energize the solenoid (GP3 high
closes the vent), run the pump at low duty, and watch the glycerin gauge climb.
Stop at 5 psi. Then the abort check: cut GP3 and confirm the vent snaps open and
pressure falls immediately.

**What to measure:**
- Glycerin gauge: a controlled creep up to 5 psi, no overshoot past about 6.
- PT-101 node: about 1.0 V at 5 psi; transducer and gauge agree within 1 psi.
- De-energize the solenoid: pressure drops to near zero within a couple of seconds.

**Do not continue unless:** gauge and transducer agree, 5 psi holds under manual
control, and the de-energize abort dumps pressure. Never push past 15 psi by hand.
Anything above that belongs to the software, with the 21 psi abort armed.

After stage 11 the rig is ready for closed-loop PID work and the pytest campaigns.
