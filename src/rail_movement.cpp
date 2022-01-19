#include "settings.h"
#include "rail_movement.h"

void takeSteps(int stepAmount)
{
    // Set direction
    if (stepAmount < 0)
    {
        // Backwards
        digitalWrite(MOTOR_DIR_PIN, LOW);
        stepAmount = -stepAmount;
    }
    else
    {
        // Forwards
        digitalWrite(MOTOR_DIR_PIN, HIGH);
    }

    for (int i = 0; i <= stepAmount; i++)
    {
        digitalWrite(MOTOR_PUL_PIN, HIGH);
        delayMicroseconds(STEPPER_PULSE_DELAY);
        digitalWrite(MOTOR_PUL_PIN, LOW);
        delayMicroseconds(STEPPER_PULSE_DELAY);
    }
}