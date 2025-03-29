debug: src/main.cpp
	g++ -D"DEBUG" -Werror=format-security -Wall -Wextra -Wstrict-overflow -Wformat -Wswitch -g src/main.cpp -o ./build/debug

opt: src/main.cpp
	g++ -march=native -fno-delete-null-pointer-checks -fno-strict-overflow -O3 src/main.cpp -o ./build/drash
