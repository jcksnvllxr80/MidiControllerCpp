#include "mc/adapters/mcu/McuInput.h"

#include <string>

#include "hardware/gpio.h"
#include "hardware/irq.h"   // irq_set_enabled
#include "hardware/sync.h"  // save_and_disable_interrupts / restore_interrupts

#include "mc/adapters/mcu/Log.h"

namespace mc::mcu {

McuInput::McuInput(const IClock& clock, McpExpander& exp, const uint8_t* fswBits, int fswCount,
                   uint8_t selectorBit, uint8_t intA, uint8_t intB, uint8_t encoderA, uint8_t encoderB)
    : clock_(clock),
      exp_(exp),
      fswBits_(fswBits),
      fswCount_(fswCount > 6 ? 6 : fswCount),
      selectorBit_(selectorBit),
      intA_(intA),
      intB_(intB),
      encA_(encoderA),
      encB_(encoderB) {
    buttons_.reserve(fswCount_);
    for (int i = 0; i < fswCount_; ++i) {
        buttons_.emplace_back(std::to_string(i + 1), "long", clock_);
    }
}

// Raw GPIO IRQ on both edges of A/B — exactly how the Pi firmware read the
// encoder. Catches EVERY transition (sampling skipped the brief intermediate
// states). Coexists with the cyw43 bank IRQ via a raw handler. Set in begin().
McuInput* McuInput::s_isr_self_ = nullptr;

void McuInput::gpioIrqHandler() {
    McuInput* s = s_isr_self_;
    if (!s) return;

    // Encoder A/B edges — decode immediately in IRQ so no transition is missed.
    uint32_t ea = gpio_get_irq_event_mask(s->encA_);
    uint32_t eb = gpio_get_irq_event_mask(s->encB_);
    if (ea) gpio_acknowledge_irq(s->encA_, ea);
    if (eb) gpio_acknowledge_irq(s->encB_, eb);
    if (ea || eb) s->decodeEncoder();

    // MCP INTA/INTB rising edge — I2C cannot run in IRQ; set flag for main loop.
    uint32_t ia = gpio_get_irq_event_mask(s->intA_);
    uint32_t ib = gpio_get_irq_event_mask(s->intB_);
    if (ia) gpio_acknowledge_irq(s->intA_, ia);
    if (ib) gpio_acknowledge_irq(s->intB_, ib);
    if (ia || ib) s->intPending_ = true;
}


void McuInput::decodeEncoder() {
    uint32_t now = time_us_32();
    int a = gpio_get(encA_);
    int b = gpio_get(encB_);

    // Per-line debounce: a real transition is accepted only if the line has been
    // quiet for kEncBounceUs; a bounce burst after it is ignored. Collapses the
    // hundreds of spurious edges into one clean transition per line.
    bool changed = false;
    if (a != encAStable_ && static_cast<uint32_t>(now - encALastUs_) >= kEncBounceUs) {
        encAStable_ = a;
        encALastUs_ = now;
        ++encEdgesA_;
        changed = true;
    }
    if (b != encBStable_ && static_cast<uint32_t>(now - encBLastUs_) >= kEncBounceUs) {
        encBStable_ = b;
        encBLastUs_ = now;
        ++encEdgesB_;
        changed = true;
    }
    if (!changed) return;
    ++encEdges_;
    a = encAStable_;
    b = encBStable_;

    // The encoder rests at A=1,B=1 (seq=3).
    // CW:  A dips low while B stays high → seq=2 (A=0,B=1)
    // CCW: B dips low while A stays high → seq=1 (A=1,B=0)
    // seq=0 (both low) is not a valid state for this encoder and is ignored.
    int seq = a + 2 * b;
    int8_t move = (seq == 1) ? 1 : (seq == 2) ? -1 : 0;

    if (move != 0 && static_cast<uint32_t>(now - encLastStepUs_) < kEncCooldownUs)
        move = 0;  // within 100 ms cooldown — suppress

    EncEdge& ev = encEdgeRing_[encEdgeHead_++ % kEncEdgeRing];
    ev = {(int8_t)a, (int8_t)b, (int8_t)seq, move};

    if (move > 0) { ++encDelta_; ++encStepsTotal_; encLastStepUs_ = now; }
    else if (move < 0) { --encDelta_; --encStepsTotal_; encLastStepUs_ = now; }
}

void McuInput::begin() {
    // Encoder A/B: native inputs with pull-ups (idle high, pull low).
    for (uint8_t pin : {encA_, encB_}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }
    // MCP INT lines: active-high push-pull (IOCON INTPOL=1, ODR=0), idle low — a
    // pull-down keeps them defined if the trace floats before the expander drives.
    for (uint8_t pin : {intA_, intB_}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_down(pin);
    }

    // All switches read released at boot (footswitches pulled high; the selector is
    // inverted via IPOL, so "released" is handled in serviceExpander()).
    for (auto& l : level_) l = true;

    encAStable_ = gpio_get(encA_);
    encBStable_ = gpio_get(encB_);
    // Edge interrupts on both A and B (matches the original Pi firmware). A raw
    // handler coexists with cyw43's use of the shared GPIO bank IRQ.
    LOG_I("input", "encoder A=GP%u B=GP%u idle A=%d B=%d", encA_, encB_, encAStable_, encBStable_);
    s_isr_self_ = this;
    // One raw handler covers all four pins (encoder A/B + MCP INTA/INTB).
    // Using a single gpio_add_raw_irq_handler_masked call keeps slot usage at 1.
    gpio_add_raw_irq_handler_masked(
        (1u << encA_) | (1u << encB_) | (1u << intA_) | (1u << intB_),
        &McuInput::gpioIrqHandler);
    gpio_set_irq_enabled(encA_, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(encB_, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    // MCP INT lines are active-high and level-held; catch the rising edge.
    gpio_set_irq_enabled(intA_, GPIO_IRQ_EDGE_RISE, true);
    gpio_set_irq_enabled(intB_, GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
    LOG_I("input", "GPIO IRQ armed enc=GP%u/GP%u mcp-int=GP%u/GP%u",
          encA_, encB_, intA_, intB_);

    double now = clock_.now();
    for (auto& t : changedAt_) t = now;
}

void McuInput::push(const InputEvent& e) {
    ring_[head_] = e;
    head_ = (head_ + 1) % kRing;
    if (head_ == tail_) tail_ = (tail_ + 1) % kRing;  // overwrite oldest if full
}

bool McuInput::accept(int idx, bool level, double nowS) {
    if (level == level_[idx]) return false;
    if (nowS - changedAt_[idx] < kDebounce) return false;
    level_[idx] = level;
    changedAt_[idx] = nowS;
    return true;
}

void McuInput::serviceExpander(double nowS) {
    // intPending_ is set by the IRQ handler when INTA or INTB rises.
    // Clear it atomically before reading GPIO so a second INT that arrives
    // during the I2C read is not lost (it will be caught next poll iteration).
    bool intFired = intPending_;
    if (intFired) intPending_ = false;
    bool selRecheck = selRecheckAt_ > 0.0 && nowS >= selRecheckAt_;
    if (!intFired && !selRecheck) return;
    if (selRecheck) selRecheckAt_ = 0.0;

    const uint16_t word = exp_.readGpio();
    LOG_I("input", "gpio word=0x%04X selBit=%d intA=%d intB=%d",
          word, (word >> selectorBit_) & 1u, gpio_get(intA_), gpio_get(intB_));

    // Footswitches (active-low: bit high == released, low == pressed).
    for (int i = 0; i < fswCount_; ++i) {
        bool released = (word >> fswBits_[i]) & 1u;
        if (accept(i, released, nowS)) {
            ButtonSM::Result r = buttons_[i].onEdge(released);
            if (r.kind == ButtonSM::PressKind::Short) {
                LOG_I("input", "footswitch short btn=%d", i + 1);
                push({InputEvent::Type::FootswitchShort, i + 1, 0});
            } else if (r.kind == ButtonSM::PressKind::Long) {
                LOG_I("input", "footswitch long btn=%d", i + 1);
                push({InputEvent::Type::FootswitchLong, i + 1, 0});
            }
        }
    }

    // Selector / rotary push button. Its IPOL bit is set on the expander, so the
    // reported bit is inverted vs the footswitches: bit high == pressed. We map it
    // back to the same "released" convention and time the hold -> RotaryPress.
    //
    // Uses kSelBounceS (30 ms) instead of the global kDebounce (5 ms) because the
    // rotary push switch bounces significantly without hardware caps, and bounce
    // transitions > 5 ms apart were being accepted as real press/release cycles,
    // causing spurious RotaryPress events and a shifted rotaryStart_ reference.
    bool selReleased = ((word >> selectorBit_) & 1u) == 0;  // bit LOW = released (active-high: VCC when pressed, floats low when released)
    if (selReleased != level_[fswCount_]) {
        if (nowS - changedAt_[fswCount_] >= kSelBounceS) {
            level_[fswCount_] = selReleased;
            changedAt_[fswCount_] = nowS;
            if (!selReleased) {
                rotaryDown_ = true;
                rotaryStart_ = nowS;
                LOG_I("input", "rotary DOWN t=%.3fs", nowS);
            } else if (rotaryDown_) {
                rotaryDown_ = false;
                double hold = nowS - rotaryStart_;
                LOG_I("input", "rotary press hold=%.3fs", hold);
                push({InputEvent::Type::RotaryPress, 0, hold});
            }
        } else {
            // Blocked by debounce. If the pin settles here without bouncing back,
            // no further INT will fire — schedule a forced re-read after the window.
            selRecheckAt_ = changedAt_[fswCount_] + kSelBounceS;
        }
    }
}

void McuInput::service() {
    const double now = clock_.now();

    serviceExpander(now);

    // Drain the net detent delta the encoder IRQ accumulated (decoded there so no
    // edge is missed) into discrete CW/CCW events. Snapshot under a brief IRQ mask
    // so we don't race the handler.
    uint32_t save = save_and_disable_interrupts();
    int32_t delta = encDelta_;
    encDelta_ = 0;
    uint32_t edges = encEdges_;
    uint8_t edgeHead = encEdgeHead_;  // ring watermark: log up to here
    restore_interrupts(save);

    while (encEdgeTail_ != edgeHead) {
        const EncEdge& e = encEdgeRing_[encEdgeTail_++ % kEncEdgeRing];
        if (e.move > 0)
            LOG_I("enc", "edge A=%d B=%d seq=%d  CW",  (int)e.a, (int)e.b, (int)e.seq);
        else if (e.move < 0)
            LOG_I("enc", "edge A=%d B=%d seq=%d  CCW", (int)e.a, (int)e.b, (int)e.seq);
        else
            LOG_I("enc", "edge A=%d B=%d seq=%d  rest", (int)e.a, (int)e.b, (int)e.seq);
    }
    if (delta > 0) LOG_I("enc", "-> CW  delta=+%ld", (long)delta);
    if (delta < 0) LOG_I("enc", "-> CCW delta=%ld",  (long)delta);
    for (; delta > 0; --delta) push({InputEvent::Type::EncoderCW, 0, 0});
    for (; delta < 0; ++delta) push({InputEvent::Type::EncoderCCW, 0, 0});

    // Optional raw-edge probe (e.g. blink the LED), driven from main context.
    if (encoderEdgeDebug_ && edges != lastEdges_) {
        lastEdges_ = edges;
        encoderEdgeDebug_();
    }
}

bool McuInput::poll(InputEvent& out) {
    if (head_ == tail_) service();
    if (head_ == tail_) return false;
    out = ring_[tail_];
    tail_ = (tail_ + 1) % kRing;
    return true;
}

}  // namespace mc::mcu
