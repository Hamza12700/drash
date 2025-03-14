debug: src/main.cpp
	g++ -D"DEBUG" -Wimplicit-fallthrough -Werror=format-security -Wall -Wextra -Wformat -Wswitch -fsanitize=undefined -g3 src/main.cpp -o ./build/debug

opt: src/main.cpp
	g++ -march=native -fno-delete-null-pointer-checks -fno-strict-overflow -O3 src/main.cpp -o ./build/drash
