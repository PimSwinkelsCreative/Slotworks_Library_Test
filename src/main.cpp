#include <Arduino.h>
#include <Slotworks.h>

int16_t counter = 0;

uint32_t lastUIUpdate = 0;
uint16_t UIUpdateInterval = 50; //20Hz ui update rate

uint16_t dmxOutputAddress = 10; // address to write the values to
uint16_t dmxInputAddress = 0;

void onDMXReceived()
{
}

void updateDMXAddress(uint16_t addr)
{
    dmxInputAddress = addr;
    Serial.println("DMX address updated! Set to: " + String(dmxInputAddress));
}

void setup()
{
    Serial.begin(115200);
    delay(3000);
    Serial.println("TEST TEST TEST");
    setupSlotworks();
    setupDMX(onDMXReceived, dmxInputAddress, 1); // setup DMX on uart
    setUserInterfaceMode(DMXADDR, updateDMXAddress); // set the user interface mode to dmx input address mode
}

void loop()
{

    if (millis() - lastUIUpdate >= UIUpdateInterval) {
        lastUIUpdate = millis();
        updateUserInterface();
    }
}