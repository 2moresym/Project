# Project

A tiny, lightweight AI playground for Linux.

## Features

- Lightweight Python desktop UI with mouse/keyboard support
- Multiple persistent chats
- Persistent memories and conversation summaries
- Automatic memory/summarization options
- Hugging Face and OpenAI-compatible providers
- Model and provider switching
- Background AI requests so the UI stays responsive
- Conversation search
- Custom AI names and themes
- Smart Unicode rendering for Markdown and common math/LaTeX
- Lightweight terminal UI remains available as a fallback
- Offline demo backend when no API token is configured

## Requirements

- Python 3
- GNU Make
- **Tkinter** for the desktop UI

Tkinter is part of Python's standard library, so there are no Python packages
to install with pip. On Debian/Ubuntu-based Linux systems, install the system
package if Tkinter is missing:

```sh
sudo apt install python3-tk
```

## Build / run

```sh
make check
make run
```

`make run` now opens the desktop application. To use the lightweight terminal
version instead:

```sh
make terminal
```

## API configuration

For Hugging Face:

```sh
export HF_TOKEN="your_token"
```

For an OpenAI-compatible provider:

```sh
export OPENAI_API_KEY="your_key"
export OPENAI_BASE_URL="https://your-endpoint/v1"
export OPENAI_MODEL="your-model"
```

The provider and model can also be changed from the desktop Settings panel.

## Desktop UI

The desktop app keeps the project lightweight while making rich AI output
much easier to read. It provides:

- Chat list with new, rename, switch, and delete actions
- Scrollable conversation view
- Multiline message input
- Background requests so the window does not freeze during API calls
- Memory viewer
- AI name, provider, and model settings
- Persistent local chat/session state

Markdown and common LaTeX are normalized for terminal-safe display as a
fallback, while the desktop view provides a more comfortable reading surface.

## Terminal commands

- `/help` — show commands
- `/ui` — return to the main UI
- `/new <name>` — create a chat
- `/search <text>` — search conversation history
- `/memory` — view saved memories
- `/remember <text>` — save a memory
- `/forget <number>` — remove a memory
- `/clear` — clear the current conversation while keeping memories
- `/model` — show the selected model
- `/models` — open model selection
- `/provider` — switch API provider
- `/theme` — switch theme
- `/name <name>` — rename the AI
- `/save` — save state
- `/quit` — save and exit

## Data

Local conversations, memories, summaries, and settings are stored under
`data/`. Keep API keys in environment variables rather than committing them
to the repository.

## Architecture

The project keeps the AI backend separate from its presentation layers:

```text
src/
├── gui.py              # desktop UI
├── main.py             # terminal UI
├── chat.py             # conversation and memory state
├── providers.py        # API providers
├── sessions.py         # persistent chats
├── settings.py         # persistent settings
└── terminal_render.py  # terminal Markdown/math rendering
```

The project is deliberately lightweight so it remains practical on older Linux
hardware while leaving room for additional features.
