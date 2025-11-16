# Detect OS
UNAME_S := $(shell uname -s)

# Set library names and extensions based on OS
ifeq ($(UNAME_S), Linux)
    LIB_NAME = m
    LIB_EXT = so
    RPATH = -Wl,-rpath,'$$ORIGIN/lib'
		LIB_DIR = ./lib/linux
		INCLUDE_DIR = ./include/linux
else ifeq ($(UNAME_S), Darwin)
    LIB_NAME = m -framework Cocoa
    LIB_EXT = dylib
    RPATH = -Wl,-rpath,'@loader_path/lib'
		LIB_DIR = ./lib/macos
		INCLUDE_DIR = ./include/macos
endif

# Compiler
CC = clang

# Directories
SRC_DIR = ./src
BUILD_DIR = ./build
COMMON_INCLUDE_DIR = ./include

# Flags
CFLAGS = -I$(INCLUDE_DIR) -I$(COMMON_INCLUDE_DIR) -MMD -MP
LDFLAGS = -L$(BUILD_DIR)/lib $(RPATH)
DEBUG_FLAGS = -g -O0 -Wall -Werror -Wextra -fsanitize=address -fsanitize=undefined -Wformat -Wformat-security
RELEASE_FLAGS = -O3 -fstack-protector-all -D_FORTIFY_SOURCE=2

# Libraries
LIBS = -l$(LIB_NAME)

# Sources and objects
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPENDS = $(OBJECTS:.o=.d)
TARGET = $(BUILD_DIR)/main

# Default target
all: debug

# Release build
release: CFLAGS += $(RELEASE_FLAGS)
release: check_dirs copy_libs $(TARGET) clean_objects

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: check_dirs copy_libs $(TARGET)

# Check if there are source files
check_dirs:
	@if [ "$(SOURCES)" = "" ]; then \
		echo "No source files found in $(SRC_DIR)"; \
		exit 1; \
	fi

# Copy library files
copy_libs: $(BUILD_DIR)/lib
	@echo "Copying library files to $(BUILD_DIR)/lib"
	cp $(LIB_DIR)/*.$(LIB_EXT)* $(BUILD_DIR)/lib 2>/dev/null || true
	cp $(LIB_DIR)/*.a* $(BUILD_DIR)/lib 2>/dev/null || true

# Build target
$(TARGET): $(BUILD_DIR) $(OBJECTS)
	@echo "Building target: $(TARGET)"
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $< to $@"
	$(CC) $(CFLAGS) -c $< -o $@

# Create build/lib directory
$(BUILD_DIR)/lib:
	mkdir -p $(BUILD_DIR)/lib

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean object files
clean_objects:
	rm -f $(OBJECTS)
	rm -f $(DEPENDS)
	rm -f $(BUILD_DIR)/lib/*.a

# Clean build directory
clean:
	rm -rf $(BUILD_DIR)

static_analysis:
	@echo "Running static analysis..."
	clang-tidy $(SOURCES) -- -I$(INCLUDE_DIR) -I$(COMMON_INCLUDE_DIR)


.PHONY: all release debug clean clean_objects check_dirs copy_libs static_analysis

-include $(DEPENDS)
