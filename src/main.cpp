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

void homeMotor()
{
    pinMode(HOME_LIMIT_SWITCH_PIN, INPUT);

    // Move backwards until switch is hit (if not already hit)
    if (digitalRead(HOME_LIMIT_SWITCH_PIN) == LOW)
    {
        STEPPER_MOTOR.move(-100000000);
        while (digitalRead(HOME_LIMIT_SWITCH_PIN) == LOW)
        {
            // Keep moving back
            STEPPER_MOTOR.run();
        }
    }
    STEPPER_MOTOR.stop();
    STEPPER_MOTOR.setCurrentPosition(0);
}

// --------
// Application lifecycle: setup & loop
// --------
void setup()
{
    Serial.begin(115200);

    STEPPER_MOTOR.setMaxSpeed(MOTOR_MAX_SPEED);
    STEPPER_MOTOR.setAcceleration(MOTOR_MAX_ACCELERATION);
    // Home stepper motor
    homeMotor();

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
        pCharWrite->setCallbacks(new ContinuousMovementCallbacks());
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
    // Change target if not in valid range
    if (STEPPER_MOTOR.targetPosition())
    {
        if (STEPPER_MOTOR.targetPosition() > MAX_STEPS_LIMIT)
        {
            STEPPER_MOTOR.moveTo(MAX_STEPS_LIMIT);
        }
        else if (STEPPER_MOTOR.targetPosition() < 0)
        {
            STEPPER_MOTOR.moveTo(0);
        }
    }

    // Update motor (if needed)
    if (abs(STEPPER_MOTOR.targetPosition() - STEPPER_MOTOR.currentPosition()) > 0)
    {
        STEPPER_MOTOR.run();
    }
}
