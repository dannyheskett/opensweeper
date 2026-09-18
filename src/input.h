#ifndef OPENSWEEPER_INPUT_H
#define OPENSWEEPER_INPUT_H

#include <stdbool.h>

typedef struct {
    // Mouse (desktop, and desktop browsers)
    int  mouse_x, mouse_y;
    bool left_clicked;
    bool right_clicked;
    bool middle_clicked;
    bool left_held;      // for the face button's :O while a cell is pressed
    bool right_held;     // left + right together chords

    // Keyboard cursor movement (held)
    bool move_left, move_right, move_up, move_down;

    // Keyboard actions (edge-triggered)
    bool reveal_pressed;    // Space or Enter
    bool flag_pressed;      // F
    bool escape_pressed;    // Escape, a two-finger tap, or Android Back
    bool fullscreen_toggle; // Alt+Enter

    // Menu navigation
    bool menu_up, menu_down;
    bool menu_left, menu_right; // cycle a value on the Options screen
    bool select_pressed;    // Enter (not Alt+Enter) or Space in menu
    bool any_pressed;

    // Touch (phones, tablets, touch browsers). touch_tap is a completed
    // one-finger tap; long_press fires once, while the finger is still down,
    // when it has rested in place long enough to flag. Both report where.
    bool  touch_tap;
    float tap_x, tap_y;
    bool  long_press;
    float press_x, press_y;
    bool  touch_down;       // a finger is on the screen (the face's :O)
} Input;

Input input_poll(void);

#endif
