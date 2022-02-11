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
int STACK_START_POSITION = 0;
int STACK_NUMBER_OF_STEPS_TO_TAKE = 0;
bool STACK_RETURN_TO_START_POSITION = true;

// --------
// Global variables
// --------
static BLEServer *g_pServer = nullptr;
static BLECharacteristic *g_pCharWrite = nullptr;
static BLECharacteristic *g_pNotifyCurrentSteps = nullptr;
static BLECharacteristic *g_pNotifyStackingStepsTaken = nullptr;
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

// Home motor
void homeMotor()
{
    pinMode(HOME_LIMIT_SWITCH_PIN, INPUT);

    // Move backwards until switch is pressed (if not already pressed)
    if (digitalRead(HOME_LIMIT_SWITCH_PIN) == LOW)
    {
        STEPPER_MOTOR.move(-MAX_STEPS_LIMIT * 2);
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
        if (g_centralConnected == true)
        {
            // Send new position to device (if changed during interval)
            char newValue[7];
            itoa(STEPPER_MOTOR.currentPosition(), newValue, 10);
            g_pNotifyCurrentSteps->setValue(newValue);
            g_pNotifyCurrentSteps->notify();
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

    pinMode(SHUTTER_PIN, OUTPUT);

    // Motor setup
    STEPPER_MOTOR.setEnablePin(MOTOR_ENA_PIN);
    // Change first bool (direction) depending on what pole pair was connected
    STEPPER_MOTOR.setPinsInverted(true, false, true); // Enable for TB6600 driver is a "LOW" signal
    STEPPER_MOTOR.enableOutputs();
    STEPPER_MOTOR.setMaxSpeed(MOTOR_MAX_SPEED);
    STEPPER_MOTOR.setAcceleration(MOTOR_MAX_ACCELERATION);
    homeMotor();

    // BLE peripheral setup
    BLEDevice::init(DEVICE_DISPLAY_NAME);
    g_pServer = BLEDevice::createServer();
    g_pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = g_pServer->createService(SERVICE_UUID);
    // Listen to step movement write
    {
        BLECharacteristic *pCharWrite = pService->createCharacteristic(STEP_MOVEMENT_WRITE_UUID, BLECharacteristic::PROPERTY_WRITE);
        pCharWrite->setCallbacks(new StepMovementCallbacks);
        pCharWrite->setValue("");
        g_pCharWrite = pCharWrite;
    }

    // Listen to continuous movement write
    {
        BLECharacteristic *pCharWrite = pService->createCharacteristic(CONTINUOUS_MOVEMENT_WRITE_UUID, BLECharacteristic::PROPERTY_WRITE);
        pCharWrite->setCallbacks(new ContinuousMovementCallbacks());
        pCharWrite->setValue("");
        g_pCharWrite = pCharWrite;
    }

    // Listen to start stacking write
    {
        BLECharacteristic *pCharWrite = pService->createCharacteristic(START_STACKING_WRTIE_UUID, BLECharacteristic::PROPERTY_WRITE);
        pCharWrite->setCallbacks(new StartStackingCallback());
        pCharWrite->setValue("");
        g_pCharWrite = pCharWrite;
    }

    // Notify device about current motor position
    {
        BLECharacteristic *pNotifyCurrentSteps = pService->createCharacteristic(NOTIFY_STEPS_UUID, BLECharacteristic::PROPERTY_NOTIFY);
        pNotifyCurrentSteps->addDescriptor(new BLE2902());
        pNotifyCurrentSteps->setValue("");
        g_pNotifyCurrentSteps = pNotifyCurrentSteps;
    }

    // Notify device about current stacking state
    {
        BLECharacteristic *pNotifyStackingStepsTaken = pService->createCharacteristic(NOTIFY_STACKING_STEPS_TAKEN_UUID, BLECharacteristic::PROPERTY_NOTIFY);
        pNotifyStackingStepsTaken->addDescriptor(new BLE2902());
        pNotifyStackingStepsTaken->setValue("");
        g_pNotifyStackingStepsTaken = pNotifyStackingStepsTaken;
    }

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    // Setup motor position update loop (on other core so it doesn't interfere with regular motor updates)
    xTaskCreatePinnedToCore(
        sendMotorPositionUpdate,   // Function of task
        "SendMotorPositionUpdate", // Name of task
        10000,                     // Stack size (in words)
        NULL,                      // Input param
        0,                         // Task priority
        NULL,                      // Task handle
        0);                        // Core
}

// Main event loop
bool motorEnabled = false;
unsigned long previousTaskTime = 0;
int stepsTakenSinceStart = 0;
int picturesTakenOnThisStep = 0;
void loop()
{
    // Process next stacking operation
    if (STACK_PROGRESS_STATE.length() > 0)
    {
        if (STACK_PROGRESS_STATE == "MoveToStart")
        {
            // Move to start position of stack
            STEPPER_MOTOR.moveTo(STACK_START_POSITION);

            if (STEPPER_MOTOR.currentPosition() == STACK_START_POSITION)
            {
                // Position reached; continue stacking process
                STACK_PROGRESS_STATE = "TakePictures";
            }
        }
        else if (STACK_PROGRESS_STATE == "TakePictures")
        {
            // Take a picture
            if (millis() - previousTaskTime > STACK_PRE_SHUTTER_WAIT_TIME * 1000)
            {
                // Send update to device (new step)
                String str = String(stepsTakenSinceStart) + ";" + String(STACK_NUMBER_OF_STEPS_TO_TAKE) +
                             ";" + String(STACK_STEP_SIZE) + ";" +
                             String(STACK_PRE_SHUTTER_WAIT_TIME * STACK_SHUTTERS_PER_STEP + STACK_POST_SHUTTER_WAIT_TIME);
                char newValue[100];
                str.toCharArray(newValue, 100);
                g_pNotifyStackingStepsTaken->setValue(newValue);
                g_pNotifyStackingStepsTaken->notify();
                Serial.println(newValue);

                digitalWrite(SHUTTER_PIN, HIGH);
                delay(1000); // TODO: Check if delay can be smaller (not super important)
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
        else if (STACK_PROGRESS_STATE == "MoveRail")
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
            if (STACK_RETURN_TO_START_POSITION == true && stepsTakenSinceStart > 0)
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
            // Send stack ended
            g_pNotifyStackingStepsTaken->setValue("");
            g_pNotifyStackingStepsTaken->notify();

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
            STACK_RETURN_TO_START_POSITION = true;
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
    if (STEPPER_MOTOR.distanceToGo() != 0)
    {
        if (motorEnabled == false)
        {
            STEPPER_MOTOR.enableOutputs();
            motorEnabled = true;
        }
        STEPPER_MOTOR.run();
    }
    else
    {
        // Disable driver
        STEPPER_MOTOR.disableOutputs();
        motorEnabled = false;
    }
}
