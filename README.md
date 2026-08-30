## Pressurized PVC, water, and $300 in electronics, what can go wrong?

Hello! My name is Charlie Autry,
at the time of writing this I'm a Senior at Oklahoma State University studying Computer Science.
In about two weeks I have to write pytest and C++ and this project is more or less an assertion that I can do that!

## So what exactly is this project?

Right. First here's the tldr caveman version: This is the software side of a DIY coldflow rig. What is a DIY coldflow rig you probably didn't ask?

My DIY coldflow rig is essentially an attempt at replicating how propulsion engineers flow some kind of an inert fluid (in my case water, mimicking propellant) through a propulsion system without ignition, and then validate & assert everything except combustion.
Hence the word "cold" -- it doesn't refer to temperature, cold refers to the lack of combustion.

## Now heres the long and boring version:

This DIY coldflow rig does a multitude of things in one project.

1. First, how can we assert & verify the properties of something physical with hardware and code? (and do so with great modularity)
2. What does a good firmware spec look like for future persons to make sense of your work?
3. Can you connect tubing, PVC, brass, hardware and not blow up your basement and ruin your mom's carpet?

What's the point of doing these said steps?
Simply put,
there is no undo button in space, and it's important to be able to make assertions on the ground that say this is going to work always.
A failure with combustion can mean a fireball, the reason a coldflow test exists is to say that if we were to light this on fire, it wouldn't kill us.

Let's deep dive each step.

**Step 1, asserting and verifying.**
Asserting and verification is through a pytest suite written in python. Through USB connection the code connects to multiple controllers and if hardware is setup correctly,
the pytest suite should run, start actions, and say yep, looks like it works!

There's a whole stack underneath that sentence though. 

So, at the bottom is C++ firmware on a Raspberry Pi Pico 2 running a state machine. On top of the firmware sits a text-over-serial protocol (documented in `docs/firmware-spec.md`), then a python driver that speaks it, then pytest asserting things like "when I inject an overpressure fault, the board aborts, and it does it fast." The modularity point from earlier is this: every layer can be tested without the layer below it being fully real. Fake serial port, fake sensors, fake clock. The hardware only has to show up for the final exam!! Pretty sick right?

**Step 2, the spec.**
The firmware spec lives in `docs/firmware-spec.md` and it is the single source of truth for this project. (I might have changed it. a lot.) The pytest suite asserts against the spec, the python driver parses what the spec says it will receive, and when the code and the spec disagree, one of them is wrong and lets be real it's probably the code. I wrote it before writing the firmware, which felt slow at the time and has already paid for itself twice.

**Step 3, the not blowing up part.**
The physical rig is a loop: reservoir -> pump -> check valve -> a small PVC pressure chamber -> a servo driven needle valve (playing the role of "the engine") -> andd back to the reservoir. A normally open solenoid on top of the chamber is the abort path, normally open meaning if power dies, the vent falls open and the pressure leaves, which I thought was pretty cool. The safe state is the lazy state, exactly how you want it.

Not ruining the carpet is a layers thing: operate at 5-15 psi, software aborts around 21, the pressure transducer maxes out at 29, and the pump physically can't push past its own relief somewhere in the 30-40 range. Software catches it first, and if software embarrasses itself, physics is standing behind it with the actual last word.

## What's in the repo

```
firmware/
  core/          the brain: state machine, PID, sensors, protocol, resolver
                 decoder, and the Rig glue. zero hardware includes, on purpose
  src/           the only code that touches pins: PicoHal, servo driver, main loop
  resolver_sim/  firmware for the second pico that pretends to be a resolver
  tests/         off-target unit tests, run on the desktop via ctest
host/
  teststand/     the python package: driver, csv logging, fake board,
                 sequence runner, redline monitor, data review tooling
  tests/         the pytest suite (runs with zero hardware by default)
  sequences/     test procedures as YAML data
docs/            firmware spec, host harness reference, wiring, bring-up, P&ID
cad/             OpenSCAD source for the printed parts (brackets, coupling, tiles)
```

The architecture rule that makes it all hang together: `firmware/core/` never
includes an SDK header, and everything hardware-shaped goes through one small
`Hal` interface. That's why the same state machine that runs the rig also runs
on my desktop against a fake, and why the pytest suite has a `FakeBoard` that
lets the entire host stack pass with the rig unplugged.

## Building and running the firmware tests

The firmware logic in `firmware/core/` is written so it has zero hardware dependencies, which means the unit tests compile and run on a normal desktop, no Pico required. This is on purpose: I can prove the state machine follows its spec before any dreaded water or electricity gets involved.

From the `firmware/` directory:

```
cmake -B build-host -DCOLDFLOW_HOST_TESTS=ON
cmake --build build-host
ctest --test-dir build-host -C Debug --output-on-failure
```

First command configures the build (you only need it once, or again if you add new files), second one compiles, third one runs the test suite.

The `-C Debug` on the ctest line is there because Visual Studio's generator is multi-config, one build folder holds Debug and Release at the same time, so ctest makes you pick one. Leave it off and you get a very unhelpful "Test not available without configuration" error. Ask me how I know :)

## Building the actual Pico firmware

This needs the Pico SDK and the ARM toolchain installed, with `PICO_SDK_PATH`
set in the environment (`pico_sdk_import.cmake` is already in `firmware/`).
Then, from `firmware/`:

```
cmake -B build
cmake --build build
```

That produces two flashables: `coldflow.uf2` (the flight computer) and
`resolver_sim.uf2` (the second pico). Hold BOOTSEL, plug in, drag the uf2 on.
Fair warning: the off-target tests run on every change, but the on-target
build only gets exercised when I'm at the bench with the toolchain, so if you
somehow have this cloned and it doesn't compile, that's why.

## The host side

One-time setup, from `host/`:

```
python -m venv .venv
.venv\Scripts\activate        (or source .venv/bin/activate)
pip install -e .[dev]
```

Then the fun part. The whole pytest suite runs against a simulated board by
default, no hardware, about a second:

```
pytest
```

Point the same suite at the real rig by naming its serial port:

```
pytest --port COM5
```

Test procedures are YAML files executed by the sequence runner, watched by
redlines that abort independently of the sequence:

```
python -m teststand run sequences/hold_10psi.yaml --fake
python -m teststand run sequences/hold_10psi.yaml --port COM5
```

Every run logs a DAQ-style CSV under `data/`, and the review tool turns one
back into judgment (stats, state timeline, redline proximity, settling time,
a plot):

```
python -m teststand review data/hold_10psi_20260830_183000.csv --plot out.png
```

The details of all of it are in `docs/host-harness.md`.

## Where things stand

Software: state machine, PID, sensor conversion, serial protocol, resolver
decoder, and the rig glue are all written and covered by the off-target suite
(242 checks) plus the host pytest suite (71 tests). The physical rig is
assembled-in-progress on the bench; wiring and bring-up live in `docs/`, and
the PID gains in the firmware are placeholder values until the water loop
gets tuned for real. Expect the performance numbers in
`test_pid_performance.py` to tighten once that happens.
