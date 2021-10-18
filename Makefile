CC = zig cc
#CC = gcc

BIN = vectornotes
BUILD_DIR = build
SRC_DIR = src
INC_DIR = inc src

INC_DIRS = $(shell find $(INC_DIR) -type d -not -path '*/\.*')
INC_FLAGS = $(addprefix -I,$(INC_DIRS))

SRC = $(shell find $(SRC_DIR) -name '*.c' -not -path '*/\.*')
OBJ = $(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

CFLAGS = -std=c18 -Werror -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -g $(INC_FLAGS)
LDFLAGS =
LDLIBS = -lglfw -ldl

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: all clean
clean:
	rm -r $(BUILD_DIR) $(EXE)

all: $(EXE)
