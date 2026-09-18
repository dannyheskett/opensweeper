#include "render.h"
#include "gfx.h"
#include "menu.h"
#include "present.h"
#include "window.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>

#define CELL_SIZE    32
#define GRID_PAD     16
#define TOPBAR_H     48
#define STATSBAR_H   80
#define HEADER_H     (TOPBAR_H + STATSBAR_H)

// Colors
static const Color COL_BG         = {20,  20,  20,  255};
static const Color COL_TOPBAR     = {30,  30,  30,  255};
static const Color COL_TITLE      = {200, 200, 200, 255};
static const Color COL_STATSBAR   = {25,  25,  25,  255};
static const Color COL_CELL_UNREV = {35,  35,  35,  255};
static const Color COL_CELL_REV   = {20,  20,  20,  255};
static const Color COL_BORDER     = {60,  60,  60,  255};
static const Color COL_CURSOR     = {180, 180, 180, 255};
static const Color COL_MINE_BG    = {220, 60,  60,  255};
static const Color COL_MINE       = {200, 200, 200, 255};
static const Color COL_FLAG       = {240, 200, 40,  255};
static const Color COL_QUESTION   = {0,   200, 240, 255};
static const Color COL_COUNTER    = {240, 200, 40,  255};
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

// Canvas is always CANVAS_W x CANVAS_H (Expert native size, defined in render.h).
// grid_off_x/y are updated each frame based on the current game's grid dimensions
// so that smaller grids are centered in the fixed canvas.
static RenderTexture2D canvas;
static int grid_off_x = GRID_PAD;
static int grid_off_y = HEADER_H + GRID_PAD;

static void update_grid_offset(const Game* g) {
    grid_off_x = (CANVAS_W - g->cols * CELL_SIZE) / 2;
    grid_off_y = HEADER_H + (CANVAS_H - HEADER_H - g->rows * CELL_SIZE) / 2;
}

// Scale that fits the canvas in a view: smaller when the view is smaller than
// the canvas, never larger (the board is a fixed size, like the other games'
// fixed desktop boards).
static float canvas_scale_for(int view_w, int view_h) {
    float sx = (float)view_w / CANVAS_W, sy = (float)view_h / CANVAS_H;
    float s = sx < sy ? sx : sy;
    return s < 1.0f ? s : 1.0f;
}

static void win_to_canvas(int mx, int my, int* cx, int* cy) {
    int ww = GetScreenWidth(), wh = GetScreenHeight();
    float s = canvas_scale_for(ww, wh);
    float off_x = (ww - CANVAS_W * s) * 0.5f;
    float off_y = (wh - CANVAS_H * s) * 0.5f;
    *cx = (int)((mx - off_x) / s);
    *cy = (int)((my - off_y) / s);
}

