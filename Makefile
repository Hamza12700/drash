debug: src/main.c
	gcc -Wall -Wextra -fsanitize=address -g3 src/main.c -o ./build/debug
