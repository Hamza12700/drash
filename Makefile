debug:
	odin build src -debug -out:build/debug -error-pos-style:unix

opt:
	odin build src -o:speed -out:build/drash

install:
	cp build/drash ~/.local/bin/drash
