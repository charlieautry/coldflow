// servo_bracket.scad
//
// bracket that holds the SCS0009 offset parallel to the needle valve stem so
// the gear pair in servo_valve_coupling.scad meshes. the servo drops into a
// pocket in a tower, hangs from its mounting tabs, and gets two self tap
// screws down into the tower top. the base has slotted holes so I can slide
// the whole bracket toward or away from the valve to set gear mesh, then
// lock it down to the baseplate tile grid.
//
// print as rendered, base flat on the bed, no supports needed.
// PETG, 0.2 mm layers, 4 walls, 30% infill.

// ---------------- gear geometry, must match servo_valve_coupling.scad ------
// center distance = (16T + 32T) * module 1.5 / 2 = 36 mm. the slots give me
// about +/- 4 mm of adjustment around wherever I put the bracket, so I place
// the servo axis 36 mm from the valve stem and fine tune by sliding.
center_dist = 36;
echo(str("set servo axis ", center_dist, " mm from the valve stem axis"));

// ---------------- servo dimensions ----------------
sv_w         = 12.1;  // case width (datasheet says 12.1)
sv_l         = 23.2;  // case length (datasheet says 23.2)
sv_h         = 25.25; // case height, tabs near the top
pocket_clr   = 0.5;   // total clearance added to the pocket
tab_spacing  = 28;    // VERIFY WITH CALIPERS: mounting tab hole spacing
tab_screw_d  = 1.7;   // pilot for the servo's own self tap screws
shaft_offset = 5.8;   // VERIFY WITH CALIPERS: output shaft offset from case
                      // center along the long axis (shaft sits toward one end)
hang_depth   = 20;    // VERIFY WITH CALIPERS: tab underside to case bottom

// ---------------- bracket geometry ----------------
base_t     = 4;              // base plate thickness
tower_h    = 22;             // servo tab face height above the base top.
                             // this sets the pinion height, check it against
                             // where the valve gear ends up on the stem before
                             // printing. must be > hang_depth so the case
                             // floats instead of bottoming out.
side_wall  = 3;              // walls beside the servo body
end_wall   = 5;              // walls at the ends, thicker so the tab screws
                             // have meat to bite into
slot_w     = 4.4;            // M4 clearance slot width
slot_len   = 12;             // slot length, about +/- 4 mm of mesh adjust
slot_x     = 30;             // slot center distance from the servo axis,
                             // pointing away from the valve
slot_y     = 20;             // slots at +/- 20 mm, so 40 mm apart, lands on
                             // the 20 mm tile grid
cable_w    = 8;              // cable exit window width
cable_h    = 10;             // cable exit window height

// derived
pocket_w = sv_w + pocket_clr;
pocket_l = sv_l + pocket_clr;

// ---------------- modules ----------------

// slotted M4 hole, long axis along X so the bracket slides in the mesh direction
module adjust_slot() {
    hull() {
        translate([slot_x - (slot_len - slot_w) / 2, 0, -0.5])
            cylinder(d = slot_w, h = base_t + 1, $fn = 24);
        translate([slot_x + (slot_len - slot_w) / 2, 0, -0.5])
            cylinder(d = slot_w, h = base_t + 1, $fn = 24);
    }
}

// the whole bracket. servo output shaft axis is the Z axis through the origin,
// so "distance to the valve stem" is measured straight from x=0, y=0.
// the case center sits at y = -shaft_offset because the shaft is not centered
// in the case.
module servo_bracket() {
    difference() {
        union() {
            // base plate, reaches out under the slots
            translate([-20, -30, 0])
                cube([64, 60, base_t]);
            // tower
            translate([-(pocket_w / 2 + side_wall),
                       -shaft_offset - pocket_l / 2 - end_wall, 0])
                cube([pocket_w + 2 * side_wall,
                      pocket_l + 2 * end_wall,
                      base_t + tower_h]);
        }
        // servo pocket, open at the top, stops at the base so nothing pokes
        // through. servo hangs from its tabs and the case bottom floats.
        translate([-pocket_w / 2, -shaft_offset - pocket_l / 2, base_t])
            cube([pocket_w, pocket_l, tower_h + 1]);
        // pilot holes for the two tab screws, straight down into the end walls
        for (yy = [-shaft_offset - tab_spacing / 2,
                   -shaft_offset + tab_spacing / 2])
            translate([0, yy, base_t + tower_h - 6])
                cylinder(d = tab_screw_d, h = 7, $fn = 20);
        // cable exit window through the far end wall, down at the bottom
        translate([-cable_w / 2, -shaft_offset - pocket_l / 2 - end_wall - 1, base_t])
            cube([cable_w, end_wall + 2, cable_h]);
        // slotted mounting holes, M4 into the tile bosses
        for (yy = [-slot_y, slot_y])
            translate([0, yy, 0]) adjust_slot();
    }
}

// ---------------- render, print orientation ----------------
servo_bracket();
