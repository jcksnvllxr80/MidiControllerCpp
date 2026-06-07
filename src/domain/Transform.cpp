#include "mc/domain/Transform.h"

#include <regex>
#include <stdexcept>

namespace mc {

Transform Transform::parse(const std::string& func) {
    Transform t;
    if (func.empty()) {
        return t;  // Identity
    }

    // Grammar 1: arithmetic  x <op> n
    static const std::regex kArith(R"(^\s*x\s*([-+*/%])\s*(-?\d+)\s*$)");
    std::smatch m;
    if (std::regex_match(func, m, kArith)) {
        t.kind_ = Kind::Arithmetic;
        t.op_ = m[1].str()[0];
        t.operand_ = std::stol(m[2].str());
        return t;
    }

    // Grammar 2: dict lookup  { "k": n, ... }.get(x, None)
    static const std::regex kLookup(R"(^\s*\{(.*)\}\s*\.\s*get\s*\(\s*x\s*,\s*None\s*\)\s*$)");
    if (std::regex_match(func, m, kLookup)) {
        t.kind_ = Kind::Lookup;
        const std::string body = m[1].str();
        static const std::regex kPair(R"RX("((?:[^"\\]|\\.)*)"\s*:\s*(-?\d+))RX");
        auto it = std::sregex_iterator(body.begin(), body.end(), kPair);
        auto end = std::sregex_iterator();
        if (it == end) {
            throw std::runtime_error("Transform: empty/unparseable lookup table in '" + func + "'");
        }
        for (; it != end; ++it) {
            t.table_[(*it)[1].str()] = std::stol((*it)[2].str());
        }
        return t;
    }

    throw std::runtime_error("Transform: unsupported expression '" + func +
                             "'. Only 'x <op> n' and '{...}.get(x, None)' are allowed.");
}

Value Transform::apply(const Value& v) const {
    switch (kind_) {
        case Kind::Identity:
            return v;

        case Kind::Arithmetic: {
            auto x = asInt(v);
            if (!x) {
                throw std::runtime_error("Transform: arithmetic '" + std::string(1, op_) +
                                         "' applied to non-integer value '" + toString(v) + "'");
            }
            long r = *x;
            switch (op_) {
                case '+': r = *x + operand_; break;
                case '-': r = *x - operand_; break;
                case '*': r = *x * operand_; break;
                case '/': r = *x / operand_; break;  // integer division (Python 2 semantics for >=0)
                case '%': r = *x % operand_; break;
            }
            return r;
        }

        case Kind::Lookup: {
            auto key = asString(v);
            if (!key) {
                throw std::runtime_error("Transform: lookup applied to non-string value '" + toString(v) + "'");
            }
            auto it = table_.find(*key);
            if (it == table_.end()) {
                return std::monostate{};  // dict.get(x, None)
            }
            return it->second;
        }
    }
    return v;
}

}  // namespace mc
