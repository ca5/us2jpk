#include <Arduino.h>
#include "USBHost_t36.h"

void BluetoothController::driver_ready_for_bluetooth(BTHIDInput *driver) {
    (void)driver;
}

void BluetoothController::sendL2CapCommand(uint8_t* data, uint8_t nbytes, int channel) {
    (void)data;
    (void)nbytes;
    (void)channel;
}
