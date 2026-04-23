# Compiler
CC = gcc

# Output library
NAME = libgj_image.a

# Directories
SRC_DIR = src
BUILD_DIR = build
TEST_DIR = src/test
TEST_SRC = $(TEST_DIR)/test.c
TEST_BIN = test

SRCS = $(shell find $(SRC_DIR) -name "*.c")
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

CFLAGS_BASE   = -Wall -Wextra -Iinclude -Isrc -g
CFLAGS_RELEASE = -O2
CFLAGS_PROFILE = -O0 -pg

LDFLAGS_BASE   = -lX11
LDFLAGS_PROFILE = -pg

# Default mode = release build
CFLAGS  = $(CFLAGS_BASE) $(CFLAGS_RELEASE)
LDFLAGS = $(LDFLAGS_BASE)

all: $(NAME)

# Build static library
$(NAME): $(OBJS)
	ar rcs $@ $^

# Compile objects
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(NAME)
	$(CC) $(CFLAGS_BASE) $(CFLAGS_RELEASE) $(TEST_SRC) -L. -lgj_image $(LDFLAGS_BASE) -o $(TEST_BIN)

profile: clean
	$(MAKE) CFLAGS="$(CFLAGS_BASE) $(CFLAGS_PROFILE)" \
	        LDFLAGS="$(LDFLAGS_BASE) $(LDFLAGS_PROFILE)" \
	        all

	$(CC) $(CFLAGS_BASE) $(CFLAGS_PROFILE) $(TEST_SRC) \
	    -L. -lgj_image $(LDFLAGS_BASE) $(LDFLAGS_PROFILE) \
	    -o $(TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -f $(NAME) $(TEST_BIN)

re: fclean all

.PHONY: all test profile clean fclean re
