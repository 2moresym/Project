# Project

A tiny, lightweight AI playground for Linux.

## Features

- Native C++/Qt desktop UI with GPU OpenGL rendering
- GPU liquid-glass surface with a lightweight GLSL shader
- Smart UI performance profiles for older hardware
- Python AI/backend engine retained behind a local process bridge
- Persistent chat history, memories, and conversation summaries
- Hugging Face and OpenAI-compatible providers
- Lightweight terminal UI remains available as a fallback
- Offline demo backend when no API token is configured

## Requirements

- Python 3
- GNU Make
- CMake 3.16+
- Qt 6 development packages with Widgets, OpenGL, and OpenGLWidgets
- A Python virtual-environment module (`python3-venv` on Debian/Ubuntu)

The native desktop renderer is C++/Qt. Python remains responsible for the AI
provider and conversation backend. The two layers communicate through a small
line-oriented JSON process bridge.

On Debian/Ubuntu-based systems, install the build/runtime dependencies once if needed:

```sh
sudo apt install cmake qt6-base-dev python3-venv
```

## Build / run

```sh
git pull
make native-build
make run
```

`make run` configures and builds the native C++ desktop application with CMake,
then launches it. The Python backend is started automatically by the C++ UI when
a message is sent.

The compatibility Python entry point also works after the native binary is built:

```sh
.venv/bin/python -m src.gui
```

Prepare the Python environment separately when needed:

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

The existing Python provider code remains responsible for API communication.

## Native desktop UI

The desktop presentation layer now lives in C++ so OpenGL and Qt rendering use
Qt's native C++ APIs instead of Python OpenGL bindings. The current native shell
provides:

- Rounded sidebar and chat surface
- GPU liquid-glass shader surface
- Three performance profiles:
  - **Low GPU** — lighter static glass
  - **Balanced** — animated glass at a modest update rate (default)
  - **Smooth** — stronger glass with faster animation
- Non-blocking C++ UI while the Python backend performs AI requests
- Existing Python `Chat` and provider logic preserved behind `src/backend_bridge.py`

The glass renderer is isolated to its own `QOpenGLWidget`; it does not require a
global OpenGL backend switch for the entire application.

## Architecture

```text
Project/
├── cpp/
│   ├── main.cpp                 # native Qt desktop shell
│   ├── LiquidGlassWidget.cpp    # C++ OpenGL/GLSL renderer
│   └── LiquidGlassWidget.hpp
├── src/
│   ├── backend_bridge.py        # JSON bridge used by the native UI
│   ├── chat.py                  # conversation and memory state
│   ├── providers.py             # API providers
│   ├── gui.py                   # compatibility launcher for native UI
│   └── main.py                  # terminal UI
└── CMakeLists.txt               # native desktop build
```

This migration is intentional: C++ owns the performance-sensitive presentation
and rendering path, while Python keeps the flexible AI/backend code.

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
