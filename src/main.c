#include "game.h"
#include "render.h"
#include "input.h"
#include "sound.h"
#include "recorder.h"
#include "tick.h"
#include "menu.h"
#include "window.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

typedef enum {
    STATE_MENU,
    STATE_OPTIONS,
    STATE_PLAYING,
    STATE_WON,
    STATE_LOST,
} AppState;

typedef enum {
    ACT_RESUME,
    ACT_NEW,
    ACT_OPTIONS,
    ACT_SOUND,
    ACT_RECORD,
    ACT_EXIT,
} MenuAction;

// Upper bound on labels[]/actions[]: one slot per MenuAction. Each action
// appears at most once, so build_menu can never overflow.
#define MAX_MENU_ITEMS 6

static const char* diff_names[3] = {"Beginner", "Intermediate", "Expert"};

static void play_event_sounds(unsigned events) {
    if (events & EV_DETONATE) { sound_play(SFX_DETONATE); return; }
    if (events & EV_WIN)       { sound_play(SFX_WIN); return; }
    if (events & EV_CHORD)     sound_play(SFX_CHORD);
    else if (events & EV_REVEAL) sound_play(SFX_REVEAL);
    if (events & EV_FLAG)      sound_play(SFX_FLAG);
    if (events & EV_UNFLAG)    sound_play(SFX_UNFLAG);
    if (events & EV_QUESTION)  sound_play(SFX_FLAG);
}

// Fill labels[]/actions[] with the current menu. Returns the item count and
// sets *gap_before to the index that should have a blank line above it -- Exit,
// which is set apart from the rest -- or -1 when this build has no Exit item at
// all (web, where the browser tab owns the lifecycle).
static int build_menu(bool resumable, const char** labels, MenuAction* actions,
                      int* gap_before) {
    int n = 0;
    *gap_before = -1;
    if (resumable) { labels[n] = "Resume Game";                     actions[n++] = ACT_RESUME; }
    labels[n] = "New Game";                                         actions[n++] = ACT_NEW;
    labels[n] = "Options";                                          actions[n++] = ACT_OPTIONS;
    labels[n] = sound_is_enabled() ? "Sound: On" : "Sound: Off";    actions[n++] = ACT_SOUND;
#ifndef PLATFORM_WEB
    // The mp4 recorder is a desktop-only feature (stubbed out on web), and a
    // browser tab can't be closed from code, so neither appears there.
    labels[n] = recorder_active()  ? "Record: On" : "Record: Off";  actions[n++] = ACT_RECORD;
    *gap_before = n;
    labels[n] = "Exit";                                             actions[n++] = ACT_EXIT;
#endif
    return n;
}

// The Options screen: difficulty and question-mark marks, plus Back. Values
// cycle with Left/Right, or by selecting the row; the last item returns to the
// menu. A new difficulty applies from the next New Game.
#define OPT_ITEMS 3
enum { OPT_DIFF, OPT_MARKS, OPT_BACK };

static int build_options(Difficulty diff, bool marks, const char** labels) {
    static char buf[OPT_ITEMS][32];
    snprintf(buf[OPT_DIFF], sizeof buf[0], "Difficulty: %s", diff_names[diff]);
    snprintf(buf[OPT_MARKS], sizeof buf[0], "Marks (?): %s", marks ? "On" : "Off");
    snprintf(buf[OPT_BACK], sizeof buf[0], "Back");
    for (int i = 0; i < OPT_ITEMS; i++) labels[i] = buf[i];
    return OPT_ITEMS;
}

// DAS for keyboard cursor movement
#define DAS_DELAY  20  // frames before auto-repeat kicks in
#define DAS_REPEAT  6  // frames between auto-repeat moves

typedef struct { int counter; int dir; } Das;

static bool das_tick(Das* d, bool held) {
    if (!held) { d->counter = 0; d->dir = 0; return false; }
    if (d->counter == 0) { d->counter = DAS_DELAY; return true; }
    d->counter--;
    if (d->counter == 0) { d->counter = DAS_REPEAT; return true; }
    return false;
}

// App state carried across frames. Kept in one struct so the web build can drive
// the loop from an emscripten per-frame callback (browsers can't block).
typedef struct {
    Game* game;
    AppState state;
    int selected;
    Difficulty diff;
    bool marks;
    bool quit;
    Das das_l, das_r, das_u, das_d;
    SimClock clock;    // fixed-timestep accumulator (only advanced while playing)
    double prev_time;  // GetTime() at the previous frame; 0 before the first
} AppCtx;

static void cycle_option(AppCtx* c, int item, int dir) {
    if (item == OPT_DIFF) {
        c->diff = (Difficulty)((c->diff + 3 + dir) % 3);
    } else if (item == OPT_MARKS) {
        c->marks = !c->marks;
        if (c->game) c->game->marks_enabled = c->marks;
    }
}

