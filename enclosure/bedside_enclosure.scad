// Bedside dashboard enclosure: a rounded box with the display on the front,
// eight buttons and the joystick on a removable lid, and the electronics
// under it. The base is Gridfinity compatible: 4 x 3 cells of 42 mm, each
// with the standard foot, so the box sits in a baseplate or on a desk.
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
//   part = "tub"        the tub, printed feet down
//   part = "lid"        the lid, printed right side up (nothing hangs below it)
//   part = "test"       the ESP32 and expander mounts, for a quick test print
//   part = "fit"        the ESP32, expander and hub board cages on one floor, in place, for a test fit
//   part = "hubtest"    the hub board's back-left corner: anchor stud and a guard
//   part = "sticktest"  just the lid around the joystick cutout, for a test print of the board's fit
//   part = "risers"     four spacers for the joystick's screws
//   part = "blank"      one round label blank, face down (magnet pocket up)
//   part = "blanks"     eight blanks, one for each button
//   part = "print"      tub and lid laid out together (too wide for most beds)

part = "assembly"; // [assembly, tub, lid, sticktest, risers, test, fit, hubtest, blank, blanks, print]
show_parts = false;
$fn = 64;

/* [Gridfinity base] */
// The box stands on a 4 x 3 block of Gridfinity feet: 42 mm cells, each foot
// 0.5 mm smaller than its cell, with the standard profile (gridfinity.xyz):
// from the bottom a 0.8 mm 45 degree chamfer, 1.8 mm straight, then a 2.15 mm
// 45 degree chamfer, 4.75 mm in all. It prints without support.
base_pitch = 42;
base_cols = 4;
base_rows = 3;
base_clear = 0.5;
base_chamfers = [0.8, 1.8, 2.15];
base_corner_r = 3.75;               // each foot's corner radius at the top
base_h = base_chamfers[0] + base_chamfers[1] + base_chamfers[2];
floor_plate = 2.4;                  // the floor on top of the feet

/* [Shell] */
// z = 0 is the bottom of the feet.
H = 63;          // the top of the lid: 9 Gridfinity units. The buttons hang btn_clearance below it; under them, 2.25 mm spare, the tallest part on the hub board (hub_header_h), the board, and the floor
W = base_cols * base_pitch - base_clear;       // 167.5
wall = 2.4;
floor_t = base_h + floor_plate;     // the top of the floor, 7.15
lid_t = 3;
corner_r = 8;    // vertical corners

/* [Lid fixing] */
// "magnets": a 6 x 2 mm magnet in a pocket in the top of each corner boss and
// another in the underside of the lid, facing each other (glue them in with the
// poles opposed, as for the label blanks). "screws": the original M3 screws.
// The lid underside prints on the bed, so its pockets open onto it and may want
// a little support.
lid_fixing = "magnets"; // [magnets, screws]
boss_r = 5;       // 4.2 for screws; 5 leaves a 1.9 mm wall round a magnet pocket
m3_pilot = 2.6;   // self-tapping into the tub's corner bosses
m3_clear = 3.4;
m3_head = 6.4;    // countersink diameter

/* [Buttons] */
btn_cols = 4;
btn_rows = 2;
btn_housing_d = 19;                 // sits flush; the button threads in from below
btn_cap_d = 20;                     // the button's cap, which overhangs its 16.5 mm hole by 1.75 mm all round
btn_hole_d = 16.5;                  // the button's threaded barrel, 1 mm up from the 15.5 that was a tad tight; the 19 mm housing sits under the lid and sets the column spacing
btn_pitch = 26;                     // between columns
btn_clearance = 35;                 // from the top surface down: body, posts, wire bends (measured, about 35)
margin_x = 14;                      // the left edge to the first button housing; the grid and joystick are centred in the 167.5 mm lid

/* [Icon labels] */
// A round magnet pocket in the lid in front of each button (below it, looking
// down on the lid with the display at the bottom), and a round blank to stick
// there: the same size as a button hole, with a matching pocket in its back.
// Glue a magnet into each pocket with its pole facing the right way (check the
// pair attract before the glue sets). The rows are spaced to leave room for
// the second row's blanks between the two rows.
magnet_d = 6;
magnet_t = 2;
magnet_play = 0.2;                  // added to the pocket's diameter and depth, for the glue
blank_d = 15.5;                    // the blanks stay 15.5 mm when the holes grow to 16.5, so ones already printed still fit
blank_t = 3;                        // the front 0.8 mm is the face: swap filament there for an icon
label_gap = 3.5;                    // from a button hole to a blank, either side: the cap (btn_cap_d) overhangs the hole, so this leaves it 1.25 mm
row_pitch = btn_hole_d + blank_d + 2 * label_gap;

