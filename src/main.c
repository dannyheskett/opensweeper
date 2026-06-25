#include "game.h"
#include "render.h"
#include "input.h"
#include "sound.h"
#include "recorder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

static int build_menu(Difficulty diff, bool marks, const char** labels, MenuAction* actions) {
    static char diff_label[32];
    snprintf(diff_label, sizeof(diff_label), "Difficulty: %s", diff_names[diff]);
    int n = 0;
    labels[n] = "New Game";                              actions[n++] = ACT_NEW;
    labels[n] = diff_label;                              actions[n++] = ACT_DIFF;
    labels[n] = marks ? "Marks (?): On" : "Marks (?): Off"; actions[n++] = ACT_MARKS;
    labels[n] = sound_is_enabled() ? "Sound: On" : "Sound: Off"; actions[n++] = ACT_SOUND;
    labels[n] = recorder_active() ? "Record: On" : "Record: Off"; actions[n++] = ACT_RECORD;
    labels[n] = "Exit";                                  actions[n++] = ACT_EXIT;
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

    Difficulty diff = DIFF_INTERMEDIATE;
    bool marks = true;

    render_init();
    sound_init();
    if (cli_record) recorder_start(cli_record_path);

    Game* game = NULL;
    AppState state = STATE_MENU;
    int selected = 0;

    // DAS state for cursor
    Das das_l = {0}, das_r = {0}, das_u = {0}, das_d = {0};

    while (!render_window_should_close()) {
        Input in = input_poll();
        if (in.fullscreen_toggle) render_toggle_fullscreen();

        const char* labels[MAX_MENU_ITEMS];
        MenuAction actions[MAX_MENU_ITEMS];
        int menu_count = build_menu(diff, marks, labels, actions);
        if (selected >= menu_count) selected = 0;

        switch (state) {
        case STATE_MENU:
            if (in.escape_pressed) goto quit;
            if (in.menu_up) {
                selected = (selected + menu_count - 1) % menu_count;
                sound_play(SFX_MENU_MOVE);
            }
            if (in.menu_down) {
                selected = (selected + 1) % menu_count;
                sound_play(SFX_MENU_MOVE);
            }
            if (in.select_pressed) {
                sound_play(SFX_MENU_SELECT);
                switch (actions[selected]) {
                case ACT_NEW:
                    if (game) game_destroy(game);
                    game = game_create(diff, marks);
                    if (recorder_active()) { recorder_stop(); recorder_start(NULL); }
                    state = STATE_PLAYING;
                    break;
                case ACT_DIFF:
                    diff = (Difficulty)((diff + 1) % 3);
                    break;
                case ACT_MARKS:
                    marks = !marks;
                    if (game) game->marks_enabled = marks;
                    break;
                case ACT_SOUND:
                    sound_toggle();
                    sound_play(SFX_MENU_SELECT);
                    break;
                case ACT_RECORD:
                    recorder_toggle();
                    break;
                case ACT_EXIT:
                    goto quit;
                }
            }
            break;

        case STATE_PLAYING: {
            if (!game) break;
            if (in.escape_pressed) { state = STATE_MENU; selected = 0; break; }

            game_frame_begin(game);

            // Mouse input
            int cx, cy;
            bool over_cell = render_cell_at(in.mouse_x, in.mouse_y, game, &cx, &cy);
            bool over_face = render_face_hit(in.mouse_x, in.mouse_y);

            if (over_face && in.left_clicked) {
                game_destroy(game);
                game = game_create(diff, marks);
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

            // Keyboard cursor movement (DAS)
            if (das_tick(&das_l, in.move_left)  && game->cursor_x > 0)            game->cursor_x--;
            if (das_tick(&das_r, in.move_right) && game->cursor_x < game->cols-1) game->cursor_x++;
            if (das_tick(&das_u, in.move_up)    && game->cursor_y > 0)            game->cursor_y--;
            if (das_tick(&das_d, in.move_down)  && game->cursor_y < game->rows-1) game->cursor_y++;

            // Keyboard actions
            if (in.reveal_pressed) game_reveal(game, game->cursor_x, game->cursor_y);
            if (in.flag_pressed)   game_flag(game, game->cursor_x, game->cursor_y);

            game_tick_timer(game);
            play_event_sounds(game->events);

            if (game->phase == PHASE_WON) {
                state = STATE_WON;
            } else if (game->phase == PHASE_LOST) {
                state = STATE_LOST;
            }
            break;
        }

        case STATE_WON:
        case STATE_LOST:
            if (in.any_pressed && !in.fullscreen_toggle) {
                state = STATE_MENU;
                selected = 0;
            }
            break;

        }

        // Render
        if (state == STATE_MENU) {
            render_menu("OPENSWEEPER", labels, menu_count, selected, menu_count-1);
        } else if (state == STATE_WON) {
            render_won(game);
        } else if (state == STATE_LOST) {
            render_lost(game);
        } else {
            render_frame(game);
        }
    }

quit:
    recorder_stop();
    if (game) game_destroy(game);
    sound_shutdown();
    render_cleanup();
    return 0;
}
