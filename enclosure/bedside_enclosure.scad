// Bedside dashboard enclosure: a rounded box with the display on the front,
// eight buttons and the joystick on a removable lid, and the electronics in a
// bay at the back.
//
// Axes: x runs left to right, y runs front to back (the display is on the
// y = 0 face), z is up. The origin is the front-left corner of the tub's
// underside.
//
// Everything measured is a variable below. Values marked MEASURE are guesses
// that need calipers before printing.
//
//   part = "assembly"   tub and lid in place (turn on show_parts to see the
//                       boards and the clearance they need)
//   part = "tub"        the base, printed feet down
//   part = "lid"        the lid, printed top face down
//   part = "test"       the ESP32 and expander mounts, for a quick test print
//   part = "foot"       one threaded foot, as printed (you need four)
//   part = "feet"       four feet in a row
//   part = "print"      tub and lid laid out together (too wide for most beds)

part = "assembly"; // [assembly, tub, lid, test, foot, feet, print]
show_parts = false;
$fn = 64;

/* [Shell] */
H = 46;          // 40 mm button clearance from the surface, plus the floor
W = 160;
wall = 2.4;
floor_t = 2.4;
lid_t = 3;
corner_r = 8;    // vertical corners

/* [Feet] */
// The DHT11 body pokes (dht_h - floor_t) = 3.8 mm out of the floor, so the feet
// stand 6.5 mm proud. Each is a printed screw that threads into a boss on the
// inside of the floor; nothing hangs below the floor, so the tub prints flat.
// The thread is coarse with 45 degree flanks, so both halves print without
// support. thread_play is the radial gap: raise it if the feet bind, lower it
// if they wobble.
thread_d = 8;
thread_pitch = 2.5;
thread_depth = 0.9;
thread_play = 0.3;
foot_h = 6.5;          // below the floor
foot_d = 16;
foot_stud = 7;         // threaded length, into the 8.4 mm of floor and boss
foot_boss_d = 14;
foot_boss_h = 6;       // above the floor
foot_x = 22;           // in from each side wall
foot_y_front = 11;     // in the display strip, clear of the button zone
foot_y_back = 8;       // in from the back wall

/* [Lid screws] */
boss_r = 4.2;
m3_pilot = 2.6;   // self-tapping into the tub's corner bosses
m3_clear = 3.4;
m3_head = 6.4;    // countersink diameter

/* [Buttons] */
btn_cols = 4;
btn_rows = 2;
btn_housing_d = 19;                 // sits flush; the button threads in from below
btn_hole_d = btn_housing_d + 0.4;
btn_pitch = 26;
btn_clearance = 40;                 // from the top surface down: body, posts, wire bends
margin_x = 10;

/* [Joystick, KY-023] */
stick_board = [27, 33.6];           // x, y
stick_body = 25.5;
stick_gap = 10;                     // board top up to the stick's underside
stick_total = 30;                   // board top to the top of the stick
stick_board_t = 1.6;
stick_hole_d = 14;                  // MEASURE: shaft plus its swing at the lid
stick_offset = [0, 0];              // MEASURE: shaft centre from the board centre
stick_gap_x = 15;                   // between the button grid and the board

/* [Display, SSD1306 0.91in] */
disp_pcb = [38.1, 12.15];
disp_win = [26.54, 12.15];          // the visible area
disp_win_dx = 0;                    // MEASURE: the glass is often off-centre on the PCB
disp_thick = 3.1;                   // PCB plus glass
dupont_len = 14;                    // a female plug on the header
display_strip = 20;                 // depth in front of the button grid

/* [Electronics bay] */
bay_depth = 44;
bay_gap = 6;

