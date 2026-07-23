#include <Arduino.h>
#include <Slotworks.h>

int16_t counter = 0;

void setup()
{
    Serial.begin(115200);
    delay(3000);
    Serial.println("TEST TEST TEST");
    setupSlotworks();
}

void loop()
{
    displayInteger(counter);
    counter++;
    delay(100);
    readButtons();

    if (upButtonPressed()) {
        Serial.println("upbutton pressed!");
    }
    if (downButtonPressed()) {
        Serial.println("downbutton pressed!");
    }
    if (enterButtonPressed()) {
        Serial.println("enterbutton pressed!");
    }
}