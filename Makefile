.PHONY: run gui terminal check test setup clean

PYTHON ?= python3
VENV ?= .venv
VENV_PYTHON := $(VENV)/bin/python
VENV_PIP := $(VENV)/bin/pip

setup: $(VENV_PYTHON)
	$(VENV_PIP) install -q -r requirements.txt

$(VENV_PYTHON):
	$(PYTHON) -m venv $(VENV)

run: setup
	$(VENV_PYTHON) -m src.gui

gui: run

terminal:
	$(PYTHON) -m src.main

check:
	$(PYTHON) -m compileall -q src tests
	$(PYTHON) -m unittest discover -s tests -q

test:
	$(PYTHON) -m unittest discover -s tests -v

clean:
	find . -type d -name __pycache__ -prune -exec rm -rf {} +
