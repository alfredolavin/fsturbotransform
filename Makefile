# C++26 with #embed requires GCC >= 15 or Clang >= 19. Unless CXX is given explicitly,
# pick the first suitable compiler on PATH (an unversioned g++/clang++ is the last resort).
ifeq ($(origin CXX),default)
CXX := $(firstword $(foreach c,g++-16 g++-15 clang++-21 clang++-20 clang++-19 g++ clang++,$(if $(shell command -v $(c) 2>/dev/null),$(c))))
endif
CXXFLAGS = -std=c++26 -O3 -march=native -flto -ffast-math -pthread -Wall -Wextra -Isrc
LDFLAGS = -static-libstdc++ -static-libgcc -pthread -flto

PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin

TARGET = fsturbotransform
SRC = src/main.cpp
DEPS = $(SRC) $(wildcard src/*.hpp) src/FiraCode-Regular.ttf src/app_icon.rgba

all: $(TARGET)

$(TARGET): $(DEPS)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

install: $(TARGET)
	@mkdir -p $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	@echo "Installed $(TARGET) to $(BINDIR)/$(TARGET)"

uninstall:
	rm -f $(BINDIR)/$(TARGET)
	@echo "Removed $(TARGET) from $(BINDIR)/$(TARGET)"

cmake-build:
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build

cmake-install: cmake-build
	cmake --install build

clean:
	rm -f $(TARGET) FiraCode-Regular.ttf
	rm -rf build

.PHONY: all install uninstall clean cmake-build cmake-install
