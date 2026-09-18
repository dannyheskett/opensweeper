// The whole renderer: chrome, the grid, mines and flags, the menu and the end
// notice. Everything is drawn through the gfx primitives (gfx.h) from a Layout
// computed off the live view each frame (layout.c), so the same code runs on
// the raylib backends and on the native Metal backend the iOS build uses.
#include "render.h"
#include "layout.h"
#include "gfx.h"
#include "menu.h"
#include "present.h"
#include "safe_area.h"
#include "window.h"
#include <stdio.h>

// --------------------------------------------------------------------------
// Palette
// --------------------------------------------------------------------------
static const Color COL_BG         = {20,  20,  20,  255};
static const Color COL_TOPBAR     = {30,  30,  30,  255};
static const Color COL_TITLE      = {200, 200, 200, 255};
static const Color COL_HUD        = {25,  25,  25,  255};
static const Color COL_CELL_UNREV = {35,  35,  35,  255};
static const Color COL_CELL_REV   = {20,  20,  20,  255};
static const Color COL_BORDER     = {60,  60,  60,  255};
static const Color COL_CURSOR     = {180, 180, 180, 255};
static const Color COL_MINE_BG    = {220, 60,  60,  255};
static const Color COL_MINE       = {200, 200, 200, 255};
static const Color COL_FLAG       = {240, 200, 40,  255};
static const Color COL_QUESTION   = {0,   200, 240, 255};
static const Color COL_COUNTER    = {240, 200, 40,  255};
static const Color COL_FACE_BG    = {60,  55,  20,  255};
static const Color COL_WHITE      = {220, 220, 220, 255};
static const Color COL_WRONG_FLAG = {180, 60,  60,  255};

static const Color NUM_COLORS[9] = {
    {0,   0,   0,   255},
    {100, 120, 240, 255},  // 1 blue
    {60,  180, 60,  255},  // 2 green
    {220, 60,  60,  255},  // 3 red
    {60,  60,  180, 255},  // 4 dark blue
    {160, 40,  40,  255},  // 5 maroon
    {60,  180, 180, 255},  // 6 teal
    {220, 220, 220, 255},  // 7 white
    {140, 140, 140, 255},  // 8 gray
};

static bool s_touch_ui =
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_IOS)
    true;
#else
    false;
#endif
static int s_cell = 32;   // cell side of the last board drawn at window size

void render_set_touch_ui(bool touch) { s_touch_ui = touch; }
bool render_touch_ui(void)           { return s_touch_ui; }

static Layout live_layout(const Game* g) {
    return layout_for(GetScreenWidth(), GetScreenHeight(), g->cols, g->rows);
}

// --------------------------------------------------------------------------
// Cells
// --------------------------------------------------------------------------
static void text_centered(const char* s, int cx, int cy, int fs, Color c) {
    gfx_text(s, cx - gfx_measure_text(s, fs) / 2, cy - fs / 2, fs, c);
}

// A mine: a round body with four arms and a highlight, scaled to the cell.
static void draw_mine(int x, int y, int s) {
    float cx = x + s / 2.0f, cy = y + s / 2.0f;
    int arm = s * 3 / 4, t = s / 6 > 1 ? s / 6 : 1;
    gfx_rect((int)cx - t / 2, (int)cy - arm / 2, t, arm, COL_MINE);
    gfx_rect((int)cx - arm / 2, (int)cy - t / 2, arm, t, COL_MINE);
    gfx_circle(cx, cy, s * 0.26f, COL_MINE);
    gfx_circle(cx - s * 0.08f, cy - s * 0.08f, s * 0.07f, COL_WHITE);
}