/* [Joystick, KY-023 / HW-504] */
// The board is screwed to the underside of the lid, components up, with its
// body poking through a cutout; four M3 screws from above, nuts on the board's
// underside. In the board's own frame u runs along its long edge, positive
// toward the pin row, and w runs across it, positive toward the side with the
// HW-504 lettering. The origin is the middle of the body, which is also the
// middle of the cutout. Sizes are from the sketch; the body is not in the
// middle of the board along u, but is assumed to be across it: MEASURE.
stick_board = [27, 33.5];           // across (w), along (u)
stick_board_t = 1.6;
stick_body = [25.62, 21];           // the housing and its side switches, as a bounding box: across, along (its corners are empty, which is where the pin-side screws sit)
stick_body_back = 5;                // the board's back edge (opposite the pins) to the body
stick_body_off_w = 0;               // MEASURE: body centre from the board centre across, toward the lettering
stick_gap = 10;                     // board top up to the stick's underside
stick_total = 30;                   // board top to the top of the stick
stick_shaft_u = 0.5;                // MEASURE: shaft from the body's centre, toward the pins
stick_pin_dir = 1;                  // which way the pin row faces: 1 toward the back of the box, -1 toward the front
stick_cut = [27, 22];               // cutout for the body: across (x), along (y)
stick_screw_d = 3.5;                // the board's holes, and the lid's
stick_hole_back = 2;                // back holes' centres in from the back edge: the holes all but touch it (MEASURE)
stick_hole_span_u = 27;               // along the board, centre to centre (25.5 on the sketch; the test print wanted 1.5 more)
stick_hole_span_w = 19;             // across the board, centre to centre
stick_clear_r = 13.5;               // the opening round the shaft, for the cap's rim to dip into: as far out as the cutout is across, where the stick moves freely
stick_riser_h = 5.5;                // a spacer on each screw, between the lid and the board, so the board sits this far below the lid; printed separately
stick_riser_d = 6.5;                // and a ring of lid this wide is kept round each screw hole for it to sit against
stick_gap_x = 15;                   // between the button grid and the board

/* [Display, SSD1306 0.91in] */
disp_pcb = [38.1, 12.15];
disp_win = [26.54, 12.15];          // the visible area
disp_win_dx = 0;                    // MEASURE: the glass is often off-centre on the PCB
disp_thick = 3.1;                   // PCB plus glass
dupont_len = 14;                    // a female plug on the header
display_strip = 18.5;               // depth in front of the button grid: the display's plug and frame need 18.2, so this is as far forward as the buttons go

/* [Boards] */
esp = [51.74, 29];                  // ELEGOO 30-pin (15 per side), flipped: pins up
esp_t = 1.6;
esp_standoff = 4.5;                 // the module and USB connector hang below it
esp_plug_height = 20;               // female plugs on the pins
esp_hole_span = [46, 24];           // stud centres: along the board, across it
esp_stud_d = 1.8;                   // MEASURE the hole; the stud wants to be well under it
esp_post_d = 4;                     // what the board rests on, round each stud

exp = [23.38, 38.25];               // Waveshare MCP23017 board, long edge front to back
exp_t = 1.6;
exp_standoff = 3;
exp_clearance = 35;                 // wires pointing up
exp_hole = 2.5;
exp_hole_edge = 1.5;                // hole edge to board edge, both ways
exp_stud_d = 1.9;                   // a little under the 2.5 hole
exp_cage_gap = 0.3;                 // between the board and its corner L's: the same as the ESP32's, now the studs hold the board
exp_hole_span = 19;                 // stud centres, measured on the board
exp_conn_arm = 4;                   // the guards' arms along the connector end (the others are 6)
exp_plug_gap = 17;                  // beside the PA header on the side facing the strip board: its pins are bent flat, so its 10-way plug goes on sideways

buz_d = 11.72;
buz_h = 8.5;
buz_port_d = 3.5;

dht = [15.84, 12.82];               // sensor body, 15.84 high and 12.82 wide when it stands against the wall
dht_h = 6.2;                        // its depth: it sits inside the right wall, behind vent slots, so nothing sticks out of the box

lux_board = [14, 19];               // BH1750 board, on the left wall
lux_win_d = 5;                      // the sensor itself is about 3 x 3

usb_w = 14;
usb_h = 5.5;                        // from the floor up; 4.5 mm off the first print's 10, which stood too tall

