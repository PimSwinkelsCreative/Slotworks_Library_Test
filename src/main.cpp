#include <Arduino.h>
#include <Slotworks.h>
#include <esp_task_wdt.h>

int16_t counter = 0;

uint64_t lastUIUpdate = 0;
uint16_t UIUpdateInterval = 50; // 20Hz ui update rate

uint16_t dmxOutputAddress = 1; // address to write the values to
uint16_t dmxInputAddress = 1; // DMX channels are 1-based; channel 0 is the DMX start code.

// DMX timing
uint64_t lastDmxSendUpdate = 0;
uint64_t lastSerialPrintMs = 0;

// DMX Dither configuration :
// currently configured for ETC Desire in 10 channel direct mode
const uint16_t numberOfDitherChannels = 8; // number of dither channels to output
const uint16_t numberOfUnditheredchannels = 2; // number of undithered channels to output
const uint8_t ditherChannelInputResolution = 16; // number of bits of input resolution for the dithered channels
const uint16_t numberOfOutputChannels = numberOfDitherChannels + numberOfUnditheredchannels; // total number of channels to output
const uint16_t numberInputChannels = 2 * numberOfDitherChannels + numberOfUnditheredchannels; // number of input channels to read from DMX
const bool channelIsDitherChannel[numberOfOutputChannels] = { true, true, true, true, true, true, true, true, false, false }; // which channels to dither (true) or not (false)

// dither timing configuration
const uint16_t dmxOutputFrequencyHz = 800; // DMX output update rate in Hz
const uint16_t ditherMinNonZeroFrequencyHz = 40; // minimum non-zero output frequency in Hz
const uint32_t dmxSendUpdateInterval = 1000000 / dmxOutputFrequencyHz; // DMX frame interval in microseconds
const uint32_t serialPrintIntervalMs = 1000;

// Dithering experiment: map a 16-bit color value to 8-bit DMX output.
// The goal is to keep the apparent brightness smooth while still using a limited 8-bit channel depth.

// =====================================================
// Generic N-bit -> 8-bit temporal dither
// =====================================================

class DitherTo8 {
public:
    DitherTo8(uint8_t inputBits, uint16_t minimumNonZeroFrequencyHz = 0, uint16_t outputFrequencyHz = 1000)
    {
        shift = inputBits - 8;
        threshold = 1U << shift;
        mask = threshold - 1;
        setMinimumNonZeroFrequency(minimumNonZeroFrequencyHz, outputFrequencyHz);
    }

    void setMinimumNonZeroFrequency(uint16_t minimumNonZeroFrequencyHz, uint16_t outputFrequencyHz)
    {
        minNonZeroValue = 0;

        if (outputFrequencyHz == 0 || minimumNonZeroFrequencyHz == 0) {
            return;
        }

        uint32_t value = ((uint32_t)minimumNonZeroFrequencyHz * threshold) / outputFrequencyHz;
        if (value > threshold) {
            value = threshold;
        }
        minNonZeroValue = (uint16_t)value;
    }

    uint8_t convert(uint16_t value)
    {
        // The low-end clamp prevents values that would otherwise spend most of their time at 0 from visibly
        // flickering between 0 and 1 when the brightness is only a few counts above the floor.
        if (value < minNonZeroValue) {
            acc = 0;
            previousValue = value;
            return 0;
        }

        // The integer part is the base output level. The low bits are kept in the accumulator so we can
        // distribute the fractional remainder across time and give the impression of a smoother value than the
        // 8-bit channel can represent.
        uint8_t out = value >> shift;

        acc += (value & mask);

        if (acc >= threshold) {
            if(out<255){
                //only increment if we are not at the max value already, otherwise we will wrap around to 0 and cause a flicker
                out++;
            }
            acc -= threshold;
        }
        previousValue = value;
        return out;
    }

private:
    uint16_t acc = 0;
    uint16_t previousValue = 0;
    uint16_t threshold;
    uint16_t mask;
    uint8_t shift;
    uint16_t minNonZeroValue = 0;
};

