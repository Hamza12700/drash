debug: src/main.cpp
	g++ -Wall -Wextra -fsanitize=address -fsanitize=undefined -fbounds-check -fstack-check -fstack-protector-all -g3 src/main.cpp -o ./build/debug
