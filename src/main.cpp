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
const uint16_t dmxSendUpdateInterval = 1000; // 1kHz dmx send update rate

// dithering experiment
#define COLOR_MAX_16BIT 500

// =====================================================
// Generic N-bit -> 8-bit temporal dither
// =====================================================

class DitherTo8 {
public:
    DitherTo8(uint8_t inputBits)
    {
        shift = inputBits - 8;
        threshold = 1U << shift;
        mask = threshold - 1;
    }

    uint8_t convert(uint16_t value)
    {
        static uint16_t previousValue = 0;
        uint8_t out = value >> shift;

        if (abs((int)value - (int)previousValue) > 32) {
            acc >>= 1;  //this should reduce low level flicker
        }

        acc += (value & mask);

        if (acc >= threshold) {
            out++;
            acc -= threshold;
        }
        previousValue = value;
        return out;
    }

private:
    uint16_t acc = 0;
    uint16_t threshold;
    uint16_t mask;
    uint8_t shift;
};

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

    if (probeState) {
        masterValue = 100;
    } else {
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

// =====================================================
// Ditherers
// =====================================================

DitherTo8 redDither(16);
DitherTo8 greenDither(16);
DitherTo8 blueDither(16);

// =====================================================
// Generate a 16-bit sine fade
// =====================================================

uint16_t fade16(float phase)
{
    float s = (sinf(phase) + 1.0f) * 0.5f;

    return (uint16_t)(s * COLOR_MAX_16BIT + 0.5f);
}

void setup()
{
    Serial.begin(115200);
    delay(3000);
    setupSlotworks();
    setupDMX(onDMXReceived, UART_NUM_1); // setup DMX on uart 1
    enableDMXOutput(true); // enable DMX output
    // setUserInterfaceMode(DMXADDR, updateDMXAddress); // set the user interface mode to dmx input address mode
    // setUserInterfaceMode(VALUE);
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

    //     if (millis() - lastDmxSendUpdate >= dmxSendUpdateInterval) {
    //         lastDmxSendUpdate = millis();
    //         // updateDMXOutputRainbow();
    //         dmxUpdateStrobe();
    //     }

    uint64_t now = micros();
    if (now - lastDmxSendUpdate >= dmxSendUpdateInterval) {
        lastDmxSendUpdate = now;
        float t = millis() * 0.001f;
        uint16_t brightness = fade16(t / 5.0f); // fade 0.1Hz
        dmxSetByte(dmxOutputAddress, redDither.convert(brightness));
        dmxSetByte(dmxOutputAddress + 1, greenDither.convert(brightness));
        dmxSetByte(dmxOutputAddress + 2, blueDither.convert(brightness));
        updateDMXOutput(4); // send 4 channels of data
    }
}