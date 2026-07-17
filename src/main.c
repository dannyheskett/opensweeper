#include "game.h"
#include "render.h"
#include "input.h"
#include "sound.h"
#include "recorder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

typedef enum {
    STATE_MENU,
    STATE_PLAYING,
    STATE_WON,
    STATE_LOST,
} AppState;

typedef enum {
    ACT_NEW,
    ACT_DIFF,
    ACT_MARKS,
    ACT_SOUND,
    ACT_RECORD,
    ACT_EXIT,
} MenuAction;

#define MAX_MENU_ITEMS 8

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

// Builds the menu and reports where the visual gap goes (-1 for none), so
// menu composition and layout stay decided in one place.
static int build_menu(Difficulty diff, bool marks, const char** labels, MenuAction* actions,
                      int* gap_before) {
    static char diff_label[32];
    snprintf(diff_label, sizeof(diff_label), "Difficulty: %s", diff_names[diff]);
    int n = 0;
    *gap_before = -1;
    labels[n] = "New Game";                              actions[n++] = ACT_NEW;
    labels[n] = diff_label;                              actions[n++] = ACT_DIFF;
    labels[n] = marks ? "Marks (?): On" : "Marks (?): Off"; actions[n++] = ACT_MARKS;
    labels[n] = sound_is_enabled() ? "Sound: On" : "Sound: Off"; actions[n++] = ACT_SOUND;
#ifndef PLATFORM_WEB
    // No mp4 recorder in the browser, and a tab can't exit itself.
    labels[n] = recorder_active() ? "Record: On" : "Record: Off"; actions[n++] = ACT_RECORD;
    *gap_before = n;
    labels[n] = "Exit";                                  actions[n++] = ACT_EXIT;
#endif
    return n;
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
#ifdef PLATFORM_WEB
    // Fixed-timestep accumulator: the browser paces frames with
    // requestAnimationFrame (display refresh), but the timer and DAS count
    // frames at an assumed 60 Hz.
    double prev_time;
    double acc;
#endif
} AppCtx;

// How many 60 Hz logic ticks this frame represents. Native runs at a fixed 60
// fps (SetTargetFPS), so it's always exactly one; on web the rAF-paced loop
// converts elapsed wall time into whole ticks. A long gap (hidden tab, load
// hitch) counts as a pause rather than banked catch-up, but short slowdowns
// are processed in full so the game timer stays honest on slow devices (a
// tick is just counter updates — cheap).
static int logic_ticks(AppCtx* c) {
#ifdef PLATFORM_WEB
    double now = GetTime();
    double dt = (c->prev_time > 0.0) ? now - c->prev_time : 1.0 / 60.0;
    c->prev_time = now;
    if (dt > 0.25) dt = 0.25;
    c->acc += dt;
    int ticks = (int)(c->acc * 60.0);
    c->acc -= ticks / 60.0;
    return ticks;
#else
    (void)c;
    return 1;
#endif
}

// One iteration of the game loop. `arg` is an AppCtx* (void* to match the
// emscripten_set_main_loop callback signature).
static void frame_step(void* arg) {
    AppCtx* c = (AppCtx*)arg;

    int ticks = logic_ticks(c);
    Input in = input_poll();
    if (in.fullscreen_toggle) render_toggle_fullscreen();

    const char* labels[MAX_MENU_ITEMS];
    MenuAction actions[MAX_MENU_ITEMS];
    int gap_before;
    int menu_count = build_menu(c->diff, c->marks, labels, actions, &gap_before);
    if (c->selected >= menu_count) c->selected = 0;

    switch (c->state) {
    case STATE_MENU:
#ifndef PLATFORM_WEB
        if (in.escape_pressed) { c->quit = true; return; }
#endif
        if (in.menu_up) {
            c->selected = (c->selected + menu_count - 1) % menu_count;
            sound_play(SFX_MENU_MOVE);
        }
        if (in.menu_down) {
            c->selected = (c->selected + 1) % menu_count;
            sound_play(SFX_MENU_MOVE);
        }
        if (in.select_pressed) {
            sound_play(SFX_MENU_SELECT);
            switch (actions[c->selected]) {
            case ACT_NEW:
                if (c->game) game_destroy(c->game);
                c->game = game_create(c->diff, c->marks);
                if (recorder_active()) { recorder_stop(); recorder_start(NULL); }
                c->state = STATE_PLAYING;
                break;
            case ACT_DIFF:
                c->diff = (Difficulty)((c->diff + 1) % 3);
                break;
            case ACT_MARKS:
                c->marks = !c->marks;
                if (c->game) c->game->marks_enabled = c->marks;
                break;
            case ACT_SOUND:
                sound_toggle();
                sound_play(SFX_MENU_SELECT);
                break;
            case ACT_RECORD:
                recorder_toggle();
                break;
            case ACT_EXIT:
                c->quit = true;
                return;
            }
        }
        break;

    case STATE_PLAYING: {
        if (!c->game) break;
        if (in.escape_pressed) { c->state = STATE_MENU; c->selected = 0; break; }

        Game* game = c->game;
        game_frame_begin(game);

        // Mouse input
        int cx, cy;
        bool over_cell = render_cell_at(in.mouse_x, in.mouse_y, game, &cx, &cy);
        bool over_face = render_face_hit(in.mouse_x, in.mouse_y);

        if (over_face && in.left_clicked) {
            game_destroy(game);
            c->game = game_create(c->diff, c->marks);
            if (recorder_active()) { recorder_stop(); recorder_start(NULL); }
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

        // Keyboard cursor movement (DAS) — frame-counted at 60 Hz
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
    // loss); logic_ticks() keeps game speed at 60 Hz on any refresh rate.
    emscripten_set_main_loop_arg(frame_step, &ctx, 0, 1);
#else
    while (!render_window_should_close() && !ctx.quit) {
        frame_step(&ctx);
    }
    recorder_stop();
    if (ctx.game) game_destroy(ctx.game);
    sound_shutdown();
    render_cleanup();
#endif
    return 0;
}
