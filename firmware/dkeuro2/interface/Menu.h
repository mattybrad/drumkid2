#pragma once
#include "hardware/Leds.h"
#include "hardware/Memory.h"
#include "hardware/CardReader.h"
#include "audio/KitManager.h"
#include "Config.h"

enum class MenuState {
    HOME,
    TAP_TEMPO,
    LIVE_EDIT,
    STEP_EDIT,
    BEAT_SELECT,
    MANUAL_TEMPO,
    TUPLET_SELECT,
    SUBMENU_SELECTING,
    SUBMENU_SELECTED,
    KIT_SELECT,
    TIME_SIGNATURE_SELECT,
};

enum class SubMenuState {
    TEST_1,
    TEST_2,
    TEST_3,
    TEST_4,
    LOAD
};

class Menu {
    public:
        void init(Leds* leds, Memory* memory, CardReader* cardReader, KitManager* kitManager);
        void handleButtonPress(int16_t buttonIndex);
        
        private:
        Leds* _leds = nullptr;
        Memory* _memory = nullptr;
        CardReader* _cardReader = nullptr;
        KitManager* _kitManager = nullptr;

        MenuState _state = MenuState::HOME;
        SubMenuState _subMenuState = SubMenuState::TEST_1;
        uint16_t _selectedKit = 0;

        void _handleButtonHome(int16_t buttonIndex);
        void _handleButtonKitSelect(int16_t buttonIndex);
        void _handleButtonSubMenuSelecting(int16_t buttonIndex);
        void _handleButtonLoad(int16_t buttonIndex);
};