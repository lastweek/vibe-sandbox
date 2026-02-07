# nano-kata Makefile
# Educational OCI Container Runtime with VM Isolation

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu11 -g -O2 -Wno-format-truncation -Wno-unused-result
LDFLAGS = -lpthread

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INCLUDE_DIR = include
BUILD_DIR = build

# Source files
MAIN_SRC = $(SRC_DIR)/main.c
OCI_SRC = $(SRC_DIR)/oci/spec.c
COMMON_SRC = $(SRC_DIR)/common/state.c
CONTAINER_SRC = $(SRC_DIR)/container/namespaces.c \
                $(SRC_DIR)/container/mounts.c \
                $(SRC_DIR)/container/cgroups.c \
                $(SRC_DIR)/container/process.c

# Object files
MAIN_OBJ = $(OBJ_DIR)/main.o
OCI_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(OCI_SRC))
COMMON_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(COMMON_SRC))
CONTAINER_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(CONTAINER_SRC))
ALL_OBJ = $(MAIN_OBJ) $(OCI_OBJ) $(COMMON_OBJ) $(CONTAINER_OBJ)

# Targets
NK_RUNTIME = $(BIN_DIR)/nk-runtime

# Include paths
INCLUDES = -I$(INCLUDE_DIR)

# Library dependencies
LIBS = -ljansson

# Optional libraries
CHECK_CAPNG = $(shell pkg-config --exists libcap-ng && echo 1)
ifeq ($(CHECK_CAPNG),1)
    CFLAGS += -DHAVE_LIBCAPNG
    LIBS += $(shell pkg-config --libs libcap-ng)
endif

# Default target
all: directories $(NK_RUNTIME)

# Create necessary directories
directories:
	@mkdir -p $(OBJ_DIR)/oci $(OBJ_DIR)/container $(OBJ_DIR)/vm
	@mkdir -p $(OBJ_DIR)/shim $(OBJ_DIR)/common
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(BUILD_DIR)
	@mkdir -p run

# Link main runtime binary
$(NK_RUNTIME): $(ALL_OBJ)
	@echo "Linking $@"
	$(CC) $(CFLAGS) $(ALL_OBJ) -o $@ $(LDFLAGS) $(LIBS)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts"
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(BUILD_DIR)

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@which gcc > /dev/null || (echo "Error: gcc not found"; exit 1)
	@echo "All dependencies OK!"

# Help target
help:
	@echo "nano-kata Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build main runtime (default)"
	@echo "  clean        - Remove build artifacts"
	@echo "  check-deps   - Check for required dependencies"
	@echo "  help         - Show this help message"

# Phony targets
.PHONY: all directories clean check-deps help
