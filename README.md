# Project

A tiny, lightweight AI playground for Linux.

## Features

- Lightweight Python desktop UI (**AI Chat**) with mouse/keyboard support
- PySide6 desktop UI with rounded surfaces, subtle animation, and responsive layout
- Smart UI performance profiles for older hardware
- Collapsible chat sidebar with smooth width/fade animation
- Clean transparent labels and themed dropdown controls
- Persistent chat list with new, rename, switch, and delete actions
- Copy/select-all support for AI and user messages
- Persistent memories and conversation summaries
- Automatic memory/summarization options
- Hugging Face and OpenAI-compatible providers
- Model and provider switching
- Unified desktop Settings window with dropdowns and toggles
- Light, dark, and system appearance modes
- Accent themes that apply to the desktop UI
- Background AI requests so the UI stays responsive
- Conversation search
- Custom AI names
- Smart Unicode rendering for Markdown and common math/LaTeX
- Native rich-text rendering for headings, bold, italic, inline code, and fenced code blocks
- Lightweight terminal UI remains available as a fallback
- Offline demo backend when no API token is configured

## Requirements

- Python 3
- GNU Make
- A Python virtual-environment module (`python3-venv` on Debian/Ubuntu)

The desktop UI uses PySide6. The project installs it into a local `.venv` so it
does not modify Debian/Ubuntu's system-managed Python environment (PEP 668).

On Debian/Ubuntu-based systems, install the venv support once if needed:

```sh
sudo apt install python3-venv
```

No global `pip install` is required.

## Build / run

```sh
git pull
make check
make run
```

`make run` automatically creates `.venv` and installs the dependencies from
`requirements.txt` before launching **AI Chat**. You can also prepare the
environment without launching the app:

```sh
make setup
```

The terminal version remains available as a lightweight fallback:

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
export OPENAI_API_KEY="your-key"
export OPENAI_BASE_URL="https://your-endpoint/v1"
export OPENAI_MODEL="your-model"
```

The provider and model can also be changed from the desktop Settings panel.

## Desktop UI

The desktop app keeps the backend lightweight while using Qt for a smoother
presentation layer. It provides:

- Collapsible/expandable sidebar with smooth width and fade animation
- Rounded cards, controls, and input surfaces
- Transparent, correctly themed labels and cleaner dropdowns
- Smart UI performance profiles:
  - **Low GPU** — no drop shadow and shorter/lightweight effects
  - **Balanced** — subtle shadow and normal animations (default)
  - **Smooth** — slightly richer shadow and longer easing transitions
- Scrollable selectable conversation view
- Copy/select-all support
- Multiline message input
- Background requests so the window does not freeze during API calls
- Memory viewer
- AI name, provider, model, appearance, accent theme, UI performance,
  streaming, automatic-memory, and automatic-summary controls in one Settings window
- Dropdowns for provider, model, appearance, accent theme, and UI performance
- Light/dark/system appearance support
- Unicode and rich Markdown/math rendering
- Persistent local chat/session state

The performance selector is deliberately conservative: it adjusts UI effects
rather than forcing a global OpenGL backend, which keeps the app friendly to
older Mesa/Intel graphics stacks while still allowing a smoother appearance
on capable systems.

## Icons

Put the application icon in the repository's `icons/` directory.

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
├── qt_gui.py            # PySide6 desktop UI
├── gui.py               # compatibility desktop entry point
├── main.py              # terminal UI
├── chat.py              # conversation and memory state
├── providers.py         # API providers
├── sessions.py          # persistent chats
├── settings.py          # persistent settings
└── terminal_render.py   # terminal Markdown/math rendering
```

The project is deliberately lightweight so it remains practical on older Linux
hardware while leaving room for additional features.
