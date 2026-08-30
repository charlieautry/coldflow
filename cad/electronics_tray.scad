// electronics_tray.scad
//
// one tray for all the electronics: half size breadboard (the two Pico 2
// boards plug into it), a spare pair of standoff fields in case I want a Pico
// screwed down directly instead (the resolver sim board probably ends up
// there), and an open zone for the 4 port USB hub. zip tie slots everywhere
// because USB cables getting yanked is how boards die.
//
// mounting ears sit outside the walls so the M4 screws stay reachable after
// everything is stuck down inside. ear holes land on the 20 mm tile grid.
//
// print flat as rendered, no supports. PETG, 0.2 mm layers, 3 walls,
// 20% infill.

// ---------------- tray shell ----------------
tray_l   = 160;   // outside length (X)
tray_w   = 120;   // outside width (Y)
wall_t   = 2.4;   // wall thickness
wall_h   = 12;    // wall height above the floor
floor_t  = 3;     // floor thickness
ear_hole_x = 90;  // ear holes at +/- 90 and +/- 50, so 180 x 100 spacing,
ear_hole_y = 50;  // both multiples of the 20 mm grid
m4_clear = 4.3;

// ---------------- breadboard pocket ----------------
bb_l = 83;        // VERIFY WITH CALIPERS: half size breadboard length
bb_w = 56;        // VERIFY WITH CALIPERS: half size breadboard width
bb_clr = 0.4;     // corner fence clearance around the board
bb_cx = -33;      // breadboard center position in the tray
bb_cy = -27;
fence_arm = 12;   // corner fence arm length
fence_t   = 1.8;  // fence wall thickness
fence_h   = 5;    // fence height, board mostly held by its adhesive anyway

// ---------------- pico standoffs ----------------
// Pico 2 board is 51 x 21 with 2.1 mm holes on a 47 x 11.4 pattern
pico_hole_dx = 47;
pico_hole_dy = 11.4;
pico_centers = [[40, 45], [40, 18]];  // two boards, long axis along X
boss_d  = 5.5;
boss_h  = 4;      // lifts the board so solder tails clear the floor
pico_pilot = 1.7; // M2 self tap pilot

// ---------------- zip tie slots ----------------
slot_l = 4.5;     // slot length, roomy for a standard 2.5 mm wide zip tie
slot_w = 2.6;
// pairs straddling the hub zone (top left area), tie wraps the hub body
hub_slot_xs = [-60, -35, -10];
hub_slot_y1 = 18;
hub_slot_y2 = 42;
// strain relief slots along the inside of the walls for incoming cables
edge_slots_right  = [[72, -45], [72, -15], [72, 15], [72, 45]];
edge_slots_bottom = [[20, -53], [45, -53], [70, -53]];
edge_slots_top    = [[-70, 53], [70, 53]];

// cable notches in the top wall so USB leads exit without kinking
notch_xs = [-40, 40];
notch_w  = 12;
notch_d  = 7;

// ---------------- modules ----------------

// slot through the floor, long axis along X by default
module zip_slot(vert = false) {
    rotate([0, 0, vert ? 90 : 0])
        translate([-slot_l / 2, -slot_w / 2, -0.5])
            cube([slot_l, slot_w, floor_t + 1]);
}

module tray_shell() {
    difference() {
        union() {
            // floor and walls
            translate([-tray_l / 2, -tray_w / 2, 0])
                cube([tray_l, tray_w, floor_t + wall_h]);
            // four mounting ears, they overlap the wall a couple mm so they
            // fuse to it and reach out to the grid holes
            for (sx = [-1, 1]) for (sy = [-1, 1])
                translate([sx > 0 ? tray_l / 2 - 2 : -(ear_hole_x + 8),
                           sy * ear_hole_y - 7, 0])
                    cube([ear_hole_x + 8 - (tray_l / 2 - 2), 14, floor_t]);
        }
        // hollow out the inside
        translate([-tray_l / 2 + wall_t, -tray_w / 2 + wall_t, floor_t])
            cube([tray_l - 2 * wall_t, tray_w - 2 * wall_t, wall_h + 1]);
        // ear holes
        for (sx = [-1, 1]) for (sy = [-1, 1])
            translate([sx * ear_hole_x, sy * ear_hole_y, -0.5])
                cylinder(d = m4_clear, h = floor_t + 1, $fn = 24);
        // cable notches, top wall
        for (nx = notch_xs)
            translate([nx - notch_w / 2, tray_w / 2 - wall_t - 1,
                       floor_t + wall_h - notch_d])
                cube([notch_w, wall_t + 2, notch_d + 1]);
    }
}

// four L shaped corner fences that locate the breadboard. each arm sits just
// outside the board outline, hugging its corner.
module bb_fence() {
    hx = bb_l / 2 + bb_clr;
    hy = bb_w / 2 + bb_clr;
    for (sx = [-1, 1]) for (sy = [-1, 1]) {
        // arm running along X, outside the board's top or bottom edge
        translate([bb_cx + (sx > 0 ? hx - fence_arm : -hx),
                   bb_cy + (sy > 0 ? hy : -hy - fence_t),
                   floor_t])
            cube([fence_arm, fence_t, fence_h]);
        // arm running along Y, outside the board's left or right edge
        translate([bb_cx + (sx > 0 ? hx : -hx - fence_t),
                   bb_cy + (sy > 0 ? hy - fence_arm : -hy),
                   floor_t])
            cube([fence_t, fence_arm, fence_h]);
    }
}

module pico_bosses() {
    for (c = pico_centers)
        for (sx = [-1, 1]) for (sy = [-1, 1])
            translate([c[0] + sx * pico_hole_dx / 2,
                       c[1] + sy * pico_hole_dy / 2, 0])
                cylinder(d = boss_d, h = floor_t + boss_h, $fn = 24);
}

module pico_pilots() {
    for (c = pico_centers)
        for (sx = [-1, 1]) for (sy = [-1, 1])
            translate([c[0] + sx * pico_hole_dx / 2,
                       c[1] + sy * pico_hole_dy / 2, 1.5])
                cylinder(d = pico_pilot, h = floor_t + boss_h, $fn = 20);
}

module electronics_tray() {
    difference() {
        union() {
            tray_shell();
            bb_fence();
            pico_bosses();
        }
        pico_pilots();
        // hub tie down pairs
        for (hx = hub_slot_xs) {
            translate([hx, hub_slot_y1, 0]) zip_slot();
            translate([hx, hub_slot_y2, 0]) zip_slot();
        }
        // strain relief along the walls
        for (p = edge_slots_right)  translate([p[0], p[1], 0]) zip_slot(vert = true);
        for (p = edge_slots_bottom) translate([p[0], p[1], 0]) zip_slot();
        for (p = edge_slots_top)    translate([p[0], p[1], 0]) zip_slot();
    }
}

// ---------------- render, print orientation ----------------
electronics_tray();
