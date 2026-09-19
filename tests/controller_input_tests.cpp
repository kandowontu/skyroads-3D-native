#include "controller_input.hpp"
#ifdef _WIN32
#include "win32_xinput.hpp"
#endif
#include <iostream>
#include <stdexcept>

void require(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
int main(int argc,char** argv) {
    try {
        using namespace skyroads;
        ControllerInput controller;
        ControllerSample sample{true};
        auto read=[&](NativeScreen screen,std::uint64_t now) {
            NativeInput input;
            controller.merge(sample,screen,now,input);
            return input;
        };
        sample.left_x=7000;
        require(!read(NativeScreen::MainMenu,0).right,"Stick drift escaped the dead zone");
        sample.left_x=16000;
        require(read(NativeScreen::MainMenu,10).right,"Stick did not navigate");
        require(!read(NativeScreen::MainMenu,20).right,"Menu direction repeated immediately");
        require(read(NativeScreen::MainMenu,360).right,"Held menu direction did not repeat");
        require(!read(NativeScreen::MainMenu,400).right,"Menu repeat ignored its interval");
        sample.left_x=6500;
        require(read(NativeScreen::Playing,410).right,"Stick hysteresis released too early");
        sample.left_x=0;
        require(!read(NativeScreen::Playing,420).right,"Centered stick remained held");
        sample.buttons=pad_button::a|pad_button::up|pad_button::left;
        auto driving=read(NativeScreen::Playing,430);
        require(driving.jump&&driving.up&&driving.left&&driving.gamepad_active,
            "Controller did not provide simultaneous driving controls");
        require(read(NativeScreen::Playing,440).jump,"Held jump was treated as a one-shot");
        sample.buttons=0; read(NativeScreen::MainMenu,450);
        sample.buttons=pad_button::a;
        require(read(NativeScreen::MainMenu,460).enter_pressed,"A did not confirm");
        require(!read(NativeScreen::LevelSelection,470).enter_pressed,
            "Held A activated the next screen too");
        sample.buttons=pad_button::start;
        require(read(NativeScreen::Playing,480).escape_pressed,"Start did not leave the road");
        require(!read(NativeScreen::Playing,490).escape_pressed,"Start repeated");
        sample.buttons=0; read(NativeScreen::Kosmonaut,500);
        sample.buttons=pad_button::start;
        require(read(NativeScreen::Kosmonaut,510).editor_play_pressed,"Start did not pause Kosmonaut");
        sample.buttons=pad_button::a|pad_button::x|pad_button::y|pad_button::lb|pad_button::left_stick;
        const auto editor=read(NativeScreen::CustomLevelEditor,520);
        require(editor.editor_space_pressed&&editor.editor_play_pressed&&
            editor.editor_shape_pressed&&editor.editor_page_up_pressed&&editor.editor_view_pressed,
            "Editor shortcuts are incomplete");
        require(!editor.enter_pressed,"Editor paint also triggered Enter");
        sample.connected=false;
        const auto removed=read(NativeScreen::Playing,530);
        require(!removed.left&&!removed.jump&&!removed.gamepad_active,
            "Disconnected controller left an action held");
        sample={true,0,0,0};
        require(read(NativeScreen::Playing,540).gamepad_active,
            "Centered pad fell back to stale DOS joystick calibration");
        NativeInput keyboard; keyboard.right=true;
        controller.merge(sample,NativeScreen::Playing,550,keyboard);
        require(keyboard.right,"Controller erased keyboard input");
        if(argc>1) {
            RecoveredGame game{std::filesystem::path(argv[1])};
            NativeInput confirm; confirm.enter_pressed=true;
            game.timer_tick(confirm);
            game.timer_tick(confirm);
            for(int i=0;i<370;++i) game.timer_tick({});
            game.timer_tick(confirm);
            for(int i=0;i<370;++i) game.timer_tick({});
            require(game.screen()==NativeScreen::Playing,"Controller integration could not enter a road");
            sample={true,pad_button::up,0,0};
            for(int i=0;i<25;++i) game.timer_tick(read(game.screen(),600+i*6));
            require(game.forward_speed()>0,"XInput throttle did not reach the original simulation");
            const auto tick=game.gameplay_ticks();
            for(int i=0;i<10 && game.gameplay_ticks()==tick;++i) game.timer_tick({});
            sample.buttons=pad_button::start;
            game.timer_tick(read(game.screen(),1000));
            require(game.screen()==NativeScreen::Playing,"Exit latch test did not begin between gameplay updates");
            for(int i=0;i<5;++i) game.timer_tick({});
            require(game.screen()!=NativeScreen::Playing,"A short Start press was lost between game ticks");
        }
#ifdef _WIN32
        XInputController physical;
        const auto actual=physical.poll(GetTickCount64());
        std::cout<<"Windows XInput smoke test: "<<(actual.connected?"controller detected":"no controller attached")<<'\n';
#endif
        std::cout<<"Controller mapping, dead zone, repeats, disconnect, and editor tests passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
