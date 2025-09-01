# Variables
CXX = em++
TARGET = game
OUTPUT = $(TARGET).html
RAYLIB_PATH = ../raylib
RAYLIB_LIB = $(RAYLIB_PATH)/src/libraylib.a

# Main source file
SOURCES = main.cpp

# All cpp files (for reference)
ALL_SOURCES = $(wildcard *.cpp)

# Compiler flags
CXXFLAGS = -Os -Wall -DPLATFORM_WEB
INCLUDES = -I. -I$(RAYLIB_PATH)/src/
LDFLAGS = -s USE_GLFW=3 -s ASYNCIFY -s DYNCALLS \
          --preload-file resources/ \
          --js-library library_webxr.js \
          --profiling \
          -s "EXPORTED_RUNTIME_METHODS=['ccall','cwrap','setValue','getValue']" \
          -s "EXPORTED_FUNCTIONS=['_malloc','_free','_main','_launchit','_launch_ar']"

# Default target
all: $(OUTPUT)

# Main target - keeps same output as before (game.html)
$(OUTPUT): $(SOURCES) $(RAYLIB_LIB)
	$(CXX) -o $@ $(SOURCES) $(CXXFLAGS) $(INCLUDES) $(RAYLIB_LIB) $(LDFLAGS)

# Clean target - removes generated files but keeps index.html
clean:
	rm -f game.html game.js game.wasm game.data game_werks.html game_werks.js game_werks.wasm game_werks.data

# Phony targets
.PHONY: all werks clean help

# Alternative target for main_werks.cpp
werks: main_werks.cpp $(RAYLIB_LIB)
	$(CXX) -o game_werks.html $< $(CXXFLAGS) $(INCLUDES) $(RAYLIB_LIB) $(LDFLAGS)

# Help target
help:
	@echo "Available targets:"
	@echo "  all     - Build the project with main.cpp (default)"
	@echo "  werks   - Build alternative version with main_werks.cpp"
	@echo "  clean   - Remove build artifacts (keeps index.html)"
	@echo "  help    - Show this help message"
	@echo ""
	@echo "Main source: $(SOURCES)"
	@echo "All cpp files: $(ALL_SOURCES)"
