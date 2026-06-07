#pragma once
// GoogleMock IMidiOut for expectation-style tests (InSequence, EXPECT_CALL).
#include "gmock/gmock.h"
#include "mc/ports/IMidiOut.h"

namespace mc::test {

class MockMidiOut : public IMidiOut {
public:
    MOCK_METHOD(void, send, (const MidiMessage& msg), (override));
};

}  // namespace mc::test
