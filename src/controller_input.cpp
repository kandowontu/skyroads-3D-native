#include "controller_input.hpp"

namespace skyroads {
void ControllerInput::reset() {
    previous_buttons_=0;
    directions_={};
    repeat_at_={};
}
void ControllerInput::merge(const ControllerSample& sample, NativeScreen screen,
    std::uint64_t now, NativeInput& input) {
    if (!sample.connected) { reset(); return; }
    using namespace pad_button;
    const unsigned pressed=sample.buttons & ~previous_buttons_;
    previous_buttons_=sample.buttons;
    // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, plus hysteresis at release.
    const auto axis=[&](int value,unsigned i) {
        return value > (directions_[i] ? 6000 : 7849);
    };
    const std::array<bool,4> held{
        (sample.buttons&left)!=0 || axis(-sample.left_x,0),
        (sample.buttons&right)!=0 || axis(sample.left_x,1),
        (sample.buttons&up)!=0 || axis(sample.left_y,2),
        (sample.buttons&down)!=0 || axis(-sample.left_y,3)};
    const bool driving=screen==NativeScreen::Playing || screen==NativeScreen::Demo ||
        screen==NativeScreen::Kosmonaut;
    std::array<bool,4> action{};
    for(unsigned i=0;i<4;++i) {
        action[i]=held[i] && (driving || !directions_[i] || now>=repeat_at_[i]);
        if(held[i] && !directions_[i]) repeat_at_[i]=now+350;
        else if(action[i] && !driving) repeat_at_[i]=now+120;
    }
    directions_=held;
    input.left |= action[0]; input.right |= action[1];
    input.up |= action[2]; input.down |= action[3];
    input.gamepad_active = driving;
    if(driving) input.jump |= (sample.buttons&a)!=0;
    if(screen==NativeScreen::Intro) input.jump |= sample.buttons!=0;
    input.escape_pressed |= (pressed&(b|back))!=0;
    if(screen==NativeScreen::CustomLevelEditor) {
        input.editor_space_pressed |= (pressed&a)!=0;
        input.editor_play_pressed |= (pressed&x)!=0;
        input.editor_shape_pressed |= (pressed&y)!=0;
        input.editor_save_pressed |= (pressed&start)!=0;
        input.editor_page_up_pressed |= (pressed&lb)!=0;
        input.editor_page_down_pressed |= (pressed&rb)!=0;
        input.editor_view_pressed |= (pressed&left_stick)!=0;
    }
    else if(screen==NativeScreen::Kosmonaut) {
        input.enter_pressed |= (pressed&a)!=0;
        input.editor_play_pressed |= (pressed&start)!=0;
    }
    else if(driving) input.escape_pressed |= (pressed&start)!=0;
    else input.enter_pressed |= (pressed&(a|start))!=0;
}
}
