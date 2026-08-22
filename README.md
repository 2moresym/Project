# Project

A tiny, lightweight AI playground for Linux.

## Features

- Terminal UI with keyboard navigation and Escape-to-menu
- Multiple persistent chats
- Persistent memories and conversation summaries
- Automatic memory/summarization options
- Hugging Face and OpenAI-compatible providers
- Model and provider switching
- Streaming responses
- Conversation search
- Custom AI names and themes
- Smart Unicode terminal rendering for common math/LaTeX
- Offline demo backend when no API token is configured

## Requirements

- Python 3
- GNU Make

No third-party Python packages are required for the current version.

## Build / run

```sh
make check
make run
```

Project opens in its terminal UI. Choose **Continue current chat** to resume
an existing conversation or **New chat** to start a separate one.

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

The exact provider/model can also be changed from the UI.

## Commands

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

Inside the UI, use **↑/↓ or W/S**, then **Enter** to select. **Escape**
returns to the UI from chat.

## Terminal rendering

Project defaults to Unicode-friendly rendering for common mathematical notation.
For example, common LaTeX such as `x^2`, `\\sqrt{x}`, and `\\frac{a}{b}` is
rendered as readable terminal text such as `x²`, `√x`, and `(a)/(b)`.
Normal prose is left unchanged.

## Data

Local conversations, memories, summaries, and settings are stored under
`data/`. Keep API keys in environment variables rather than committing them
to the repository.

## Architecture

The project is split into small modules for configuration, settings, providers,
chat state, sessions, terminal rendering, and the terminal controller. The
provider layer is replaceable, allowing remote APIs or the offline demo backend
to share the same chat interface.

The project is deliberately lightweight so it remains practical on older Linux
hardware while leaving room for additional features.
