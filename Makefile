.PHONY: run native native-build check test clean

BUILD_DIR ?= build
NATIVE_BIN := $(BUILD_DIR)/ai_chat_native

native-build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel

run: native-build
	./$(NATIVE_BIN)

native: run

check: native-build
	python3 -m compileall -q src tests
	python3 -m unittest discover -s tests -q

test:
	python3 -m unittest discover -s tests -v

clean:
	rm -rf $(BUILD_DIR)
	find . -type d -name __pycache__ -prune -exec rm -rf {} +