static void start_new_game(AppCtx* c) {
    if (c->game) game_destroy(c->game);
    c->game = game_create(c->diff, c->marks);
    if (recorder_active()) { recorder_stop(); recorder_start(NULL); }
    c->state = STATE_PLAYING;
}

// A menu row picked by the pointer: a mouse click.
static bool menu_pointer(const Input* in, Vector2* p) {
    if (in->left_clicked) {
        *p = (Vector2){(float)in->mouse_x, (float)in->mouse_y};
        return true;
    }
    return false;
}

// One iteration of the game loop. `arg` is an AppCtx* (void* to match the
// emscripten_set_main_loop callback signature).
static void frame_step(void* arg) {
    AppCtx* c = (AppCtx*)arg;

    // Real seconds since the previous frame, feeding the fixed-timestep
    // accumulator so the timer and DAS count 60 Hz frames on any display
    // refresh. The first frame (prev_time == 0) is treated as exactly one step.
    double now = GetTime();
    double dt = (c->prev_time > 0.0) ? now - c->prev_time : SIM_DT;
    c->prev_time = now;
    if (c->state != STATE_PLAYING) sim_clock_reset(&c->clock);

    // Sampled every frame, not only while playing, so a stale "was focused"
    // cannot survive a menu visit and fire on the first frame of the next game.
    bool focus_lost = window_focus_lost();

    Input in = input_poll();
    if (in.fullscreen_toggle) window_toggle_fullscreen();

    bool resumable = (c->game != NULL && c->game->phase != PHASE_WON
                      && c->game->phase != PHASE_LOST);
    const char* labels[MAX_MENU_ITEMS];
    MenuAction actions[MAX_MENU_ITEMS];
    int gap_before = -1;
    int menu_count = build_menu(resumable, labels, actions, &gap_before);

    switch (c->state) {
    case STATE_MENU: {
        if (c->selected >= menu_count) c->selected = 0;
        if (in.escape_pressed) {
            // Escape backs out: resume a game in progress, else quit (native).
            if (resumable) { c->state = STATE_PLAYING; break; }
#ifndef PLATFORM_WEB
            c->quit = true; return;
#else
            break;
#endif
        }
        if (in.menu_up) {
            c->selected = (c->selected + menu_count - 1) % menu_count;
            sound_play(SFX_MENU_MOVE);
        }
        if (in.menu_down) {
            c->selected = (c->selected + 1) % menu_count;
            sound_play(SFX_MENU_MOVE);
        }
        // A click on a row chooses it; a keyboard select activates the
        // highlighted row.
        bool do_select = in.select_pressed;
        Vector2 p;
        if (menu_pointer(&in, &p)) {
            int hit = menu_hit_test(p);
            if (hit >= 0 && hit < menu_count) { c->selected = hit; do_select = true; }
        }
        if (do_select) {
            sound_play(SFX_MENU_SELECT);
            switch (actions[c->selected]) {
            case ACT_RESUME:  c->state = STATE_PLAYING; break;
            case ACT_NEW:     start_new_game(c); break;
            case ACT_OPTIONS: c->state = STATE_OPTIONS; c->selected = 0; break;
            case ACT_SOUND:   sound_toggle(); sound_play(SFX_MENU_SELECT); break;
            case ACT_RECORD:  recorder_toggle(); break;
            case ACT_EXIT:    c->quit = true; return;
            }
        }
        break;
    }

    case STATE_OPTIONS: {
        const char* opt_labels[OPT_ITEMS];
        int opt_count = build_options(c->diff, c->marks, opt_labels);
        if (c->selected >= opt_count) c->selected = 0;
        if (in.escape_pressed) { c->state = STATE_MENU; c->selected = 0; break; }
        if (in.menu_up) {
            c->selected = (c->selected + opt_count - 1) % opt_count;
            sound_play(SFX_MENU_MOVE);
        }
        if (in.menu_down) {
            c->selected = (c->selected + 1) % opt_count;
            sound_play(SFX_MENU_MOVE);
        }
        int dir = (in.menu_right ? 1 : 0) - (in.menu_left ? 1 : 0);
        bool do_select = in.select_pressed;
        Vector2 p;
        if (menu_pointer(&in, &p)) {
            int hit = menu_hit_test(p);
            if (hit >= 0 && hit < opt_count) { c->selected = hit; do_select = true; }
        }
        if (do_select && c->selected == OPT_BACK) {
            c->state = STATE_MENU;
            c->selected = 0;
            sound_play(SFX_MENU_SELECT);
        } else if (dir != 0 || do_select) {
            cycle_option(c, c->selected, dir ? dir : 1);
            sound_play(SFX_MENU_SELECT);
        }
        break;
    }

    case STATE_PLAYING: {
        if (!c->game) { c->state = STATE_MENU; break; }
        // Losing focus (tab hidden, window deactivated) returns to the menu;
        // the game stays resumable.
        if (focus_lost) { c->state = STATE_MENU; c->selected = 0; break; }
        if (in.escape_pressed) { c->state = STATE_MENU; c->selected = 0; break; }

        Game* game = c->game;
        game_frame_begin(game);
        int ticks = sim_clock_advance(&c->clock, dt);

        // Mouse input
        int cx, cy;
        bool over_cell = render_cell_at(in.mouse_x, in.mouse_y, game, &cx, &cy);
        bool over_face = render_face_hit(in.mouse_x, in.mouse_y);

        if (over_face && in.left_clicked) {
            start_new_game(c);
            break;
        }

        if (over_cell) {
            game->cursor_x = cx;
            game->cursor_y = cy;
            if (in.left_clicked) {
                game_reveal(game, cx, cy);
            } else if (in.right_clicked) {
                game_flag(game, cx, cy);
            } else if (in.middle_clicked) {
                game_chord(game, cx, cy);
            } else if (in.left_held && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                // Left+right chord
                game_chord(game, cx, cy);
            }
        }

        // Keyboard cursor movement (DAS), counted in fixed 60 Hz steps
        for (int t = 0; t < ticks; t++) {
            if (das_tick(&c->das_l, in.move_left)  && game->cursor_x > 0)            game->cursor_x--;
            if (das_tick(&c->das_r, in.move_right) && game->cursor_x < game->cols-1) game->cursor_x++;
            if (das_tick(&c->das_u, in.move_up)    && game->cursor_y > 0)            game->cursor_y--;
            if (das_tick(&c->das_d, in.move_down)  && game->cursor_y < game->rows-1) game->cursor_y++;
        }

        // Keyboard actions
        if (in.reveal_pressed) game_reveal(game, game->cursor_x, game->cursor_y);
        if (in.flag_pressed)   game_flag(game, game->cursor_x, game->cursor_y);

        for (int t = 0; t < ticks; t++) game_tick_timer(game);
        play_event_sounds(game->events);

        if (game->phase == PHASE_WON) {
            c->state = STATE_WON;
        } else if (game->phase == PHASE_LOST) {
            c->state = STATE_LOST;
        }
        break;
    }

    case STATE_WON:
    case STATE_LOST:
        if (in.any_pressed && !in.fullscreen_toggle) {
            c->state = STATE_MENU;
            c->selected = 0;
        }
        break;
    }

    // Render
    if (c->state == STATE_MENU) {
        render_menu("OPENSWEEPER", labels, menu_count, c->selected, gap_before);
    } else if (c->state == STATE_OPTIONS) {
        const char* opt_labels[OPT_ITEMS];
        int opt_count = build_options(c->diff, c->marks, opt_labels);
        render_menu("OPTIONS", opt_labels, opt_count, c->selected, OPT_BACK);
    } else if (c->state == STATE_WON) {
        render_won(c->game);
    } else if (c->state == STATE_LOST) {
        render_lost(c->game);
    } else {
        render_frame(c->game);
    }
}

