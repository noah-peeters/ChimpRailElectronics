/*
  Original version of BLE implementation created by Alexander Lavrushko on 22/03/2021: https://github.com/alexanderlavrushko/BLEProof-collection
  Sets up BLE as a "Peripheral" with characeristics:
    * Read (Central can read from this characteristic)
    * Write (Central can write to this characteristic)
    * Indicate (central can subscribe to this characteristic)
*/
#include "settings.h"
#include "ble_characteristic_callbacks.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// Shared variables setup
bool STACK_IN_PROGRESS = false;
bool CONTINUOUS_MOVEMENT_IN_PROGRESS = false;
AccelStepper STEPPER_MOTOR(AccelStepper::DRIVER, MOTOR_PUL_PIN, MOTOR_DIR_PIN);

// --------
// Global variables
// --------
static BLEServer *g_pServer = nullptr;
static BLECharacteristic *g_pCharRead = nullptr;
static BLECharacteristic *g_pCharWrite = nullptr;
static BLECharacteristic *g_pCharIndicate = nullptr;
static bool g_centralConnected = false;
static std::string g_cmdLine;

// --------
// Bluetooth event callbacks
// --------
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer) override
    {
        Serial.println("Connected!");
        g_centralConnected = true;
    }

    void onDisconnect(BLEServer *pServer) override
    {
        Serial.println("Disconnected! Start advertising...");
        g_centralConnected = false;
        BLEDevice::startAdvertising();
    }
};

class MyCharPrintingCallbacks : public BLECharacteristicCallbacks
{
public:
    explicit MyCharPrintingCallbacks(const char *name) : m_name(name) {}

private:
    void PrintEvent(const char *event, const char *value)
    {
        Serial.print(event);
        Serial.print("(");
        Serial.print(m_name.c_str());
        Serial.print(")");
        if (value)
        {
            Serial.print(" value='");
            Serial.print(value);
            Serial.print("'");
        }
        Serial.println();
    }

private:
    void onRead(BLECharacteristic *pCharacteristic) override
    {
        PrintEvent("onRead", pCharacteristic->getValue().c_str());
    }

    void onWrite(BLECharacteristic *pCharacteristic) override
    {
        PrintEvent("onWrite", pCharacteristic->getValue().c_str());
    }

    void onNotify(BLECharacteristic *pCharacteristic) override
    {
        PrintEvent("onNotify", pCharacteristic->getValue().c_str());
    }

    void onStatus(BLECharacteristic *pCharacteristic, Status status, uint32_t code) override
    {
        std::string event("onStatus:");
        switch (status)
        {
        case SUCCESS_INDICATE:
            event += "SUCCESS_INDICATE";
            break;
        case SUCCESS_NOTIFY:
            event += "SUCCESS_NOTIFY";
            break;
        case ERROR_INDICATE_DISABLED:
            event += "ERROR_INDICATE_DISABLED";
            break;
        case ERROR_NOTIFY_DISABLED:
            event += "ERROR_NOTIFY_DISABLED";
            break;
        case ERROR_GATT:
            event += "ERROR_GATT";
            break;
        case ERROR_NO_CLIENT:
            event += "ERROR_NO_CLIENT";
            break;
        case ERROR_INDICATE_TIMEOUT:
            event += "ERROR_INDICATE_TIMEOUT";
            break;
        case ERROR_INDICATE_FAILURE:
            event += "ERROR_INDICATE_FAILURE";
            break;
        }
        event += ":";
        event += String(code).c_str();
        PrintEvent(event.c_str(), nullptr);
    }

private:
    std::string m_name;
};

// --------
// Application lifecycle: setup & loop
// --------
void setup()
{
    Serial.begin(115200);

    // Stepper motor setup
    STEPPER_MOTOR.setMaxSpeed(250);
    STEPPER_MOTOR.setAcceleration(20);

    // Home stepper motor
    while (digitalRead(HOME_LIMIT_SWITCH_PIN) == LOW)
    {
        // Keep moving one step back
        STEPPER_MOTOR.move(-1);
        STEPPER_MOTOR.run();
    }
    STEPPER_MOTOR.setCurrentPosition(0);

    // BLE peripheral setup
    BLEDevice::init(DEVICE_DISPLAY_NAME);
    g_pServer = BLEDevice::createServer();
    g_pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = g_pServer->createService(SERVICE_UUID);

    // Characteristic for read
    {
        uint32_t propertyFlags = BLECharacteristic::PROPERTY_READ;
        BLECharacteristic *pCharRead = pService->createCharacteristic(CHAR_READ_UUID, propertyFlags);
        pCharRead->setCallbacks(new MyCharPrintingCallbacks("CharRead"));
        pCharRead->setValue("Ready to be used!");
        g_pCharRead = pCharRead;
    }

    // Step movement write
    {
        uint32_t propertyFlags = BLECharacteristic::PROPERTY_WRITE;
        BLECharacteristic *pCharWrite = pService->createCharacteristic(STEP_MOVEMENT_WRITE_UUID, propertyFlags);
        pCharWrite->setCallbacks(new StepMovementCallbacks);
        pCharWrite->setValue("");
        g_pCharWrite = pCharWrite;
    }

    // Continuous movement write
    {
        uint32_t propertyFlags = BLECharacteristic::PROPERTY_WRITE;
        BLECharacteristic *pCharWrite = pService->createCharacteristic(CONTINUOUS_MOVEMENT_WRITE_UUID, propertyFlags);
        pCharWrite->setCallbacks(new MyCharPrintingCallbacks("CharWrite"));
        pCharWrite->setValue("");
        g_pCharWrite = pCharWrite;
    }

    // Start stacking write
    {
        uint32_t propertyFlags = BLECharacteristic::PROPERTY_WRITE;
        BLECharacteristic *pCharWrite = pService->createCharacteristic(START_STACKING_WRTIE_UUID, propertyFlags);
        pCharWrite->setCallbacks(new StartStackingCallback());
        pCharWrite->setValue("");
        g_pCharWrite = pCharWrite;
    }

    // Characteristic for indicate
    {
        uint32_t propertyFlags = BLECharacteristic::PROPERTY_INDICATE;
        BLECharacteristic *pCharIndicate = pService->createCharacteristic(CHAR_INDICATE_UUID, propertyFlags);
        pCharIndicate->setCallbacks(new MyCharPrintingCallbacks("CharIndicate"));
        pCharIndicate->addDescriptor(new BLE2902());
        pCharIndicate->setValue("");
        g_pCharIndicate = pCharIndicate;
    }

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    // this fixes iPhone connection issue (don't know how it works)
    {
        pAdvertising->setMinPreferred(0x06);
        pAdvertising->setMinPreferred(0x12);
    }
    BLEDevice::startAdvertising();

    Serial.println("BLE Peripheral setup done, started advertising");
}

// Main event loop
void loop()
{
    // Update motor (if needed)
    if (abs(STEPPER_MOTOR.targetPosition() - STEPPER_MOTOR.currentPosition()) > 0)
    {
        STEPPER_MOTOR.run();
    }
}
