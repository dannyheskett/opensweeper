#ifndef OPENSWEEPER_INPUT_H
#define OPENSWEEPER_INPUT_H

#include <stdbool.h>

typedef struct {
    // Mouse
    int  mouse_x, mouse_y;
    bool left_clicked;
    bool right_clicked;
    bool middle_clicked;
    bool left_held;      // for face button :-O animation

    // Keyboard cursor movement (held)
    bool move_left, move_right, move_up, move_down;

    // Keyboard actions (edge-triggered)
    bool reveal_pressed;    // Space or Enter
    bool flag_pressed;      // F
    bool escape_pressed;
    bool fullscreen_toggle; // Alt+Enter

    // Menu navigation
    bool menu_up, menu_down;
    bool select_pressed;    // Enter or Space in menu
    bool any_pressed;
} Input;

Input input_poll(void);

#endif
