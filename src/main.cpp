#include <Arduino.h>
#include <Slotworks.h>

int16_t counter = 0;

uint64_t lastUIUpdate = 0;
uint16_t UIUpdateInterval = 50; // 20Hz ui update rate

uint16_t dmxOutputAddress = 1; // address to write the values to
uint16_t dmxInputAddress = 0;

// dmx
uint64_t lastDmxPoll = 0;
const uint16_t dmxPollInterval = 5; // 200Hz dmx update rate
uint64_t lastDmxSendUpdate = 0;
const uint16_t dmxSendUpdateInterval = 23; // 44hz dmx send update rate

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

void updateDMXOutputRainbow()
{
    static int16_t redValue = 0;
    static int16_t greenValue = 0;
    static int16_t blueValue = 255;
    static int16_t strobeValue = 0;
    static int16_t masterValue = 100;
    static int16_t fadeincrement = 1;

    static uint8_t fadeState = 0;

    switch (fadeState) {
    case 0:
        blueValue -= fadeincrement;
        redValue += fadeincrement;
        greenValue = 0;
        if (redValue >= 255 || blueValue <= 0) {
            redValue = 255;
            blueValue = 0;
            fadeState = 1;
        }
        break;
    case 1:
        redValue -= fadeincrement;
        greenValue += fadeincrement;
        blueValue = 0;
        if (greenValue >= 255 || redValue <= 0) {
            greenValue = 255;
            redValue = 0;
            fadeState = 2;
        }
        break;
    case 2:
        greenValue -= fadeincrement;
        blueValue += fadeincrement;
        redValue = 0;
        if (blueValue >= 255 || greenValue <= 0) {
            blueValue = 255;
            greenValue = 0;
            fadeState = 0;
        }
        break;
    default:
        fadeState = 0;
        break;
    }
    dmxSetByte(dmxOutputAddress, redValue);
    dmxSetByte(dmxOutputAddress + 1, greenValue);
    dmxSetByte(dmxOutputAddress + 2, blueValue);
    dmxSetByte(dmxOutputAddress + 3, strobeValue);
    dmxSetByte(dmxOutputAddress + 4, masterValue);
    updateDMXOutput(6); // send 6 channels of data
}

void dmxUpdateStrobe()
{
    static int16_t redValue = 255;
    static int16_t greenValue = 255;
    static int16_t blueValue = 255;
    static int16_t strobeValue = 0;
    static int16_t masterValue = 0;
    static bool probeState = false;

    if(probeState){
        masterValue = 100;
    } else{
        masterValue = 0;
    }

    probeState = !probeState;

    dmxSetByte(dmxOutputAddress, redValue);
    dmxSetByte(dmxOutputAddress + 1, greenValue);
    dmxSetByte(dmxOutputAddress + 2, blueValue);
    dmxSetByte(dmxOutputAddress + 3, strobeValue);
    dmxSetByte(dmxOutputAddress + 4, masterValue);
    updateDMXOutput(6); // send 6 channels of data
}

void setup()
{
    Serial.begin(115200);
    delay(3000);
    setupSlotworks();
    setupDMX(onDMXReceived, UART_NUM_1); // setup DMX on uart 1
    enableDMXOutput(true); // enable DMX output
    setUserInterfaceMode(DMXADDR, updateDMXAddress); // set the user interface mode to dmx input address mode
    setUserInterfaceMode(VALUE);
}

void loop()
{
    // if (millis() - lastUIUpdate >= UIUpdateInterval) {
    //     lastUIUpdate = millis();
    //     updateUserInterface();
    // }

    // if (millis() - lastDmxPoll >= dmxPollInterval) {
    //     lastDmxPoll = millis();
    //     updateDMXInput();
    // }

    if (millis() - lastDmxSendUpdate >= dmxSendUpdateInterval) {
        lastDmxSendUpdate = millis();
        // updateDMXOutputRainbow();
        dmxUpdateStrobe();
    }
}