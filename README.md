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

## Building and running the tests

The firmware logic in `firmware/core/` is written so it has zero hardware dependencies, which means the unit tests compile and run on a normal desktop, no Pico required. This is on purpose: I can prove the state machine follows its spec before any dreaded water or electricity gets involved.

From the `firmware/` directory:

```
cmake -B build-host -DCOLDFLOW_HOST_TESTS=ON
cmake --build build-host
ctest --test-dir build-host -C Debug --output-on-failure
```

First command configures the build (you only need it once, or again if you add new files), second one compiles, third one runs the test suite.

The `-C Debug` on the ctest line is there because Visual Studio's generator is multi-config, one build folder holds Debug and Release at the same time, so ctest makes you pick one. Leave it off and you get a very unhelpful "Test not available without configuration" error. Ask me how I know :)

The actual on-Pico firmware build isn't wired up yet (needs the Pico SDK pulled in), and the pytest suite doesn't exist yet either. Both are coming, this section will grow.
