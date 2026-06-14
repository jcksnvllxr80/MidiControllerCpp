// Firmware entry point for the Pico 2 W (RP2350). Wires the MCU adapters to the
// same domain core the desktop sim uses, then runs its own poll loop.
//
// Note: it does NOT call Application::run() — that loop stops when input.poll()
// returns false, which is the right contract for scripted desktop input but not
// for a real-time device. Here poll() returns false simply means "no event yet".
#include "hardware/flash.h"  // FLASH_SECTOR_SIZE, PICO_FLASH_SIZE_BYTES
#include "pico/stdlib.h"

#include "mc/adapters/mcu/EmbeddedData.h"
#include "mc/adapters/mcu/FlashKv.h"
#include "mc/adapters/mcu/McuClock.h"
#include "mc/adapters/mcu/McuConfigStore.h"
#include "mc/adapters/mcu/McuInput.h"
#include "mc/adapters/mcu/McuLed.h"
#include "mc/adapters/mcu/McuMidiOut.h"
#include "mc/adapters/mcu/McuTempoOut.h"
#include "mc/adapters/mcu/Pins.h"
#include "mc/adapters/mcu/Ssd1306Display.h"
#include "mc/adapters/mcu/StdioConfigTransport.h"
#include "mc/app/Application.h"
#include "mc/app/EditorProtocol.h"

using namespace mc;
using namespace mc::mcu;

int main() {
    stdio_init_all();

    McuClock clock;
    McuMidiOut midiA(uart0, pins::MIDI_A_TX);
    McuMidiOut midiB(uart1, pins::MIDI_B_TX);
    TeeMidiOut midi(&midiA, &midiB);  // mirror to both DIN jacks
    McuTempoOut tempo(pins::TEMPO, 4);
    Ssd1306Display display(i2c1, pins::OLED_I2C_ADDR, pins::OLED_SDA, pins::OLED_SCL);
    McuLed led(pins::LED_R, pins::LED_G, pins::LED_B);
    McuInput input(clock, pins::FOOTSWITCH, 6, pins::ENCODER_A, pins::ENCODER_B, pins::ROTARY_PB);
    // Persist "save defaults" in the last flash sector (survives reboot).
    FlashKv persist(PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    McuConfigStore store(kEmbeddedData, kEmbeddedDataCount, &persist);

    midiA.begin();
    midiB.begin();
    tempo.begin();
    display.begin();
    led.begin();
    input.begin();

    // The Application drives the rig; the loop also services the editor link over
    // the USB CDC. (Transport isn't passed to the Application — we run the loop.)
    Application app({&store, &midi, &tempo, &display, &led, &clock, &input, nullptr});
    app.setup();

    EditorProtocol protocol(store, app);
    StdioConfigTransport transport(protocol);
    transport.begin();

    InputEvent ev;
    while (true) {
        while (input.poll(ev)) app.handleEvent(ev);
        transport.poll();
        tight_loop_contents();
    }
}