// A flag: a pole on a base with a pennant.
static void draw_flag(int x, int y, int s) {
    int pole_x = x + s * 17 / 32, pole_w = s / 16 > 1 ? s / 16 : 1;
    gfx_rect(pole_x, y + s * 5 / 32, pole_w, s * 22 / 32, COL_MINE);
    gfx_rect(x + s * 10 / 32, y + s * 26 / 32, s * 12 / 32, s * 2 / 32 > 1 ? s * 2 / 32 : 1,
             COL_MINE);
    gfx_triangle((Vector2){ (float)pole_x, (float)(y + s * 5 / 32) },
                 (Vector2){ (float)pole_x, (float)(y + s * 15 / 32) },
                 (Vector2){ (float)(x + s * 7 / 32), (float)(y + s * 10 / 32) }, COL_FLAG);
}

static void draw_cell(const Game* g, Layout l, int col, int row, bool show_cursor) {
    int x, y;
    layout_cell_pos(l, col, row, &x, &y);
    int s = l.cell;
    const Cell* c = &g->cells[row][col];
    bool is_cursor    = show_cursor && col == g->cursor_x && row == g->cursor_y;
    bool is_detonated = (col == g->detonated_x && row == g->detonated_y);
    int fs = s * 3 / 4;

    if (!c->revealed) {
        gfx_rect(x, y, s, s, COL_CELL_UNREV);
        gfx_rect_lines(x, y, s, s, is_cursor ? COL_CURSOR : COL_BORDER);
        if (c->flagged) {
            if (g->phase == PHASE_LOST && !c->mine)
                text_centered("X", x + s / 2, y + s / 2, fs, COL_WRONG_FLAG);
            else
                draw_flag(x, y, s);
        } else if (c->question) {
            text_centered("?", x + s / 2, y + s / 2, fs, COL_QUESTION);
        }
    } else {
        gfx_rect(x, y, s, s, is_detonated ? COL_MINE_BG : COL_CELL_REV);
        gfx_rect_lines(x, y, s, s, is_cursor ? COL_CURSOR : COL_BORDER);
        if (c->mine) {
            draw_mine(x, y, s);
        } else if (c->adj > 0) {
            char num[2] = { (char)('0' + c->adj), 0 };
            text_centered(num, x + s / 2, y + s / 2, fs, NUM_COLORS[c->adj]);
        }
    }
}

// --------------------------------------------------------------------------
// Chrome
// --------------------------------------------------------------------------
static void draw_title_bar(Layout l) {
    gfx_rect(0, 0, l.view_w, l.titlebar_h, COL_TOPBAR);
    gfx_line(0, l.titlebar_h, l.view_w, l.titlebar_h, COL_BORDER);
    const char* title = "OPENSWEEPER";
    int fs = l.title_fs;
    int tw = gfx_measure_text(title, fs);
    int ty = (l.titlebar_h - fs) / 2;

    // Keep the wordmark clear of a camera cutout: if the centre is taken, put
    // it on whichever side has room, and if neither has, leave the bar bare.
    SafeArea sa = safe_area_get();
    int cx = (l.view_w - tw) / 2;
    if (sa.cutout_right > sa.cutout_left) {
        int pad = fs / 2;
        bool clash = !(cx + tw + pad <= sa.cutout_left || cx >= sa.cutout_right + pad);
        if (clash) {
            if (sa.cutout_left >= tw + pad) cx = sa.cutout_left - pad - tw;
            else if (l.view_w - sa.cutout_right >= tw + pad) cx = sa.cutout_right + pad;
            else return;
        }
    }
    gfx_text(title, cx, ty, fs, COL_TITLE);
}

static void counter_text(char* buf, size_t n, int value) {
    if (value < 0) snprintf(buf, n, "-%02d", -value > 99 ? 99 : -value);
    else           snprintf(buf, n, "%03d", value > 999 ? 999 : value);
}