esp = [51.74, 29];                  // ELEGOO 38-pin, flipped: pins up
esp_t = 1.6;
esp_standoff = 4.5;                 // the module and USB connector hang below it
esp_plug_height = 20;               // female plugs on the pins
esp_hole_span = [46, 24];           // stud centres: along the board, across it
esp_stud_d = 2.0;                   // MEASURE the hole; the stud wants to be about 0.4 smaller
esp_post_d = 4;                     // what the board rests on, round each stud

exp = [23.38, 38.25];               // Waveshare MCP23017 board, long edge front to back
exp_t = 1.6;
exp_standoff = 3;
exp_clearance = 35;                 // wires pointing up
exp_hole = 2.5;
exp_hole_edge = 1.5;                // hole edge to board edge, both ways
exp_stud_d = 2.1;                   // a little under the hole, as for the ESP32

buz_d = 11.72;
buz_h = 8.5;
buz_port_d = 3.5;

dht = [15.84, 12.82];               // sensor body; it passes through the floor
dht_h = 6.2;

lux_board = [14, 19];               // BH1750 board, on the right wall
lux_win_d = 5;                      // the sensor itself is about 3 x 3

usb_w = 14;
usb_h = 10;

/* [Derived] */
inner_top = H - lid_t;              // tub rim and the lid's underside
grid_w = (btn_cols - 1) * btn_pitch + btn_housing_d;
grid_d = (btn_rows - 1) * btn_pitch + btn_housing_d;
D = wall + display_strip + grid_d + bay_gap + bay_depth + wall;

function col_x(c) = margin_x + btn_housing_d / 2 + c * btn_pitch;
function row_y(r) = wall + display_strip + btn_housing_d / 2 + r * btn_pitch;

bay_y0 = wall + display_strip + grid_d + bay_gap;
bay_y1 = D - wall;

stick_x = margin_x + grid_w + stick_gap_x + stick_board[0] / 2;
stick_y = (row_y(0) + row_y(btn_rows - 1)) / 2;

disp_cx = W / 2;
disp_cz = (floor_t + inner_top) / 2;
pad_t = disp_thick + 0.1;           // the display's frame, in from the front wall

// Bay, left to right: ESP32, expander, buzzer, DHT11, BH1750 on the wall.
esp_x0 = wall + 3;
esp_y0 = bay_y0 + 1.5;
esp_z = floor_t + esp_standoff;
exp_x0 = esp_x0 + esp[0] + 7;
exp_y1 = bay_y1 - 2;                // room for the cage behind the board
exp_y0 = exp_y1 - exp[1];
buz_x = exp_x0 + exp[0] + 14;
buz_cz = floor_t + 1.2 + buz_d / 2;
dht_x = buz_x + 19;
dht_y = bay_y0 + 18;
lux_y = bay_y0 + 16;
lux_z = 24;

assert(foot_h > dht_h - floor_t + 1, "feet are too short for the DHT11 body");
assert(foot_stud <= floor_t + foot_boss_h, "foot stud is longer than its hole");
assert(H - floor_t >= btn_clearance, "buttons need more height");
assert(inner_top - (floor_t + exp_standoff + exp_t) >= exp_clearance, "expander wires need more height");
assert(inner_top - (esp_z + esp_t) >= esp_plug_height, "ESP32 plugs need more height");
assert(pad_t + dupont_len + 1 <= display_strip, "display plug runs into the button zone");
assert(esp_x0 + esp[0] < exp_x0, "ESP32 overlaps the expander");
assert(exp_x0 + exp[0] < buz_x - buz_d / 2 - 1.6, "expander overlaps the buzzer");
assert(buz_x + buz_d / 2 + 1.6 < dht_x - dht[0] / 2 - 2, "buzzer overlaps the DHT11");
assert(dht_x + dht[0] / 2 + 2 < W - wall - 14, "DHT11 overlaps the BH1750");
assert(stick_x + stick_board[0] / 2 <= W - wall, "joystick board is past the right wall");

// ---------------------------------------------------------------- helpers

module rrect2d(w, d, r) {
    translate([r, r]) offset(r = r) square([w - 2 * r, d - 2 * r]);
}

