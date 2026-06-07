#include "gmock/gmock.h"

int main(int argc, char** argv) {
    // InitGoogleMock also initializes GoogleTest.
    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
