# Project

A tiny, lightweight AI playground for Linux.

## Features

- Lightweight Python desktop UI (**AI Chat**) with mouse/keyboard support
- Multiple persistent chats
- Persistent memories and conversation summaries
- Automatic memory/summarization options
- Hugging Face and OpenAI-compatible providers
- Model and provider switching
- Unified desktop Settings window with dropdowns and toggles
- Light, dark, and system appearance modes
- Accent themes that apply to the desktop UI
- Rounded, lightweight custom UI controls
- Background AI requests so the UI stays responsive
- Conversation search
- Custom AI names
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

`make run` opens the desktop application as **AI Chat**. The terminal version
remains available as a lightweight fallback.

## API configuration

For Hugging Face:

```sh
export HF_TOKEN="your_token"
```

For an OpenAI-compatible provider:

```sh
export OPENAI_API_KEY="your-key"
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
- AI name, provider, model, appearance, accent theme, streaming,
  automatic-memory, and automatic-summary controls in one Settings window
- Dropdowns for provider, model, appearance, and accent theme instead of
  sequential prompts
- Light/dark/system appearance support
- Rounded buttons and cleaner panel/input styling
- Persistent local chat/session state
- Broad-Unicode UI fonts when Noto Sans is installed

The Settings dialog defers its modal grab until the window is mapped, avoiding
Tk/X11 `window not viewable` errors on systems where an immediate `grab_set()`
can fail.

The window/app identity is **AI Chat** rather than the default Tk name.

## Icons

Put the application icon in the repository's `icons/` directory. Once an icon
is added there, it can be wired into the desktop launcher/window without
changing the rest of the UI.

## Terminal fallback

Use the terminal version when you want the smallest possible interface. It
supports commands such as:

- `/help` — show commands
- `/ui` — return to the main UI
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
