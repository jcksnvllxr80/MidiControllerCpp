// Value variant: predicates, accessors, toString, equality.
#include "gtest/gtest.h"
#include "mc/domain/Value.h"

using namespace mc;

TEST(Value, PredicatesNone) {
    Value v = std::monostate{};
    EXPECT_TRUE(isNone(v));
    EXPECT_FALSE(isBool(v));
    EXPECT_FALSE(isInt(v));
    EXPECT_FALSE(isString(v));
    EXPECT_FALSE(isEngagedRef(v));
}
TEST(Value, PredicatesBool) {
    Value v = true;
    EXPECT_TRUE(isBool(v));
    EXPECT_FALSE(isInt(v));  // bool is NOT int here (distinct variant alternative)
    EXPECT_FALSE(isNone(v));
}
TEST(Value, PredicatesInt) {
    Value v = 7L;
    EXPECT_TRUE(isInt(v));
    EXPECT_FALSE(isBool(v));
    EXPECT_FALSE(isString(v));
}
TEST(Value, PredicatesString) {
    Value v = std::string("hi");
    EXPECT_TRUE(isString(v));
    EXPECT_FALSE(isInt(v));
}
TEST(Value, PredicatesEngagedRef) {
    Value v = EngagedRef{"Loop 1", true};
    EXPECT_TRUE(isEngagedRef(v));
    EXPECT_FALSE(isBool(v));
}

TEST(Value, AsIntHitAndMiss) {
    EXPECT_EQ(asInt(Value{42L}).value_or(-1), 42);
    EXPECT_FALSE(asInt(Value{std::string("x")}).has_value());
    EXPECT_FALSE(asInt(Value{true}).has_value());
    EXPECT_FALSE(asInt(Value{std::monostate{}}).has_value());
}
TEST(Value, AsStringHitAndMiss) {
    EXPECT_EQ(asString(Value{std::string("abc")}).value_or(""), "abc");
    EXPECT_FALSE(asString(Value{1L}).has_value());
}

TEST(Value, ToStringAllVariants) {
    EXPECT_EQ(toString(Value{std::monostate{}}), "None");
    EXPECT_EQ(toString(Value{true}), "True");
    EXPECT_EQ(toString(Value{false}), "False");
    EXPECT_EQ(toString(Value{0L}), "0");
    EXPECT_EQ(toString(Value{-5L}), "-5");
    EXPECT_EQ(toString(Value{127L}), "127");
    EXPECT_EQ(toString(Value{std::string("Plexi")}), "Plexi");
    EXPECT_EQ(toString(Value{EngagedRef{"Loop 1", true}}), "Loop 1(engaged)");
    EXPECT_EQ(toString(Value{EngagedRef{"Loop 2", false}}), "Loop 2(bypassed)");
}

TEST(Value, EngagedRefEquality) {
    EXPECT_EQ((EngagedRef{"a", true}), (EngagedRef{"a", true}));
    EXPECT_FALSE((EngagedRef{"a", true} == EngagedRef{"a", false}));
    EXPECT_FALSE((EngagedRef{"a", true} == EngagedRef{"b", true}));
}

TEST(Value, VariantEquality) {
    EXPECT_EQ(Value{42L}, Value{42L});
    EXPECT_NE(Value{42L}, Value{43L});
    EXPECT_NE(Value{42L}, Value{std::string("42")});
    EXPECT_NE(Value{true}, Value{1L});  // bool vs int are different alternatives
    EXPECT_EQ(Value{std::string("x")}, Value{std::string("x")});
    const Value a = EngagedRef{"L", true};
    const Value b = EngagedRef{"L", true};
    EXPECT_EQ(a, b);
}
