#include "layout.h"
#include "safe_area.h"

static int imax(int a, int b) { return (a > b) ? a : b; }
static int imin(int a, int b) { return (a < b) ? a : b; }

// Chrome is sized from the view's LONG edge, never from the live height, so
// the wordmark and the counters do not resize when the device is merely
// turned. The same rule, and the same divisors, as the rest of the family.
static int chrome_ref(int view_w, int view_h) { return imax(view_w, view_h); }

static int title_fs_of(int ref)  { int fs = ref / 45; return (fs < 10) ? 10 : fs; }
static int title_bar_of(int ref) { int fs = title_fs_of(ref); return fs + fs / 2; }
static int hud_fs_of(int ref)    { int fs = ref / 38; return (fs < 9) ? 9 : fs; }

// The wordmark bar, grown to clear a display cutout when the surface draws
// under one. iOS hands the game a viewport that already excludes the notch, so
// this only fires on Android.
static int top_bar_of(int ref) {
    int bar = title_bar_of(ref);
    SafeArea s = safe_area_get();
    return (s.top > bar) ? s.top : bar;
}

Layout layout_for(int view_w, int view_h, int game_cols, int game_rows) {
    Layout l = (Layout){0};
    l.view_w = view_w;
    l.view_h = view_h;
    if (game_cols < 1) game_cols = 1;
    if (game_rows < 1) game_rows = 1;

    int shortd = imin(view_w, view_h);
    int ref    = chrome_ref(view_w, view_h);

    l.margin     = imax(shortd / 28, 6);
    l.titlebar_h = top_bar_of(ref);
    l.title_fs   = title_fs_of(ref);
    l.hud_fs     = hud_fs_of(ref);
    l.hud_h      = l.hud_fs * 2;
    l.hud_y      = l.titlebar_h + l.margin / 2;
    l.face_size  = l.hud_h;
    l.face_x     = view_w / 2 - l.face_size / 2;
    l.face_y     = l.hud_y;

    // Space left for the grid, below the band and inside the margins and the
    // side / bottom insets.
    SafeArea sa = safe_area_get();
    int avail_x = l.margin + sa.left;
    int avail_y = l.hud_y + l.hud_h + l.margin;
    int avail_w = view_w - avail_x - l.margin - sa.right;
    int avail_h = view_h - avail_y - l.margin - sa.bottom;
    if (avail_w < 1) avail_w = 1;
    if (avail_h < 1) avail_h = 1;

    // Both ways round; keep whichever gives the bigger cell. A tie keeps the
    // game's own orientation.
    int straight = imin(avail_w / game_cols, avail_h / game_rows);
    int turned   = imin(avail_w / game_rows, avail_h / game_cols);
    l.transposed = turned > straight;
    l.cols = l.transposed ? game_rows : game_cols;
    l.rows = l.transposed ? game_cols : game_rows;
    l.cell = imax(l.transposed ? turned : straight, 1);

    l.board_w = l.cols * l.cell;
    l.board_h = l.rows * l.cell;
    l.board_x = avail_x + (avail_w - l.board_w) / 2;
    l.board_y = avail_y + (avail_h - l.board_h) / 2;
    return l;
}

void layout_cell_pos(Layout l, int col, int row, int* x, int* y) {
    int sc = l.transposed ? row : col;
    int sr = l.transposed ? col : row;
    *x = l.board_x + sc * l.cell;
    *y = l.board_y + sr * l.cell;
}

bool layout_cell_at(Layout l, int x, int y, int* col, int* row) {
    if (l.cell <= 0) return false;
    int dx = x - l.board_x, dy = y - l.board_y;
    if (dx < 0 || dy < 0) return false;
    int sc = dx / l.cell, sr = dy / l.cell;
    if (sc >= l.cols || sr >= l.rows) return false;
    *col = l.transposed ? sr : sc;
    *row = l.transposed ? sc : sr;
    return true;
}

bool layout_face_hit(Layout l, int x, int y) {
    return x >= l.face_x && x < l.face_x + l.face_size &&
           y >= l.face_y && y < l.face_y + l.face_size;
}
