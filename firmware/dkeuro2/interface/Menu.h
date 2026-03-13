#pragma once
#include "hardware/Leds.h"
#include "Config.h"

enum class MenuState {
    HOME,
    TAP_TEMPO,
    LIVE_EDIT,
    STEP_EDIT,
    BEAT_SELECT,
    MANUAL_TEMPO,
    TUPLET_SELECT,
    SUBMENU_SELECT,
    KIT_SELECT,
    TIME_SIGNATURE_SELECT,
};

class Menu {
    public:
        void init(Leds* leds);
        void handleButtonPress(int16_t buttonIndex);
        int tempKitNum = 0;
        
        private:
        MenuState _state = MenuState::HOME;
        Leds* _leds = nullptr;
        void _handleButtonHome(int16_t buttonIndex);
        void _handleButtonKitSelect(int16_t buttonIndex);
        
};