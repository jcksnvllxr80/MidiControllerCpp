// Table-driven Transform coverage: arithmetic across operators/inputs, dict
// lookups, and a battery of malformed expressions that must throw.
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mc/domain/Transform.h"

using namespace mc;

struct ArithCase {
    std::string func;
    long x;
    long want;
    std::string id;
};
class TransformArith : public ::testing::TestWithParam<ArithCase> {};
TEST_P(TransformArith, Eval) {
    const auto& c = GetParam();
    Transform t = Transform::parse(c.func);
    EXPECT_EQ(t.kind(), Transform::Kind::Arithmetic);
    EXPECT_EQ(std::get<long>(t.apply(Value{c.x})), c.want) << c.func << " applied to " << c.x;
}
INSTANTIATE_TEST_SUITE_P(
    Cases, TransformArith,
    ::testing::Values(
        ArithCase{"x + 0", 5, 5, "add0"}, ArithCase{"x + 1", 5, 6, "add1"},
        ArithCase{"x + 100", 27, 127, "add100"}, ArithCase{"x - 5", 10, 5, "sub5"},
        ArithCase{"x - 10", 10, 0, "sub_to_0"}, ArithCase{"x * 2", 10, 20, "mul2"},
        ArithCase{"x * 3", 4, 12, "mul3"}, ArithCase{"x * 0", 99, 0, "mul0"},
        ArithCase{"x / 128", 0, 0, "div128_0"}, ArithCase{"x / 128", 127, 0, "div128_127"},
        ArithCase{"x / 128", 128, 1, "div128_128"}, ArithCase{"x / 128", 200, 1, "div128_200"},
        ArithCase{"x / 128", 255, 1, "div128_255"}, ArithCase{"x / 128", 256, 2, "div128_256"},
        ArithCase{"x % 128", 0, 0, "mod128_0"}, ArithCase{"x % 128", 127, 127, "mod128_127"},
        ArithCase{"x % 128", 128, 0, "mod128_128"}, ArithCase{"x % 128", 200, 72, "mod128_200"},
        ArithCase{"x % 128", 130, 2, "mod128_130"}, ArithCase{"x / 100", 110, 1, "div100"},
        ArithCase{"x / 100", 250, 2, "div100_250"}, ArithCase{"x % 100", 110, 10, "mod100"},
        ArithCase{"x % 100", 250, 50, "mod100_250"}, ArithCase{"x/128", 200, 1, "nospace"},
        ArithCase{"  x  /  128  ", 200, 1, "extraspace"}),
    [](const ::testing::TestParamInfo<ArithCase>& i) { return i.param.id; });

struct BadCase {
    std::string func;
    std::string id;
};
class TransformBad : public ::testing::TestWithParam<BadCase> {};
TEST_P(TransformBad, Throws) { EXPECT_THROW(Transform::parse(GetParam().func), std::runtime_error); }
INSTANTIATE_TEST_SUITE_P(Malformed, TransformBad,
                         ::testing::Values(BadCase{"x ** 2", "pow"}, BadCase{"sin(x)", "sin"},
                                           BadCase{"y + 1", "wrong_var"}, BadCase{"x +", "no_operand"},
                                           BadCase{"+ 1", "no_var"}, BadCase{"x & 1", "bitand"},
                                           BadCase{"x << 2", "shift"}, BadCase{"2 + x", "reversed"},
                                           BadCase{"x + 1.5", "float"}, BadCase{"lambda x: x", "lambda"},
                                           BadCase{"{x}.get(x)", "bad_dict"}, BadCase{"x / 1 / 2", "double_op"}),
                         [](const ::testing::TestParamInfo<BadCase>& i) { return i.param.id; });

TEST(TransformLookup, AllKeysAndMiss) {
    Transform t = Transform::parse(R"({"TS808":20, "Plexi":21, "Klone":22}.get(x, None))");
    EXPECT_EQ(t.kind(), Transform::Kind::Lookup);
    EXPECT_EQ(t.table().size(), 3u);
    EXPECT_EQ(std::get<long>(t.apply(Value{std::string("TS808")})), 20);
    EXPECT_EQ(std::get<long>(t.apply(Value{std::string("Plexi")})), 21);
    EXPECT_EQ(std::get<long>(t.apply(Value{std::string("Klone")})), 22);
    EXPECT_TRUE(isNone(t.apply(Value{std::string("Nope")})));
    EXPECT_TRUE(isNone(t.apply(Value{std::string("")})));
}

TEST(TransformLookup, NoSpaceVariant) {
    Transform t = Transform::parse(R"({"a":1,"b":2}.get(x,None))");
    EXPECT_EQ(std::get<long>(t.apply(Value{std::string("b")})), 2);
}

TEST(TransformLookup, SingleEntry) {
    Transform t = Transform::parse(R"({"only":7}.get(x, None))");
    EXPECT_EQ(std::get<long>(t.apply(Value{std::string("only")})), 7);
}

TEST(TransformIdentity, PassesEveryType) {
    Transform t;  // default identity
    EXPECT_EQ(t.kind(), Transform::Kind::Identity);
    EXPECT_EQ(std::get<long>(t.apply(Value{42L})), 42);
    EXPECT_EQ(std::get<std::string>(t.apply(Value{std::string("hi")})), "hi");
    EXPECT_TRUE(std::get<bool>(t.apply(Value{true})));
    EXPECT_TRUE(isNone(t.apply(Value{std::monostate{}})));
}

class ArithBadType : public ::testing::TestWithParam<Value> {};
TEST_P(ArithBadType, Throws) {
    Transform t = Transform::parse("x / 128");
    EXPECT_THROW(t.apply(GetParam()), std::runtime_error);
}
INSTANTIATE_TEST_SUITE_P(NonInt, ArithBadType,
                         ::testing::Values(Value{std::string("s")}, Value{std::monostate{}}, Value{true}));

TEST(TransformLookupBadType, ThrowsOnNonString) {
    Transform t = Transform::parse(R"({"a":1}.get(x, None))");
    EXPECT_THROW(t.apply(Value{5L}), std::runtime_error);
}

TEST(TransformAccessors, ArithmeticFields) {
    Transform t = Transform::parse("x % 128");
    EXPECT_EQ(t.op(), '%');
    EXPECT_EQ(t.operand(), 128);
}
