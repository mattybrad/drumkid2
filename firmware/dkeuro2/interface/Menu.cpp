#include "Menu.h"
#include <stdio.h>

void Menu::init(Leds* leds, Memory* memory, CardReader* cardReader, KitManager* kitManager, Transport* transport) {
    _leds = leds;
    _memory = memory;
    _cardReader = cardReader;
    _kitManager = kitManager;
    _transport = transport;
    _leds->setDisplayString("HOME");
}

void Menu::handleButtonPress(int16_t buttonIndex) {
    printf("Menu handling button %d\n", buttonIndex);
    _handleButtonGeneral(buttonIndex);
    switch(_state) {
        case MenuState::MANUAL_TEMPO:
            _handleButtonManualTempo(buttonIndex);
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
        case MenuState::KIT_DELETE:
            _handleButtonKitDelete(buttonIndex);
            break;
        case MenuState::CHECK_SPACE:
            _handleButtonCheckSpace(buttonIndex);
            break;
        case MenuState::CLOCK_MODE_SELECT:
            _handleButtonClockModeSelect(buttonIndex);
            break;
        case MenuState::TIME_SIGNATURE_SELECT:
            _handleButtonTimeSignatureSelect(buttonIndex);
            break;
        default:
            printf("Unhandled menu state\n");
            break;
    }
    _updateDisplay();
}

