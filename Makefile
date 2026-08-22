.PHONY: run check clean

PYTHON ?= python3

run:
	$(PYTHON) -m src.main

check:
	$(PYTHON) -m compileall -q src

clean:
	find . -type d -name __pycache__ -prune -exec rm -rf {} +
