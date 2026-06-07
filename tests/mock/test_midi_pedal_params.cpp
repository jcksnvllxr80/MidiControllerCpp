// Data-driven: every reachable param on every real pedal becomes its own test
// case. For each top-level Knobs/Switches & Parameters entry we feed a
// representative value and assert the exact MIDI (or that nothing is sent for
// non-emitting / sub-group entries). Expectations are derived from the config,
// so this pins the whole setParams -> convert_to_int -> MidiMessage path against
// the real data.
#include <set>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mc/domain/MidiPedal.h"
#include "support/Fixtures.h"
#include "support/RecordingMidiOut.h"

using namespace mc;
using namespace mc::test;
using Bytes = std::vector<uint8_t>;

namespace {

struct ParamCase {
    std::string pedal;
    std::string param;
    Value input;
    bool emit = false;
    Bytes expected;
    std::string id;
};

Bytes cc(int ch, long num, long val) {
    return MidiMessage::controlChange(ch, static_cast<int>(num), static_cast<int>(val)).bytes();
}

std::vector<ParamCase> genParamCases() {
    std::vector<ParamCase> out;
    for (const std::string& pedal : allPedals()) {
        const PedalConfig& cfg = pedalConfig(pedal);
        const int ch = channelFor(pedal);
        const Action* ks = cfg.group("Knobs/Switches");
        const Action* pr = cfg.group("Parameters");

        // Unique param names in setParams resolution order (K/S first, then Parameters).
        std::vector<std::string> names;
        std::set<std::string> seen;
        for (const Action* grp : {ks, pr}) {
            if (!grp) continue;
            for (const auto& kv : grp->children)
                if (seen.insert(kv.first).second) names.push_back(kv.first);
        }

        for (const std::string& name : names) {
            const Action* k = ks ? ks->child(name) : nullptr;
            const Action* p = pr ? pr->child(name) : nullptr;
            const Action* eff = (k && k->hasTruthyCc()) ? k : ((p && p->hasTruthyCc()) ? p : nullptr);
            const std::string base = pedal + "_" + sanitize(name);

            auto push = [&](Value in, bool emit, Bytes exp, const std::string& sfx) {
                out.push_back({pedal, name, std::move(in), emit, std::move(exp), base + "_" + sfx});
            };

            if (!eff) {  // sub-group / cc:0 / unresolvable -> nothing sent
                push(Value{1L}, false, {}, "noemit");
                continue;
            }
            const long ccnum = *eff->cc;

            if (!eff->dict.empty()) {
                // Representative samples (first / middle / last) — the dispatch path is
                // the same for every entry, so a few cover it without inflating the count.
                std::set<size_t> picks = {0, eff->dict.size() / 2, eff->dict.size() - 1};
                for (size_t i : picks) {
                    const auto& kv = eff->dict[i];
                    push(Value{kv.first}, true, cc(ch, ccnum, kv.second), "dict" + std::to_string(i));
                }
            } else if (eff->on && eff->off) {
                push(Value{std::string("on")}, true, cc(ch, ccnum, *eff->on), "on");
                push(Value{std::string("off")}, true, cc(ch, ccnum, *eff->off), "off");
                push(Value{true}, true, cc(ch, ccnum, *eff->on), "btrue");
                push(Value{false}, true, cc(ch, ccnum, *eff->off), "bfalse");
                push(Value{EngagedRef{"x", true}}, true, cc(ch, ccnum, *eff->on), "engon");
                push(Value{EngagedRef{"x", false}}, true, cc(ch, ccnum, *eff->off), "engoff");
            } else if (eff->press && eff->release) {
                push(Value{std::string("press")}, true, cc(ch, ccnum, *eff->press), "press");
                push(Value{std::string("release")}, true, cc(ch, ccnum, *eff->release), "release");
            } else if (eff->min && eff->max) {
                long lo = *eff->min, hi = *eff->max, mid = (lo + hi) / 2;
                push(Value{lo}, true, cc(ch, ccnum, lo), "min");
                push(Value{hi}, true, cc(ch, ccnum, hi), "max");
                push(Value{mid}, true, cc(ch, ccnum, mid), "mid");
                push(Value{hi + 1}, false, {}, "over");
                push(Value{lo - 1}, false, {}, "under");
            } else if (eff->value) {
                push(Value{std::monostate{}}, true, cc(ch, ccnum, *eff->value), "valnone");
                push(Value{*eff->value}, true, cc(ch, ccnum, *eff->value), "valexact");
            } else {
                push(Value{std::monostate{}}, false, {}, "noval");
            }
        }
    }
    return out;
}

}  // namespace

class PedalParam : public ::testing::TestWithParam<ParamCase> {};

TEST_P(PedalParam, EmitsExpected) {
    const ParamCase& c = GetParam();
    RecordingMidiOut out;
    MidiPedal pedal(c.pedal, channelFor(c.pedal), &pedalConfig(c.pedal), &out);
    pedal.setParams({{c.param, c.input}});
    if (c.emit) {
        ASSERT_EQ(out.messages.size(), 1u) << "param=" << c.param;
        EXPECT_EQ(out.messages[0].bytes(), c.expected) << "param=" << c.param;
    } else {
        EXPECT_TRUE(out.messages.empty()) << "param=" << c.param << " unexpectedly emitted " << out.dump();
    }
}

INSTANTIATE_TEST_SUITE_P(AllPedals, PedalParam, ::testing::ValuesIn(genParamCases()),
                         [](const ::testing::TestParamInfo<ParamCase>& info) { return info.param.id; });
