.PHONY: build clean clean-all cpp-test

build:
	mkdir -p build && cd build && cmake .. && make -j8

clean:
	find . -path ./venv -prune -o -type d -name __pycache__ -exec rm -rf {} +
	rm -rf dist pandana.egg-info .pytest_cache src/cyaccess.cpp test-output.xml

clean-all: clean
	rm -rf build cmake-build-* pandana/cyaccess*.so

cpp-test: build
	cd build && make test
