#pragma once
#include "controller_input.hpp"
#include <windows.h>
#include <xinput.h>
#include <string>

namespace skyroads {
class XInputController {
public:
    XInputController() {
        wchar_t directory[MAX_PATH]{};
        const auto count=GetSystemDirectoryW(directory,MAX_PATH);
        if(!count || count>=MAX_PATH) return;
        // Load only from Windows' system directory. No dependency on the
        // separately installed DirectX redistributable or a bundled DLL.
        for(const auto* name : {L"xinput1_4.dll",L"xinput9_1_0.dll"}) {
            const auto path=std::wstring(directory)+L"\\"+name;
            module_=LoadLibraryW(path.c_str());
            if(!module_) continue;
            get_state_=reinterpret_cast<GetState>(GetProcAddress(module_,"XInputGetState"));
            if(get_state_) break;
            FreeLibrary(module_); module_=nullptr;
        }
    }
    ~XInputController() { if(module_) FreeLibrary(module_); }
    XInputController(const XInputController&)=delete;
    XInputController& operator=(const XInputController&)=delete;
    ControllerSample poll(std::uint64_t now) {
        if(!get_state_) return {};
        XINPUT_STATE state{};
        if(slot_<4 && get_state_(slot_,&state)==ERROR_SUCCESS) return sample(state);
        slot_=4;
        if(now<scan_at_) return {};
        scan_at_=now+1000;
        for(DWORD slot=0;slot<4;++slot) {
            if(get_state_(slot,&state)==ERROR_SUCCESS) {
                slot_=slot;
                return sample(state);
            }
        }
        return {};
    }
private:
    using GetState=DWORD (WINAPI*)(DWORD,XINPUT_STATE*);
    static ControllerSample sample(const XINPUT_STATE& state) {
        return {true,state.Gamepad.wButtons,state.Gamepad.sThumbLX,state.Gamepad.sThumbLY};
    }
    HMODULE module_{};
    GetState get_state_{};
    DWORD slot_{4};
    std::uint64_t scan_at_{};
};
}
