#pragma once
//
// Transform — the safe replacement for Python's eval('lambda x: ' + func).
//
// The original pedal configs stuffed tiny lambdas into YAML and eval'd them.
// Only two real shapes appear across every pedal config:
//
//   1. Arithmetic:  "x / 128", "x % 128", "x / 100"   (op in + - * / %)
//   2. Dict lookup: '{"TS808":20, "Plexi":21, "Klone":22}.get(x, None)'
//
// We parse exactly those two grammars and refuse anything else, loudly — a
// config needing an unsupported expression is a bug we want surfaced at load
// time, never silently guessed (see docs/plan.md "Key risks").
//
// Integer division/modulo match the original Python 2 behaviour for the
// non-negative MIDI values used here (truncation == floor for non-negatives).
//
#include <map>
#include <string>

#include "mc/domain/Value.h"

namespace mc {

class Transform {
public:
    enum class Kind { Identity, Arithmetic, Lookup };

    Transform() = default;  // Identity (no-op), mirrors a missing 'func'

    // Throws std::runtime_error if `func` is non-empty and matches neither grammar.
    static Transform parse(const std::string& func);

    // Mirrors check_for_func: returns f(v). Identity returns v unchanged.
    // Lookup of a missing key returns None (std::monostate), like dict.get(x, None).
    Value apply(const Value& v) const;

    Kind kind() const { return kind_; }
    char op() const { return op_; }
    long operand() const { return operand_; }
    const std::map<std::string, long>& table() const { return table_; }

private:
    Kind kind_ = Kind::Identity;
    char op_ = 0;
    long operand_ = 0;
    std::map<std::string, long> table_;
};

}  // namespace mc
