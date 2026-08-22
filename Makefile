.PHONY: run gui native native-build terminal check test setup clean

PYTHON ?= python3
VENV ?= .venv
VENV_PYTHON := $(VENV)/bin/python
VENV_PIP := $(VENV)/bin/pip
BUILD_DIR ?= build
NATIVE_BIN := $(BUILD_DIR)/ai_chat_native

setup: $(VENV_PYTHON)
	$(VENV_PIP) install -q -r requirements.txt

$(VENV_PYTHON):
	$(PYTHON) -m venv $(VENV)

native-build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel

run: native-build
	./$(NATIVE_BIN)

gui: run

native: native-build
	./$(NATIVE_BIN)

terminal:
	$(PYTHON) -m src.main

check:
	$(PYTHON) -m compileall -q src tests
	$(PYTHON) -m unittest discover -s tests -q


test:
	$(PYTHON) -m unittest discover -s tests -v

clean:
	rm -rf $(BUILD_DIR)
	find . -type d -name __pycache__ -prune -exec rm -rf {} +
