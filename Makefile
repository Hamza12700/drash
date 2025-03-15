debug: src/main.cpp
	g++ -D"DEBUG" -fsanitize=undefined -fsanitize=address -fsanitize=null -Wimplicit-fallthrough -Werror=format-security -Wall -Wextra -Wformat -Wswitch -g3 src/main.cpp -o ./build/debug

opt: src/main.cpp
	g++ -march=native -fno-delete-null-pointer-checks -fno-strict-overflow -O3 src/main.cpp -o ./build/drash
