// sightglass_bracket.scad
//
// holds the vinyl sight glass tube vertical against a backing plate with an
// embossed scale, so I can actually read the water level instead of eyeballing
// a floppy tube. two parts: the scale plate with snap clips for the tube, and
// a shoe that grips the plate bottom and screws down to the tile grid.
//
// tube snaps into the clips from the front. ticks every 10 mm, a longer
// taller tick every 50 mm with a small number next to it. the scale is
// relative (I zero it against the actual water line with a marker).
//
// print both parts as rendered, plate on its back, shoe on its flange.
// PETG, 0.2 mm layers, 3 walls, 25% infill.

// ---------------- tube and clips ----------------
tube_od   = 9.5;   // VERIFY WITH CALIPERS: vinyl tube OD, nominal 3/8"
clip_wall = 2.4;   // clip ring wall
clip_clr  = 0.2;   // radial clearance so the tube slides for adjustment
mouth_w   = 7.5;   // clip opening, smaller than the tube so it snaps in
clip_h    = 8;     // clip length along the tube
clip_ys   = [25, 85, 145, 180];  // clip positions up the plate

// ---------------- plate ----------------
plate_w  = 34;     // plate width
plate_h  = 200;    // plate height, covers the usable chamber level range
plate_t  = 4;      // plate thickness
tube_x   = 10;     // tube centerline from the left edge
tick_x   = 19;     // ticks start here, clear of the clip bodies
minor_len = 6;     // minor tick length
major_len = 12;    // major tick length
tick_emboss = 0.6; // minor ticks raised this much
major_emboss = 0.8; // majors stand a bit prouder so a finger finds them

// ---------------- shoe ----------------
shoe_slot_clr = 0.4;  // slot width over plate thickness
m4_clear      = 4.3;  // M4 clearance holes down into the tile bosses
cross_hole_d  = 3.2;  // M3 clearance, screws clamp the plate into the shoe

// ---------------- modules ----------------

// clip cross section: C shape with the mouth up (up = away from the plate
// once assembled). local +Y becomes global +Z after the rotate below.
module clip_2d() {
    outer_r = tube_od / 2 + clip_wall;
    difference() {
        circle(r = outer_r, $fn = 64);
        circle(r = tube_od / 2 + clip_clr, $fn = 64);
        translate([-mouth_w / 2, 0]) square([mouth_w, outer_r + 1]);
    }
}

module scale_plate() {
    tube_z = plate_t + tube_od / 2;  // tube rests just off the plate face
    union() {
        cube([plate_w, plate_h, plate_t]);
        // clips
        for (yc = clip_ys)
            translate([tube_x, yc + clip_h / 2, tube_z])
                rotate([90, 0, 0])
                    linear_extrude(height = clip_h) clip_2d();
        // graduated scale
        for (y = [10 : 10 : plate_h - 10]) {
            is_major = (y % 50 == 0);
            t_len = is_major ? major_len : minor_len;
            t_emb = is_major ? major_emboss : tick_emboss;
            t_w   = is_major ? 1.8 : 1.2;
            translate([tick_x, y - t_w / 2, 0])
                cube([t_len, t_w, plate_t + t_emb]);
            // number the majors so I don't have to count ticks
            if (is_major)
                translate([tick_x, y + 2, plate_t])
                    linear_extrude(height = tick_emboss)
                        text(str(y), size = 4, halign = "left", $fn = 16);
        }
    }
}

// plate with the shoe screw holes cut in
module scale_plate_drilled() {
    difference() {
        scale_plate();
        for (xx = [plate_w / 2 - 10, plate_w / 2 + 10])
            translate([xx, 6, -0.5])
                cylinder(d = cross_hole_d, h = plate_t + 1, $fn = 24);
    }
}

// shoe: flange screws to the tile grid, the plate bottom drops into the slot
// and two M3 screws go through sideways to clamp it. plate holes sit 6 mm
// above the plate bottom, slot floor is at z = 6, so the cross holes land
// at z = 12.
module shoe() {
    difference() {
        union() {
            translate([-20, -5, 0]) cube([40, 30, 4]);   // flange
            translate([-20, -5, 0]) cube([40, 10, 18]);  // slot body
        }
        // plate slot, open at the top
        translate([-(plate_w + 0.5) / 2, -(plate_t + shoe_slot_clr) / 2, 6])
            cube([plate_w + 0.5, plate_t + shoe_slot_clr, 13]);
        // M4 holes to the tile, 20 mm apart on the grid
        for (xx = [-10, 10])
            translate([xx, 15, -0.5])
                cylinder(d = m4_clear, h = 5, $fn = 24);
        // cross clamp holes, match the pair in the plate bottom
        for (xx = [-10, 10])
            translate([xx, -6, 12])
                rotate([-90, 0, 0])
                    cylinder(d = cross_hole_d, h = 12, $fn = 24);
    }
}

// ---------------- render, print orientation ----------------
scale_plate_drilled();
translate([60, 0, 0]) shoe();
