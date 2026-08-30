// baseplate_tile.scad
//
// 120 x 120 interlocking floor tile for the rig. dovetail tabs on two edges,
// matching sockets on the other two, so tiles chain together in both
// directions and the whole deck grows as the rig does. screw bosses on a
// 20 mm grid take M4 self tappers, every bracket in this folder puts its
// mounting holes on multiples of 20 mm so anything lands anywhere.
//
// two shallow channels run front to back, one for signal wiring and one for
// 12V power, kept 60 mm apart so the pump PWM doesn't sit right next to the
// 4-20 mA loop. wires drop in below the boss tops and stay put.
//
// print flat as rendered. PETG, 0.2 mm layers, 3 walls, 20% infill is fine,
// this thing is just a floor.

// ---------------- parameters ----------------
tile      = 120;   // tile edge length, keep it a multiple of grid_pitch
grid_pitch = 20;   // boss grid spacing, shared with every bracket
base_t    = 6;     // tile thickness, thick enough for a 3 mm deep channel
boss_d    = 9;     // screw boss diameter
boss_h    = 5;     // boss height above the tile
pilot_d   = 3.4;   // M4 self taps nicely into 3.4 mm in PETG

// dovetail interlock
tab_neck  = 10;    // tab width at the tile edge
tab_head  = 14;    // tab width at the tip, wider than the neck so it locks
tab_depth = 8;     // how far the tab sticks out
joint_clr = 0.15;  // clearance added around the sockets, tune to your printer.
                   // 0.15 was a snug press fit on mine

// wiring channels
chan_w    = 8;     // channel width
chan_deep = 3;     // channel depth into the tile top
power_x   = 30;    // 12V power lane, clears the boss columns at 20 and 40
signal_x  = 90;    // signal lane, mirrored on the other side

// ---------------- modules ----------------

// dovetail tab, 2D, pointing in +X with its neck on the Y axis
module tab_2d() {
    polygon([
        [0,         -tab_neck / 2],
        [tab_depth, -tab_head / 2],
        [tab_depth,  tab_head / 2],
        [0,          tab_neck / 2]
    ]);
}

// tile outline: square plus male tabs on the +X and +Y edges, female sockets
// cut into the -X and -Y edges. a neighbor's male tab reaches into this tile,
// so the socket is the same shape grown by the clearance.
module tile_2d() {
    difference() {
        union() {
            square(tile);
            for (p = [tile / 3, 2 * tile / 3]) {
                translate([tile, p]) tab_2d();               // +X tabs
                translate([p, tile]) rotate(90) tab_2d();    // +Y tabs
            }
        }
        for (p = [tile / 3, 2 * tile / 3]) {
            translate([0, p]) offset(delta = joint_clr) tab_2d();            // -X sockets
            translate([p, 0]) rotate(90) offset(delta = joint_clr) tab_2d(); // -Y sockets
        }
    }
}

// keep bosses out of the wiring lanes
function boss_ok(x) =
    abs(x - power_x)  > (chan_w / 2 + boss_d / 2) &&
    abs(x - signal_x) > (chan_w / 2 + boss_d / 2);

module baseplate_tile() {
    difference() {
        union() {
            linear_extrude(height = base_t) tile_2d();
            // bosses, one grid pitch in from the edges so they clear the
            // interlock sockets
            for (x = [grid_pitch : grid_pitch : tile - grid_pitch])
                for (y = [grid_pitch : grid_pitch : tile - grid_pitch])
                    if (boss_ok(x))
                        translate([x, y, 0])
                            cylinder(d = boss_d, h = base_t + boss_h, $fn = 32);
        }
        // wiring channels. they start past the -Y socket zone and run out the
        // +Y edge, wires enter from that end or hop the short dam
        for (cx = [power_x, signal_x])
            translate([cx - chan_w / 2, tab_depth + 1, base_t - chan_deep])
                cube([chan_w, tile - tab_depth, chan_deep + boss_h + 1]);
        // pilot holes, blind, 3 mm of floor left under each one
        for (x = [grid_pitch : grid_pitch : tile - grid_pitch])
            for (y = [grid_pitch : grid_pitch : tile - grid_pitch])
                if (boss_ok(x))
                    translate([x, y, 3])
                        cylinder(d = pilot_d, h = base_t + boss_h, $fn = 24);
    }
}

// ---------------- render, print orientation ----------------
baseplate_tile();
