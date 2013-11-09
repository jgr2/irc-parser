bin := parser

CFLAGS := -ansi -pedantic -Wall -Wextra -O0 -g3
.PHONY: all clean

all: $(bin)
	./$(bin)

clean:
	$(RM) ./$(bin)