module inner2d() {
    translate([wall, wall]) rrect2d(W - 2 * wall, D - 2 * wall, corner_r - wall);
}

// A box between two opposite corners, in any order.
module span(a, b) {
    translate([min(a[0], b[0]), min(a[1], b[1]), min(a[2], b[2])])
        cube([abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2])]);
}

// An L of wall around a board's corner, outside it. (cx, cy) is the corner,
// (sx, sy) points away from the board.
module corner_l(cx, cy, sx, sy, z0, z1, leg = 6, t = 1.2) {
    span([cx + sx * t, cy, z0], [cx - sx * leg, cy + sy * t, z1]);
    span([cx, cy, z0], [cx + sx * t, cy - sy * leg, z1]);
}

// ---------------------------------------------------------------- the tub

module tub() {
    difference() {
        union() {
            difference() {
                linear_extrude(inner_top) rrect2d(W, D, corner_r);
                translate([0, 0, floor_t]) linear_extrude(H) inner2d();
            }
            screw_bosses();
            display_frame();
            esp_supports();
            expander_cage();
            buzzer_cradle();
            for (p = feet) translate([p[0], p[1], floor_t - 0.01]) cylinder(d = foot_boss_d, h = foot_boss_h + 0.01);
        }
        tub_cutouts();
    }
}

feet = [[foot_x, foot_y_front], [W - foot_x, foot_y_front],
        [foot_x, D - foot_y_back], [W - foot_x, D - foot_y_back]];

boss_c = [[corner_r, corner_r], [W - corner_r, corner_r],
          [corner_r, D - corner_r], [W - corner_r, D - corner_r]];
boss_corner = [[0, 0], [W, 0], [0, D], [W, D]];

module screw_bosses() {
    for (i = [0:3])
        translate([0, 0, floor_t - 0.01]) linear_extrude(inner_top - floor_t + 0.01)
            intersection() {
                inner2d();
                hull() {
                    translate(boss_c[i]) circle(r = boss_r);
                    translate(boss_corner[i]) square(0.01, center = true);
                }
            }
}

// A frame on the inside of the front wall that holds the display PCB. Its
// underside slopes at 45 degrees so it prints without support.
module display_frame() {
    fw = disp_pcb[0] + 0.4 + 3.2;
    fh = disp_pcb[1] + 0.4 + 3.2;
    z0 = disp_cz - fh / 2;
    z1 = disp_cz + fh / 2;
    x0 = disp_cx - fw / 2;
    x1 = disp_cx + fw / 2;
    hull() {
        span([x0, wall - 0.01, z0 - pad_t], [x1, wall, z1]);
        span([x0, wall - 0.01, z0], [x1, wall + pad_t, z1]);
    }
}

// A post under a board's mounting hole with a stud through it.
module stud(x, y, post_d, stud_d, post_h, stud_h) {
    translate([x, y, floor_t - 0.01]) {
        cylinder(d = post_d, h = post_h + 0.01);
        cylinder(d = stud_d, h = stud_h);
    }
}

esp_holes = [for (sx = [-1, 1], sy = [-1, 1])
    [esp_x0 + esp[0] / 2 + sx * esp_hole_span[0] / 2, esp_y0 + esp[1] / 2 + sy * esp_hole_span[1] / 2]];

module esp_supports() {
    // An L around each corner to locate the board, and a stud through each
    // mounting hole. Hot glue holds it.
    c = 0.3;
    for (sx = [-1, 1], sy = [-1, 1]) {
        bx = esp_x0 + (sx > 0 ? esp[0] : 0);
        by = esp_y0 + (sy > 0 ? esp[1] : 0);
        corner_l(bx + sx * c, by + sy * c, sx, sy, floor_t - 0.01, esp_z + esp_t + 2);
    }
    for (p = esp_holes) stud(p[0], p[1], esp_post_d, esp_stud_d, esp_standoff, esp_standoff + esp_t + 1);
}

