debug: src/main.cpp
	g++ -Wall -Wextra -Wformat -Wswitch -fsanitize=address -fsanitize=undefined -fstack-protector-all -g3 src/main.cpp -o ./build/debug
