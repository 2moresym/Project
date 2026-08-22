.PHONY: run check test clean

PYTHON ?= python3

run:
	$(PYTHON) -m src.main

check:
	$(PYTHON) -m compileall -q src tests
	$(PYTHON) -m unittest discover -s tests -q

test:
	$(PYTHON) -m unittest discover -s tests -v

clean:
	find . -type d -name __pycache__ -prune -exec rm -rf {} +