void Menu::_updateDisplay() {
    char displayStr[5] = {0};
    switch(_state) {
        case MenuState::HOME:
            _leds->setDisplayString("HOME");
            break;
        case MenuState::MANUAL_TEMPO:
            printf("BPM: %d\n", _transport->getBpmQ16() >> 16);
            _leds->setDisplayNumberFP(_transport->getBpmQ16(), 1);
            break;
        case MenuState::KIT_SELECT:
            if(_kitManager->kits[_kitManager->kitNum].numSamples > 0) {
                strncpy(displayStr, _kitManager->kits[_kitManager->kitNum].name, 4);
                displayStr[4] = '\0';
            } else {
                snprintf(displayStr, 5, "na\0"); // untested, need to delete kits first!
            }
            _leds->setDisplayString(displayStr);
            break;
        case MenuState::SUBMENU_SELECTING:
            switch(_subMenuStates[_subMenuIndex]) {
                case MenuState::CLOCK_MODE_SELECT:
                    _leds->setDisplayString("CLK");
                    break;
                case MenuState::KIT_LOAD_FOLDER_SELECT:
                    _leds->setDisplayString("LOAD");
                    break;
                case MenuState::INPUT_PPQN_SELECT:
                    _leds->setDisplayString("PPQI");
                    break;
                case MenuState::OUTPUT_PPQN_SELECT:
                    _leds->setDisplayString("PPQO");
                    break;
                case MenuState::KIT_DELETE:
                    _leds->setDisplayString("DEL");
                    break;
                case MenuState::CHECK_SPACE:
                    _leds->setDisplayString("FREE");
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
            displayStr[0] = '1' + _kitLoadSlot;
            displayStr[1] = '\0';
            _leds->setDisplayString(displayStr);
            break;
        case MenuState::KIT_DELETE:
            displayStr[0] = '1' + _kitLoadSlot;
            displayStr[1] = '\0';
            _leds->setDisplayString(displayStr);
            break;
        case MenuState::CHECK_SPACE:
        {
            uint32_t freeSectors = _kitManager->getFreeSectors(MAX_KITS); // pass invalid kit slot to get total free sectors
            uint32_t freePercentage = (freeSectors * 100) / ((1023 - SECTOR_AUDIO_DATA_START) + 1);
            printf("Free sectors: %d (%d%%)\n", freeSectors, freePercentage);
            if(freePercentage > 100) {
                snprintf(displayStr, 5, "err");
            } else {
                snprintf(displayStr, 5, "%d", freePercentage);
            }
            _leds->setDisplayString(displayStr);
            break;
        }
        case MenuState::CLOCK_MODE_SELECT:
        {
            uint32_t clockMode = _transport->getClockMode();
            if(clockMode == MODE_CLOCK_INTERNAL) {
                _leds->setDisplayString("INT");
            } else if(clockMode == MODE_CLOCK_EXTERNAL) {
                _leds->setDisplayString("EXT");
            } else {
                _leds->setDisplayString("????");
            }
            break;
        }
        case MenuState::TIME_SIGNATURE_SELECT:
        {
            // todo - handle two-digit numerators at least (e.g. 13/8)
            Transport::TimeSignature ts = _transport->getTimeSignature();
            snprintf(displayStr, 5, "%d-%d ", ts.numerator, ts.denominator);
            _leds->setDisplayString(displayStr);
            break;
        }
        default:
            _leds->setDisplayString("????");
            break;
    }
}

void Menu::_handleButtonGeneral(int16_t buttonIndex) {
    switch(buttonIndex) {
        case BUTTON_PLAY:
            _transport->toggleStartStop();
            break;
        case BUTTON_TAP:
            _transport->handleTapTempoPulse();
            _state = MenuState::TAP_TEMPO;
            break;
        case BUTTON_LIVE:
            _state = MenuState::LIVE_EDIT;
            break;
        case BUTTON_STEP:
            _state = MenuState::STEP_EDIT;
            break;
        case BUTTON_BEAT:
            _state = MenuState::BEAT_SELECT;
            break;
        case BUTTON_TEMPO:
            _state = MenuState::MANUAL_TEMPO;
            break;
        case BUTTON_TUPLET:
            _state = MenuState::TUPLET_SELECT;
            break;
        case BUTTON_MENU:
            _state = MenuState::SUBMENU_SELECTING;
            break;
        case BUTTON_KIT:
            _state = MenuState::KIT_SELECT;
            break;
        case BUTTON_TIME_SIGNATURE:
            _state = MenuState::TIME_SIGNATURE_SELECT;
            break;
    }
}

void Menu::_handleButtonKitSelect(int16_t buttonIndex) {
    switch(buttonIndex) {
        case BUTTON_BACK:
            _state = MenuState::HOME;
            break;
        case BUTTON_INC:
            _kitManager->kitNum = (_kitManager->kitNum + 1) % 8;
            _kitManager->initKit(_kitManager->kitNum);
            printf("Selected kit %d\n", _kitManager->kitNum+1);
            break;
        case BUTTON_DEC:
            _kitManager->kitNum = (_kitManager->kitNum - 1 + 8) % 8;
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
        case BUTTON_DEC:
            _subMenuIndex = (_subMenuIndex - 1 + _subMenuStates.size()) % _subMenuStates.size();
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
        case BUTTON_DEC:
            printf("Load: DEC button pressed\n");
            _kitLoadFolderIndex = (_kitLoadFolderIndex - 1 + _cardReader->getNumSampleFolders()) % _cardReader->getNumSampleFolders();
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
        case BUTTON_DEC:
            _kitLoadSlot = (_kitLoadSlot - 1 + MAX_KITS) % MAX_KITS;
            break;
        case BUTTON_YES:
            // force display update
            _leds->specialUpdateBeforeBlockingProcess();

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

void Menu::_handleButtonKitDelete(int16_t buttonIndex) {
    switch(buttonIndex) {
        case BUTTON_BACK:
            _state = MenuState::SUBMENU_SELECTING;
            break;
        case BUTTON_INC:
            _kitLoadSlot = (_kitLoadSlot + 1) % MAX_KITS;
            break;
        case BUTTON_DEC:
            _kitLoadSlot = (_kitLoadSlot - 1 + MAX_KITS) % MAX_KITS;
            break;
        case BUTTON_YES:
            _leds->specialUpdateBeforeBlockingProcess();
            printf("Deleting kit in slot %d\n", _kitLoadSlot);
            _kitManager->deleteKit(_kitLoadSlot);
            _kitManager->initKit(_kitLoadSlot);
            _state = MenuState::SUBMENU_SELECTING;
            break;
        default:
            printf("Unhandled button in KIT_DELETE\n");
            break;
    }
}

void Menu::_handleButtonCheckSpace(int16_t buttonIndex) {
    switch(buttonIndex) {
        case BUTTON_BACK:
            _state = MenuState::SUBMENU_SELECTING;
            break;
        default:
            printf("Unhandled button in CHECK_SPACE\n");
            break;
    }
}

void Menu::_handleButtonClockModeSelect(int16_t buttonIndex) {
    switch(buttonIndex) {
        case BUTTON_BACK:
        case BUTTON_YES:
        case BUTTON_NO:
            _state = MenuState::SUBMENU_SELECTING;
            break;
        case BUTTON_INC:
        case BUTTON_DEC:
        {
            // cycle through clock modes
            uint32_t currentMode = _transport->getClockMode();
            if(currentMode == MODE_CLOCK_INTERNAL) {
                _transport->setClockMode(MODE_CLOCK_EXTERNAL);
            } else {
                _transport->setClockMode(MODE_CLOCK_INTERNAL);
            }
            break;
        }
        default:
            printf("Unhandled button in CLOCK_MODE_SELECT\n");
            break;
    }
}

void Menu::_handleButtonManualTempo(int16_t buttonIndex) {
    uint32_t bpmQ16 = _transport->getBpmQ16();
    switch(buttonIndex) {
        case BUTTON_BACK:
            _state = MenuState::HOME;
            break;
        case BUTTON_INC:
            bpmQ16 += (1 << 16) / 10;
            _transport->setBpmQ16(bpmQ16);
            break;
        case BUTTON_DEC:
            bpmQ16 -= (1 << 16) / 10;
            _transport->setBpmQ16(bpmQ16);
            break;
        default:
            printf("Unhandled button in MANUAL_TEMPO\n");
            break;
    }
}

void Menu::_handleButtonTimeSignatureSelect(int16_t buttonIndex) {
    switch(buttonIndex) {
        case BUTTON_BACK:
            _state = MenuState::HOME;
            break;
        case BUTTON_INC:
            _transport->incrementTimeSignatureNumerator();
            break;
        case BUTTON_DEC:
            _transport->decrementTimeSignatureNumerator();
            break;
        case BUTTON_YES:
            _transport->incrementTimeSignatureDenominator();
            break;
        case BUTTON_NO:
            _transport->decrementTimeSignatureDenominator();
            break;
        default:
            printf("Unhandled button in TIME_SIGNATURE_SELECT\n");
            break;
    }
}

void Menu::forceDisplayUpdate() {
    // todo: rate limit

    // currently only used for external clock mode to update tempo display
    if(_state == MenuState::MANUAL_TEMPO) {
        _updateDisplay();
    }
}