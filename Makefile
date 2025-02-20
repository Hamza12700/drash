debug: src/main.c
	gcc -Wall -Wextra -fsanitize=address -fsanitize=undefined -fstack-protector-all -g3 src/main.c -o ./build/debug
