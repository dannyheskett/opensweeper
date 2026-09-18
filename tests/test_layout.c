// Tests for the board layout (src/layout.c): every grid fits every view shape,
// a grid that fits better sideways is drawn transposed, the chrome keeps its
// size when the device is merely turned, and hit testing inverts cell
// placement. Pure: no raylib, no window (compiled -DPLATFORM_IOS, so the types
// header needs no raylib).
#define PLATFORM_IOS 1
#include "../src/layout.c"
#include "../src/safe_area.c"

#include <stdio.h>
#include <stdlib.h>

#define PASS(name) printf("PASS: %s\n", name)
#define FAIL(name, msg) do { fprintf(stderr, "FAIL: %s -- %s\n", name, msg); exit(1); } while (0)

static const int GRIDS[3][2] = { {9, 9}, {16, 16}, {30, 16} };   // cols x rows
static const int VIEWS[][2] = {
    {640, 480}, {960, 720}, {1920, 1080}, {390, 844}, {844, 390},
    {1170, 2532}, {2532, 1170}, {820, 1180}, {1180, 820}, {2048, 2732},
};
#define NVIEWS (int)(sizeof VIEWS / sizeof VIEWS[0])

static void test_fits(void) {
    for (int v = 0; v < NVIEWS; v++)
        for (int g = 0; g < 3; g++) {
            int w = VIEWS[v][0], h = VIEWS[v][1];
            Layout l = layout_for(w, h, GRIDS[g][0], GRIDS[g][1]);
            if (l.cell < 1) FAIL("fits", "no cell size");
            if (l.board_x < 0 || l.board_y < l.hud_y + l.hud_h ||
                l.board_x + l.board_w > w || l.board_y + l.board_h > h) {
                fprintf(stderr, "%dx%d grid %d: board %d,%d %dx%d\n", w, h, g,
                        l.board_x, l.board_y, l.board_w, l.board_h);
                FAIL("fits", "the grid leaves the view or overlaps the band");
            }
        }
    PASS("fits");
}

static void test_transpose(void) {
    // Expert on a portrait phone stands upright, and its cells are larger than
    // they would be laid flat.
    Layout up = layout_for(1170, 2532, 30, 16);
    if (!up.transposed) FAIL("transpose", "Expert was not turned on a portrait phone");
    if (up.cols != 16 || up.rows != 30) FAIL("transpose", "the turned grid is 16 x 30");
    Layout flat = layout_for(2532, 1170, 30, 16);
    if (flat.transposed) FAIL("transpose", "Expert was turned on a landscape phone");
    // A square grid never turns.
    if (layout_for(1170, 2532, 16, 16).transposed) FAIL("transpose", "a square grid turned");
    PASS("transpose");
}

static void test_chrome_keeps_size_on_rotation(void) {
    Layout a = layout_for(1170, 2532, 16, 16), b = layout_for(2532, 1170, 16, 16);
    if (a.title_fs != b.title_fs || a.hud_fs != b.hud_fs || a.face_size != b.face_size)
        FAIL("rotation", "the chrome changed size when the device turned");
    PASS("rotation");
}

static void test_hit_inverts_position(void) {
    for (int v = 0; v < NVIEWS; v++)
        for (int g = 0; g < 3; g++) {
            Layout l = layout_for(VIEWS[v][0], VIEWS[v][1], GRIDS[g][0], GRIDS[g][1]);
            for (int r = 0; r < GRIDS[g][1]; r++)
                for (int c = 0; c < GRIDS[g][0]; c++) {
                    int x, y, hc = -1, hr = -1;
                    layout_cell_pos(l, c, r, &x, &y);
                    if (!layout_cell_at(l, x + l.cell / 2, y + l.cell / 2, &hc, &hr) ||
                        hc != c || hr != r)
                        FAIL("hit", "a cell's centre does not hit that cell");
                }
            int dc, dr;
            if (layout_cell_at(l, l.board_x - 1, l.board_y, &dc, &dr))
                FAIL("hit", "a point left of the grid hit a cell");
            if (layout_cell_at(l, l.board_x, l.board_y + l.board_h, &dc, &dr))
                FAIL("hit", "a point below the grid hit a cell");
        }
    PASS("hit");
}

static void test_face(void) {
    Layout l = layout_for(960, 720, 16, 16);
    if (!layout_face_hit(l, l.face_x + l.face_size / 2, l.face_y + l.face_size / 2))
        FAIL("face", "the face's centre does not hit it");
    if (layout_face_hit(l, l.board_x + 1, l.board_y + 1))
        FAIL("face", "the grid hits the face");
    PASS("face");
}

static void test_clears_a_cutout(void) {
    Layout plain = layout_for(1080, 2400, 16, 16);
    s_area.top = 140;
    Layout notched = layout_for(1080, 2400, 16, 16);
    s_area = (SafeArea){0};
    if (notched.titlebar_h < 140) FAIL("cutout", "the title bar does not clear the inset");
    if (notched.board_y <= plain.board_y && notched.cell >= plain.cell)
        FAIL("cutout", "the grid was not moved or shrunk for the cutout");
    PASS("cutout");
}

int main(void) {
    test_fits();
    test_transpose();
    test_chrome_keeps_size_on_rotation();
    test_hit_inverts_position();
    test_face();
    test_clears_a_cutout();
    printf("All layout tests passed.\n");
    return 0;
}
