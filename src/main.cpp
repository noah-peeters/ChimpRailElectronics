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
String STACK_PROGRESS_STATE = "";
int STACK_PRE_SHUTTER_WAIT_TIME = 0;
int STACK_POST_SHUTTER_WAIT_TIME = 0;
int STACK_SHUTTERS_PER_STEP = 0;
int STACK_STEP_SIZE = 0;
String STACK_MOVEMENT_DIRECTION = "";
int STACK_NUMBER_OF_STEPS_TO_TAKE = 0;
String STACK_RETURN_TO_START_POSITION = "";

// --------
// Global variables
// --------
static BLEServer *g_pServer = nullptr;
static BLECharacteristic *g_pCharRead = nullptr;
static BLECharacteristic *g_pCharWrite = nullptr;
static BLECharacteristic *g_pNotifyCurrentSteps = nullptr;
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

    // Move backwards until switch is pressed (if not already pressed)
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

    // Move away from switch (release the switch)
    STEPPER_MOTOR.move(5000);
    STEPPER_MOTOR.runToPosition();
    STEPPER_MOTOR.stop();
    STEPPER_MOTOR.setCurrentPosition(0);
}

// Infinite loop sending motor position update
void sendMotorPositionUpdate(void *pvParameters)
{
    String taskMessage = "Task running on core ";
    taskMessage = taskMessage + xPortGetCoreID();
    Serial.println(taskMessage);
    while (true)
    {
        if (g_centralConnected == false)
        {
            // Reset value so an update is sent after device connects
            g_pNotifyCurrentSteps->setValue("");
        }
        else
        {
            // Send new position to device (if changed during interval)
            char newValue[7];
            itoa(STEPPER_MOTOR.currentPosition(), newValue, 10);
            if (g_pNotifyCurrentSteps->getValue() != newValue)
            {
                g_pNotifyCurrentSteps->setValue(newValue);
                g_pNotifyCurrentSteps->notify();
            }
        }
        delay(SEND_POSITION_UPDATE_INTERVAL);
    }
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
        uint32_t propertyFlags = BLECharacteristic::PROPERTY_NOTIFY;
        BLECharacteristic *pNotifyCurrentSteps = pService->createCharacteristic(NOTIFY_STEPS_UUID, propertyFlags);
        pNotifyCurrentSteps->setCallbacks(new MyCharPrintingCallbacks("CharIndicate"));
        // pNotifyCurrentSteps->addDescriptor(new BLEDescriptor(NOTIFY_STEPS_DESCRIPTOR_UUID));
        pNotifyCurrentSteps->addDescriptor(new BLE2902());
        pNotifyCurrentSteps->setValue("");
        g_pNotifyCurrentSteps = pNotifyCurrentSteps;
    }

    // TODO: Implement/remove following

    // Characteristic for read
    {
        uint32_t propertyFlags = BLECharacteristic::PROPERTY_READ;
        BLECharacteristic *pCharRead = pService->createCharacteristic(CHAR_READ_UUID, propertyFlags);
        pCharRead->setCallbacks(new MyCharPrintingCallbacks("CharRead"));
        pCharRead->setValue("Ready to be used!");
        g_pCharRead = pCharRead;
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

    // Setup motor position update loop (other thread so it doesn't interfere with regular motor updates)
    xTaskCreatePinnedToCore(
        sendMotorPositionUpdate,   /* Function to implement the task */
        "SendMotorPositionUpdate", /* Name of the task */
        10000,                     /* Stack size in words */
        NULL,                      /* Task input parameter */
        0,                         /* Priority of the task */
        NULL,                      /* Task handle. */
        0);                        /* Core where the task should run */

    Serial.println("BLE Peripheral setup done, started advertising");
}