/* [Hub board: stripboard with JST-XH headers] */
// Sits on the floor, partly under the button grid, below the buttons' clearance.
hub = [34, 74.5];                   // x, y: the stripboard cut to 10 strips and turned 90 degrees
hub_t = 1.6;
hub_standoff = 3;                   // solder tails under the board
hub_header_h = 14;                  // the tallest part above the board: the screw-terminal blocks. 2.54 mm ones look about 6 to 10 mm; 14 allows a 5.08 mm KF301 block too. MEASURE
hub_gap = [1.0, 0.5];               // between the board and its corner guards, each side: along the long edge, along the short edge
hub_hole = [3, 5.25, 2.5];          // its one mounting hole, back left: edge to the short edge, edge to the long edge, diameter
hub_post_d = 6;
hub_stud_d = 1.9;                   // a little under its 2.5 mm mounting hole, as for the expander

/* [PIR sensor, AM312 mini] */
// Front wall, bottom right. The dome pokes out through the hole and the board
// sits in a pocket behind it. Sizes are from retailer listings: MEASURE yours.
pir_pcb = [10, 8];                  // x, z
pir_dome_d = 12;
pir_hole_d = pir_dome_d + 0.5;
pir_x = W - 22;
pir_z = floor_t + 11.6;               // 14 mm above the old floor
pir_plug_depth = 20;                // from the front wall: board, pins and a Dupont plug

/* [Derived] */
inner_top = H - lid_t;              // tub rim and the lid's underside
grid_w = (btn_cols - 1) * btn_pitch + btn_housing_d;
grid_d = (btn_rows - 1) * row_pitch + btn_housing_d;
D = base_rows * base_pitch - base_clear;       // 125.5

function col_x(c) = margin_x + btn_housing_d / 2 + c * btn_pitch;
function row_y(r) = wall + display_strip + btn_housing_d / 2 + r * row_pitch;
// Centre of a button's magnet pocket and blank, in front of it, halfway to the next hole.
function label_y(r) = row_y(r) - btn_hole_d / 2 - label_gap - blank_d / 2;

stick_x = margin_x + grid_w + stick_gap_x + stick_board[0] / 2;
stick_y = (row_y(0) + row_y(btn_rows - 1)) / 2;

btn_back = row_y(btn_rows - 1) + btn_hole_d / 2;

// A point in the board's frame, in the model's: u along the board, v along the pin row.
// Seen from above, the pin row points up the box (stick_pin_dir = 1) or down it.
function stick_pt(u, w) = [stick_x - stick_pin_dir * w, stick_y + stick_pin_dir * u];
// The board's edges and its hole pairs, in u and w from the middle of the body.
stick_u0 = -stick_body[1] / 2 - stick_body_back;        // back edge
stick_u1 = stick_u0 + stick_board[1];                   // pin edge
stick_wc = -stick_body_off_w;                           // middle of the board across
stick_holes = [for (s = [-1, 1]) each [
    stick_pt(stick_u0 + stick_hole_back, stick_wc + s * stick_hole_span_w / 2),
    stick_pt(stick_u0 + stick_hole_back + stick_hole_span_u, stick_wc + s * stick_hole_span_w / 2)]];

hub_y0 = 30;                        // clear of the PIR's plug (to y 22)
hub_z = floor_t + hub_standoff;     // underside of the board

disp_cx = W / 2;
disp_cz = (floor_t + inner_top) / 2;
pad_t = disp_thick + 0.1;           // the display's frame, in from the front wall

// Left to right under the lid: the ESP32 at the left wall with its USB slot in
// the middle of that wall, centred on the back row of buttons so that its two
// plug rows straddle the buttons' bodies; the expander beside it, turned so its
// PA header (pins bent flat, plug going on sideways) faces the strip board; then
// the strip board. The buzzer and the light sensor sit on the left wall, in
// front of and behind the ESP32. The DHT11 is on the right wall.
esp_x0 = wall + 3;
esp_y0 = row_y(btn_rows - 1) - esp[1] / 2;
esp_z = floor_t + esp_standoff;
exp_x0 = esp_x0 + esp[0] + 5;
exp_y0 = row_y(btn_rows - 1) - exp[1] / 2;
exp_y1 = exp_y0 + exp[1];
buz_y = 42;
buz_cz = floor_t + 1.2 + buz_d / 2;
dht_y = 75;                         // on the right wall, as far from the ESP32 as it gets
dht_z = floor_t + 21;
lux_y = 98;
lux_z = floor_t + 21.6;             // 24 mm above the old floor
hub_x0 = exp_x0 + exp[0] + exp_cage_gap + 1.2 + exp_plug_gap + hub_gap[0] + 1.2;   // past the expander's sideways plug
hub_hole_x = hub_x0 + hub_hole[1] + hub_hole[2] / 2;                // back left, after the board was turned
hub_hole_y = hub_y0 + hub[1] - hub_hole[0] - hub_hole[2] / 2;

