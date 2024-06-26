BUILD_DIR := ./build
SRC_DIRS := ./src

SRCS := $(shell find $(SRC_DIRS) -name '*.cpp')

CFLAGS := -std=c++17 -g
LDFLAGS := -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

WX_CXXFLAGS := $(shell wx-config --cxxflags)
WX_LIBS := $(shell wx-config --libs)

$(BUILD_DIR)/app: $(SRCS)
	mkdir -p $(BUILD_DIR)
	g++ $(CFLAGS) $(WX_CXXFLAGS) $^ $(LDFLAGS) $(WX_LIBS) -o $@

.PHONY: run clean

run: $(BUILD_DIR)/app
	$(BUILD_DIR)/app

clean:
	rm -fr $(BUILD_DIR)