// One dither instance per dithered channel. It converts a 16-bit brightness value to an 8-bit DMX channel while
// maintaining smooth dimming at low levels through temporal dithering.
DitherTo8* ditherChannels[numberOfDitherChannels];

void onDMXReceived()
{
    // Keep the UI/diagnostic display in sync with the configured DMX input start address.
    setDisplayValue(dmxInputAddress);
}

void updateDMXAddress(uint16_t addr)
{
    dmxInputAddress = addr;
    setDisplayValue(dmxInputAddress);
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
    updateDMXOutput(5); // send 5 DMX data channels after the start code
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
    updateDMXOutput(5); // send 5 DMX data channels after the start code
}

void dmxOutputTask(void* parameter)
{
    // This task runs continuously on core 0 and never yields to the scheduler. The default TWDT monitors idle
    // tasks, so we must reconfigure it to stop watching idle on this core and only feed it from this task.
    esp_task_wdt_config_t wdtConfig = {
        .timeout_ms = 5000,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&wdtConfig);
    esp_task_wdt_add(NULL);

    while (true) {
        uint64_t now = micros();
        if (now - lastDmxSendUpdate >= dmxSendUpdateInterval) {
            lastDmxSendUpdate = now;

            uint16_t inputChannelIndex = dmxInputAddress;
            for (int i = 0; i < numberOfOutputChannels; i++) {
                if (channelIsDitherChannel[i]) {
                    uint16_t value16 = ((uint16_t)getDMXValue(inputChannelIndex) << 8) | (uint16_t)getDMXValue(inputChannelIndex + 1);
                    dmxSetByte(dmxOutputAddress + i, ditherChannels[i]->convert(value16));
                    inputChannelIndex += 2;
                } else {
                    dmxSetByte(dmxOutputAddress + i, getDMXValue(inputChannelIndex));
                    inputChannelIndex += 1;
                }
            }
            updateDMXOutput(numberOfOutputChannels); // send the configured number of DMX data channels after the start code
        }

        esp_task_wdt_reset();
    }
}

void setup()
{
    Serial.begin(115200);
    delay(3000);
    setupSlotworks();
    setUserInterfacePollInterval(50); // poll the UI at 20Hz

    // The Slotworks UI is used to configure the DMX input start address. The output address remains fixed at 1.
    setUserInterfaceMode(DMXADDR, updateDMXAddress);
    setDisplayValue(dmxInputAddress);

    // perform a check to see if the output frame is short enough to be sent at the required output framerate
    // 160us is reserved for the startframe, 44us for every byte to write to the output
    if (dmxOutputFrequencyHz * (160 + numberOfOutputChannels * 44) >= 1000000) {
        while (1) {
            Serial.println("ERROR: OUTPUT DMX FRAME TOO LONG");
            delay(1000);
        }
    }

    // create the ditherchannels
    for (int i = 0; i < numberOfDitherChannels; i++) {
        ditherChannels[i] = new DitherTo8(ditherChannelInputResolution, ditherMinNonZeroFrequencyHz, dmxOutputFrequencyHz);
    }

    // Use a separate RX UART and TX UART to avoid the shared-port corruption.
    // The board pins are fixed, so pass in the fixed DMX RX/TX pins explicitly.
    setupDMX(onDMXReceived, UART_NUM_1, UART_NUM_2, DMX_RX, DMX_TX, DMX_TX_EN);
    enableDMXOutput(true); // enable the DMX TX driver path once
    xTaskCreatePinnedToCore(dmxOutputTask, "dmxOutputTask", 10000, NULL, 1, NULL, 0);
}

void loop()
{
    uint32_t nowMs = millis();

    updateUserInterface();

    if (nowMs - lastSerialPrintMs >= serialPrintIntervalMs) {
        lastSerialPrintMs = nowMs;

        //add periodic debug prints here
    }
}