assert(H - floor_t >= btn_clearance, "buttons need more height");
assert(abs(H / 7 - round(H / 7)) < 0.001, "the box is not a whole number of Gridfinity units tall");
assert(H - btn_clearance - (hub_z + hub_t + hub_header_h) >= 1.5, "hub board and its terminal blocks reach into the button clearance");
assert(hub_y0 - hub_gap[1] - 1.2 >= wall + pir_plug_depth + 1 || hub_x0 + hub[0] + hub_gap[0] + 1.2 + 0.5 <= pir_x - 8, "hub board and its guards run into the PIR frame");
assert(hub_y0 + hub[1] + hub_gap[1] + 1.2 <= D - wall, "hub board's back guards run into the back wall");
assert(hub_x0 + hub[0] + hub_gap[0] + 1.2 <= W - wall, "hub board and its guards run into the right wall");
assert(esp[1] - 8.44 >= btn_housing_d + 0.5, "the ESP32's plug rows are too close together to straddle a button row");
assert(inner_top - (floor_t + exp_standoff + exp_t) >= exp_clearance, "expander wires need more height");
assert(inner_top - (esp_z + esp_t) >= esp_plug_height, "ESP32 plugs need more height");
assert(pad_t + dupont_len + 1 <= display_strip, "display plug runs into the button zone");
assert(esp_x0 + esp[0] + 1.5 < exp_x0 - exp_cage_gap - 1.2, "ESP32 cage overlaps the expander cage");
assert(exp_x0 + exp[0] + exp_cage_gap + 1.2 + exp_plug_gap <= hub_x0 - hub_gap[0] - 1.2 + 0.01, "the expander's sideways plug runs into the hub board");
assert(norm(pir_pcb) > pir_hole_d, "PIR board would fall through its hole");
assert(buz_y + buz_d / 2 + 1.6 < esp_y0 - 1.5 - 1, "the buzzer cradle runs into the ESP32's cage");
assert(lux_y - lux_board[0] / 2 > esp_y0 + esp[1] / 2 + usb_w / 2 + 2, "the light sensor is over the USB slot");
assert(stick_x + stick_board[0] / 2 <= W - wall, "joystick board is past the right wall");
assert(stick_hole_back >= stick_screw_d / 2 - 0.5
       && stick_hole_back + stick_hole_span_u + stick_screw_d / 2 + 0.5 <= stick_board[1]
       && stick_hole_span_w / 2 + stick_screw_d / 2 + 0.5 <= stick_board[0] / 2, "the joystick's screw holes are off its board");
assert(stick_body_back + stick_body[1] <= stick_board[1], "the joystick's body runs off its board");
assert(stick_hole_back - stick_screw_d / 2 < stick_body_back, "a back screw hole is under the body");
assert(stick_y + min(stick_pin_dir * stick_u0, stick_pin_dir * stick_u1) >= wall
       && stick_y + max(stick_pin_dir * stick_u0, stick_pin_dir * stick_u1) <= D - wall, "joystick board is past the front or back wall");
assert(stick_cut[0] >= stick_body[0] + 0.6 && stick_cut[1] >= stick_body[1] + 0.6, "the cutout is too tight round the joystick's body");
assert(stick_riser_d >= stick_screw_d + 2, "a joystick spacer would have no wall");
assert(stick_x - stick_clear_r > col_x(btn_cols - 1) + btn_housing_d / 2, "the joystick opening runs into the buttons");
assert(stick_x - stick_cut[0] / 2 > col_x(btn_cols - 1) + btn_housing_d / 2, "joystick cutout runs into the buttons");
assert(label_y(0) - blank_d / 2 >= wall, "the first row's blanks hang over the front edge");
assert(len([for (c = [0:btn_cols - 1], r = [0:btn_rows - 1], b = boss_c)
            if (norm([col_x(c), label_y(r)] - b) < blank_d / 2 + m3_head / 2 + 0.5) 1]) == 0,
       "a label blank runs into a lid screw");