exp_holes = [[exp_x0 + exp_hole_edge + exp_hole / 2, exp_y1 - exp_hole_edge - exp_hole / 2],
             [exp_x0 + exp[0] - exp_hole_edge - exp_hole / 2, exp_y1 - exp_hole_edge - exp_hole / 2]];

module expander_cage() {
    // Same idea as the ESP32: an L at each corner, studs through the two holes
    // at the back end, and a pad under each front corner.
    c = 0.3;
    top = floor_t + exp_standoff;
    for (sx = [-1, 1], sy = [-1, 1]) {
        bx = exp_x0 + (sx > 0 ? exp[0] : 0);
        by = exp_y0 + (sy > 0 ? exp[1] : 0);
        corner_l(bx + sx * c, by + sy * c, sx, sy, floor_t - 0.01, top + exp_t + 2);
        if (sy < 0) span([bx, by, floor_t - 0.01], [bx - sx * 4, by + 4, top]);
    }
    for (p = exp_holes) stud(p[0], p[1], 5, exp_stud_d, exp_standoff, exp_standoff + exp_t + 1);
}

module buzzer_cradle() {
    // The buzzer lies on its side with its sound face against the back wall.
    span([buz_x - buz_d / 2 - 1.6, bay_y1 - buz_h, floor_t - 0.01],
         [buz_x + buz_d / 2 + 1.6, bay_y1 + 0.01, buz_cz]);
}

module tub_cutouts() {
    // Display window through the front wall, and the frame's pocket behind it.
    translate([disp_cx + disp_win_dx - disp_win[0] / 2, -1, disp_cz - disp_win[1] / 2])
        cube([disp_win[0], wall + 1.02, disp_win[1]]);
    span([disp_cx - disp_pcb[0] / 2 - 0.2, wall, disp_cz - disp_pcb[1] / 2 - 0.2],
         [disp_cx + disp_pcb[0] / 2 + 0.2, wall + pad_t + 0.5, disp_cz + disp_pcb[1] / 2 + 0.2]);

    // USB cable through the left wall, level with the ESP32's connector.
    translate([-1, esp_y0 + esp[1] / 2 - usb_w / 2, floor_t])
        cube([wall + 2, usb_w, usb_h]);

    // Light sensor window in the right wall.
    translate([W - wall - 1, lux_y, lux_z]) rotate([0, 90, 0]) cylinder(d = lux_win_d, h = wall + 2);

    // DHT11 body through the floor.
    translate([dht_x - dht[0] / 2 - 0.2, dht_y - dht[1] / 2 - 0.2, -1])
        cube([dht[0] + 0.4, dht[1] + 0.4, floor_t + 2]);

    // Buzzer trough and its sound port.
    translate([buz_x, bay_y1 - buz_h - 1, buz_cz]) rotate([-90, 0, 0])
        cylinder(d = buz_d + 0.3, h = buz_h + 1.01);
    translate([buz_x, bay_y1 - 1, buz_cz]) rotate([-90, 0, 0])
        cylinder(d = buz_port_d, h = wall + 2);

    for (p = feet) translate([p[0], p[1], 0]) foot_thread_hole();

    // Pilot holes for the lid screws.
    for (c = boss_c) translate([c[0], c[1], inner_top - 14]) cylinder(d = m3_pilot, h = 15);
}

// ---------------------------------------------------------------- the feet

// Radius of the thread at height z: root flat, 45 degree flank up, crest
// flat, flank down, repeating every pitch.
function thread_r(z, rmin) =
    let(flat = (thread_pitch - 2 * thread_depth) / 2,
        zz = z % thread_pitch,
        a = flat / 2,
        b = a + thread_depth,
        c = b + flat,
        d = c + thread_depth)
    zz < a ? rmin :
    zz < b ? rmin + (zz - a) :
    zz < c ? rmin + thread_depth :
    zz < d ? rmin + thread_depth - (zz - c) :
    rmin;

