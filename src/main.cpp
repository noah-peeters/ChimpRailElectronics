/*
  Original version of BLE implementation created by Alexander Lavrushko on 22/03/2021: https://github.com/alexanderlavrushko/BLEProof-collection
  Sets up BLE as a "Peripheral" with characeristics:
    * Read (Central can read from this characteristic)
    * Write (Central can write to this characteristic)
    * Indicate (central can subscribe to this characteristic)
*/
#include <settings.h>
#include <rail_movement.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

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
    Serial.println("BLE Peripheral setup started");

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
        pCharWrite->setCallbacks(new StepMovementCallbacks());
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

void loop()
{
    /*
      Serial.print("Setting read characteristic: '");
      Serial.print(commandData.c_str());
      Serial.println("'");
      g_pCharRead->setValue(commandData);
    */

    /*
      Serial.print("Setting indicate characteristic: '");
      Serial.print(commandData.c_str());
      Serial.println("'");
      g_pCharIndicate->setValue(commandData);
      g_pCharIndicate->indicate();
      return;
    */
}
