#include "mc/adapters/mcu/McuMidiOut.h"

#include "hardware/gpio.h"

namespace mc::mcu {

McuMidiOut::McuMidiOut(uart_inst_t* uart, unsigned txPin) : uart_(uart), txPin_(txPin) {}

void McuMidiOut::begin() {
    uart_init(uart_, 31250);
    gpio_set_function(txPin_, GPIO_FUNC_UART);
    uart_set_format(uart_, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart_, true);
}

void McuMidiOut::send(const MidiMessage& msg) {
    for (uint8_t b : msg.bytes()) {
        uart_putc_raw(uart_, static_cast<char>(b));
    }
}

}  // namespace mc::mcu
