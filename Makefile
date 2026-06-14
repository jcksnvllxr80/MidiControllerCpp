# MidiController C++ firmware — build in WSL (g++ C++17). No apt deps:
# googletest/googlemock and nlohmann/json are vendored under third_party/.
#
#   make build   compile the sim app  -> build/midicontroller
#   make test    build & run all unit/mock/e2e suites
#   make run     launch the simulator (reads data/)
#   make clean   remove build artifacts

CXX      ?= g++
CXXSTD   := -std=c++17
WARN     := -Wall -Wextra
CXXFLAGS := $(CXXSTD) $(WARN) -O0 -g -MMD -MP
LDFLAGS  := -pthread

BUILD := build

INC := -Iinclude -Ithird_party

GTEST_DIR := third_party/googletest/googletest
GMOCK_DIR := third_party/googletest/googlemock
# Includes for *using* gtest/gmock (in our test TUs):
GTEST_INC := -I$(GTEST_DIR)/include -I$(GMOCK_DIR)/include
# Includes for *building* the gtest/gmock amalgamations (need the package roots):
GTEST_BUILD_INC := -I$(GTEST_DIR)/include -I$(GTEST_DIR) -I$(GMOCK_DIR)/include -I$(GMOCK_DIR)

# --- sources -----------------------------------------------------------------
# The desktop build excludes adapters/mcu (it needs the Pico SDK; built via CMake).
ALL_SRC  := $(shell find src -name '*.cpp' -not -path 'src/adapters/mcu/*')
LIB_SRC  := $(filter-out src/main.cpp,$(ALL_SRC))
LIB_OBJ  := $(patsubst %.cpp,$(BUILD)/%.o,$(LIB_SRC))
MAIN_OBJ := $(BUILD)/src/main.o

TEST_SRC := $(shell find tests -name '*.cpp')
TEST_OBJ := $(patsubst %.cpp,$(BUILD)/%.o,$(TEST_SRC))

GTEST_OBJ := $(BUILD)/gtest-all.o $(BUILD)/gmock-all.o

APP     := $(BUILD)/midicontroller
TESTBIN := $(BUILD)/midicontroller_tests

# --- top-level targets -------------------------------------------------------
.PHONY: build test run clean
build: $(APP)

test: $(TESTBIN)
	@echo "=== running tests ==="
	@./$(TESTBIN)

run: $(APP)
	@./$(APP)

clean:
	rm -rf $(BUILD)

# --- link --------------------------------------------------------------------
$(APP): $(LIB_OBJ) $(MAIN_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(TESTBIN): $(LIB_OBJ) $(TEST_OBJ) $(GTEST_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# --- compile -----------------------------------------------------------------
# App/lib sources (also given gtest includes so test helpers in headers compile).
$(BUILD)/src/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

# Test sources need the gtest/gmock headers and the tests/ support dir.
$(BUILD)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INC) $(GTEST_INC) -Itests -c $< -o $@

# Vendored gtest/gmock amalgamations.
$(BUILD)/gtest-all.o: $(GTEST_DIR)/src/gtest-all.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) -O0 -g $(GTEST_BUILD_INC) -c $< -o $@

$(BUILD)/gmock-all.o: $(GMOCK_DIR)/src/gmock-all.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXSTD) -O0 -g $(GTEST_BUILD_INC) -c $< -o $@

-include $(LIB_OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
