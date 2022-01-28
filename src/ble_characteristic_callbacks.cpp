#include "settings.h"
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

void StartStackingCallback::onWrite(BLECharacteristic *pCharacteristic)
{
    String stringValue = pCharacteristic->getValue().c_str();
    Serial.println(stringValue);

    // Stop currently running stacking loop
    STACK_PROGRESS_STATE = "";

    if (stringValue.length() > 0)
    {
        if (stringValue == "Stop")
        {
            // Invalid data; user likely wanted to stop stacking
            STACK_PROGRESS_STATE = "StopStacking";
            return;
        }
        else
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
                        STACK_PRE_SHUTTER_WAIT_TIME = result;
                    }
                }
                else if (commandId == "PST")
                {
                    // Post-shutter wait time
                    int result = commandValue.toInt();
                    if (result != 0)
                    {
                        STACK_POST_SHUTTER_WAIT_TIME = result;
                    }
                }
                else if (commandId == "STP")
                {
                    // Shutters per step
                    int result = commandValue.toInt();
                    if (result != 0)
                    {
                        STACK_SHUTTERS_PER_STEP = result;
                    }
                }
                else if (commandId == "STS")
                {
                    // Stepsize
                    int result = commandValue.toInt();
                    if (result != 0)
                    {
                        STACK_STEP_SIZE = result;
                    }
                }
                else if (commandId == "DIR")
                {
                    // Movement direction
                    STACK_MOVEMENT_DIRECTION = commandValue;
                }
                else if (commandId == "SPS")
                {
                    // Stack start position
                    int result = commandValue.toInt();
                    if (result != 0)
                    {
                        STACK_START_POSITION = result;
                    }
                }
                else if (commandId == "NST")
                {
                    // Number of steps to take
                    int result = commandValue.toInt();
                    if (result != 0)
                    {
                        STACK_NUMBER_OF_STEPS_TO_TAKE = result;
                    }
                }
                else if (commandId == "RTS")
                {
                    // Return to start position
                    if (commandValue == "false")
                    {
                        STACK_RETURN_TO_START_POSITION = false;
                    }
                    else if (commandValue == "true")
                    {
                        STACK_RETURN_TO_START_POSITION = true;
                    }
                }
                previousSemicolonIndex = currentSemicolonIndex;
            }
        }
    }

    // Start stacking process
    STACK_PROGRESS_STATE = "MoveToStart";
}
