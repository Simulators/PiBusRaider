// CommandSerial
// Rob Dobson 2018

#include "CommandSerial.h"

HardwareSerial* CommandSerial::_pSerial = NULL;

CommandSerialFrameRxFnType* CommandSerial::_pFrameRxCallback = NULL;
