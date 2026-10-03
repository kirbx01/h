TARGET  ?= after_the_fall
SRC_DIR := src
BUILD   := build
BUILD_TEST ?= build-test

RAYLIB_DIR ?= external/raylib
RAYGUI_DIR ?= external/raygui
RAYGUI_CLASSIC_STYLE ?= 0

# Credits metadata. ATF_ITCH_URL is intentionally empty by default: the game refuses to
# invent a URL, and assets/itch_url.txt can point the credits at the real page instead.
ATF_AUTHOR   ?= kirbx01
ATF_ITCH_URL ?=

UNAME := $(shell uname -s)

ifeq ($(UNAME),Darwin)
  PLATFORM_LIBS := -framework CoreVideo -framework IOKit -framework Cocoa \
                   -framework GLUT -framework OpenGL
else ifeq ($(UNAME),Linux)
  PLATFORM_LIBS := -lpthread -ldl -lm -lrt -lX11
endif

ifeq ($(shell pkg-config --exists raylib 2>/dev/null && echo yes),yes)
  RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib)
  RAYLIB_LIBS   := $(shell pkg-config --libs raylib)
  RAYLIB_DEP    :=
else
  RAYLIB_CFLAGS := -I$(RAYLIB_DIR)/src
  RAYLIB_LIBS   := -L$(RAYLIB_DIR)/src -lraylib $(PLATFORM_LIBS)
  RAYLIB_DEP    := $(RAYLIB_DIR)/src/libraylib.a
endif

ifeq ($(RAYGUI_CLASSIC_STYLE),1)
  RAYGUI_FLAG := -DRAYGUI_CLASSIC_STYLE
endif

INCLUDES := $(RAYLIB_CFLAGS) \
            -I$(SRC_DIR)/game \
            -I$(SRC_DIR)/ui \
            -I$(SRC_DIR)/sound \
            -I$(SRC_DIR)/input \
            -I$(SRC_DIR)/platform \
            -I$(RAYGUI_DIR)/src

DEFINES := -DATF_AUTHOR='"$(ATF_AUTHOR)"' -DATF_ITCH_URL='"$(ATF_ITCH_URL)"'

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra $(RAYGUI_FLAG) $(DEFINES) $(INCLUDES) -MMD -MP
LDFLAGS  := $(RAYLIB_LIBS)
LDLIBS   :=

SOURCES := $(wildcard $(SRC_DIR)/*.cpp) \
           $(wildcard $(SRC_DIR)/game/*.cpp) \
           $(wildcard $(SRC_DIR)/ui/*.cpp) \
           $(wildcard $(SRC_DIR)/sound/*.cpp) \
           $(wildcard $(SRC_DIR)/input/*.cpp) \
           $(wildcard $(SRC_DIR)/platform/*.cpp)

OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD)/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)

# The smoke test links only the simulation translation units and stubs input, audio and
# drawing, so the whole progression can be verified without a display or a sound card.
TEST_SOURCES := $(SRC_DIR)/game/board.cpp \
                $(SRC_DIR)/game/trail.cpp \
                $(SRC_DIR)/game/story.cpp \
                $(SRC_DIR)/game/game.cpp \
                $(SRC_DIR)/game/persist.cpp \
                $(SRC_DIR)/platform/platform.cpp
TEST_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_TEST)/%.o,$(TEST_SOURCES)) \
                $(patsubst tests/%.cpp,$(BUILD_TEST)/tests/%.o,$(wildcard tests/*.cpp))
TEST_TARGET  := $(BUILD_TEST)/smoke_test

# Capture tool: same sources as the game plus tools/capture.cpp, so the PNGs it writes
# show the real renderer.
BUILD_CAPTURE ?= build-capture
CAPTURE_TARGET := $(BUILD_CAPTURE)/capture

# Set SAVE_DIR=/tmp/atf to keep the session file out of your real profile while testing:
#   SAVE_DIR=/tmp/atf make run

.PHONY: all run clean deps raylib test capture

all: $(TARGET)

$(TARGET): $(OBJECTS) $(RAYLIB_DEP)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)

$(RAYLIB_DEP):
	@echo "building raylib from $(RAYLIB_DIR) (one-time, a few minutes) ..."
	$(MAKE) -C $(RAYLIB_DIR)/src
	@touch $@

$(BUILD)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

raylib:
	@if [ -z "$(RAYLIB_DEP)" ]; then \
	    echo "using system raylib via pkg-config"; \
	elif [ -f "$(RAYLIB_DEP)" ]; then \
	    echo "raylib already built: $(RAYLIB_DEP)"; \
	else \
	    echo "building raylib from $(RAYLIB_DIR) ...";
	    $(MAKE) -C $(RAYLIB_DIR)/src; \
	fi

run: $(TARGET)
	./$(TARGET)

# Headless verification of movement, erosion, traces, progression and persistence.
# SAVE_DIR keeps the session file out of your real profile.
test: $(TEST_TARGET)
	@SAVE_DIR=$(CURDIR)/$(BUILD_TEST) ./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(TEST_OBJECTS) -o $@

$(BUILD_TEST)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_TEST)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

capture: $(CAPTURE_TARGET)
	@mkdir -p $(BUILD_CAPTURE)
	./$(CAPTURE_TARGET)

# main.o is replaced by the tool's own main, so it is left out of the link.
CAPTURE_OBJECTS := $(filter-out $(BUILD)/main.o,$(OBJECTS))

$(CAPTURE_TARGET): $(CAPTURE_OBJECTS) $(BUILD_CAPTURE)/capture.o $(RAYLIB_DEP)
	$(CXX) $(BUILD_CAPTURE)/capture.o $(CAPTURE_OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_CAPTURE)/capture.o: tools/capture.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DOUTPUT_DIR='"$(CURDIR)/$(BUILD_CAPTURE)"' -c $< -o $@

deps:
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS : $(LDFLAGS)"
	@echo "SOURCES :"; for s in $(SOURCES); do echo "    $$s"; done

clean:
	rm -rf $(BUILD) $(BUILD_TEST) $(BUILD_CAPTURE) $(TARGET)

-include $(DEPS)
