CXX = g++
CXXFLAGS = -std=c++26 -O3 -march=native -flto -ffast-math -pthread -Wall -Wextra -Isrc
LDFLAGS = -static-libstdc++ -static-libgcc -pthread -flto

PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin

TARGET = fsturbotransform
SRC = src/main.cpp

all: $(TARGET)

$(TARGET): $(SRC) src/fira_code_font.hpp src/terminal_style.hpp src/case_converter.hpp src/matcher.hpp src/renamer.hpp src/cli_parser.hpp src/sixel_renderer.hpp
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
