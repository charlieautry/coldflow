# cad

Printed parts for the coldflow rig. Everything here is structural or
organizational only. Nothing printed touches pressure or does any sealing,
that is all brass, PVC, and rubber.

All parts are OpenSCAD source. Open a file, tweak the parameters at the top,
render (F6), export STL, slice. Printer is a Bambu P1S, material is PETG for
everything (it shrugs off water splash and doesn't creep like PLA when a servo
is leaning on it all day).

## The tile grid convention

Every mounting hole in this folder lands on a 20 mm grid and takes an M4
screw. The baseplate tiles carry bosses with 3.4 mm pilot holes (M4 self taps
straight into PETG), and every bracket puts its holes on multiples of 20 mm so
anything can bolt down anywhere on the deck. If you add a part, keep its holes
on the grid.

## Parts

### baseplate_tile.scad

120 x 120 mm floor tile. Dovetail tabs on two edges, matching sockets on the
other two, so tiles chain in both directions. M4 screw bosses on the 20 mm
grid, plus two 8 mm wide x 3 mm deep wiring channels running front to back,
one lane for signal (the 4-20 mA loop, thermocouple, flow sensor) and one for
12V power (pump PWM, solenoid), kept 60 mm apart to keep switching noise away
from the analog lines.

Print: 0.2 mm layers, 3 walls, 20% infill, no supports. Print one, check the
dovetail fit against a second one, adjust `joint_clr` if the fit is wrong
(bigger number = looser), then batch the rest.

### servo_valve_coupling.scad

Two spur gears, 16T pinion and 32T gear, module 1.5, giving a 2:1 reduction
from the SCS0009 servo to the needle valve stem. Doubles torque and position
resolution at the valve. The pinion screws onto the stock plastic horn disc
with two small self tap screws (the real spline is 20T at 3.95 mm OD, no
chance of printing that). The valve gear has a flatted bore matching the
valve stem plus an M3 set screw bearing on the flat.

Center distance is 36 mm and `servo_bracket.scad` is built around that number.
Change the tooth counts or module in one file and you must change the other.

Print: 0.2 mm layers, 4 walls, 40% infill. Gears take the actual load here so
don't skimp on walls.

### servo_bracket.scad

Tower that holds the servo with its output shaft parallel to the valve stem at
the gear center distance. The servo drops into a pocket, hangs from its tabs,
and screws down with its own hardware. The base has slotted M4 holes (slots
run in the mesh direction, about plus or minus 4 mm) so gear backlash gets set
by sliding the bracket, then locked down to the tile grid.

Print: 0.2 mm layers, 4 walls, 30% infill, no supports.

### sightglass_bracket.scad

Two pieces. A backing plate with snap clips that hold the vinyl sight glass
tube vertical, with an embossed scale (ticks every 10 mm, longer prouder ticks
every 50 mm with numbers) so the chamber level is readable at a glance. And a
shoe that screws to the tile grid and clamps the plate bottom with two M3
cross screws. The scale is relative, zero it against the actual water line.

Print: 0.2 mm layers, 3 walls, 25% infill. Plate prints on its back, clips
facing up. If the tube snap fit is too tight or loose, tune `clip_clr` and
`mouth_w`.

### electronics_tray.scad

Tray for the electronics: pocket with corner fences for the half size
breadboard (both Pico 2 boards ride the breadboard), two spare standoff fields
sized for a Pico 2 bolted down directly (M2 self taps), an open zone for the
4 port USB hub, zip tie slots through the floor for hub tie down and cable
strain relief, and cable notches in the top wall. Mounting ears sit outside
the walls so the tray screws stay reachable with everything installed.

Print: 0.2 mm layers, 3 walls, 20% infill, no supports.

## Measure before printing

Grab the calipers and check these against the actual hardware, then update the
parameters. They are all marked VERIFY in the source. Datasheet numbers and
nominal sizes got the models close, but close is not a fit.

servo_valve_coupling.scad

- `stem_d` valve stem diameter (drawn at 6.0)
- `stem_across_flat` flat face to the opposite round side of the stem (drawn at 4.5)
- `horn_disc_d` stock horn disc diameter (drawn at 11.8)
- `horn_disc_t` horn disc thickness (drawn at 1.8)
- `horn_screw_r` radius from horn center to the arm holes the screws grab (drawn at 4.0)
- `horn_hub_h` height of the horn hub plus spline screw head (drawn at 5.0)

servo_bracket.scad

- `tab_spacing` servo mounting tab hole spacing (drawn at 28)
- `shaft_offset` output shaft offset from the case center along the long axis (drawn at 5.8)
- `hang_depth` tab underside to case bottom, `tower_h` must be bigger (drawn at 20)
- case width and length if the servo in hand disagrees with 12.1 x 23.2

sightglass_bracket.scad

- `tube_od` actual OD of the vinyl tube, nominal 3/8 inch is 9.5 but vinyl varies (drawn at 9.5)

electronics_tray.scad

- `bb_l` and `bb_w` the half size breadboard footprint (drawn at 83 x 56)

The Pico 2 hole pattern (47 x 11.4 mm, 2.1 mm holes) and the tile grid pitch
are from published drawings and my own convention, those are safe.

## Fit test tip

Before committing to a full gear or bracket print, print just the interface:
comment out the render calls and render only the bore or pocket region scaled
down to a 5 mm tall slice. Costs ten minutes instead of an hour and tells you
everything about the fit.
