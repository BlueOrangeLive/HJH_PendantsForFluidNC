// Copyright (c) 2023 Mitch Bradley
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.

#include "Config.h"

#include "Scene.h"
#include "ConfirmScene.h"
#include "e4math.h"

extern Scene helpScene;
extern Scene fileSelectScene;

static const char* help_text[] = { "Jog",
                                   "Touch:",
                                   "Top/Bottom: choose axis",
                                   "Left/Right: set digit"
                                   "",
                                   "Turn: jog by digit",
                                   "Red/Grn: hold to jog",
                                   NULL };

class MultiJogScene : public Scene {
private:
    int       _dist_index[3] = { -1, -1, -1 };
    int       max_index() { return 6 - num_digits(); }  // 10^3 = 1000;
    int       min_index() { return -num_digits(); }     // 10^3 = 1000;
    int       _selected_mask = 1 << 0;
    const int num_axes       = 3;

public:
    MultiJogScene() : Scene("Jog", 4) {}

    e4_t distance(int axis) { return e4_power10(_dist_index[axis]); }
    void unselect_all() { _selected_mask = 0; }
    bool selected(int axis) { return _selected_mask & (1 << axis); }
    bool only(int axis) { return _selected_mask == (1 << axis); }

    int  next(int axis) { return (axis < 2) ? axis + 1 : 0; }
    void select(int axis) { _selected_mask |= 1 << axis; }
    void unselect(int axis) { _selected_mask &= ~(1 << axis); }

    int the_selected_axis() {
        if ((_selected_mask & (_selected_mask - 1)) != 0) {
            return -2;  // Multiple axes are selected
        }
        for (size_t axis = 0; axis < num_axes; axis++) {
            if (selected(axis)) {
                return axis;
            }
        }
        return -1;  // No axis is selected
    }

    void reDisplay() {
        background();
        drawPngBackground("/jogbg.png");

        drawMenuTitle(current_scene->name());

        DRO dro(30, 68, 200, 32);
        for (size_t axis = 0; axis < num_axes; axis++) {
            dro.draw(axis, 3 + _dist_index[axis], selected(axis));
        }
        drawButtonLegends("Jog-", "Jog+", "Zero");
        refreshDisplay();
    }
    void zero_axes() {
        std::string cmd = "G10L20P0";
        for (int axis = 0; axis < num_axes; axis++) {
            if (selected(axis)) {
                cmd += axisNumToChar(axis);
                cmd += "0";
            }
        }
        send_line(cmd.c_str());
    }
    void onEntry(void* arg) {
        if (arg) {
            dbg_printf("Entry %s\n", (const char*)arg);
        }

        if (arg && strcmp((const char*)arg, "Confirmed") == 0) {
            zero_axes();
        }
    }

    int which(int x, int y) {
        if (y > 130) {
            return 2;
        }
        return y > 90 ? 1 : 0;
    }

    void confirm_zero_axes() {
        std::string confirmMsg("Zero ");

        for (int axis = 0; axis < num_axes; axis++) {
            if (selected(axis)) {
                confirmMsg += axisNumToChar(axis);
            }
        }
        confirmMsg += " ?";
        dbg_println(confirmMsg.c_str());
        push_scene(&confirmScene, (void*)confirmMsg.c_str());
    }
    void increment_distance(int axis) {
        if (_dist_index[axis] < max_index()) {
            ++_dist_index[axis];
        }
    }
    void increment_distance() {
        for (int axis = 0; axis < num_axes; axis++) {
            if (selected(axis)) {
                increment_distance(axis);
            }
        }
    }
    void decrement_distance(int axis) {
        if (_dist_index[axis] > min_index()) {
            --_dist_index[axis];
        }
    }

