#ifndef BLE_CHARACTERISTIC_CALLBACKS_H
#define BLE_CHARACTERISTIC_CALLBACKS_H

class StepMovementCallbacks: public BLECharacteristicCallbacks
{
public:
    void onWrite(BLECharacteristic *pCharacteristic);
};

class StartStackingCallback: public BLECharacteristicCallbacks
{
public:
    void onWrite(BLECharacteristic *pCharacteristic);
};

#endif