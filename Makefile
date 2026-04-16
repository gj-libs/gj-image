# Compiler
CC = gcc

# Output library
NAME = libgj_image.a

# Flags
CFLAGS = -Wall -Wextra -O2 -Iinclude -Isrc

# Directories
SRC_DIR = src
BUILD_DIR = build

# Find all source files (internal only)
SRCS = $(shell find $(SRC_DIR) -name "*.c")

# Object files (mirror structure in build/)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Default target
all: $(NAME)

# Build static library
$(NAME): $(OBJS)
	ar rcs $@ $^

# Compile objects
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
