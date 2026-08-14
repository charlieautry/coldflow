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

Asserting and verification is through a pytest suite written in python. Through USB connection the code connects to multiple controllers and if hardware is setup correctly,
the pytest suite should run, start actions, and say yep, looks like it works! 