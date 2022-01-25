#include "settings.h"
#include "io_functions.h"
#include "ble_characteristic_callbacks.h"

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

TaskHandle_t STACK_IN_PROGRESS_TASK;
typedef struct Data_t
{
    int preShutterWaitTime;
    int postShutterWaitTime;
    int shuttersPerStep;
    int stepSize;
    String movementDirection;
    int numberOfStepsToTake;
    String returnToStartPosition;

} GenericData_t;

// TODO: Implement Task Scheduler
Task stackingLoopTask(TASK_ONCE, &stackingLoop);
// Task stackingLoopTask(5000, TASK_ONCE, &stackingLoop);

void stackingLoop(void *xStruct)
{
    Serial.println("Started stacking on a new thread!");

    GenericData_t *params = (GenericData_t *)xStruct;

    Serial.println(params->numberOfStepsToTake);

    // Start stacking loop
    int numberOfStepsTaken = 0; // Keep track of amount of steps taken (for stopping on request)
    for (int i = 1; i <= params->numberOfStepsToTake; i++)
    {
        Serial.println("Step" + String(i));
        // Take picture(s)
        takePictures(params->shuttersPerStep);

        // Move rail forwards
        delay(params->postShutterWaitTime * 1000);
        if (params->movementDirection == "FWD")
        {
            STEPPER_MOTOR.move(params->stepSize);
        }
        else if (params->movementDirection == "BCK")
        {
            STEPPER_MOTOR.move(-params->stepSize);
        }
        numberOfStepsTaken += 1;
        delay(params->preShutterWaitTime * 1000);
    }

    // Return to start position
    if (params->returnToStartPosition == "true")
    {
        Serial.println("Return to start position");
        if (params->movementDirection == "FWD")
        {
            STEPPER_MOTOR.move(-params->stepSize * numberOfStepsTaken);
        }
        else if (params->movementDirection == "BCK")
        {
            STEPPER_MOTOR.move(params->stepSize * numberOfStepsTaken);
        }
    }
    vTaskDelete(NULL);
}

// TODO: Don't yield inside callback
void StartStackingCallback::onWrite(BLECharacteristic *pCharacteristic)
{
    String stringValue = pCharacteristic->getValue().c_str();
    Serial.println(stringValue);

    // Vars that get extracted from received msg
    int preShutterWaitTime;
    int postShutterWaitTime;
    int shuttersPerStep;
    int stepSize;
    String movementDirection;
    int numberOfStepsToTake;
    String returnToStartPosition;

    // Stop currently running stacking loop
    if (STACK_IN_PROGRESS_TASK != NULL)
    {
        vTaskDelete(STACK_IN_PROGRESS_TASK);
    }

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
                returnToStartPosition = commandValue;
            }
            previousSemicolonIndex = currentSemicolonIndex;
        }
    }
    else
    {
        // Invalid data; user likely wanted to stop stacking
        Serial.println("Invalid data!");
        return;
    }

    if (preShutterWaitTime && postShutterWaitTime && shuttersPerStep && stepSize && movementDirection && numberOfStepsToTake && returnToStartPosition)
    {
        TASK_RUNNER.addTask(stackingLoopTask);

        // Convert strings to character arrays
        // char *movementDirectionChar = movementDirection.c_str();
        // char *returnToStartPositionChar = returnToStartPosition.c_str();
        GenericData_t stackingParams = {
            preShutterWaitTime,
            postShutterWaitTime,
            shuttersPerStep,
            stepSize,
            movementDirection,
            numberOfStepsToTake,
            returnToStartPosition};

        int taskPriority = 4;
        int coreId = 0;
        Serial.println(numberOfStepsToTake);
        Serial.println("Create stacking loop");
        xTaskCreatePinnedToCore(stackingLoop, "StackingLoopTask", 10000, (void *)&stackingParams, taskPriority, &STACK_IN_PROGRESS_TASK, coreId);
    }
}
