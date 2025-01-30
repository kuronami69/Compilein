# Compiler and tools
NDK_TOOLCHAIN ?= /path/to/android-ndk/toolchains/llvm/prebuilt/linux-x86_64/bin
CXX := $(NDK_TOOLCHAIN)/aarch64-linux-android30-clang++
AR  := $(NDK_TOOLCHAIN)/llvm-ar
STRIP := $(NDK_TOOLCHAIN)/llvm-strip

# Target architecture and API level
TARGET := aarch64-linux-android
API_LEVEL := 30

# Directories
SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
BIN_DIR := bin

# Files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
TARGET_BIN := $(BIN_DIR)/app

# Compiler flags
CXXFLAGS := -std=c++17 -I$(INC_DIR) --target=$(TARGET)$(API_LEVEL) -fPIC -Wall -D__ANDROID_API__=$(API_LEVEL)
LDFLAGS := -static-libstdc++ -landroid -llog

# Default target
all: $(TARGET_BIN)

# Build object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link executable
$(TARGET_BIN): $(OBJS) | $(BIN_DIR)
	$(CXX) $(LDFLAGS) -o $@ $^
	$(STRIP) $@

# Create necessary directories
$(OBJ_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

# Clean build files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean
