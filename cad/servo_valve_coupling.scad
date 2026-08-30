// servo_valve_coupling.scad
//
// gear pair that couples the SCS0009 serial servo to the 1/4" brass needle
// valve stem. 2:1 reduction (16T pinion on the servo, 32T gear on the valve)
// so I get double the torque and double the position resolution at the valve.
//
// the servo spline is 20T at 3.95 mm OD which is way too small to print, so
// the pinion screws onto the stock plastic horn disc with two self tap screws
// instead. the valve gear grabs the flatted valve stem (the stock handle was
// held on by a screw, so the stem has a flat) plus an M3 set screw for backup.
//
// print both parts flat as rendered. PETG, 0.2 mm layers, 4 walls, 40% infill.

// ---------------- gear parameters ----------------
// these two files have to agree: servo_bracket.scad uses the same center
// distance. if I change tooth counts or module here, change it there too.
gear_module   = 1.5;   // coarse module, plenty strong at 2.3 kg-cm
pinion_teeth  = 16;    // on the servo
valve_teeth   = 32;    // on the valve stem, 2:1 down from the servo
pressure_ang  = 20;    // standard 20 deg flanks
gear_face     = 8;     // tooth face width
gear_backlash = 0.25;  // total backlash allowance, tune after a test print

// ---------------- servo horn interface ----------------
horn_disc_d    = 11.8; // VERIFY WITH CALIPERS: stock horn disc diameter
horn_disc_t    = 1.8;  // VERIFY WITH CALIPERS: horn disc thickness
horn_hub_d     = 9.0;  // clearance pocket over the horn center hub + screw head
horn_hub_h     = 5.0;  // VERIFY WITH CALIPERS: hub + screw head standoff height
horn_screw_r   = 4.0;  // VERIFY WITH CALIPERS: radius to the horn arm holes
horn_screw_d   = 2.0;  // through holes for the two self tap screws
disc_clearance = 0.3;  // extra diameter so the disc drops into its recess

// ---------------- valve stem interface ----------------
stem_d           = 6.0;  // VERIFY WITH CALIPERS: valve stem diameter
stem_across_flat = 4.5;  // VERIFY WITH CALIPERS: flat face to opposite round side
stem_clearance   = 0.2;  // added to the bore all around, tune to fit
hub_d            = 16;   // valve gear hub above the teeth
hub_h            = 7;    // hub height, gives the set screw something to bite
set_screw_d      = 2.5;  // M3 self taps into a 2.5 mm hole in PETG

// derived, don't edit
pinion_pd  = gear_module * pinion_teeth;
valve_pd   = gear_module * valve_teeth;
center_d   = (pinion_pd + valve_pd) / 2;
echo(str("gear center distance = ", center_d, " mm (servo_bracket.scad must match)"));

// ---------------- modules ----------------

// simple approximate spur gear. teeth are trapezoids hung on the root circle,
// with flank angles taken from the pressure angle. not a true involute but at
// module 1.5 in PETG it meshes fine, I checked a printed pair before trusting it.
module spur_gear(teeth, gmod, face, pa, backlash) {
    pr     = gmod * teeth / 2;          // pitch radius
    r_tip  = pr + gmod;                 // addendum = module
    r_root = pr - 1.25 * gmod;          // dedendum = 1.25 * module
    half_a = 90 / teeth;                // half tooth angle at the pitch circle
    bl_a   = (backlash / 2) / pr * 180 / PI;
    tip_a  = half_a - (gmod * tan(pa) / pr) * 180 / PI - bl_a;
    root_a = half_a + (1.25 * gmod * tan(pa) / pr) * 180 / PI - bl_a;
    linear_extrude(height = face)
        union() {
            circle(r = r_root, $fn = teeth * 3);
            for (i = [0 : teeth - 1])
                rotate([0, 0, i * 360 / teeth])
                    polygon([
                        [r_root * cos(-root_a), r_root * sin(-root_a)],
                        [r_tip  * cos(-tip_a),  r_tip  * sin(-tip_a)],
                        [r_tip  * cos(tip_a),   r_tip  * sin(tip_a)],
                        [r_root * cos(root_a),  r_root * sin(root_a)]
                    ]);
        }
}

// 2D bore profile for the flatted valve stem. circle with one side cut off at
// the across flat dimension. clearance grows the round part and backs the flat
// off by the same amount.
module stem_bore_2d() {
    r = stem_d / 2;
    difference() {
        circle(d = stem_d + 2 * stem_clearance, $fn = 48);
        // flat sits on the +X side
        translate([stem_across_flat - r + stem_clearance, -r - 2])
            square([2 * r, 2 * r + 4]);
    }
}

// pinion: gear body with a recess underneath that swallows the horn disc,
// a taller pocket over the horn hub and screw head, and two through holes
// for the self tap screws that grab the horn arms.
module servo_pinion() {
    difference() {
        spur_gear(pinion_teeth, gear_module, gear_face, pressure_ang, gear_backlash);
        // horn disc recess, bottom face
        translate([0, 0, -0.1])
            cylinder(d = horn_disc_d + disc_clearance, h = horn_disc_t + 0.1, $fn = 64);
        // pocket over the center hub and the spline screw head
        translate([0, 0, -0.1])
            cylinder(d = horn_hub_d, h = horn_hub_h + 0.1, $fn = 48);
        // two self tap screw holes, driven from the top into the horn arms
        for (a = [0, 180])
            rotate([0, 0, a])
                translate([horn_screw_r, 0, -0.1])
                    cylinder(d = horn_screw_d, h = gear_face + 0.2, $fn = 24);
    }
}

// valve gear: gear body plus a hub, flatted bore all the way through, and a
// radial M3 set screw hole in the hub aimed at the stem flat.
module valve_gear() {
    difference() {
        union() {
            spur_gear(valve_teeth, gear_module, gear_face, pressure_ang, gear_backlash);
            translate([0, 0, gear_face])
                cylinder(d = hub_d, h = hub_h, $fn = 64);
        }
        // stem bore, through everything
        translate([0, 0, -0.5])
            linear_extrude(height = gear_face + hub_h + 1)
                stem_bore_2d();
        // set screw hole through the hub wall, lands on the flat (+X side)
        translate([0, 0, gear_face + hub_h / 2])
            rotate([0, 90, 0])
                cylinder(d = set_screw_d, h = hub_d, $fn = 24);
    }
}

// ---------------- render, print orientation ----------------
// both parts flat on the bed. pinion is flipped so the horn recess faces up
// and the pockets print as clean blind holes with no bridging. valve gear
// prints teeth down with the hub up.
translate([0, 0, gear_face]) rotate([180, 0, 0]) servo_pinion();
translate([valve_pd + pinion_pd, 0, 0]) valve_gear();
