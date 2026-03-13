#include "Menu.h"
#include <stdio.h>

void Menu::init(Leds* leds) {
    _leds = leds;
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
            kitStr[0] = '1' + tempKitNum;
            kitStr[1] = '\0';
            _leds->setDisplayString(kitStr);
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
            tempKitNum = (tempKitNum + 1) % 3;
            printf("Selected kit %d\n", tempKitNum+1);
            char kitStr[2];
            kitStr[0] = '1' + tempKitNum;
            kitStr[1] = '\0';
            _leds->setDisplayString(kitStr);
            break;
        case BUTTON_DEC:
            tempKitNum = (tempKitNum - 1 + 3) % 3;
            printf("Selected kit %d\n", tempKitNum+1);
            char kitStrDec[2];
            kitStrDec[0] = '1' + tempKitNum;
            kitStrDec[1] = '\0';
            _leds->setDisplayString(kitStrDec);
            break;
        default:
            printf("Unhandled button in KIT_SELECT\n");
            break;
    }
}