// A threaded rod along +z, right-handed. Each slice of the polygon is the
// thread's cross-section, so twisting it one turn per pitch makes the helix.
module thread_rod(h, play = 0) {
    rmin = thread_d / 2 - thread_depth;
    n = 180;
    linear_extrude(height = h, twist = -360 * h / thread_pitch,
                   slices = max(4, ceil(h / thread_pitch * 24)), convexity = 10)
        polygon([for (i = [0:n - 1])
            let(a = 360 * i / n, r = thread_r(thread_pitch * a / 360, rmin) + play)
            [r * cos(a), r * sin(a)]]);
}

// The nut: cut through the floor and its boss, with a lead-in chamfer.
module foot_thread_hole() {
    translate([0, 0, -1]) thread_rod(floor_t + foot_boss_h + 2, thread_play);
    translate([0, 0, -0.01])
        cylinder(h = 1, r1 = thread_d / 2 + thread_play + 1, r2 = thread_d / 2 + thread_play);
}

// One foot, as printed: the disk on the bed, the stud pointing up.
module foot() {
    difference() {
        union() {
            cylinder(d1 = foot_d - 1.6, d2 = foot_d, h = 0.8);
            translate([0, 0, 0.79]) cylinder(d = foot_d, h = foot_h - 0.79);
            translate([0, 0, foot_h - 0.01]) intersection() {
                thread_rod(foot_stud + 0.01);
                // taper the tip so it starts in the hole
                rotate_extrude() polygon([[0, 0], [thread_d / 2 + 0.5, 0],
                                          [thread_d / 2 + 0.5, foot_stud - thread_depth - 0.5],
                                          [thread_d / 2 - thread_depth, foot_stud + 0.01],
                                          [0, foot_stud + 0.01]]);
            }
        }
        // finger flutes round the rim
        for (i = [0:11]) rotate(i * 30) translate([foot_d / 2, 0, -1]) cylinder(d = 2.2, h = foot_h + 2, $fn = 24);
    }
}

// ---------------------------------------------------------------- the lid

module lid() {
    difference() {
        union() {
            translate([0, 0, inner_top]) linear_extrude(lid_t) rrect2d(W, D, corner_r);
            stick_cradle();
        }
        for (r = [0:btn_rows - 1], c = [0:btn_cols - 1])
            translate([col_x(c), row_y(r), inner_top - 1]) cylinder(d = btn_hole_d, h = lid_t + 2);
        translate([stick_x + stick_offset[0], stick_y + stick_offset[1], inner_top - 1])
            cylinder(d = stick_hole_d, h = lid_t + 2);
        for (c = boss_c) {
            translate([c[0], c[1], inner_top - 1]) cylinder(d = m3_clear, h = lid_t + 2);
            translate([c[0], c[1], H - 1.5]) cylinder(h = 1.51, d1 = m3_clear, d2 = m3_head);
        }
    }
}

// Four L's hanging from the underside that locate the joystick board. It
// faces the lid with its stick body 10 mm tall, so the cradle reaches down to
// the board's lower face. Hot glue holds it.
module stick_cradle() {
    z0 = inner_top - (stick_gap + stick_board_t + 1);
    c = 0.3;
    for (sx = [-1, 1], sy = [-1, 1]) {
        bx = stick_x + sx * stick_board[0] / 2;
        by = stick_y + sy * stick_board[1] / 2;
        corner_l(bx + sx * c, by + sy * c, sx, sy, z0, inner_top + 0.01, leg = 8, t = 1.6);
    }
}

// ------------------------------------------------- parts, for checking fit

