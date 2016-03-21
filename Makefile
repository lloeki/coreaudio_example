all: example

example:
	clang++ -std=c++11 -stdlib=libc++ -Wl,-framework,CoreAudio -Wl,-framework,AudioUnit coreaudio_example.cc coreaudio_example_main.cc -o example

clean:
	rm -rf example

PHONY: clean
