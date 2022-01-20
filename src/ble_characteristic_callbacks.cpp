#include "settings.h"
#include "rail_movement.h"
#include "ble_characteristic_callbacks.h"

class StepMovementCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        String stringValue = pCharacteristic->getValue().c_str();

        if (stringValue.length() > 0)
        {
            String commandName = stringValue.substring(0, 3);
            int commandNumber = stringValue.substring(3).toInt();
            if (commandName == "FWD")
            {
                Serial.println("Take " + String(commandNumber) + " steps FWD");
                takeSteps(commandNumber);
            }
            else if (commandName == "BCK")
            {
                Serial.println("Take " + String(commandNumber) + " steps BCK");
                takeSteps(-commandNumber);
            }
        }
    }
};

class StartStackingCallback : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        String stringValue = pCharacteristic->getValue().c_str();
        Serial.println(stringValue);

        if (stringValue.length() > 0)
        {
            // Define vars
            int preShutterWaitTime = 0;
            int postShutterWaitTime = 0;
            int shuttersPerStep = 0;
            int stepSize = 0;
            String movementDirection = "";
            int numberOfStepsToTake = 0;
            bool returnToStartPosition = 0;

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

            Serial.println(String(preShutterWaitTime));
            Serial.println(String(postShutterWaitTime));
            Serial.println(String(shuttersPerStep));
            Serial.println(String(stepSize));
            Serial.println(movementDirection);
            Serial.println(String(numberOfStepsToTake));
            Serial.println(String(returnToStartPosition));
        }
    }
};