BUILD_DIR := ./build
SRC_DIRS := ./src

SRCS := $(shell find $(SRC_DIRS) -name '*.cpp')

CFLAGS := -std=c++17 -g
LDFLAGS := -lvulkan -ldl -lpthread

WX_CXXFLAGS := $(shell wx-config --cxxflags)
WX_LIBS := $(shell wx-config --libs)

GTK_CXXFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS := $(shell pkg-config --libs gtk+-3.0)

$(BUILD_DIR)/app: $(SRCS)
	mkdir -p $(BUILD_DIR)
	g++ $(CFLAGS) $(WX_CXXFLAGS) $(GTK_CXXFLAGS) $^ $(LDFLAGS) $(WX_LIBS) $(GTK_LIBS) -o $@

.PHONY: run clean

run: $(BUILD_DIR)/app
	$(BUILD_DIR)/app

clean:
	rm -fr $(BUILD_DIR)
