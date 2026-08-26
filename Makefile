BUILD ?= build

.PHONY: all configure test clean

all: test

configure:
	cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release

test: configure
	cmake --build $(BUILD) -j
	ctest --test-dir $(BUILD) --output-on-failure

clean:
	rm -rf $(BUILD)