assert(lid_t - (magnet_t + magnet_play) >= 0.6, "the lid is too thin under a magnet pocket");
assert(boss_r - (magnet_d + magnet_play) / 2 >= 1.5 || lid_fixing == "screws", "the corner bosses are too thin round the magnet pockets");
assert(blank_t - (magnet_t + magnet_play) >= 0.6, "a blank is too thin over its magnet pocket");
assert(blank_d <= btn_pitch - 4, "blanks are too wide for the column spacing");
assert(btn_hole_d / 2 + label_gap >= btn_cap_d / 2 + 1, "a button cap would touch its label blank");

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
// legx is the length of the arm that runs along the board's end edge (the
// other arm, along its side, is leg).
module corner_l(cx, cy, sx, sy, z0, z1, leg = 6, t = 1.2, legx = 6) {
    span([cx + sx * t, cy, z0], [cx - sx * legx, cy + sy * t, z1]);
    span([cx, cy, z0], [cx + sx * t, cy - sy * leg, z1]);
}

// ---------------------------------------------------------------- the tub

// One Gridfinity foot, in cell (i, j): hulls of four outlines, from the bottom
// 35.6 mm square up to the 41.5 mm square at base_h, where the box starts.
function base_inset(k) = base_clear / 2 + (k == 0 ? base_chamfers[0] + base_chamfers[2] : k == 1 ? base_chamfers[2] : k == 2 ? base_chamfers[2] : 0);
module base_slab(i, j, inset, z) {
    translate([i * base_pitch - base_clear / 2 + inset, j * base_pitch - base_clear / 2 + inset, z])
        linear_extrude(0.001)
            rrect2d(base_pitch - 2 * inset, base_pitch - 2 * inset, max(0.01, base_corner_r - (inset - base_clear / 2)));
}
module base_foot(i, j) {
    z1 = base_chamfers[0];
    z2 = z1 + base_chamfers[1];
    hull() { base_slab(i, j, base_inset(0), 0);  base_slab(i, j, base_inset(1), z1); }
    hull() { base_slab(i, j, base_inset(1), z1); base_slab(i, j, base_inset(2), z2); }
    hull() { base_slab(i, j, base_inset(2), z2); base_slab(i, j, base_inset(3), base_h); }
}
module gridfinity_base() {
    for (i = [0:base_cols - 1], j = [0:base_rows - 1]) base_foot(i, j);
}

module tub() {
    difference() {
        union() {
            difference() {
                union() {
                    translate([0, 0, base_h - 0.01]) linear_extrude(inner_top - base_h + 0.01) rrect2d(W, D, corner_r);
                    gridfinity_base();
                }
                translate([0, 0, floor_t]) linear_extrude(H) inner2d();
            }
            screw_bosses();
            display_frame();
            pir_frame();
            esp_supports();
            expander_cage();
            hub_cage();
            buzzer_cradle();
        }
        tub_cutouts();
    }
}

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

module hub_cage() {
    // A guard (an L) at each of the four corners locates the board, and a pad
    // under each corner lifts it so its solder tails clear the floor. The back
    // left corner, which has the board's mounting hole, takes a post with a
    // stud through the hole instead of a pad. Hot glue does the rest.
    for (sx = [-1, 1], sy = [-1, 1]) {
        bx = hub_x0 + (sx > 0 ? hub[0] : 0);
        by = hub_y0 + (sy > 0 ? hub[1] : 0);
        if (!(sx < 0 && sy > 0)) span([bx, by, floor_t - 0.01], [bx - sx * 5, by - sy * 5, hub_z]);
        corner_l(bx + sx * hub_gap[0], by + sy * hub_gap[1], sx, sy, floor_t - 0.01, hub_z + hub_t + 2);
    }
    stud(hub_hole_x, hub_hole_y, hub_post_d, hub_stud_d, hub_standoff, hub_standoff + hub_t + 1);
}

esp_holes = [for (sx = [-1, 1], sy = [-1, 1])
    [esp_x0 + esp[0] / 2 + sx * esp_hole_span[0] / 2, esp_y0 + esp[1] / 2 + sy * esp_hole_span[1] / 2]];

// The PIR board's frame on the inside of the front wall, sloped underneath so
// it prints without support.
module pir_frame() {
    fw = 16;
    hull() {
        span([pir_x - fw / 2, wall - 0.01, pir_z - fw / 2 - 3], [pir_x + fw / 2, wall, pir_z + fw / 2]);
        span([pir_x - fw / 2, wall - 0.01, pir_z - fw / 2], [pir_x + fw / 2, wall + 3, pir_z + fw / 2]);
    }
}

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

exp_holes = [[exp_x0 + exp[0] / 2 - exp_hole_span / 2, exp_y0 + exp_hole_edge + exp_hole / 2],
             [exp_x0 + exp[0] / 2 + exp_hole_span / 2, exp_y0 + exp_hole_edge + exp_hole / 2]];

