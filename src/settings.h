#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include "AccelStepper.h"

// I/O pins
#define MOTOR_ENA_PIN 25
#define MOTOR_DIR_PIN 26
#define MOTOR_PUL_PIN 27
#define HOME_LIMIT_SWITCH_PIN 33
#define SHUTTER_PIN 16

// Other constants
#define MAX_STEPS_LIMIT 25000 // Max amount of steps to allow (move from absolute 0 to end of rail)

// Bluetooth settings
#define DEVICE_DISPLAY_NAME "Stacking rail"
#define SERVICE_UUID "a7f8fe1b-a57d-46a3-a0da-947a1d8c59ce"
#define STEP_MOVEMENT_WRITE_UUID "908badf3-fd6b-4eec-b362-2810e97db94e"
#define CONTINUOUS_MOVEMENT_WRITE_UUID "28c74a57-67cb-4b43-8adf-8776c1dbc475"
#define START_STACKING_WRTIE_UUID "bed1dd25-79f2-4ce2-a6fa-000471efe3a0"

// TODO: Remove/replace
#define CHAR_READ_UUID "f33dee97-d3d8-4fbd-8162-c980133f0c93"
#define CHAR_INDICATE_UUID "d2f362f4-6542-4b13-be5e-8c81d423a347"

// Shared variables (between multiple files)
extern AccelStepper STEPPER_MOTOR;

#endif