# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -I./src/include -D_LOG_

# Directories
SRC_DIR := ./src
INCLUDE_DIR := ./src/include
BUILD_DIR := ./build

# Source files
SRC_FILES := \
    $(SRC_DIR)/main.cc \
    $(SRC_DIR)/microprocessor.cc \
    $(SRC_DIR)/dramcontroller.cc \
    $(SRC_DIR)/flashcontroller.cc \
    $(INCLUDE_DIR)/defs.cc \
    $(INCLUDE_DIR)/random.cc

# Object files (replace .cc with .o)
OBJ_FILES := $(patsubst %.cc, %.o, $(SRC_FILES))

# Output binary
TARGET := $(BUILD_DIR)/SSDsim

# Default rule
all: $(TARGET)

# Link object files to create the binary
$(TARGET): $(OBJ_FILES) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ_FILES)

# Compile source files to object files
%.o: %.cc
	$(CXX) -g $(CXXFLAGS) -c $< -o $@

$(SRC_DIR):
	rm -rf $(OBJ_FILES)

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	
# Clean build artifacts
clean:
	rm -rf $(TARGET) $(OBJ_FILES)
.PHONY: all clean

