#include "mc/domain/Transform.h"

#include <stdexcept>

#include "gtest/gtest.h"

using namespace mc;

namespace {

long arith(const char* func, long x) {
    Value r = Transform::parse(func).apply(Value{x});
    return std::get<long>(r);
}

}  // namespace

TEST(Transform, IdentityWhenEmpty) {
    Transform t = Transform::parse("");
    EXPECT_EQ(t.kind(), Transform::Kind::Identity);
    EXPECT_EQ(std::get<long>(t.apply(Value{42L})), 42);
}

TEST(Transform, DivisionIsIntegerLikePython2) {
    // The real Set Preset multi: bank = x / 128.
    EXPECT_EQ(arith("x / 128", 200), 1);
    EXPECT_EQ(arith("x / 128", 100), 0);
    EXPECT_EQ(arith("x / 128", 256), 2);
}

TEST(Transform, Modulo) {
    // The real Set Preset multi: preset = x % 128.
    EXPECT_EQ(arith("x % 128", 200), 72);
    EXPECT_EQ(arith("x % 128", 1), 1);
    EXPECT_EQ(arith("x % 128", 2), 2);
}

TEST(Transform, QuartzTempoSplit) {
    EXPECT_EQ(arith("x / 100", 110), 1);
    EXPECT_EQ(arith("x % 100", 110), 10);
}

TEST(Transform, OtherOperators) {
    EXPECT_EQ(arith("x + 5", 10), 15);
    EXPECT_EQ(arith("x - 5", 10), 5);
    EXPECT_EQ(arith("x * 3", 4), 12);
}

TEST(Transform, DictLookup) {
    // ScarlettLove Set Preset (control change func).
    Transform t = Transform::parse(R"({"TS808":20, "Plexi":21, "Klone":22}.get(x, None))");
    EXPECT_EQ(t.kind(), Transform::Kind::Lookup);
    EXPECT_EQ(std::get<long>(t.apply(Value{std::string("TS808")})), 20);
    EXPECT_EQ(std::get<long>(t.apply(Value{std::string("Plexi")})), 21);
    EXPECT_EQ(std::get<long>(t.apply(Value{std::string("Klone")})), 22);
}

TEST(Transform, DictLookupMissingKeyReturnsNone) {
    Transform t = Transform::parse(R"({"TS808":20, "Plexi":21}.get(x, None))");
    EXPECT_TRUE(isNone(t.apply(Value{std::string("Nope")})));
}

TEST(Transform, UnsupportedExpressionThrows) {
    EXPECT_THROW(Transform::parse("x ** 2"), std::runtime_error);
    EXPECT_THROW(Transform::parse("sin(x)"), std::runtime_error);
    EXPECT_THROW(Transform::parse("x / 0 + nonsense"), std::runtime_error);
}

TEST(Transform, ArithmeticOnNonIntThrows) {
    Transform t = Transform::parse("x / 128");
    EXPECT_THROW(t.apply(Value{std::string("hello")}), std::runtime_error);
}

