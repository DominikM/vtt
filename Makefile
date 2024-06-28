TARGET_EXEC := app

BUILD_DIR := ./build
SRC_DIRS := ./src

WX_CXXFLAGS := $(shell wx-config --cxxflags)
WX_LIBS := $(shell wx-config --libs)

GTK_CXXFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS := $(shell pkg-config --libs gtk+-3.0)

CXXFLAGS := -std=c++17 -g $(WX_CXXFLAGS) $(GTK_CXXFLAGS)
LDFLAGS := -lvulkan -ldl $(WX_LIBS) $(GTK_LIBS)

SRCS := $(shell find $(SRC_DIRS) -name '*.cpp')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run: $(BUILD_DIR)/app
	$(BUILD_DIR)/app

clean:
	rm -fr $(BUILD_DIR)

.PHONY: run clean
