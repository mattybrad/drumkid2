#pragma once
#include <array>
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
    INPUT_PPQN_SELECT,
    OUTPUT_PPQN_SELECT,
    KIT_LOAD_FOLDER_SELECT,
    KIT_LOAD_SLOT_SELECT,
    KIT_SELECT,
    TIME_SIGNATURE_SELECT,
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
        uint8_t _subMenuIndex = 0;
        uint16_t _kitLoadFolderIndex = 0;
        uint16_t _kitLoadSlot = 0;
        std::array<MenuState, 3> _subMenuStates = {
            MenuState::INPUT_PPQN_SELECT,
            MenuState::OUTPUT_PPQN_SELECT,
            MenuState::KIT_LOAD_FOLDER_SELECT,
        };

        void _handleButtonHome(int16_t buttonIndex);
        void _handleButtonKitSelect(int16_t buttonIndex);
        void _handleButtonSubMenuSelecting(int16_t buttonIndex);
        void _handleButtonKitLoadFolderSelect(int16_t buttonIndex);
        void _handleButtonKitLoadSlotSelect(int16_t buttonIndex);
        void _updateDisplay();
};