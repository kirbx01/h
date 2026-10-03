TARGET  := i_forgor
SRC_DIR := src
BUILD   := build

RAYLIB_DIR ?= external/raylib
RAYGUI_DIR ?= external/raygui
RAYGUI_CLASSIC_STYLE ?= 0

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
else
  RAYGUI_FLAG :=
endif

INCLUDES := $(RAYLIB_CFLAGS) \
            -I$(SRC_DIR)/game \
            -I$(SRC_DIR)/ui \
            -I$(SRC_DIR)/sound \
            -I$(SRC_DIR)/input \
            -I$(RAYGUI_DIR)/src

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra $(RAYGUI_FLAG) $(INCLUDES) -MMD -MP
LDFLAGS  := $(RAYLIB_LIBS)
LDLIBS   :=

SOURCES := $(wildcard $(SRC_DIR)/*.cpp) \
           $(wildcard $(SRC_DIR)/game/*.cpp) \
           $(wildcard $(SRC_DIR)/ui/*.cpp) \
           $(wildcard $(SRC_DIR)/sound/*.cpp) \
           $(wildcard $(SRC_DIR)/input/*.cpp)

OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD)/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)

.PHONY: all run clean deps raylib

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
	    echo "building raylib from $(RAYLIB_DIR) ..."; \
	    $(MAKE) -C $(RAYLIB_DIR)/src; \
	fi

run: $(TARGET)
	./$(TARGET)

deps:
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS : $(LDFLAGS)"
	@echo "SOURCES :"; for s in $(SOURCES); do echo "    $$s"; done

clean:
	rm -rf $(BUILD) $(TARGET)

-include $(DEPS)