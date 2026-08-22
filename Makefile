.PHONY: run gui terminal check test clean

PYTHON ?= python3

run: gui

gui:
	$(PYTHON) -m src.gui

terminal:
	$(PYTHON) -m src.main

check:
	$(PYTHON) -m compileall -q src tests
	$(PYTHON) -m unittest discover -s tests -q

test:
	$(PYTHON) -m unittest discover -s tests -v

clean:
	find . -type d -name __pycache__ -prune -exec rm -rf {} +
