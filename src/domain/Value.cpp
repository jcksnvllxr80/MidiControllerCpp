#include "mc/domain/Value.h"

namespace mc {

std::string toString(const Value& v) {
    struct Visitor {
        std::string operator()(std::monostate) const { return "None"; }
        std::string operator()(bool b) const { return b ? "True" : "False"; }
        std::string operator()(long i) const { return std::to_string(i); }
        std::string operator()(const std::string& s) const { return s; }
        std::string operator()(const EngagedRef& e) const {
            return e.name + (e.engaged ? "(engaged)" : "(bypassed)");
        }
    };
    return std::visit(Visitor{}, v);
}

}  // namespace mc