module ghosts() {
    color("seagreen", 0.45) {
        // ESP32: board, pin rows with plugs, and what hangs underneath.
        difference() {
            translate([esp_x0, esp_y0, esp_z]) cube([esp[0], esp[1], esp_t]);
            for (p = esp_holes) translate([p[0], p[1], esp_z - 1]) cylinder(d = esp_stud_d + 0.4, h = esp_t + 2);
        }
        for (y = [1.68, esp[1] - 1.68 - 2.54])
            translate([esp_x0 + 1.74, esp_y0 + y, esp_z + esp_t + 1.2]) cube([48.26, 2.54, esp_plug_height - 1.2]);
        translate([esp_x0 + esp[0] - 18, esp_y0 + 1.75, esp_z - 3.1]) cube([18, 25.5, 3.1]);
        translate([esp_x0 - 1, esp_y0 + esp[1] / 2 - 3.75, esp_z - 2.8]) cube([7, 7.5, 2.8]);
    }
    color("steelblue", 0.45) {
        // Expander, and the wire clearance above it.
        difference() {
            translate([exp_x0, exp_y0, floor_t + exp_standoff]) cube([exp[0], exp[1], exp_t]);
            for (p = exp_holes) translate([p[0], p[1], floor_t + exp_standoff - 1]) cylinder(d = exp_hole, h = exp_t + 2);
        }
        translate([exp_x0, exp_y0, floor_t + exp_standoff + exp_t + 1.2]) cube([exp[0], exp[1], exp_clearance - 1.2]);
    }
    color("teal", 0.25)
        for (r = [0:btn_rows - 1], c = [0:btn_cols - 1])
            translate([col_x(c), row_y(r), H - btn_clearance]) cylinder(d = btn_housing_d, h = btn_clearance - lid_t);
    color("purple", 0.45) {
        bz = inner_top - stick_gap - stick_board_t;
        translate([stick_x - stick_board[0] / 2, stick_y - stick_board[1] / 2, bz]) cube([stick_board[0], stick_board[1], stick_board_t]);
        translate([stick_x - stick_body / 2, stick_y - stick_body / 2, bz + stick_board_t]) cube([stick_body, stick_body, stick_gap]);
        translate([stick_x + stick_offset[0], stick_y + stick_offset[1], bz + stick_board_t + stick_gap])
            cylinder(d = 8, h = stick_total - stick_gap);
    }
    color("orange", 0.5) {
        translate([buz_x, bay_y1 - buz_h, buz_cz]) rotate([-90, 0, 0]) cylinder(d = buz_d, h = buz_h);
        translate([dht_x - dht[0] / 2, dht_y - dht[1] / 2, floor_t - dht_h]) cube([dht[0], dht[1], dht_h]);
        translate([W - wall - 1.6, lux_y - lux_board[0] / 2, lux_z - lux_board[1] / 2]) cube([1.6, lux_board[0], lux_board[1]]);
        translate([disp_cx - disp_pcb[0] / 2, wall, disp_cz - disp_pcb[1] / 2]) cube([disp_pcb[0], disp_thick, disp_pcb[1]]);
    }
}

// A slice of the tub to test the board mounts: the floor, the left and back
// walls up to 14 mm (the USB slot is in the left wall), the ESP32 and expander
// cages, one lid-screw boss and one threaded foot boss.
module test_piece() {
    intersection() {
        tub();
        translate([-1, bay_y0 - 3, -1]) cube([93, D, 15]);
    }
}

// ---------------------------------------------------------------- render

if (part == "tub") {
    tub();
} else if (part == "lid") {
    translate([0, D, H]) rotate([180, 0, 0]) lid();
} else if (part == "test") {
    test_piece();
} else if (part == "foot") {
    foot();
} else if (part == "feet") {
    for (i = [0:3]) translate([i * (foot_d + 4), 0, 0]) foot();
} else if (part == "print") {
    tub();
    translate([W + 10, D, H]) rotate([180, 0, 0]) lid();
} else if (part == "assembly") {
    tub();
    lid();
    if (show_parts) ghosts();
}
