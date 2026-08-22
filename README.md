# Project

A tiny, lightweight AI playground for Linux.

## Requirements

- Python 3
- GNU Make

No Python packages are required for the current version.

## Build / run

```sh
make check
make run
```

The program starts an interactive terminal chat. The included demo backend is
fully offline and deterministic, so the project works immediately without an
API key or a large local model.

## Commands

- `/help` — show commands
- `/history` — show the current conversation
- `/clear` — clear the conversation
- `/save` — save history to `data/history.json`
- `/quit` — save and exit

## Architecture

The terminal UI talks to a small `AIProvider` interface. The demo provider is
intentionally replaceable; a future provider can connect to a local model,
remote API, or another inference engine without rewriting the chat shell.

The project is deliberately small so it can remain usable on older Linux
machines and grow one feature at a time.
