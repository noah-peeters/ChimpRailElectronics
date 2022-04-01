#ifndef BLE_CHARACTERISTIC_CALLBACKS_H
#define BLE_CHARACTERISTIC_CALLBACKS_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

class StepMovementCallback : public BLECharacteristicCallbacks
{
public:
    void onWrite(BLECharacteristic *pCharacteristic);
};

class ContinuousMovementCallback : public BLECharacteristicCallbacks
{
public:
    void onWrite(BLECharacteristic *pCharacteristic);
};

class ShutterReleaseCallback : public BLECharacteristicCallbacks
{
public:
    void onWrite(BLECharacteristic *pCharacteristic);
};

class StartStackingCallback : public BLECharacteristicCallbacks
{
public:
    void onWrite(BLECharacteristic *pCharacteristic);
};

#endif