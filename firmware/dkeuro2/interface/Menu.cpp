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
        case MenuState::SUBMENU_SELECTED:
            switch(_subMenuState) {
                case SubMenuState::TEST_1:
                    printf("In TEST_1 submenu\n");
                    break;
                case SubMenuState::TEST_2:
                    printf("In TEST_2 submenu\n");
                    break;
                case SubMenuState::TEST_3:
                    printf("In TEST_3 submenu\n");
                    break;
                case SubMenuState::TEST_4:
                    printf("In TEST_4 submenu\n");
                    break;
                case SubMenuState::LOAD:
                    printf("In LOAD submenu\n");
                    _handleButtonLoad(buttonIndex);
                    break;
                default:
                    printf("Unhandled submenu state\n");
            }
            break;
        default:
            printf("Unhandled menu state\n");
            break;
    }
}

void Menu::_handleButtonHome(int16_t buttonIndex) {
    switch(buttonIndex) {
        case BUTTON_KIT:
            _state = MenuState::KIT_SELECT;
            printf("Entered KIT_SELECT state\n");
            char kitStr[2];
            kitStr[0] = '1' + _kitManager->kitNum;
            kitStr[1] = '\0';
            _leds->setDisplayString(kitStr);
            break;
        case BUTTON_MENU:
            _state = MenuState::SUBMENU_SELECTING;
            printf("Entered SUBMENU_SELECTING state\n");
            _subMenuState = SubMenuState::TEST_1; // Default to TEST for now
            _leds->setDisplayString("TEST");
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
            _leds->setDisplayString("HOME");
            break;
        case BUTTON_INC:
            _kitManager->kitNum = (_kitManager->kitNum + 1) % 3;
            printf("Selected kit %d\n", _kitManager->kitNum+1);
            char kitStr[2];
            kitStr[0] = '1' + _kitManager->kitNum;
            kitStr[1] = '\0';
            _leds->setDisplayString(kitStr);
            break;
        case BUTTON_DEC:
            _kitManager->kitNum = (_kitManager->kitNum - 1 + 3) % 3;
            printf("Selected kit %d\n", _kitManager->kitNum+1);
            char kitStrDec[2];
            kitStrDec[0] = '1' + _kitManager->kitNum;
            kitStrDec[1] = '\0';
            _leds->setDisplayString(kitStrDec);
            break;
        default:
            printf("Unhandled button in KIT_SELECT\n");
            break;
    }
}

void Menu::_handleButtonSubMenuSelecting(int16_t buttonIndex) {
    int currentSubMenu;
    char subMenuString[5];
    switch(buttonIndex) {
        case BUTTON_BACK:
            _state = MenuState::HOME;
            _leds->setDisplayString("HOME");
            break;
        case BUTTON_INC:
            currentSubMenu = static_cast<int>(_subMenuState);
            currentSubMenu = (currentSubMenu + 1) % 5; // temp
            _subMenuState = static_cast<SubMenuState>(currentSubMenu);
            printf("Selected submenu %d\n", currentSubMenu);
            subMenuString[0] = 'S';
            subMenuString[1] = 'u';
            subMenuString[2] = 'b';
            subMenuString[3] = '1' + currentSubMenu;
            subMenuString[4] = '\0';
            _leds->setDisplayString(subMenuString);
            break;
        case BUTTON_YES:
            _state = MenuState::SUBMENU_SELECTED;
            break;
        default:
            printf("Unhandled button in SUBMENU_SELECTING\n");
            break;
    }
}

void Menu::_handleButtonLoad(int16_t buttonIndex) {
    char subMenuString[5];
    const char* kitFolderName = nullptr;
    char testFolderString[5];
    int kitSize = 0;
    int freeSectors = 0;
    switch(buttonIndex) {
        case BUTTON_BACK:
            _state = MenuState::SUBMENU_SELECTING;
            subMenuString[0] = 'S';
            subMenuString[1] = 'u';
            subMenuString[2] = 'b';
            subMenuString[3] = '1' + static_cast<int>(_subMenuState);
            subMenuString[4] = '\0';
            _leds->setDisplayString(subMenuString);
            break;
        case BUTTON_INC:
            printf("Load: INC button pressed\n");
            _selectedKit = (_selectedKit + 1) % _cardReader->getNumSampleFolders();
            printf("Selected kit index: %d\n", _selectedKit);
            kitFolderName = _cardReader->getSampleFolderName(_selectedKit);
            strncpy(testFolderString, kitFolderName, 4);
            testFolderString[4] = '\0';
            _leds->setDisplayString(testFolderString);
            break;
        case BUTTON_YES:
            printf("Load: YES button pressed\n");
            // attempt to load the selected kit
            kitSize = _cardReader->getKitSize(_selectedKit);
            printf("Selected kit size: %d bytes\n", kitSize);
            freeSectors = _kitManager->getFreeSectors();
            printf("Free sectors available: %d\n", freeSectors);
            if(kitSize > freeSectors * FLASH_SECTOR_SIZE) {
                printf("Not enough free space to load kit %d\n", _selectedKit);
                // could add some UI feedback here to indicate failure
            } else {
                printf("Enough space for kit %d, choose slot...\n", _selectedKit);
                // switch to special state for choosing kit slot?
            }
            break;
        default:
            printf("Unhandled button in LOAD submenu\n");
            break;
    }
}