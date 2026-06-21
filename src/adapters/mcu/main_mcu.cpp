// Firmware entry point for the Pico 2 W (RP2350). Wires the MCU adapters to the
// same domain core the desktop sim uses, then runs its own poll loop.
//
// Note: it does NOT call Application::run() — that loop stops when input.poll()
// returns false, which is the right contract for scripted desktop input but not
// for a real-time device. Here poll() returns false simply means "no event yet".
#include <cstdio>

#include "hardware/flash.h"  // FLASH_SECTOR_SIZE, PICO_FLASH_SIZE_BYTES
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"  // pico_get_unique_board_id_string

#include "hardware/i2c.h"
#include "hardware/spi.h"

#include "mc/adapters/mcu/EmbeddedData.h"
#include "mc/adapters/mcu/Log.h"
#include "mc/adapters/mcu/FlashKv.h"
#include "mc/adapters/mcu/McpExpander.h"
#include "mc/adapters/mcu/McuClock.h"
#include "mc/adapters/mcu/McuConfigStore.h"
#include "mc/adapters/mcu/McuInput.h"
#include "mc/adapters/mcu/McuLed.h"
#include "mc/adapters/mcu/LedPulse.h"
#include "mc/adapters/mcu/McuMidiOut.h"
#include "mc/adapters/mcu/McuSystemControl.h"
#include "mc/adapters/mcu/Pins.h"
#include "mc/adapters/mcu/Ssd1306Display.h"
#include "mc/adapters/mcu/WifiManager.h"
#include "mc/app/Application.h"
#include "mc/app/EditorProtocol.h"
#ifdef MC_ENABLE_USB_EDITOR
#include "mc/adapters/mcu/UsbConfigTransport.h"  // raw USB vendor link (WinUSB), stdio on UART
#else
#include "mc/adapters/mcu/StdioConfigTransport.h"  // editor link over the stdio USB CDC
#endif

using namespace mc;
using namespace mc::mcu;

int main() {
    stdio_init_all();
    LOG_I("boot", "stdio ready");

    if (watchdog_caused_reboot()) {
        LOG_W("boot", "recovered from a watchdog hang");
    }

    McuClock clock;

    // Shared I2C bus (i2c0): the MCP23017 expander (0x22) and the PIC MIDI bridge
    // (0x04) both live here, exactly as on the Pi's single I2C bus. Bring it up once.
    i2c_init(i2c0, 400'000);
    gpio_set_function(pins::I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(pins::I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(pins::I2C_SDA);
    gpio_pull_up(pins::I2C_SCL);
    LOG_I("boot", "I2C bus up at 400 kHz (SDA=GP%u SCL=GP%u)", pins::I2C_SDA, pins::I2C_SCL);

    McuMidiOut midi(i2c0, pins::MIDI_PIC_ADDR);  // raw MIDI -> PIC -> 6 jacks
    McpExpander expander(i2c0, pins::MCP23017_ADDR);
    Ssd1306Display display(spi0, pins::OLED_SCLK, pins::OLED_MOSI, pins::OLED_CS, pins::OLED_DC,
                           pins::OLED_RST);
    McuLed led(pins::LED_R, pins::LED_G, pins::LED_B);
    McuInput input(clock, expander, pins::FOOTSWITCH_BITS, 5, pins::SELECTOR_BIT, pins::MCP_INT_A,
                   pins::MCP_INT_B, pins::ENCODER_A, pins::ENCODER_B);
    // Persist "save defaults" in the last flash sector (survives reboot).
    FlashKv persist(PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    McuConfigStore store(kEmbeddedData, kEmbeddedDataCount, &persist);

    LOG_I("boot", "display begin");
    display.begin();   // bring up the OLED FIRST so a stuck I2C bus can't keep it dark
    LOG_I("boot", "expander begin");
    expander.begin();
    LOG_I("boot", "led begin");
    led.begin();
    LOG_I("boot", "input begin");
    input.begin();

    // The Application drives the rig; the loop also services the editor link over
    // the USB CDC. (Transport isn't passed to the Application — we run the loop.)
    Application app({&store, &midi, &display, &led, &clock, &input, nullptr});
    LOG_I("boot", "app setup");
    app.setup();

    // WiFi: connects to a saved network on boot; the editor link also runs over
    // TCP (same protocol) + mDNS. Credentials are set over USB the first time.
    WifiManager wifi(store);
    wifi.setStatusSink([&display](const std::string& s) { display.setMessage(s); });

    McuSystemControl sys;  // reboot / BOOTSEL on editor command
    EditorProtocol protocol(store, app, &wifi, &sys);

    // Surface the RP2350 board id in `identify` so the editor can tell units apart
    // and dedupe the same device seen over both USB and WiFi.
    char boardId[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(boardId, sizeof(boardId));
    protocol.setDeviceId(boardId);
    LOG_I("boot", "board id: %s", boardId);

    wifi.setProtocol(&protocol);

#ifdef MC_ENABLE_USB_EDITOR
    UsbConfigTransport transport(protocol);  // raw USB vendor link (WinUSB)
    LOG_I("boot", "transport: raw USB vendor (WinUSB)");
#else
    StdioConfigTransport transport(protocol);  // USB CDC link
    LOG_I("boot", "transport: stdio USB CDC");
#endif
    transport.begin();
    LOG_I("boot", "wifi begin");
    wifi.begin();  // CYW43 init + auto-connect if a known network is enabled

    // Onboard LED blips ~50ms on every handled input/command. Safe to drive only
    // after wifi.begin() has run cyw43_arch_init (the LED is on the CYW43 chip).
    LedPulse activityLed(50);
    app.setActivitySink([&activityLed] { activityLed.trigger(); });

    // Enable the hardware watchdog only after the (potentially slow) boot init is
    // done. Any single loop iteration that hangs > 8 s reboots the device. The
    // longest in-loop work — a ~30 KB get_pedal parse/stream or a debounced flash
    // write — is well under that, and WiFi is serviced non-blocking in poll().
    watchdog_enable(8000, /*pause_on_debug=*/true);
    LOG_I("boot", "watchdog armed 8s — entering main loop");

    InputEvent ev;
    while (true) {
        watchdog_update();
        while (input.poll(ev)) app.handleEvent(ev);
        transport.poll();
        wifi.poll();
        display.tick(clock.now());  // drive the title marquee
        activityLed.update();       // clear the activity blip after its window
        app.tick();  // debounced "save defaults" flush, off the input path
        sys.poll();  // performs a scheduled reboot/BOOTSEL once the ack has flushed
        tight_loop_contents();
    }
}
