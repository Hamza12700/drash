cc=g++

debug: src/main.cpp
	$(cc) -D"DEBUG" -fno-exceptions -fno-rtti -Werror=format-security -Wall -Wextra -Wstrict-overflow -Wformat -Wswitch -g src/main.cpp -o ./build/debug
	cp build/debug ~/.local/bin/debug-drash

opt: src/main.cpp
	$(cc) -march=native -fno-delete-null-pointer-checks -fno-exceptions -fno-rtti -O2 -static src/main.cpp -o ./build/drash
	cp build/drash ~/.local/bin
