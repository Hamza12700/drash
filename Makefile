debug: src/main.cpp
	g++ -D"DEBUG" -fno-exceptions -fno-rtti -Werror=format-security -Wall -Wextra -Wstrict-overflow -Wformat -Wswitch -g src/main.cpp -o ./build/debug

opt: src/main.cpp
	g++ -march=native -fno-delete-null-pointer-checks -fno-exceptions -fno-rtti -fno-strict-overflow -O2 src/main.cpp -o ./build/drash
