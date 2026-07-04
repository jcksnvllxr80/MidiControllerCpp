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

// Raw GPIO IRQ on both edges of encoder A/B — exactly how the Pi firmware read
// the encoder. Catches EVERY transition. Coexists with the cyw43 bank IRQ via a
// raw handler. MCP INTA/INTB are polled in serviceExpander() instead of using
// interrupts, so the encoder has the IRQ to itself.
McuInput* McuInput::s_isr_self_ = nullptr;

void McuInput::gpioIrqHandler() {
    McuInput* s = s_isr_self_;
    if (!s) return;

    uint32_t ea = gpio_get_irq_event_mask(s->encA_);
    uint32_t eb = gpio_get_irq_event_mask(s->encB_);
    if (ea) gpio_acknowledge_irq(s->encA_, ea);
    if (eb) gpio_acknowledge_irq(s->encB_, eb);
    if (ea || eb) s->decodeEncoder(ea, eb);
}


void McuInput::decodeEncoder(uint32_t ea, uint32_t eb) {
    uint32_t now = time_us_32();

    // Both-edge IRQs: an edge on a line means that line TOGGLED. The bouncy line has
    // usually settled back to rest before the handler runs, so a live gpio_get is
    // unreliable — instead we keep an internal record per line and flip it on each
    // accepted edge. The per-line quiet-time gate (kEncBounceUs) collapses each
    // bounce burst so exactly one flip lands per real transition.
    bool changed = false;
    if (ea && static_cast<uint32_t>(now - encALastUs_) >= kEncBounceUs) {
        encAStable_ ^= 1;
        encALastUs_ = now;
        ++encEdgesA_;
        changed = true;
    }
    if (eb && static_cast<uint32_t>(now - encBLastUs_) >= kEncBounceUs) {
        encBStable_ ^= 1;
        encBLastUs_ = now;
        ++encEdgesB_;
        changed = true;
    }
    if (!changed) return;  // edge fell inside the bounce window
    ++encEdges_;
    int a = encAStable_;
    int b = encBStable_;

    // Count a step on these transitions, nothing on any other:
    //   1 → 3 : CW (+1)      0 → 2 : CW (+1)
    //   2 → 3 : CCW (-1)     0 → 1 : CCW (-1)
    int seq = a + 2 * b;
    int8_t prevSeq = (int8_t)encPrevSeq_;  // before update, for the log
    int8_t move = 0;
    if (seq == 3) {
        if (encPrevSeq_ == 1) move = +1;
        else if (encPrevSeq_ == 2) move = -1;
    } else if (seq == 2) {
        if (encPrevSeq_ == 0) move = +1;
    } else if (seq == 1) {
        if (encPrevSeq_ == 0) move = -1;
    }
    encPrevSeq_ = seq;  // every state is a valid direction reference now (incl. 0)

    // After a counted CW/CCW, stay quiet for kEncCooldownUs: don't count another
    // step and don't log the intermediate flips happening during that window. We
    // still flip the internal A/B record above so the decoder stays in sync.
    bool cooling = static_cast<uint32_t>(now - encLastStepUs_) < kEncCooldownUs;
    int8_t countedMove = (move != 0 && !cooling) ? move : 0;
    if (countedMove != 0) encLastStepUs_ = now;  // opens a fresh cooldown window

    if (!cooling) {
        EncFlip& f = encFlipRing_[encFlipHead_++ % kEncFlipRing];
        f = {(int8_t)(ea ? 1 : 0), (int8_t)(eb ? 1 : 0), (int8_t)a, (int8_t)b,
             (int8_t)seq, prevSeq, countedMove};
    }

    if (countedMove > 0) { ++encDelta_; ++encStepsTotal_; }
    else if (countedMove < 0) { --encDelta_; --encStepsTotal_; }
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

    // All switches start as released. Hardware sync below overrides with real state.
    for (auto& l : level_) l = true;

    encAStable_ = gpio_get(encA_);
    encBStable_ = gpio_get(encB_);
    // Edge interrupts on both A and B (matches the original Pi firmware). A raw
    // handler coexists with cyw43's use of the shared GPIO bank IRQ.
    LOG_I("input", "encoder A=GP%u B=GP%u idle A=%d B=%d", encA_, encB_, encAStable_, encBStable_);
    s_isr_self_ = this;
    // Encoder A/B get the raw IRQ handler exclusively. MCP INTA/INTB are polled
    // in serviceExpander() via gpio_get() — the lines stay high until we read the
    // expander GPIO register, so polling never misses a button event.
    gpio_add_raw_irq_handler_masked(
        (1u << encA_) | (1u << encB_),
        &McuInput::gpioIrqHandler);
    gpio_set_irq_enabled(encA_, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(encB_, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
    LOG_I("input", "GPIO IRQ armed enc=GP%u/GP%u (mcp-int GP%u/GP%u polled)",
          encA_, encB_, intA_, intB_);

    double now = clock_.now();
    for (auto& t : changedAt_) t = now;

    // Sync level_[] to the real hardware state. For footswitches (active-low, pull-up
    // on) bit=1 reliably means released, so latching the boot read is correct.
    const uint16_t initWord = exp_.readGpio();
    for (int i = 0; i < fswCount_; ++i)
        level_[i] = (initWord >> fswBits_[i]) & 1u;
    // The selector has NO pull-up (active-high, IPOL-inverted); its idle level is not
    // defined at boot, so do NOT latch the possibly-floating read — assume released
    // (the button is effectively never held at power-on). The kSelPollS backstop in
    // serviceExpander() resyncs within a few ms if this assumption is ever wrong,
    // and avoids a stuck "already pressed" state that silently eats the first press.
    level_[fswCount_] = true;  // released
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
    // MCP INT lines are active-high and held asserted until we read the GPIO
    // register. Poll them directly — no interrupt needed since the line stays
    // high until serviceExpander() clears it via exp_.readGpio().
    //
    // We ALSO force a read on a fixed interval (kSelPollS) regardless of INT. The
    // selector has no pull-up, so its boot level is undefined: if the MCP latched
    // "pressed" as its compare baseline, the first press makes no edge and INT never
    // asserts. The periodic read samples the level directly (catching that first
    // press) and re-establishes the MCP baseline so later edges fire normally.
    bool intFired = gpio_get(intA_) || gpio_get(intB_);
    bool selRecheck = selRecheckAt_ > 0.0 && nowS >= selRecheckAt_;
    bool periodic = nowS >= nextPollS_;
    if (!intFired && !selRecheck && !periodic) return;
    if (periodic) nextPollS_ = nowS + kSelPollS;
    if (selRecheck) selRecheckAt_ = 0.0;

    const uint16_t word = exp_.readGpio();

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

    // Selector / rotary push button. IPOL=1 on B7 inverts the hardware level so the
    // register reads 1=released / 0=pressed — the same convention as the footswitches.
    // "released" is therefore simply bit-is-1, no comparison needed.
    //
    // Uses kSelBounceS (30 ms) instead of the global kDebounce (5 ms) because the
    // rotary push switch bounces significantly without hardware caps, and bounce
    // transitions > 5 ms apart were being accepted as real press/release cycles,
    // causing spurious RotaryPress events and a shifted rotaryStart_ reference.
    bool selReleased = (word >> selectorBit_) & 1u;  // IPOL: 1=released(pin LOW), 0=pressed(pin HIGH)
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
    uint8_t flipHead = encFlipHead_;  // ring watermark to log up to
    restore_interrupts(save);

    // Log only valid, debounced transitions (one line per accepted flip).
    while (encFlipTail_ != flipHead) {
        const EncFlip& f = encFlipRing_[encFlipTail_++ % kEncFlipRing];
        const char* dir = f.move > 0 ? "CW" : f.move < 0 ? "CCW" : "rest";
        LOG_I("enc", "flip edgeA=%d edgeB=%d -> A=%d B=%d seq=%d prev=%d %s",
              (int)f.ea, (int)f.eb, (int)f.a, (int)f.b, (int)f.seq, (int)f.prevSeq, dir);
    }

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
