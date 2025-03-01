debug: src/main.cpp
	g++ -Wimplicit-fallthrough -Wnrvo -Werror=format-security -fstack-clash-protection -Wall -Wextra -Wformat -Wswitch -fsanitize=address -fsanitize=undefined -fstack-protector-all -g3 src/main.cpp -o ./build/debug

opt: src/main.cpp
	g++ -march=native -Wnrvo -fno-delete-null-pointer-checks -fno-strict-overflow -O3 src/main.cpp -o ./build/drash