// Main event loop
unsigned long previousTaskTime = 0;
int stepsTakenSinceStart = 0;
int picturesTakenOnThisStep = 0;
void loop()
{
    // Process next stacking operation
    if (STACK_PROGRESS_STATE.length() > 0)
    {
        if (STACK_PROGRESS_STATE == "TakePictures")
        {
            // Take a picture
            if (millis() - previousTaskTime > STACK_PRE_SHUTTER_WAIT_TIME * 1000)
            {
                Serial.println("Take picture");
                digitalWrite(SHUTTER_PIN, HIGH);
                delay(50); // TODO: Check if delay can be smaller (not super important)
                digitalWrite(SHUTTER_PIN, LOW);

                previousTaskTime = millis();
                picturesTakenOnThisStep += 1;
                if (picturesTakenOnThisStep >= STACK_SHUTTERS_PER_STEP)
                {
                    // Proceed to rail movement
                    picturesTakenOnThisStep = 0;
                    STACK_PROGRESS_STATE = "MoveRail";
                }
            }
        }
        if (STACK_PROGRESS_STATE == "MoveRail")
        {
            // Move rail
            if (millis() - previousTaskTime > STACK_POST_SHUTTER_WAIT_TIME * 1000)
            {
                if (STACK_MOVEMENT_DIRECTION == "FWD")
                {
                    STEPPER_MOTOR.move(STACK_STEP_SIZE);
                }
                else if (STACK_MOVEMENT_DIRECTION == "BCK")
                {
                    STEPPER_MOTOR.move(-STACK_STEP_SIZE);
                }

                previousTaskTime = millis();
                stepsTakenSinceStart += 1;
                if (stepsTakenSinceStart >= STACK_NUMBER_OF_STEPS_TO_TAKE)
                {
                    // Stop stacking
                    STACK_PROGRESS_STATE = "StopStacking";
                }
                else
                {
                    // Proceed to taking pictures
                    STACK_PROGRESS_STATE = "TakePictures";
                }
            }
        }
        if (STACK_PROGRESS_STATE == "StopStacking")
        {
            // Stop stacking process
            // TODO: Change "STACK_RETURN_TO_START_POSITION" to be a bool instead of a string
            if (STACK_RETURN_TO_START_POSITION == "true" && stepsTakenSinceStart > 0)
            {
                // Move to start position (if asked by user)
                Serial.println("Return to start position");
                if (STACK_MOVEMENT_DIRECTION == "FWD")
                {
                    STEPPER_MOTOR.move(-STACK_STEP_SIZE * stepsTakenSinceStart);
                }
                else if (STACK_MOVEMENT_DIRECTION == "BCK")
                {
                    STEPPER_MOTOR.move(STACK_STEP_SIZE * stepsTakenSinceStart);
                }
            }

            // Reset vars
            previousTaskTime = 0;
            stepsTakenSinceStart = 0;
            picturesTakenOnThisStep = 0;

            STACK_PROGRESS_STATE = "";
            STACK_PRE_SHUTTER_WAIT_TIME = 0;
            STACK_POST_SHUTTER_WAIT_TIME = 0;
            STACK_SHUTTERS_PER_STEP = 0;
            STACK_STEP_SIZE = 0;
            STACK_MOVEMENT_DIRECTION = "";
            STACK_NUMBER_OF_STEPS_TO_TAKE = 0;
            STACK_RETURN_TO_START_POSITION = "";
        }
    }

    // Move motor
    if (STEPPER_MOTOR.targetPosition())
    {
        // Change target if not in valid range
        if (STEPPER_MOTOR.targetPosition() > MAX_STEPS_LIMIT)
        {
            STEPPER_MOTOR.moveTo(MAX_STEPS_LIMIT);
        }
        else if (STEPPER_MOTOR.targetPosition() < 0)
        {
            STEPPER_MOTOR.moveTo(0);
        }
    }
    // Update motor position (if needed)
    if (abs(STEPPER_MOTOR.targetPosition() - STEPPER_MOTOR.currentPosition()) > 0)
    {
        STEPPER_MOTOR.run();
    }
}
