debug: src/main.c
	gcc -Wall -Wextra -fsanitize=address -g src/main.c -o ./build/debug
