#include "mc/domain/Transform.h"

#include <cctype>
#include <stdexcept>
#include <utility>

// Hand-rolled parser for the two real `func` grammars — no <regex>, so the core
// stays light enough for a microcontroller (std::regex is huge in flash + stack):
//   1. arithmetic:  x <op> n      op in + - * / %
//   2. dict lookup: { "k": n, ... }.get(x, None)
// Behaviour matches the previous regex version for every config we ship.

namespace mc {

namespace {

struct Scanner {
    const std::string& s;
    size_t i = 0;
    explicit Scanner(const std::string& str) : s(str) {}

    void ws() {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    }
    bool eof() const { return i >= s.size(); }
    bool peek(char c) const { return i < s.size() && s[i] == c; }
    bool accept(char c) {
        if (peek(c)) { ++i; return true; }
        return false;
    }
    bool literal(const char* w) {
        size_t start = i;
        for (const char* p = w; *p; ++p) {
            if (i >= s.size() || s[i] != *p) { i = start; return false; }
            ++i;
        }
        return true;
    }
    bool integer(long& out) {
        size_t start = i;
        if (i < s.size() && s[i] == '-') ++i;
        size_t firstDigit = i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        if (i == firstDigit) { i = start; return false; }
        out = std::stol(s.substr(start, i - start));
        return true;
    }
    // A double-quoted string with \" and \\ escapes; `out` gets the unescaped content.
    bool quoted(std::string& out) {
        if (!accept('"')) return false;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '\\') {
                if (i >= s.size()) return false;
                out.push_back(s[i++]);  // take the escaped char literally
            } else if (c == '"') {
                return true;
            } else {
                out.push_back(c);
            }
        }
        return false;  // unterminated
    }
};

bool parseArithmetic(const std::string& func, char& op, long& operand) {
    Scanner sc(func);
    sc.ws();
    if (!sc.accept('x')) return false;
    sc.ws();
    if (sc.eof()) return false;
    char c = sc.s[sc.i];
    if (c != '+' && c != '-' && c != '*' && c != '/' && c != '%') return false;
    ++sc.i;
    sc.ws();
    if (!sc.integer(operand)) return false;
    sc.ws();
    if (!sc.eof()) return false;
    op = c;
    return true;
}

bool parseLookup(const std::string& func, std::map<std::string, long>& table) {
    Scanner sc(func);
    sc.ws();
    if (!sc.accept('{')) return false;
    sc.ws();
    if (!sc.peek('}')) {
        while (true) {
            sc.ws();
            std::string key;
            if (!sc.quoted(key)) return false;
            sc.ws();
            if (!sc.accept(':')) return false;
            sc.ws();
            long val = 0;
            if (!sc.integer(val)) return false;
            table[key] = val;
            sc.ws();
            if (sc.accept(',')) continue;
            break;
        }
    }
    sc.ws();
    if (!sc.accept('}')) return false;
    sc.ws();
    if (!sc.accept('.')) return false;
    sc.ws();
    if (!sc.literal("get")) return false;
    sc.ws();
    if (!sc.accept('(')) return false;
    sc.ws();
    if (!sc.accept('x')) return false;
    sc.ws();
    if (!sc.accept(',')) return false;
    sc.ws();
    if (!sc.literal("None")) return false;
    sc.ws();
    if (!sc.accept(')')) return false;
    sc.ws();
    if (!sc.eof()) return false;
    return !table.empty();
}

}  // namespace

Transform Transform::parse(const std::string& func) {
    Transform t;
    if (func.empty()) return t;  // Identity

    char op = 0;
    long operand = 0;
    if (parseArithmetic(func, op, operand)) {
        t.kind_ = Kind::Arithmetic;
        t.op_ = op;
        t.operand_ = operand;
        return t;
    }

    std::map<std::string, long> table;
    if (parseLookup(func, table)) {
        t.kind_ = Kind::Lookup;
        t.table_ = std::move(table);
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
            if (it == table_.end()) return std::monostate{};  // dict.get(x, None)
            return it->second;
        }
    }
    return v;
}

}  // namespace mc