    void decrement_distance() {
        for (int axis = 0; axis < num_axes; axis++) {
            if (selected(axis)) {
                decrement_distance(axis);
            }
        }
    }
    void rotate_distance() {
        for (int axis = 0; axis < num_axes; axis++) {
            if (selected(axis)) {
                if (++_dist_index[axis] >= max_index()) {
                    _dist_index[axis] = 0;
                }
            }
        }
    }
    void cancel_jog() {
        dbg_println("Cancel");
        fnc_realtime(JogCancel);
    }
    void next_axis() {
        int the_axis = the_selected_axis();
        if (the_axis == -2) {
            unselect_all();
            select(num_axes - 1);
            return;
        }
        if (the_axis == -1) {
            select(num_axes - 1);
            return;
        }
        unselect(the_axis);
        if (++the_axis == num_axes) {
            the_axis = 0;
        }
        select(the_axis);
    }
    void prev_axis() {
        int the_axis = the_selected_axis();
        if (the_axis == -2) {
            unselect_all();
            select(0);
            return;
        }
        if (the_axis == -1) {
            select(0);
            return;
        }
        unselect(the_axis);
        if (--the_axis < 0) {
            the_axis = num_axes - 1;
        }
        select(the_axis);
    }
    void touch_top() {
        prev_axis();
        reDisplay();
    }
    void touch_bottom() {
        next_axis();
        reDisplay();
    }
    void touch_left() {
        increment_distance();
        reDisplay();
    }
    void touch_right() {
        decrement_distance();
        reDisplay();
    }

    void onTouchRelease(int x, int y) {
        // Convert from screen coordinates to 0,0 in the center
        Point ctr = Point { x, y }.from_display();

        x = ctr.x;
        y = ctr.y;

        int center_radius = display.width() / 6;

        if ((x * x + y * y) < (center_radius * center_radius)) {
            // Center of screen
            push_scene(&helpScene, (void*)help_text);
            return;
        }

        // Sense touches at top, bottom, left, and right
        if (std::abs(y) > std::abs(x)) {
            if (y > 0) {
                touch_top();
            } else {
                touch_bottom();
            }
        } else {
            if (x > 0) {
                touch_right();
            } else {
                touch_left();
            }
        }
    }
    void onTouchHold(int x, int y) {
        // Select multiple axes
        if (x < 80) {
            int axis = which(x, y);
            if (selected(axis) && !only(axis)) {
                unselect(axis);
            } else {
                select(axis);
            }
            reDisplay();
            return;
        }
#if 0
        if (x < 160 && y > 160) {
            confirm_zero_axes();
        }
#endif
    }

    void onRightFlick() override { activate_scene(&fileSelectScene); }

    void onDialButtonPress() { zero_axes(); }

    void start_mpg_jog(int delta) {
        // e.g. $J=G91F1000X-10000
        std::string cmd(inInches ? "$J=G91F400" : "$J=G91F10000");
        for (int axis = 0; axis < num_axes; ++axis) {
            if (selected(axis)) {
                cmd += axisNumToChar(axis);
                cmd += e4_to_cstr(delta * distance(axis), inInches ? 3 : 2);
            }
        }
        send_line(cmd.c_str());
    }
    void start_button_jog(bool negative) {
        // e.g. $J=G91F1000X-10000
        e4_t total_distance = 0;
        int  n_axes         = 0;
        for (int axis = 0; axis < num_axes; ++axis) {
            if (selected(axis)) {
                total_distance = e4_magnitude(total_distance, distance(axis));
                ++n_axes;
            }
        }

        e4_t feedrate = total_distance * 300;  // go 5x the highlighted distance in 1 second

        std::string cmd("$J=G91");
        cmd += inInches ? "G20" : "G21";
        cmd += "F";
        cmd += e4_to_cstr(feedrate, 3);
        for (int axis = 0; axis < num_axes; ++axis) {
            if (selected(axis)) {
                e4_t axis_distance;
                if (n_axes == 1) {
                    axis_distance = e4_from_int(inInches ? 200 : 5000);
                } else {
                    axis_distance = distance(axis) * 20;
                }
                if (negative) {
                    axis_distance = -axis_distance;
                }
                cmd += axisNumToChar(axis);
                cmd += e4_to_cstr(axis_distance, 0);
            }
        }
        send_line(cmd.c_str());
    }

    void onGreenButtonPress() {
        if (state == Idle) {
            start_button_jog(false);
        }
    }
    void onGreenButtonRelease() { cancel_jog(); }
    void onRedButtonPress() {
        if (state == Idle) {
            start_button_jog(true);
        }
    }
    void onRedButtonRelease() { cancel_jog(); }

    void onEncoder(int delta) {
        if (delta != 0) {
            start_mpg_jog(delta);
        }
    }

    void onDROChange() { reDisplay(); }
    void onLimitsChange() { reDisplay(); }
    void onAlarm() { reDisplay(); }
    void onExit() { cancel_jog(); }
} multiJogScene;
