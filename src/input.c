#include "input.h"
#include "render.h"
#include "platform.h"
#if !defined(PLATFORM_IOS)
#include <raylib.h>  // keyboard/mouse; iOS is touch-only (queries come from plat_ios)
#endif

// input_poll() composes up to two sources into one Input:
//   - mouse + keyboard: desktop native builds and the web build (PC browsers)
//   - touch:            Android, iOS, and the web build (mobile browsers)
// The web build runs both, so a phone uses taps while a desktop browser uses
// the mouse -- same binary. Android and iOS run only touch; desktop native runs
// only mouse + keyboard.

#if !defined(PLATFORM_ANDROID) && !defined(PLATFORM_IOS)
static void poll_mouse_keyboard(Input* in) {
    Vector2 mp = GetMousePosition();
    in->mouse_x = (int)mp.x;
    in->mouse_y = (int)mp.y;

    in->left_clicked   = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in->right_clicked  = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in->middle_clicked = IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
    in->left_held      = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in->right_held     = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

    // Keyboard movement (held)
    in->move_left  = IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A);
    in->move_right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    in->move_up    = IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W);
    in->move_down  = IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S);

    // Alt+Enter = fullscreen, never a reveal or a menu select
    bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    in->fullscreen_toggle = alt && IsKeyPressed(KEY_ENTER);

    // Edge-triggered actions
    in->reveal_pressed = IsKeyPressed(KEY_SPACE)
                      || (IsKeyPressed(KEY_ENTER) && !in->fullscreen_toggle);
    in->flag_pressed   = IsKeyPressed(KEY_F);
    in->escape_pressed = IsKeyPressed(KEY_ESCAPE);

    // Menu navigation: Up/Down (or W/S) move, Left/Right (or A/D) cycle an
    // Options value, Enter (not Alt+Enter) or Space selects.
    in->menu_up    = IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W);
    in->menu_down  = IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S);
    in->menu_left  = IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A);
    in->menu_right = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D);
    in->select_pressed = (IsKeyPressed(KEY_ENTER) && !in->fullscreen_toggle)
                      || IsKeyPressed(KEY_SPACE);

    in->any_pressed = in->left_clicked || in->right_clicked || in->reveal_pressed
                   || in->flag_pressed || in->escape_pressed || in->menu_up
                   || in->menu_down    || in->select_pressed;
}
#endif // !PLATFORM_ANDROID && !PLATFORM_IOS

#ifdef OS_TOUCH
// Recognizer state for the current touch sequence (first finger down to last
// finger up), kept in one module-owned value so the hidden state is explicit
// and resettable (tests/test_input.c).
typedef struct {
    Vector2 last_pos;  // last single-finger position (source of tap coords)
    bool    active;    // a touch sequence is in progress
    Vector2 origin;    // where the sequence started (px)
    double  t0;        // sequence start time (s)
    int     max_np;    // most simultaneous fingers seen during the sequence
    bool    moved;     // travelled past the tap slop: a swipe, not a tap
    bool    held;      // the long press already fired for this sequence
} TouchState;

static TouchState s_touch;

// A finger resting this long in place flags the cell under it, the way every
// mobile minesweeper works; it fires while the finger is still down so the
// player feels it land. Anything shorter that lifts in place is a tap.
#define LONG_PRESS_S     0.40
#define TWO_FINGER_MAX_S 0.5

// Touch source: one-finger taps (reveal, chord, menu rows), press and hold
// (flag), two-finger tap (menu), and swipes (menu navigation). Only ever sets
// fields true, so it composes over the mouse source on web without clobbering
// it. Rendering is at native resolution, so touch points map 1:1 to the grid.
static void poll_touch(Input* in) {
    int n = GetTouchPointCount();
    Vector2 pts[8];
    int np = 0;
    for (int i = 0; i < n && np < 8; i++) pts[np++] = GetTouchPosition(i);

    double now = GetTime();

    // Movement tolerance: most of a cell, so a tap that rolls a little still
    // lands on the cell it started on, but a swipe is never read as a tap.
    float slop = (float)render_cell_size() * 0.6f;
    if (slop < 10.0f) slop = 10.0f;

    if (np > 0) {
        Vector2 p = pts[0];
        if (!s_touch.active) {
            s_touch.active = true;
            s_touch.origin = p;
            s_touch.last_pos = p;
            s_touch.t0 = now;
            s_touch.max_np = 0;
            s_touch.moved = false;
            s_touch.held = false;
        }
        if (np > s_touch.max_np) s_touch.max_np = np;
        // pts[0] can jump when a second finger lands or lifts, so position is
        // only tracked while the sequence is still single-finger.
        if (s_touch.max_np < 2) {
            float dx = p.x - s_touch.origin.x, dy = p.y - s_touch.origin.y;
            if (dx * dx + dy * dy > slop * slop) s_touch.moved = true;
            s_touch.last_pos = p;
            in->touch_down = !s_touch.moved;
            if (!s_touch.moved && !s_touch.held && now - s_touch.t0 >= LONG_PRESS_S) {
                s_touch.held = true;
                in->long_press = true;
                in->press_x = s_touch.origin.x;
                in->press_y = s_touch.origin.y;
            }
        }
    } else if (s_touch.active) {
        // Touch ended: decide the tap on RELEASE (raylib's GESTURE_TAP fires on
        // touch-down, before a swipe or a hold can be ruled out).
        double dur = now - s_touch.t0;
        if (s_touch.max_np >= 2) {
            if (dur < TWO_FINGER_MAX_S) {
                in->escape_pressed = true;
                in->any_pressed = true;
            }
        } else if (!s_touch.moved && !s_touch.held) {
            in->touch_tap = true;
            in->tap_x = s_touch.last_pos.x;
            in->tap_y = s_touch.last_pos.y;
            in->any_pressed = true;
        }
        s_touch.active = false;
    }

    // Swipes drive menu navigation: up / down move the highlight, left / right
    // cycle an Options value.
    int g = GetGestureDetected();
    if (g == GESTURE_SWIPE_UP)    in->menu_up    = true;
    if (g == GESTURE_SWIPE_DOWN)  in->menu_down  = true;
    if (g == GESTURE_SWIPE_LEFT)  in->menu_left  = true;
    if (g == GESTURE_SWIPE_RIGHT) in->menu_right = true;

#if !defined(PLATFORM_IOS)
    // Android hardware/gesture Back button (KEY_BACK); harmless no-op on web.
    if (IsKeyPressed(KEY_BACK)) {
        in->escape_pressed = true;
        in->any_pressed    = true;
    }
#endif
}
#endif // OS_TOUCH

Input input_poll(void) {
    Input in = {0};
#if !defined(PLATFORM_ANDROID) && !defined(PLATFORM_IOS)
    poll_mouse_keyboard(&in);   // desktop native + web (PC browsers)
#endif
#ifdef OS_TOUCH
    poll_touch(&in);            // Android + iOS + web (mobile browsers)
#endif
    return in;
}