int main(int argc, char** argv) {
    srand((unsigned int)time(NULL));

    bool cli_record = false;
    const char* cli_record_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--record") == 0) {
            cli_record = true;
            if (i+1 < argc && argv[i+1][0] != '-') cli_record_path = argv[++i];
        }
    }

    render_init();
    sound_init();
    if (cli_record) recorder_start(cli_record_path);

    // Static so the pointer handed to emscripten stays valid after main()'s
    // stack is unwound on the web build (see the PLATFORM_WEB branch below).
    // Everything not named here starts zeroed (MENU state, no game, DAS idle).
    static AppCtx ctx = { .diff = DIFF_INTERMEDIATE, .marks = true };

#ifdef PLATFORM_WEB
    // Browsers drive the loop via a per-frame callback; with the infinite-loop
    // flag this call does not return, so the native cleanup below never runs on
    // web (the browser tab owns the lifetime). fps=0 paces with
    // requestAnimationFrame — rendering the preserveDrawingBuffer canvas
    // outside rAF forces compositor readbacks (GPU stalls, eventual context
    // loss); the fixed-timestep clock keeps game speed at 60 Hz on any refresh.
    emscripten_set_main_loop_arg(frame_step, &ctx, 0, 1);
#else
    while (!window_should_close() && !ctx.quit) {
        frame_step(&ctx);
    }
    recorder_stop();
    if (ctx.game) game_destroy(ctx.game);
    sound_shutdown();
    render_cleanup();
#endif
    return 0;
}
