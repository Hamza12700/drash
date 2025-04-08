cc=g++

debug: src/main.cpp
	$(cc) -D"DEBUG" -fno-exceptions -fno-rtti -Werror=format-security -Wall -Wextra -Wstrict-overflow -Wformat -Wswitch -g src/main.cpp -o ./build/debug

opt: src/main.cpp
	$(cc) -march=native -fno-delete-null-pointer-checks -fno-exceptions -fno-rtti -fno-strict-overflow -O2 src/main.cpp -o ./build/drash
	cp build/drash ~/.local/bin