module expander_cage() {
    // Same idea as the ESP32: an L at each corner, studs through the two holes
    // at the front end, and a pad under each corner of the connector end, at
    // the back.
    c = exp_cage_gap;
    top = floor_t + exp_standoff;
    for (sx = [-1, 1], sy = [-1, 1]) {
        bx = exp_x0 + (sx > 0 ? exp[0] : 0);
        by = exp_y0 + (sy > 0 ? exp[1] : 0);
        // The guards' arms along the connector end (the back) are 2 mm shorter,
        // to clear the connector that overhangs that edge.
        corner_l(bx + sx * c, by + sy * c, sx, sy, floor_t - 0.01, top + exp_t + 2,
                 legx = sy > 0 ? exp_conn_arm : 6);
        if (sy > 0) span([bx, by, floor_t - 0.01], [bx - sx * 4, by - 4, top]);
    }
    for (p = exp_holes) stud(p[0], p[1], 4.2, exp_stud_d, exp_standoff, exp_standoff + exp_t + 1);
}

module buzzer_cradle() {
    // The buzzer lies on its side with its sound face against the left wall.
    span([wall - 0.01, buz_y - buz_d / 2 - 1.6, floor_t - 0.01],
         [wall + buz_h, buz_y + buz_d / 2 + 1.6, buz_cz]);
}

module tub_cutouts() {
    // Display window through the front wall, and the frame's pocket behind it.
    translate([disp_cx + disp_win_dx - disp_win[0] / 2, -1, disp_cz - disp_win[1] / 2])
        cube([disp_win[0], wall + 1.02, disp_win[1]]);
    span([disp_cx - disp_pcb[0] / 2 - 0.2, wall, disp_cz - disp_pcb[1] / 2 - 0.2],
         [disp_cx + disp_pcb[0] / 2 + 0.2, wall + pad_t + 0.5, disp_cz + disp_pcb[1] / 2 + 0.2]);

    // PIR: the dome through the front wall, the board's pocket behind it.
    translate([pir_x, -1, pir_z]) rotate([-90, 0, 0]) cylinder(d = pir_hole_d, h = wall + 1.02);
    span([pir_x - pir_pcb[0] / 2 - 0.2, wall, pir_z - pir_pcb[1] / 2 - 0.2],
         [pir_x + pir_pcb[0] / 2 + 0.2, wall + 3.5, pir_z + pir_pcb[1] / 2 + 0.2]);

    // USB cable through the left wall, level with the ESP32's connector.
    translate([-1, esp_y0 + esp[1] / 2 - usb_w / 2, floor_t])
        cube([wall + 2, usb_w, usb_h]);

    // Light sensor window in the left wall.
    translate([-1, lux_y, lux_z]) rotate([0, 90, 0]) cylinder(d = lux_win_d, h = wall + 2);

    // DHT11: five vertical vent slots through the right wall, with the sensor
    // glued inside behind them.
    for (i = [-2:2]) translate([W - wall - 1, dht_y + i * 3 - 0.8, dht_z - 6]) cube([wall + 2, 1.6, 12]);

    // Buzzer trough and its sound port.
    translate([wall - 0.01, buz_y, buz_cz]) rotate([0, 90, 0])
        cylinder(d = buz_d + 0.3, h = buz_h + 1.01);
    translate([-1, buz_y, buz_cz]) rotate([0, 90, 0])
        cylinder(d = buz_port_d, h = wall + 2);

    // Pilot holes for the lid screws.
    if (lid_fixing == "screws")
        for (c = boss_c) translate([c[0], c[1], inner_top - 14]) cylinder(d = m3_pilot, h = 15);
    else
        for (c = boss_c) translate([c[0], c[1], inner_top - magnet_t - magnet_play]) cylinder(d = magnet_d + magnet_play, h = magnet_t + magnet_play + 1);
}

// ---------------------------------------------------------------- the lid

module lid() {
    difference() {
        translate([0, 0, inner_top]) linear_extrude(lid_t) rrect2d(W, D, corner_r);
        for (r = [0:btn_rows - 1], c = [0:btn_cols - 1])
            translate([col_x(c), row_y(r), inner_top - 1]) cylinder(d = btn_hole_d, h = lid_t + 2);
        for (r = [0:btn_rows - 1], c = [0:btn_cols - 1])
            translate([col_x(c), label_y(r), H - magnet_t - magnet_play])
                cylinder(d = magnet_d + magnet_play, h = magnet_t + magnet_play + 1);
        translate([0, 0, inner_top - 1]) linear_extrude(lid_t + 2) stick_cutout2d();
        for (p = stick_holes) translate([p[0], p[1], inner_top - 1]) cylinder(d = stick_screw_d, h = lid_t + 2);
        for (c = boss_c) {
            if (lid_fixing == "screws") {
                translate([c[0], c[1], inner_top - 1]) cylinder(d = m3_clear, h = lid_t + 2);
                translate([c[0], c[1], H - 1.5]) cylinder(h = 1.51, d1 = m3_clear, d2 = m3_head);
            } else {
                // magnet pocket in the underside; the top keeps lid_t minus the pocket's depth
                translate([c[0], c[1], inner_top - 1]) cylinder(d = magnet_d + magnet_play, h = 1 + magnet_t + magnet_play);
            }
        }
    }
}