// Mines left on the left, the face button in the middle, the clock on the
// right, over the width of the grid.
static void draw_hud(const Game* g, Layout l, bool pressing) {
    gfx_rect(0, l.titlebar_h + 1, l.view_w, l.hud_y + l.hud_h + l.margin / 2 - l.titlebar_h - 1,
             COL_HUD);
    int fs = l.hud_fs * 4 / 3;
    int y = l.hud_y + (l.hud_h - fs) / 2;
    char buf[8];

    counter_text(buf, sizeof buf, g->mines - g->flags_placed);
    gfx_text(buf, l.board_x, y, fs, COL_COUNTER);

    counter_text(buf, sizeof buf, g->timer_frames / 60);
    gfx_text(buf, l.board_x + l.board_w - gfx_measure_text(buf, fs), y, fs, COL_COUNTER);

    gfx_rect_rounded(l.face_x, l.face_y, l.face_size, l.face_size, 0.2f, COL_FACE_BG);
    gfx_rect_rounded_lines(l.face_x, l.face_y, l.face_size, l.face_size, 0.2f, COL_BORDER);
    const char* face;
    if (g->phase == PHASE_WON)       face = "B)";
    else if (g->phase == PHASE_LOST) face = "X(";
    else if (pressing)               face = ":O";
    else                             face = ":)";
    text_centered(face, l.face_x + l.face_size / 2, l.face_y + l.face_size / 2,
                  l.face_size / 2, COL_COUNTER);
}

// --------------------------------------------------------------------------
// Scenes
// --------------------------------------------------------------------------
typedef struct {
    const Game* g;
    bool pressing;
    const char* notice;   // end-of-game title, or NULL
} BoardCtx;

static MenuTheme menu_theme(void) {
    MenuTheme t = { .background = BLACK, .panel = (Color){15, 15, 25, 255},
                    .edge = LIGHTGRAY, .title = WHITE, .item = GRAY,
                    .selected = YELLOW };
    return t;
}

static void draw_board_scene(void* vctx, int view_w, int view_h) {
    BoardCtx* ctx = (BoardCtx*)vctx;
    const Game* g = ctx->g;
    Layout l = layout_for(view_w, view_h, g->cols, g->rows);
    if (view_w == GetScreenWidth() && view_h == GetScreenHeight()) s_cell = l.cell;

    gfx_clear(COL_BG);
    draw_title_bar(l);
    draw_hud(g, l, ctx->pressing);
    bool show_cursor = !s_touch_ui && g->phase != PHASE_WON && g->phase != PHASE_LOST;
    for (int r = 0; r < g->rows; r++)
        for (int c = 0; c < g->cols; c++)
            draw_cell(g, l, c, r, show_cursor);

    if (ctx->notice) {
        MenuTheme t = menu_theme();
        menu_draw_notice(&t, view_w, view_h, ctx->notice,
                         s_touch_ui ? "Tap to continue" : "Press any key");
    }
}

// --------------------------------------------------------------------------
// Public entry points
// --------------------------------------------------------------------------
void render_init(void) {
    window_init(GAME_NAME);
    present_init();
}

void render_cleanup(void) {
    present_cleanup();
    window_close();
}

void render_frame(const Game* g, bool pressing) {
    BoardCtx ctx = { g, pressing, NULL };
    present(draw_board_scene, &ctx);
}

void render_won(const Game* g) {
    BoardCtx ctx = { g, false, "YOU WIN" };
    present(draw_board_scene, &ctx);
}

void render_lost(const Game* g) {
    BoardCtx ctx = { g, false, "GAME OVER" };
    present(draw_board_scene, &ctx);
}

void render_menu(const char* title, const char* const* labels, int count,
                 int selected, int gap_before) {
    MenuTheme t = menu_theme();
    menu_show(&t, title, labels, count, selected, gap_before);
}

bool render_cell_at(const Game* g, int x, int y, int* col, int* row) {
    return layout_cell_at(live_layout(g), x, y, col, row);
}

bool render_face_hit(const Game* g, int x, int y) {
    return layout_face_hit(live_layout(g), x, y);
}

int render_board_top(const Game* g)    { return live_layout(g).board_y; }
bool render_transposed(const Game* g)  { return live_layout(g).transposed; }
int render_cell_size(void)             { return s_cell; }
