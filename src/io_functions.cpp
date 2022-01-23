#include <AccelStepper.h>
#include "settings.h"
#include "io_functions.h"

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