// The joystick's body goes through this, and its cap's rim dips into it when
// the stick is tilted: a rectangle for the body, widened by a circle round the
// shaft. A full ring of lid is kept round each screw hole for its spacer.
module stick_cutout2d() {
    difference() {
        union() {
            translate([stick_x - stick_cut[0] / 2, stick_y - stick_cut[1] / 2]) square(stick_cut);
            translate(stick_pt(stick_shaft_u, 0)) circle(r = stick_clear_r);
        }
        for (p = stick_holes) translate(p) circle(d = stick_riser_d);
    }
}

// One spacer for a joystick screw, as printed.
module stick_riser() {
    difference() {
        cylinder(d = stick_riser_d, h = stick_riser_h);
        translate([0, 0, -1]) cylinder(d = stick_screw_d, h = stick_riser_h + 2);
    }
}

// A label blank as printed: the face on the bed, a small chamfer round it, and
// the magnet pocket in the back, open upward.
module label_blank() {
    difference() {
        union() {
            cylinder(d1 = blank_d - 0.8, d2 = blank_d, h = 0.4);
            translate([0, 0, 0.39]) cylinder(d = blank_d, h = blank_t - 0.39);
        }
        translate([0, 0, blank_t - magnet_t - magnet_play]) cylinder(d = magnet_d + magnet_play, h = magnet_t + magnet_play + 1);
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
        // The WROOM-32 module: 25.5 mm along the board, 18 mm across, centred, at the antenna end.
        translate([esp_x0 + esp[0] - 25.5, esp_y0 + (esp[1] - 18) / 2, esp_z - 3.1]) cube([25.5, 18, 3.1]);
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
    color("lightsteelblue", 0.6)
        for (r = [0:btn_rows - 1], c = [0:btn_cols - 1])
            translate([col_x(c), label_y(r), H]) cylinder(d = blank_d, h = blank_t);
    color("purple", 0.45) {
        // Joystick: the board against the lid's underside, its body through the cutout, then the shaft.
        bz = inner_top - stick_riser_h - stick_board_t;
        y0 = stick_pt(stick_u0, 0)[1];
        y1 = stick_pt(stick_u1, 0)[1];
        bx = stick_pt(0, stick_wc)[0];
        difference() {
            translate([bx - stick_board[0] / 2, min(y0, y1), bz]) cube([stick_board[0], abs(y1 - y0), stick_board_t]);
            for (p = stick_holes) translate([p[0], p[1], bz - 1]) cylinder(d = stick_screw_d, h = stick_board_t + 2);
        }
        translate([stick_x - stick_body[0] / 2, stick_y - stick_body[1] / 2, bz + stick_board_t]) cube([stick_body[0], stick_body[1], stick_gap]);
        translate([stick_pt(stick_shaft_u, 0)[0], stick_pt(stick_shaft_u, 0)[1], bz + stick_board_t + stick_gap]) cylinder(d = 8, h = stick_total - stick_gap);
        for (p = stick_holes) translate([p[0], p[1], inner_top - stick_riser_h]) difference() { cylinder(d = stick_riser_d, h = stick_riser_h); translate([0, 0, -1]) cylinder(d = stick_screw_d, h = stick_riser_h + 2); }
    }
    color("silver", 0.5) {
        // Hub board and its headers.
        difference() {
            translate([hub_x0, hub_y0, hub_z]) cube([hub[0], hub[1], hub_t]);
            translate([hub_hole_x, hub_hole_y, hub_z - 1]) cylinder(d = hub_hole[2] + 0.4, h = hub_t + 2);
        }
        translate([hub_x0, hub_y0, hub_z + hub_t + 1.2]) cube([hub[0], hub[1], hub_header_h - 1.2]);
    }
    color("gold", 0.5) {
        // PIR: board, dome through the wall, and the pins and plug behind it.
        translate([pir_x - pir_pcb[0] / 2, wall, pir_z - pir_pcb[1] / 2]) cube([pir_pcb[0], 1.2, pir_pcb[1]]);
        translate([pir_x, -6, pir_z]) rotate([-90, 0, 0]) cylinder(d = pir_dome_d, h = 6 + wall);
        translate([pir_x - pir_pcb[0] / 2, wall + 1.2, pir_z - pir_pcb[1] / 2])
            cube([pir_pcb[0], pir_plug_depth - wall - 1.2, pir_pcb[1]]);
    }
    color("orange", 0.5) {
        translate([wall, buz_y, buz_cz]) rotate([0, 90, 0]) cylinder(d = buz_d, h = buz_h);
        translate([W - wall - dht_h, dht_y - dht[1] / 2, dht_z - dht[0] / 2]) cube([dht_h, dht[1], dht[0]]);
        translate([wall, lux_y - lux_board[0] / 2, lux_z - lux_board[1] / 2]) cube([1.6, lux_board[0], lux_board[1]]);
        translate([disp_cx - disp_pcb[0] / 2, wall, disp_cz - disp_pcb[1] / 2]) cube([disp_pcb[0], disp_thick, disp_pcb[1]]);
    }
}

// A slice of the tub to test the board mounts: the floor and feet, the left wall
// up to 14 mm above the floor (the USB slot is in it), and the ESP32 and expander cages.
module test_piece() {
    intersection() {
        tub();
        translate([-1, esp_y0 - 6, -1]) cube([hub_x0 - 12, esp[1] + 12 + 14, floor_t + 14]);
    }
}

// How thick the floor is on the test pieces. The real one is floor_t; these
// are only for fitting boards, so a single thin layer saves filament and time.
// Everything above the floor keeps its true height above it.
fit_floor_t = 0.3;

// The hub board's back-left corner, to check the anchor stud and the corner
// guard before printing the whole tub.
module hub_test_piece() {
    dz = floor_t - fit_floor_t;
    x0 = hub_x0 - 6;
    y1 = hub_y0 + hub[1] + 3;
    translate([x0, y1 - 46, 0]) cube([46, 46, fit_floor_t]);
    translate([0, 0, -dz]) intersection() {
        hub_cage();
        translate([x0, y1 - 46, -1]) cube([46, 46, 30]);
    }
}

// The three boards' cages on one thin floor, in their true positions relative
// to each other: the ESP32 and the expander, and the whole hub board beside
// them. A stub of the left wall (with the USB-C slot) is included so you can
// dry-fit the real boards together before printing the whole tub. No lid, no
// other features.
module fit_test_piece() {
    dz = floor_t - fit_floor_t;
    esp_yc = esp_y0 + esp[1] / 2;
    translate([0, hub_y0 - 4, 0]) cube([hub_x0 + hub[0] + 8, D - wall - (hub_y0 - 4), fit_floor_t]);
    translate([0, 0, -dz]) difference() {
        union() {
            translate([0, esp_y0 - 8, floor_t - 0.01]) cube([wall, esp[1] + 16, 14 - floor_t + 0.01]);
            esp_supports();
            expander_cage();
            hub_cage();
        }
        translate([-1, esp_yc - usb_w / 2, floor_t]) cube([wall + 2, usb_w, usb_h]);
    }
}

// The lid's joystick cutout and screw holes on a small flat piece, to see
// whether the board and its body fit before printing a whole lid.
module stick_test_piece() {
    intersection() {
        lid();
        translate([stick_x - 22, stick_y - 26, inner_top - 1]) cube([44, 52, lid_t + 2]);
    }
}

// ---------------------------------------------------------------- render

if (part == "tub") {
    tub();
} else if (part == "lid") {
    translate([0, 0, -inner_top]) lid();
} else if (part == "test") {
    test_piece();
} else if (part == "fit") {
    fit_test_piece();
} else if (part == "hubtest") {
    hub_test_piece();
} else if (part == "sticktest") {
    translate([0, 0, -inner_top]) stick_test_piece();
} else if (part == "risers") {
    for (i = [0:3]) translate([i * (stick_riser_d + 3), 0, 0]) stick_riser();
} else if (part == "blank") {
    label_blank();
} else if (part == "blanks") {
    for (r = [0:btn_rows - 1], c = [0:btn_cols - 1])
        translate([c * (blank_d + 3) + blank_d / 2, r * (blank_d + 3) + blank_d / 2, 0]) label_blank();
} else if (part == "print") {
    tub();
    translate([W + 10, 0, -inner_top]) lid();
} else if (part == "assembly") {
    tub();
    lid();
    if (show_parts) ghosts();
}
