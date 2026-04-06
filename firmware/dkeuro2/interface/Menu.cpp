#include "Menu.h"
#include <stdio.h>

void Menu::init(Leds* leds, Memory* memory, CardReader* cardReader, KitManager* kitManager) {
    _leds = leds;
    _memory = memory;
    _cardReader = cardReader;
    _kitManager = kitManager;
    _leds->setDisplayString("HOME");
}

void Menu::handleButtonPress(int16_t buttonIndex) {
    printf("Menu handling button %d\n", buttonIndex);
    switch(_state) {
        case MenuState::HOME:
            _handleButtonHome(buttonIndex);
            break;
        case MenuState::KIT_SELECT:
            _handleButtonKitSelect(buttonIndex);
            break;
        case MenuState::SUBMENU_SELECTING:
            _handleButtonSubMenuSelecting(buttonIndex);
            break;
        case MenuState::KIT_LOAD_FOLDER_SELECT:
            _handleButtonKitLoadFolderSelect(buttonIndex);
            break;
        case MenuState::KIT_LOAD_SLOT_SELECT:
            _handleButtonKitLoadSlotSelect(buttonIndex);
            break;
        default:
            printf("Unhandled menu state\n");
            break;
    }
    _updateDisplay();
}

void Menu::_updateDisplay() {
    char displayStr[5];
    switch(_state) {
        case MenuState::HOME:
            _leds->setDisplayString("HOME");
            break;
        case MenuState::KIT_SELECT:
            char kitStr[2];
            kitStr[0] = '1' + _kitManager->kitNum;
            kitStr[1] = '\0';
            _leds->setDisplayString(kitStr);
            break;
        case MenuState::SUBMENU_SELECTING:
            switch(_subMenuStates[_subMenuIndex]) {
                case MenuState::INPUT_PPQN_SELECT:
                    _leds->setDisplayString("PPQI");
                    break;
                case MenuState::OUTPUT_PPQN_SELECT:
                    _leds->setDisplayString("PPQO");
                    break;
                case MenuState::KIT_LOAD_FOLDER_SELECT:
                    _leds->setDisplayString("LOAD");
                    break;
                default:
                    _leds->setDisplayString("????");
                    break;

            }
            break;
        case MenuState::KIT_LOAD_FOLDER_SELECT:
            strncpy(displayStr, _cardReader->getSampleFolderName(_kitLoadFolderIndex), 4);
            displayStr[4] = '\0';
            _leds->setDisplayString(displayStr);
            break;
        case MenuState::KIT_LOAD_SLOT_SELECT:
            char slotStr[2];
            slotStr[0] = '1' + _kitLoadSlot;
            slotStr[1] = '\0';
            _leds->setDisplayString(slotStr);
            break;
        default:
            _leds->setDisplayString("????");
            break;
    }
}

void Menu::_handleButtonHome(int16_t buttonIndex) {
    switch(buttonIndex) {
        case BUTTON_KIT:
            _state = MenuState::KIT_SELECT;
            break;
        case BUTTON_MENU:
            _state = MenuState::SUBMENU_SELECTING;
            break;
        default:
            printf("Unhandled button in HOME\n");
            break;
    }
}

void Menu::_handleButtonKitSelect(int16_t buttonIndex) {
    switch(buttonIndex) {
        case BUTTON_BACK:
            _state = MenuState::HOME;
            break;
        case BUTTON_INC:
            _kitManager->kitNum = (_kitManager->kitNum + 1) % 3;
            _kitManager->initKit(_kitManager->kitNum);
            printf("Selected kit %d\n", _kitManager->kitNum+1);
            break;
        case BUTTON_DEC:
            _kitManager->kitNum = (_kitManager->kitNum - 1 + 3) % 3;
            _kitManager->initKit(_kitManager->kitNum);
            printf("Selected kit %d\n", _kitManager->kitNum+1);
            break;
        default:
            printf("Unhandled button in KIT_SELECT\n");
            break;
    }
}

void Menu::_handleButtonSubMenuSelecting(int16_t buttonIndex) {
    switch(buttonIndex) {
        case BUTTON_BACK:
            _state = MenuState::HOME;
            break;
        case BUTTON_INC:
            _subMenuIndex = (_subMenuIndex + 1) % _subMenuStates.size();
            break;
        case BUTTON_YES:
            _state = _subMenuStates[_subMenuIndex];
            break;
        default:
            printf("Unhandled button in SUBMENU_SELECTING\n");
            break;
    }
}

void Menu::_handleButtonKitLoadFolderSelect(int16_t buttonIndex) {
    switch(buttonIndex) {
        case BUTTON_BACK:
            _state = MenuState::SUBMENU_SELECTING;
            break;
        case BUTTON_INC:
            printf("Load: INC button pressed\n");
            _kitLoadFolderIndex = (_kitLoadFolderIndex + 1) % _cardReader->getNumSampleFolders();
            printf("Selected kit index: %d\n", _kitLoadFolderIndex);
            break;
        case BUTTON_YES:
            printf("Load: YES button pressed\n");
            // attempt to load the selected kit
            _state = MenuState::KIT_LOAD_SLOT_SELECT;
            break;
        default:
            printf("Unhandled button in LOAD submenu\n");
            break;
    }
}

void Menu::_handleButtonKitLoadSlotSelect(int16_t buttonIndex) {
    int kitSizeSectors = 0;
    int freeSectors = 0;
    int newKitStartSector = 0;
    switch(buttonIndex) {
        case BUTTON_BACK:
            _state = MenuState::KIT_LOAD_FOLDER_SELECT;
            break;
        case BUTTON_INC:
            _kitLoadSlot = (_kitLoadSlot + 1) % MAX_KITS;
            break;
        case BUTTON_YES:
            // check kit size and free space, then load if possible
            kitSizeSectors = _cardReader->getKitSizeSectors(_kitLoadFolderIndex);
            freeSectors = _kitManager->getFreeSectors(_kitLoadSlot);
            if(kitSizeSectors > freeSectors) {
                printf("Not enough free space to load kit, need %d sectors but only %d sectors free\n", kitSizeSectors, freeSectors);
                return;
            }

            printf("Loading kit %d into slot %d\n", _kitLoadFolderIndex, _kitLoadSlot);
            _kitManager->createSpaceForKit(_kitLoadSlot, _cardReader->getKitSizeSectors(_kitLoadFolderIndex));
            newKitStartSector = SECTOR_AUDIO_DATA_START;
            for(int i=0; i<_kitLoadSlot; i++) {
                if(_kitManager->kits[i].sizeSectors > 0) {
                    newKitStartSector += _kitManager->kits[i].sizeSectors;
                }
            }
            _cardReader->transferAudioFolderToFlash(_cardReader->getSampleFolderName(_kitLoadFolderIndex), _kitLoadSlot, newKitStartSector);
            _kitManager->reloadMetaData();
            _kitManager->initKit(_kitLoadSlot);
            break;
        default:
            printf("Unhandled button in KIT_LOAD_SLOT_SELECT\n");
            break;
    }
}