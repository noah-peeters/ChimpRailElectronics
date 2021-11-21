#include <Arduino.h>
#include "BluetoothSerial.h"

const int STEPPER_PULSE_DELAY = 50; // Delay in µs between stepper motor pulses

BluetoothSerial SerialBluetooth;
String bluetoothMessage;

// Define pins
const int enaPin = 25;
const int dirPin = 26;
const int pulPin = 27;

void takeSteps(int stepAmount)
{
  // Set direction
  if (stepAmount < 0)
  {
    // Backwards
    digitalWrite(dirPin, LOW);
    stepAmount = -stepAmount;
  }
  else
  {
    // Forwards
    digitalWrite(dirPin, HIGH);
  }

  for (int i = 0; i <= stepAmount; i++)
  {
    digitalWrite(pulPin, HIGH);
    delayMicroseconds(STEPPER_PULSE_DELAY);
    digitalWrite(pulPin, LOW);
    delayMicroseconds(STEPPER_PULSE_DELAY);
  }
}

void setup()
{
  // Set pinModes
  pinMode(enaPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(pulPin, OUTPUT);

  // Enable stepper driver
  digitalWrite(enaPin, LOW);

  Serial.begin(9600);
  SerialBluetooth.begin("Esp32BluetoothStackingRail");
}

void loop()
{
  if (SerialBluetooth.available())
  {
    // Read all incoming characters
    bluetoothMessage = "";
    while (SerialBluetooth.available())
    {
      char newChar = SerialBluetooth.read();
      bluetoothMessage += String(newChar);
    }
  }
  String commandName = bluetoothMessage.substring(0, 3);
  int commandNumber = bluetoothMessage.substring(3).toInt();
  if (commandName == "FWD")
  {
    takeSteps(commandNumber);
  }
  else if (commandName == "BCK")
  {
    takeSteps(-commandNumber);
  }
  bluetoothMessage = "";

  // Serial.println("FORWARDS");
  // takeSteps(5000);
  // delay(1000);
  // Serial.println("BACKWARDS");
  // takeSteps(-5000);
  // delay(1000);

  delay(50);
}