.PHONY: build clean clean-all cpp-test

build:
	mkdir -p build && cd build && cmake .. && make -j8

clean:
	rm -rf dist pandana.egg-info

clean-all: clean
	rm -rf build cmake-build-*

cpp-test: build
	cd build && make test
