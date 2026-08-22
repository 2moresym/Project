# Project

A tiny, lightweight AI playground for Linux.

## Features

- Lightweight Python desktop UI (**AI Chat**) with mouse/keyboard support
- PySide6/Qt desktop interface with smooth sidebar animation and subtle shadowing
- Collapsible chat sidebar for more conversation space
- Persistent chat list with new, rename, switch, and delete actions
- Selectable rich AI output with Unicode math and Markdown formatting
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
- Smart Unicode rendering for common math/LaTeX
- Lightweight terminal UI remains available as a fallback
- Offline demo backend when no API token is configured

## Requirements

- Python 3
- GNU Make
- PySide6 for the desktop app

Install the desktop dependency with:

```sh
python3 -m pip install -r requirements.txt
```

The project also keeps its terminal fallback dependency-light.

## Build / run

```sh
make check
make run
```

`make run` opens the PySide6 desktop application as **AI Chat**. The terminal
version remains available as a lightweight fallback:

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

The desktop app keeps the project lightweight while providing a more polished
Qt surface for rich AI output. It provides:

- Collapsible/expandable animated sidebar
- Rounded cards, native Qt controls, and subtle drop shadows
- Scrollable selectable conversation view
- Unicode math and basic Markdown rendering
- Multiline message input
- Background requests so the window does not freeze during API calls
- Memory viewer
- AI name, provider, model, appearance, accent theme, streaming,
  automatic-memory, and automatic-summary controls in one Settings window
- Dropdowns for provider, model, appearance, and accent theme instead of
  sequential prompts
- Light/dark/system appearance support
- Persistent local chat/session state
- Application icon loaded from `icons/`

The UI deliberately avoids Qt WebEngine and other heavyweight components so the
visual layer stays practical on older integrated graphics.

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
├── gui.py              # desktop entry point
├── qt_gui.py           # PySide6 desktop UI
├── main.py             # terminal UI
├── chat.py             # conversation and memory state
├── providers.py        # API providers
├── sessions.py         # persistent chats
├── settings.py         # persistent settings
└── terminal_render.py  # terminal Markdown/math rendering
```

The project is deliberately lightweight so it remains practical on older Linux
hardware while leaving room for additional features.
