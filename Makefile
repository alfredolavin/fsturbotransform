# C++26 with #embed requires GCC >= 15 or Clang >= 19. Unless CXX is given explicitly,
# pick the first suitable compiler on PATH (an unversioned g++/clang++ is the last resort).
ifeq ($(origin CXX),default)
CXX := $(firstword $(foreach c,g++-16 g++-15 clang++-21 clang++-20 clang++-19 g++ clang++,$(if $(shell command -v $(c) 2>/dev/null),$(c))))
endif
# PCRE2 (libpcre2-dev) is the regex engine and zlib (zlib1g-dev) compresses Kitty images; both are system libraries.
PCRE2_CFLAGS := $(shell pkg-config --cflags libpcre2-8 2>/dev/null)
PCRE2_LIBS := $(shell pkg-config --libs libpcre2-8 2>/dev/null || echo -lpcre2-8)
CXXFLAGS = -std=c++26 -O3 -march=native -flto -ffast-math -pthread -Wall -Wextra -Isrc $(PCRE2_CFLAGS) -DFSTURBO_LIBDIR='"$(LIBDIR)"'
ZLIB_LIBS := $(shell pkg-config --libs zlib 2>/dev/null || echo -lz)
LDFLAGS = -static-libstdc++ -static-libgcc -pthread -flto -ldl $(PCRE2_LIBS) $(ZLIB_LIBS)

PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin

TARGET = fsturbotransform
SRC = src/main.cpp
DEPS = $(SRC) $(wildcard src/*.hpp) src/FiraCode-Regular.ttf src/app_icon.rgba

# The JavaScript expression bridge: V8 used as a library through libnode (libnode-dev), loaded
# with dlopen on first use. It is skipped when the V8 headers are not installed.
V8_INCLUDE ?= /usr/include/node
JS_BRIDGE = libfsturbo_js.so
LIBDIR ?= $(PREFIX)/lib/fsturbotransform
ifneq ($(wildcard $(V8_INCLUDE)/v8.h),)
BRIDGE_TARGET = $(JS_BRIDGE)
endif

all: $(TARGET) $(BRIDGE_TARGET)
ifeq ($(BRIDGE_TARGET),)
	@echo "note: $(V8_INCLUDE)/v8.h not found (libnode-dev): built without $(JS_BRIDGE); expression replacements will report an error"
endif

# No -ffast-math here: the bridge's NaN/Infinity checks must stay meaningful.
$(JS_BRIDGE): src/js_bridge.cpp src/js_bridge_api.h
	$(CXX) -std=c++26 -O2 -fPIC -shared -fvisibility=hidden -Wall -Wextra -isystem $(V8_INCLUDE) src/js_bridge.cpp -o $@ -lnode

$(TARGET): $(DEPS)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

install: all
	@mkdir -p $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	@echo "Installed $(TARGET) to $(BINDIR)/$(TARGET)"
ifneq ($(BRIDGE_TARGET),)
	@mkdir -p $(LIBDIR)
	install -m 755 $(JS_BRIDGE) $(LIBDIR)/$(JS_BRIDGE)
	@echo "Installed $(JS_BRIDGE) to $(LIBDIR)/$(JS_BRIDGE)"
endif

uninstall:
	rm -f $(BINDIR)/$(TARGET)
	rm -rf $(LIBDIR)
	@echo "Removed $(TARGET) from $(BINDIR)/$(TARGET)"

cmake-build:
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build

# The QML GUI needs Qt's build tools (moc, qmlcachegen): it is built through CMake.
gui:
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build --target fsturbotransform-gui

cmake-install: cmake-build
	cmake --install build

clean:
	rm -f $(TARGET) $(JS_BRIDGE) FiraCode-Regular.ttf
	rm -rf build

.PHONY: all install uninstall clean cmake-build cmake-install gui
