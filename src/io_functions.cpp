#include <AccelStepper.h>
#include "settings.h"
#include "io_functions.h"

AccelStepper stepperMotor(AccelStepper::DRIVER, MOTOR_PUL_PIN, MOTOR_DIR_PIN);

// Move the rail an amount of steps (direction specified with a positive or negative number)
void takeSteps(int stepAmount, int acceleration)
{
    // TODO: Test with AccelStepper library
    stepperMotor.setMaxSpeed(250);
    stepperMotor.setAcceleration(acceleration);

    stepperMotor.move(stepAmount);
    stepperMotor.runToPosition();

    // Set direction
    // if (stepAmount < 0)
    // {
    //     // Backwards
    //     digitalWrite(MOTOR_DIR_PIN, LOW);
    //     stepAmount = -stepAmount;
    // }
    // else
    // {
    //     // Forwards
    //     digitalWrite(MOTOR_DIR_PIN, HIGH);
    // }

    // for (int i = 0; i <= stepAmount; i++)
    // {
    //     digitalWrite(MOTOR_PUL_PIN, HIGH);
    //     delayMicroseconds(STEPPER_PULSE_DELAY);
    //     digitalWrite(MOTOR_PUL_PIN, LOW);
    //     delayMicroseconds(STEPPER_PULSE_DELAY);
    // }
}

void setMotorDirection()
{
}

// Take a single or multiple picture(s)
void takePictures(int amount)
{
    for (int i = 1; i <= amount; i++)
    {
        digitalWrite(SHUTTER_PIN, HIGH);
        delay(50);
        digitalWrite(SHUTTER_PIN, LOW);
    }
}