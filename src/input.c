#include "input.h"
#include <raylib.h>

Input input_poll(void) {
    Input in = {0};

    Vector2 mp = GetMousePosition();
    in.mouse_x = (int)mp.x;
    in.mouse_y = (int)mp.y;

    in.left_clicked   = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in.right_clicked  = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in.middle_clicked = IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
    in.left_held      = IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    // Keyboard movement (held)
    in.move_left  = IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A);
    in.move_right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    in.move_up    = IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W);
    in.move_down  = IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S);

    // Edge-triggered actions
    in.reveal_pressed = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER);
    in.flag_pressed   = IsKeyPressed(KEY_F);
    in.escape_pressed = IsKeyPressed(KEY_ESCAPE);

    // Alt+Enter = fullscreen
    bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    in.fullscreen_toggle = alt && IsKeyPressed(KEY_ENTER);
    if (in.fullscreen_toggle) in.reveal_pressed = false;

    // Menu navigation (up/down arrow or W/S)
    in.menu_up   = IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W);
    in.menu_down = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
    in.select_pressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    in.any_pressed = in.left_clicked || in.right_clicked || in.reveal_pressed
                  || in.flag_pressed || in.escape_pressed || in.menu_up
                  || in.menu_down    || in.select_pressed;

    return in;
}
