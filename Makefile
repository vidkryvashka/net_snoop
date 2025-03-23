# CC = aarch64-openwrt-linux-musl-gcc # gcc
LOCAL_FLAGS = -Wall -Wno-unused-variable -Wno-unused-function -Wno-main -Iinclude
SRC_LOCAL_NS_DIR = ./src
OBJ_LOCAL_NS_DIR = ./obj
BIN_LOCAL_NS_DIR = ./bin
LOCAL_NS_TARGET = $(BIN_LOCAL_NS_DIR)/net_snoop

SRCS = $(wildcard $(SRC_LOCAL_NS_DIR)/*.c)

OBJS = $(patsubst $(SRC_LOCAL_NS_DIR)/%.c, $(OBJ_LOCAL_NS_DIR)/%.o, $(SRCS))

all: $(OBJ_LOCAL_NS_DIR) $(BIN_LOCAL_NS_DIR) $(LOCAL_NS_TARGET)

$(BIN_LOCAL_NS_DIR):
	mkdir $(BIN_LOCAL_NS_DIR)

$(OBJ_LOCAL_NS_DIR):
	mkdir $(OBJ_LOCAL_NS_DIR)

$(OBJ_LOCAL_NS_DIR)/%.o: $(SRC_LOCAL_NS_DIR)/%.c
	$(CC) $(LOCAL_FLAGS) -c $< -o $@

$(LOCAL_NS_TARGET): $(OBJS)
	$(CC) $(LOCAL_FLAGS) $^ -o $@

run: $(LOCAL_NS_TARGET)
	./$(LOCAL_NS_TARGET)

clean:
	rm -f $(OBJ_LOCAL_NS_DIR)/*.o
	rm -rf $(BIN_LOCAL_NS_DIR)/*

help:
	@echo "Available targets:"
	@echo "  all        	: Compile the project"
	@echo "  clean      	: Remove object files and executable"
