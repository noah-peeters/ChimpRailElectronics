#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include "AccelStepper.h"

// I/O pins
#define MOTOR_PUL_PIN 21
#define MOTOR_DIR_PIN 19
#define MOTOR_ENA_PIN 18
#define HOME_LIMIT_SWITCH_PIN 4
#define SHUTTER_PIN 32

// Other constants
#define MAX_STEPS_LIMIT 210000            // Max amount of steps to allow (move from absolute 0 to end of rail)
#define MOTOR_MAX_SPEED 16000             // Max speed of motor
#define MOTOR_MAX_ACCELERATION 16000      // Max acceleration of motor
#define SEND_POSITION_UPDATE_INTERVAL 100 // Time interval for sending position update to device (ms)

// Bluetooth settings
#define DEVICE_DISPLAY_NAME "Stacking rail"
#define SERVICE_UUID "a7f8fe1b-a57d-46a3-a0da-947a1d8c59ce"
#define STEP_MOVEMENT_WRITE_UUID "908badf3-fd6b-4eec-b362-2810e97db94e"
#define CONTINUOUS_MOVEMENT_WRITE_UUID "28c74a57-67cb-4b43-8adf-8776c1dbc475"
#define START_STACKING_WRTIE_UUID "bed1dd25-79f2-4ce2-a6fa-000471efe3a0"
#define NOTIFY_STEPS_UUID "d2f362f4-6542-4b13-be5e-8c81d423a347"
#define NOTIFY_STACKING_STEPS_TAKEN_UUID "c14b46e2-553a-4664-98b0-653494882964"
#define TAKE_PICTURE_UUID "74c3cf2a-a5ce-4ee3-9a31-6a997cfc89e3"

// Shared variables (between multiple files)
extern AccelStepper STEPPER_MOTOR;

extern String STACK_PROGRESS_STATE;
extern int STACK_PRE_SHUTTER_WAIT_TIME;
extern int STACK_POST_SHUTTER_WAIT_TIME;
extern int STACK_SHUTTERS_PER_STEP;
extern int STACK_STEP_SIZE;
extern String STACK_MOVEMENT_DIRECTION;
extern int STACK_START_POSITION;
extern int STACK_NUMBER_OF_STEPS_TO_TAKE;
extern bool STACK_RETURN_TO_START_POSITION;

#endif