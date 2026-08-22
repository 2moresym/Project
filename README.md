# Project

A small Linux AI playground with a native C++/Qt desktop UI and a lightweight Python AI backend.

## What it includes

- Native C++/Qt desktop application
- Left/right chat bubbles with selectable text
- Character-by-character assistant typing animation
- Persistent Python chat/memory backend through a local JSON process bridge
- Hugging Face and OpenAI-compatible providers
- First-run provider setup window
- GUI for API keys, endpoint, and model selection
- Local chat memory viewer
- Native debug window for runtime/backend diagnostics
- Lightweight UI designed to remain usable on older Linux hardware
- No terminal UI and no Python Qt dependency

## Requirements

- Linux
- CMake 3.16 or newer
- Qt 6 Widgets development files
- A C++17 compiler
- Python 3 for the AI backend
- Git

The Python backend uses only the standard library. The desktop application does **not** require PySide6, a Python virtual environment, or PyOpenGL.

## Installation by distro

### Debian / Ubuntu / Linux Mint / Pop!_OS

```sh
sudo apt update
sudo apt install git cmake build-essential qt6-base-dev python3
```

### Fedora

```sh
sudo dnf install git cmake gcc-c++ qt6-qtbase-devel python3
```

### Arch Linux / Manjaro

```sh
sudo pacman -Syu --needed git cmake base-devel qt6-base python
```

Arch Linux provides Qt 6 development support through the `qt6-base` package.

### openSUSE Tumbleweed / Leap

```sh
sudo zypper install git cmake gcc-c++ libqt6-qtbase-devel python3
```

### Gentoo

Install Git, CMake, a C++ compiler, Python 3, and Qt 6 with the Widgets module enabled. Package names and USE flags depend on the selected profile.

### NixOS

Use a development shell containing `cmake`, `gcc` or `clang`, `qt6.qtbase`, `python3`, and `git`, then build the project normally.

## Build and run

```sh
git clone https://github.com/2moresym/Project.git
cd Project
make run
```

The first build creates `build/ai_chat_native`. Later launches only need:

```sh
make run
```

To force a clean native rebuild:

```sh
make clean
make run
```

## API setup — no terminal required

On first launch the app opens **AI provider setup** automatically.

You can also open it at any time from:

```text
Sidebar → AI provider & API key
```

Choose either **Hugging Face** or **OpenAI-compatible**, then enter the appropriate credential. For OpenAI-compatible providers you can also enter a custom endpoint and model name.

Credentials are saved in the local desktop settings used by the application and are not written into the Git repository. Treat your desktop account as sensitive because the saved credential is local configuration data.

### Hugging Face

Paste your Hugging Face access token into the **Hugging Face token** field.

The default model is:

```text
openai/gpt-oss-120b:groq
```

### OpenAI-compatible

Paste your API key into **OpenAI API key**.

The default endpoint is:

```text
https://api.openai.com/v1/chat/completions
```

You can replace it with another OpenAI-compatible endpoint and choose its model.

## Desktop controls

The native sidebar includes:

```text
＋ New chat
Chats
AI provider & API key
Memory
Settings
Debug
```

The main conversation uses separate bubbles:

```text
You   → left
Vaxx  → right
```

Assistant responses arrive from the Python backend and are then typed into the UI one character at a time rather than appearing as one large block.

## Debug mode

Open **Debug** when something does not behave correctly. The panel records useful runtime information such as Qt version, backend state, provider selection, backend stderr, and request/response events without exposing API keys.

## Architecture

```text
Project/
├── cpp/
│   ├── main.cpp          # native Qt desktop UI
│   └── ChatBubble.hpp    # chat message widget
├── src/
│   ├── backend_bridge.py # local JSON bridge for the native UI
│   ├── chat.py           # conversation and memory state
│   ├── config.py         # backend defaults
│   ├── providers.py      # Hugging Face / OpenAI-compatible APIs
│   └── sessions.py       # persistent chat sessions
├── CMakeLists.txt
└── Makefile
```

The native C++ program owns the entire desktop UI. Python remains a small backend service so the AI/provider code can stay easy to maintain.

## Useful commands

```sh
make run
make native-build
make check
make test
make clean
```

There is intentionally **no terminal UI target**. The project is designed around the graphical application so new Linux users do not need shell commands to configure an AI provider.
