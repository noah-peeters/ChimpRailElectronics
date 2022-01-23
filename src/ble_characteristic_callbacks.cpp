#include "settings.h"
#include "io_functions.h"
#include "ble_characteristic_callbacks.h"

TaskHandle_t STACK_IN_PROGRESS_TASK;

// TODO: Test step movement + continuous movement
void StepMovementCallbacks::onWrite(BLECharacteristic *pCharacteristic)
{
    String stringValue = pCharacteristic->getValue().c_str();

    if (stringValue.length() > 0)
    {
        String commandName = stringValue.substring(0, 3);
        int commandNumber = stringValue.substring(3).toInt();
        if (commandName == "FWD")
        {
            Serial.println("Take " + String(commandNumber) + " steps FWD");
            STEPPER_MOTOR.move(commandNumber);
        }
        else if (commandName == "BCK")
        {
            Serial.println("Take " + String(commandNumber) + " steps BCK");
            STEPPER_MOTOR.move(-commandNumber);
        }
    }
}

void ContinuousMovementCallbacks::onWrite(BLECharacteristic *pCharacteristic)
{
    String stringValue = pCharacteristic->getValue().c_str();

    if (stringValue.length() > 0)
    {
        String commandName = stringValue.substring(0, 3);
        String commandValue = stringValue.substring(3);

        if (commandValue == "false")
        {
            // Stop movement
            STEPPER_MOTOR.stop();
            Serial.println("Stop motor movement.");
            return;
        }

        if (commandName == "FWD")
        {
            // Move to end of rail
            STEPPER_MOTOR.moveTo(MAX_STEPS_LIMIT);
            Serial.println("Move forwards.");
        }
        else if (commandName == "BCK")
        {
            // Move to start of rail
            STEPPER_MOTOR.moveTo(0);
            Serial.println("Move backwards.");
        }
    }
}

// Define vars that get extracted from received msg
int preShutterWaitTime;
int postShutterWaitTime;
int shuttersPerStep;
int stepSize;
String movementDirection;
int numberOfStepsToTake;
bool returnToStartPosition;

void stackingLoop(void *parameter)
{
    if (preShutterWaitTime && postShutterWaitTime && shuttersPerStep && stepSize && movementDirection && numberOfStepsToTake && returnToStartPosition != NULL)
    {
        Serial.println("Started stacking!");

        // Start stacking loop
        int numberOfStepsTaken = 0; // Keep track of amount of steps taken (for stopping on request)
        for (int i = 1; i <= numberOfStepsToTake; i++)
        {
            Serial.println("Step" + String(i));
            // Take picture(s)
            takePictures(shuttersPerStep);

            // Move rail forwards
            delay(postShutterWaitTime * 1000);
            if (movementDirection == "FWD")
            {
                STEPPER_MOTOR.move(stepSize);
            }
            else if (movementDirection == "BCK")
            {
                STEPPER_MOTOR.move(-stepSize);
            }
            delay(preShutterWaitTime * 1000);
        }

        // Return to start position
        if (returnToStartPosition == true)
        {
            Serial.println("Return to start position");
            if (movementDirection == "FWD")
            {
                STEPPER_MOTOR.move(-stepSize * numberOfStepsTaken);
            }
            else if (movementDirection == "BCK")
            {
                STEPPER_MOTOR.move(stepSize * numberOfStepsTaken);
            }
        }
    }
    // Delete task
    // TODO: Check if needed
    vTaskDelete(NULL);
}

// TODO: Don't yield inside callback
void StartStackingCallback::onWrite(BLECharacteristic *pCharacteristic)
{
    String stringValue = pCharacteristic->getValue().c_str();
    Serial.println(stringValue);

    // Stop currently running stacking loop
    vTaskDelete(STACK_IN_PROGRESS_TASK);

    // TODO: Stop stacking button in app will send "STOP"
    if (stringValue.length() > 0 && stringValue != "STOP")
    {
        // Extract data
        int previousSemicolonIndex = -1; // Start on "-1" b/c of +1
        for (int i = 0; i <= stringValue.length(); i++)
        {
            int currentSemicolonIndex = stringValue.indexOf(";", previousSemicolonIndex + 1);

            int start_index = previousSemicolonIndex + 1;
            // Command ID is always 3 chars long (ex. "FWD", "BCK", ...)
            String commandId = stringValue.substring(start_index, start_index + 3);
            // Command value is all the next characters upto (excluding) the semicolon
            String commandValue = stringValue.substring(start_index + 3, currentSemicolonIndex);

            if (commandId == "PRE")
            {
                // Pre-shutter wait time
                int result = commandValue.toInt();
                if (result != 0)
                {
                    preShutterWaitTime = result;
                }
            }
            else if (commandId == "PST")
            {
                // Post-shutter wait time
                int result = commandValue.toInt();
                if (result != 0)
                {
                    postShutterWaitTime = result;
                }
            }
            else if (commandId == "STP")
            {
                // Shutters per step
                int result = commandValue.toInt();
                if (result != 0)
                {
                    shuttersPerStep = result;
                }
            }
            else if (commandId == "STS")
            {
                // Stepsize
                int result = commandValue.toInt();
                if (result != 0)
                {
                    stepSize = result;
                }
            }
            else if (commandId == "DIR")
            {
                // Movement direction
                movementDirection = commandValue;
            }
            else if (commandId == "NST")
            {
                // Number of steps to take
                int result = commandValue.toInt();
                if (result != 0)
                {
                    numberOfStepsToTake = result;
                }
            }
            else if (commandId == "RTS")
            {
                // Return to start position
                if (commandValue == "true")
                {
                    returnToStartPosition = true;
                }
                else if (commandValue == "false")
                {
                    returnToStartPosition = false;
                }
            }
            previousSemicolonIndex = currentSemicolonIndex;
        }
    }
    else
    {
        // Invalid data; user likely wanted to stop stacking
        return;
    }

    char task1Param[12] = "task1Param";
    int taskPriority = 4;
    int coreId = 0;
    xTaskCreatePinnedToCore(stackingLoop, "StackingLoopTask", 10000, (void *)task1Param, taskPriority, &STACK_IN_PROGRESS_TASK, coreId);
}