// The scene present() draws: the finished canvas, fitted to the view and
// centred on black. The recorder's pass is exactly the canvas size.
static void canvas_scene(void* ctx, int view_w, int view_h) {
    (void)ctx;
    float s = canvas_scale_for(view_w, view_h);
    Rectangle src = {0, 0, (float)CANVAS_W, -(float)CANVAS_H};
    Rectangle dst = {(view_w - CANVAS_W * s) * 0.5f, (view_h - CANVAS_H * s) * 0.5f,
                     CANVAS_W * s, CANVAS_H * s};
    gfx_clear(BLACK);
    DrawTexturePro(canvas.texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
}

static void present_canvas(void) {
    present(canvas_scene, NULL);
}

void render_init(void) {
    window_init(GAME_NAME);
    present_init();
    canvas = LoadRenderTexture(CANVAS_W, CANVAS_H);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
}

void render_cleanup(void) {
    UnloadRenderTexture(canvas);
    present_cleanup();
    window_close();
}

bool render_cell_at(int mx, int my, const Game* g, int* cx, int* cy) {
    int cmx, cmy;
    win_to_canvas(mx, my, &cmx, &cmy);
    int rx = cmx - grid_off_x;
    int ry = cmy - grid_off_y;
    if (rx < 0 || ry < 0) return false;
    int col = rx / CELL_SIZE;
    int row = ry / CELL_SIZE;
    if (col >= g->cols || row >= g->rows) return false;
    *cx = col; *cy = row;
    return true;
}

// Face button: 40x40, centered horizontally in the stats bar (always same position)
static Rectangle face_rect(void) {
    return (Rectangle){CANVAS_W/2 - 20, TOPBAR_H + STATSBAR_H/2 - 20, 40, 40};
}

bool render_face_hit(int mx, int my) {
    int cmx, cmy;
    win_to_canvas(mx, my, &cmx, &cmy);
    Rectangle r = face_rect();
    return cmx >= r.x && cmx < r.x+r.width && cmy >= r.y && cmy < r.y+r.height;
}

// --------------------------------------------------------------------------
// Draw helpers
// --------------------------------------------------------------------------
static void draw_text_centered(const char* text, int x, int y, int w, int fs, Color col) {
    int tw = gfx_measure_text(text, fs);
    gfx_text(text, x + (w - tw) / 2, y, fs, col);
}

static void draw_mine(int px, int py, Color bg) {
    DrawRectangle(px + 13, py +  4,  6, 24, COL_MINE);  // vertical arm
    DrawRectangle(px +  4, py + 13, 24,  6, COL_MINE);  // horizontal arm
    DrawRectangle(px +  9, py +  9, 14, 14, COL_MINE);  // body
    DrawRectangle(px + 10, py + 10,  5,  5, COL_WHITE); // highlight
    (void)bg;
}

static void draw_flag(int px, int py) {
    DrawRectangle(px + 17, py +  5,  2, 22, COL_MINE);  // pole
    DrawRectangle(px +  8, py +  5,  9,  3, COL_FLAG);  // flag row 1
    DrawRectangle(px + 10, py +  8,  7,  3, COL_FLAG);  // flag row 2
    DrawRectangle(px + 12, py + 11,  5,  3, COL_FLAG);  // flag row 3
    DrawRectangle(px + 10, py + 27, 12,  2, COL_MINE);  // base
}

static void draw_cell(int col, int row, const Game* g) {
    int px = grid_off_x + col * CELL_SIZE;
    int py = grid_off_y + row * CELL_SIZE;
    const Cell* c = &g->cells[row][col];

    bool is_cursor    = (col == g->cursor_x && row == g->cursor_y);
    bool is_detonated = (col == g->detonated_x && row == g->detonated_y);

    if (!c->revealed) {
        DrawRectangle(px, py, CELL_SIZE, CELL_SIZE, COL_CELL_UNREV);
        Color bdr = is_cursor ? COL_CURSOR : COL_BORDER;
        DrawRectangleLines(px, py, CELL_SIZE, CELL_SIZE, bdr);

        if (c->flagged) {
            if (g->phase == PHASE_LOST && !c->mine) {
                DrawRectangle(px+1, py+1, CELL_SIZE-2, CELL_SIZE-2, COL_CELL_UNREV);
                draw_text_centered("X", px, py + 6, CELL_SIZE, 20, COL_WRONG_FLAG);
            } else {
                draw_flag(px, py);
            }
        } else if (c->question) {
            draw_text_centered("?", px, py + 6, CELL_SIZE, 20, COL_QUESTION);
        }
    } else {
        Color bg = is_detonated ? COL_MINE_BG : COL_CELL_REV;
        DrawRectangle(px, py, CELL_SIZE, CELL_SIZE, bg);
        DrawRectangleLines(px, py, CELL_SIZE, CELL_SIZE, COL_BORDER);

        if (c->mine) {
            draw_mine(px, py, bg);
        } else if (c->adj > 0) {
            char num[2] = {(char)('0' + c->adj), 0};
            draw_text_centered(num, px, py + 6, CELL_SIZE, 20, NUM_COLORS[c->adj]);
        }
    }
}

static void draw_counter(int x, int y, int value, int digits) {
    char buf[8];
    if (value < 0)
        snprintf(buf, sizeof(buf), "-%0*d", digits-1, -value > 99 ? 99 : -value);
    else
        snprintf(buf, sizeof(buf), "%0*d", digits, value > 999 ? 999 : value);
    int fs = 24;
    int tw = gfx_measure_text(buf, fs);
    gfx_text(buf, x - tw/2, y, fs, COL_COUNTER);
}

static void draw_face(bool left_held, GamePhase phase) {
    Rectangle r = face_rect();
    DrawRectangleRec(r, (Color){60,55,20,255});
    DrawRectangleLinesEx(r, 1, COL_BORDER);

    const char* face;
    if (phase == PHASE_WON)        face = "B)";
    else if (phase == PHASE_LOST)  face = "X(";
    else if (left_held)            face = ":O";
    else                           face = ":)";

    int fs = 20;
    int tw = gfx_measure_text(face, fs);
    gfx_text(face, (int)r.x + ((int)r.width - tw)/2, (int)r.y + 10, fs, COL_COUNTER);
}

// --------------------------------------------------------------------------
// Public render calls
// --------------------------------------------------------------------------
static void draw_game_to_canvas(const Game* g) {
    BeginTextureMode(canvas);
    ClearBackground(COL_BG);

    // Header — full canvas width, same regardless of difficulty
    DrawRectangle(0, 0, CANVAS_W, TOPBAR_H, COL_TOPBAR);
    draw_text_centered("OPENSWEEPER", 0, (TOPBAR_H - 20) / 2, CANVAS_W, 20, COL_TITLE);

    DrawRectangle(0, TOPBAR_H, CANVAS_W, STATSBAR_H, COL_STATSBAR);

    int stats_mid_y = TOPBAR_H + (STATSBAR_H - 24) / 2;
    draw_counter(CANVAS_W / 4,     stats_mid_y, g->mines - g->flags_placed, 3);
    draw_counter(CANVAS_W * 3 / 4, stats_mid_y, g->timer_frames / 60, 3);

    bool showing_held = IsMouseButtonDown(MOUSE_BUTTON_LEFT)
                        && g->phase != PHASE_WON && g->phase != PHASE_LOST;
    draw_face(showing_held, g->phase);

    DrawLine(0, HEADER_H, CANVAS_W, HEADER_H, COL_BORDER);

    // Grid — centered in the space below the header
    update_grid_offset(g);
    for (int row = 0; row < g->rows; row++)
        for (int col = 0; col < g->cols; col++)
            draw_cell(col, row, g);

    EndTextureMode();
}

void render_frame(const Game* g) {
    draw_game_to_canvas(g);
    present_canvas();
}

// The family menu (menu.c) in this game's colours.
static MenuTheme menu_theme(void) {
    MenuTheme t = { .background = BLACK, .panel = (Color){15, 15, 25, 255},
                    .edge = LIGHTGRAY, .title = WHITE, .item = GRAY,
                    .selected = YELLOW };
    return t;
}

void render_menu(const char* title, const char* const* labels, int count,
                 int selected, int gap_before) {
    MenuTheme t = menu_theme();
    menu_show(&t, title, labels, count, selected, gap_before);
}

// The end-of-game notice (menu.c) over the board, in canvas coordinates.
static void draw_overlay(const Game* g, const char* title) {
    draw_game_to_canvas(g);
    BeginTextureMode(canvas);
    MenuTheme t = menu_theme();
    menu_draw_notice(&t, CANVAS_W, CANVAS_H, title, "Press any key");
    EndTextureMode();
    present_canvas();
}

void render_won(const Game* g)  { draw_overlay(g, "YOU WIN"); }
void render_lost(const Game* g) { draw_overlay(g, "GAME OVER"); }
