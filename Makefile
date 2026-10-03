TARGET  ?= i_forgor
SRC_DIR := src
BUILD   := build
BUILD_TEST ?= build-test

RAYLIB_DIR ?= external/raylib
RAYGUI_DIR ?= external/raygui
RAYGUI_CLASSIC_STYLE ?= 0

IFG_AUTHOR   ?= kirbx01
IFG_ITCH_URL ?=

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

DEFINES := -DSUPPORT_MODULE_RAAC -DIFG_AUTHOR='"$(IFG_AUTHOR)"' -DIFG_ITCH_URL='"$(IFG_ITCH_URL)"'

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

TEST_SOURCES := $(SRC_DIR)/game/board.cpp \
                $(SRC_DIR)/game/trail.cpp \
                $(SRC_DIR)/game/story.cpp \
                $(SRC_DIR)/game/game.cpp \
                $(SRC_DIR)/game/persist.cpp \
                $(SRC_DIR)/platform/platform.cpp
TEST_OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_TEST)/%.o,$(TEST_SOURCES)) \
                $(patsubst tests/%.cpp,$(BUILD_TEST)/tests/%.o,$(wildcard tests/*.cpp))
TEST_TARGET  := $(BUILD_TEST)/smoke_test

.PHONY: all run clean deps raylib test

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

deps:
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS : $(LDFLAGS)"
	@echo "SOURCES :"; for s in $(SOURCES); do echo "    $$s"; done

clean:
	rm -rf $(BUILD) $(BUILD_TEST) $(TARGET)

-include $(DEPS) $(TEST_OBJECTS:.o=.d)
