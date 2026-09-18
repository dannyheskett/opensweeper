// Tests for the touch recognizer in input.c: a tap is decided on release, a
// press held in place flags once while the finger is still down (and is then
// not also a tap), a drag is neither, a two-finger tap opens the menu, and
// swipes move the menu. Compiled -DPLATFORM_IOS, the raylib-free configuration
// input.c supports: os_types.h supplies the types and declares the touch and
// clock queries, and this file provides scripted fakes of them. Frames advance
// at exactly 60 Hz.
#define PLATFORM_IOS 1
#include "../src/input.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PASS(name) printf("PASS: %s\n", name)
#define FAIL(name, msg) do { fprintf(stderr, "FAIL: %s -- %s\n", name, msg); exit(1); } while (0)

static double  fake_now;
static int     fake_np;
static Vector2 fake_pts[8];
static int     fake_gesture;

double  GetTime(void)            { return fake_now; }
int     GetTouchPointCount(void) { return fake_np; }
Vector2 GetTouchPosition(int i)  { return fake_pts[i]; }
int     GetGestureDetected(void) { return fake_gesture; }
int     render_cell_size(void)   { return 40; }   // => 24px tap slop

#define DT (1.0 / 60.0)

static void reset(void) {
    memset(&s_touch, 0, sizeof s_touch);
    fake_now = 100.0;
    fake_np = 0;
    fake_gesture = 0;
}

static Input frame(void) { fake_now += DT; Input in = input_poll(); fake_gesture = 0; return in; }
static void down(float x, float y) { fake_np = 1; fake_pts[0] = (Vector2){ x, y }; }
static void up(void) { fake_np = 0; }

static void test_tap(void) {
    reset();
    down(100, 200);
    Input in = frame();
    if (in.touch_tap) FAIL("tap", "a tap fired before the finger lifted");
    if (!in.touch_down) FAIL("tap", "the finger was not reported down");
    frame();
    up();
    in = frame();
    if (!in.touch_tap) FAIL("tap", "a still contact was not a tap");
    if (in.tap_x != 100 || in.tap_y != 200) FAIL("tap", "the tap reported the wrong point");
    if (in.long_press) FAIL("tap", "a short tap flagged");
    PASS("tap");
}

static void test_long_press_flags_once(void) {
    reset();
    down(50, 60);
    int presses = 0;
    for (int i = 0; i < 60; i++) {           // a full second in place
        Input in = frame();
        if (in.long_press) {
            presses++;
            if (in.press_x != 50 || in.press_y != 60) FAIL("hold", "wrong point");
            if (i < (int)(LONG_PRESS_S * 60) - 1) FAIL("hold", "fired too early");
        }
    }
    if (presses != 1) FAIL("hold", "a held press must flag exactly once");
    up();
    Input in = frame();
    if (in.touch_tap) FAIL("hold", "a held press was also read as a tap");
    PASS("hold");
}

static void test_drag_is_nothing(void) {
    reset();
    down(100, 100);
    frame();
    down(160, 100);                          // well past the slop
    for (int i = 0; i < 40; i++) {
        Input in = frame();
        if (in.long_press) FAIL("drag", "a drag flagged");
    }
    up();
    Input in = frame();
    if (in.touch_tap) FAIL("drag", "a drag was read as a tap");
    PASS("drag");
}

static void test_wobble_is_still_a_tap(void) {
    reset();
    down(100, 100);
    frame();
    down(110, 108);                          // inside the slop
    frame();
    up();
    if (!frame().touch_tap) FAIL("wobble", "a small wobble was not forgiven");
    PASS("wobble");
}

static void test_two_finger_tap_opens_menu(void) {
    reset();
    fake_np = 2;
    fake_pts[0] = (Vector2){ 100, 100 };
    fake_pts[1] = (Vector2){ 200, 100 };
    frame();
    up();
    Input in = frame();
    if (!in.escape_pressed) FAIL("two_finger", "a two-finger tap did not open the menu");
    if (in.touch_tap) FAIL("two_finger", "a two-finger tap was also a tap");
    PASS("two_finger");
}

static void test_swipes_move_menu(void) {
    reset();
    fake_gesture = GESTURE_SWIPE_UP;
    if (!frame().menu_up) FAIL("swipe", "swipe up did not move the menu");
    fake_gesture = GESTURE_SWIPE_RIGHT;
    if (!frame().menu_right) FAIL("swipe", "swipe right did not cycle a value");
    PASS("swipe");
}

int main(void) {
    test_tap();
    test_long_press_flags_once();
    test_drag_is_nothing();
    test_wobble_is_still_a_tap();
    test_two_finger_tap_opens_menu();
    test_swipes_move_menu();
    printf("All input tests passed.\n");
    return 0;
}
