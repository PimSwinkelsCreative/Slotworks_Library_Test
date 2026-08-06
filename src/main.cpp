#include <Arduino.h>
#include <Slotworks.h>

int16_t counter = 0;

uint64_t lastUIUpdate = 0;
uint16_t UIUpdateInterval = 50; // 20Hz ui update rate

uint16_t dmxOutputAddress = 10; // address to write the values to
uint16_t dmxInputAddress = 0;

uint64_t lastDmxPoll = 0;
const uint16_t dmxPollInterval = 5; // 200Hz dmx update rate

void onDMXReceived()
{
    uint8_t dmxValue = getDMXValue(1);
    setDisplayValue(dmxValue);
}

void updateDMXAddress(uint16_t addr)
{
    dmxInputAddress = addr;
    startDisplayBlink();
    Serial.println("DMX address updated! Set to: " + String(dmxInputAddress));
}

void setup()
{
    Serial.begin(115200);
    delay(3000);
    Serial.println("TEST TEST TEST");
    setupSlotworks();
    setupDMX(onDMXReceived, UART_NUM_1); // setup DMX on uart 1
    // setUserInterfaceMode(DMXADDR, updateDMXAddress); // set the user interface mode to dmx input address mode
    setUserInterfaceMode(VALUE);
}

void loop()
{
    if (millis() - lastUIUpdate >= UIUpdateInterval) {
        lastUIUpdate = millis();
        updateUserInterface();
    }

    if (millis() - lastDmxPoll >= dmxPollInterval) {
        lastDmxPoll = millis();
        updateDMXInput();
    }
}