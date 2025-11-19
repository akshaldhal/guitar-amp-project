UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Linux)
  LIB_NAME = portaudio -lrt -lm -lasound -pthread
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

CC = clang

SRC_DIR = ./src
BUILD_DIR = ./build
COMMON_INCLUDE_DIR = ./include
TEST_DIR = ./tests

CFLAGS = -I$(INCLUDE_DIR) -I$(COMMON_INCLUDE_DIR) -MMD -MP
LDFLAGS = -L$(BUILD_DIR)/lib $(RPATH)
DEBUG_FLAGS = -g -O0 -Wall -Werror -Wextra -fsanitize=address -fsanitize=undefined -Wformat -Wformat-security
RELEASE_FLAGS = -O3 -flto -march=native -ffast-math

LIBS = -l$(LIB_NAME)

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)
DEPENDS = $(OBJECTS:.o=.d)
TARGET = $(BUILD_DIR)/main

TEST_SOURCES = $(filter-out $(SRC_DIR)/main.c, $(SOURCES)) $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS = $(TEST_SOURCES:%.c=$(BUILD_DIR)/%.o)
TEST_TARGET = $(BUILD_DIR)/test_runner

all: debug

release: CFLAGS += $(RELEASE_FLAGS)
release: check_dirs copy_libs $(TARGET) clean_objects

debug: CFLAGS += $(DEBUG_FLAGS)
debug: check_dirs copy_libs $(TARGET)

tests: CFLAGS += $(DEBUG_FLAGS)
tests: check_dirs copy_libs $(TEST_TARGET)

check_dirs:
	@if [ "$(SOURCES)" = "" ]; then \
		echo "No source files found in $(SRC_DIR)"; \
		exit 1; \
	fi

copy_libs: $(BUILD_DIR)/lib
	cp $(LIB_DIR)/*.$(LIB_EXT)* $(BUILD_DIR)/lib 2>/dev/null || true
	cp $(LIB_DIR)/*.a* $(BUILD_DIR)/lib 2>/dev/null || true

$(TARGET): $(BUILD_DIR) $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

$(TEST_TARGET): $(BUILD_DIR) $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJECTS) $(LDFLAGS) $(LIBS)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/lib:
	mkdir -p $(BUILD_DIR)/lib

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean_objects:
	rm -rf $(BUILD_DIR)/${SRC_DIR}
	rm -rf $(BUILD_DIR)/${TEST_DIR}
	rm -f $(OBJECTS)
	rm -f $(DEPENDS)
	rm -f $(BUILD_DIR)/lib/*.a

clean:
	rm -rf $(BUILD_DIR)

static_analysis:
	clang-tidy $(SOURCES) -- -I$(INCLUDE_DIR) -I$(COMMON_INCLUDE_DIR)

.PHONY: all release debug clean clean_objects check_dirs copy_libs static_analysis tests

-include $(DEPENDS)
