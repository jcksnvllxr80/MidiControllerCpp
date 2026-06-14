# MidiController C++ — build in WSL. Two builds, two outputs:
#
#   make build       compile the FIRMWARE    -> build-pico/midicontroller_pico.uf2  (Pico 2 W; needs Pico SDK)
#   make build-sim   compile the desktop SIM -> build/midicontroller                (host g++ C++17)
#   make test        build & run all unit/mock/e2e suites (host)
#   make run         launch the simulator (reads data/)
#   make clean       remove the desktop build/ artifacts
#   make pico-clean  remove the firmware build-pico/ artifacts
#
# googletest/googlemock and nlohmann/json are vendored under third_party/ (no apt deps for the host build).
# The firmware build needs PICO_SDK_PATH (defaults to $HOME/pico-sdk); override with:
#   make build PICO_SDK_PATH=/path/to/pico-sdk

CXX      ?= g++
CXXSTD   := -std=c++17
WARN     := -Wall -Wextra
CXXFLAGS := $(CXXSTD) $(WARN) -O0 -g -MMD -MP
LDFLAGS  := -pthread

BUILD := build

# Firmware (Pico) build — delegated to CMakeLists.txt in a separate dir.
PICO_SDK_PATH ?= $(HOME)/pico-sdk
export PICO_SDK_PATH
PICO_BUILD := build-pico
PICO_UF2   := $(PICO_BUILD)/midicontroller_pico.uf2

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
.PHONY: build build-sim test run clean pico-clean

# Default target: build the FIRMWARE .uf2 for the Pico 2 W (RP2350) via CMake.
build:
	@if [ ! -d "$(PICO_SDK_PATH)" ]; then \
		echo "ERROR: Pico SDK not found at PICO_SDK_PATH=$(PICO_SDK_PATH)"; \
		echo "  set it, e.g.:  make build PICO_SDK_PATH=/path/to/pico-sdk"; \
		echo "  (for the desktop simulator instead, run:  make build-sim)"; \
		exit 1; \
	fi
	@if [ ! -f $(PICO_BUILD)/CMakeCache.txt ]; then \
		cmake -B $(PICO_BUILD) -DPICO_BOARD=pico2_w; \
	fi
	cmake --build $(PICO_BUILD) -j
	@echo "=== built $(PICO_UF2) ==="

# Desktop simulator (host g++; no Pico SDK needed).
build-sim: $(APP)

test: $(TESTBIN)
	@echo "=== running tests ==="
	@./$(TESTBIN)

run: $(APP)
	@./$(APP)

clean:
	rm -rf $(BUILD)

# Firmware build dir is separate (and slow to regenerate — it rebuilds the SDK).
pico-clean:
	rm -rf $(PICO_BUILD)

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
