# Project

A lightweight Linux AI playground with a native C++/Qt desktop application and a small Python AI backend.

## Features

- Native C++17 + Qt 6 desktop UI
- Left/right conversation bubbles with selectable text
- Character-by-character assistant responses
- Persistent chats and conversation state
- Memory viewer and automatic memory support
- Conversation summaries for long chats
- Hugging Face and OpenAI-compatible providers
- Provider-specific API setup inside Settings
- Automatic detection of existing `HF_TOKEN` / `OPENAI_API_KEY` configuration
- Dynamic provider fields and model selection
- Dark, light, and system appearance modes
- Accent themes
- Low GPU / Balanced / Smooth UI performance profiles
- Native runtime debug panel with backend diagnostics
- No terminal UI and no PySide6 desktop dependency
- Lightweight design intended for older Linux hardware

## Requirements

- Linux
- CMake 3.16+
- A C++17 compiler
- Qt 6 Widgets development files
- Python 3
- Git

The Python backend uses only the standard library. PySide6 and PyOpenGL are not required.

## Installation

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

### openSUSE

```sh
sudo zypper install git cmake gcc-c++ libqt6-qtbase-devel python3
```

### Gentoo

Install Git, CMake, a C++17 compiler, Python 3, and Qt 6 with Widgets enabled.

### NixOS

Use a development shell containing Git, CMake, a C++ compiler, Python 3, and `qt6.qtbase`.

## Build and run

```sh
git clone https://github.com/2moresym/Project.git
cd Project
make run
```

Clean rebuild:

```sh
make clean
make run
```

## API setup

Open **Settings → AI provider**.

Choose a provider and its credentials. The form changes automatically for the selected provider.

### Hugging Face

- Hugging Face token
- Model dropdown

The default model is `openai/gpt-oss-120b:groq`. Hugging Face's OpenAI-compatible router supports `openai/gpt-oss-120b` and other conversational models. citeturn682582search0turn682582search1

### OpenAI-compatible

- API key
- Endpoint
- Model dropdown

A custom OpenAI-compatible endpoint can be used for other providers.

Existing `HF_TOKEN` or `OPENAI_API_KEY` environment variables are detected automatically, so the setup dialog only appears when the app still needs configuration.

Credentials are stored in the desktop application's local settings and are never committed to the repository.

## Desktop UI

The application is organized around a conventional chat layout:

```text
Sidebar                 Conversation
────────                ─────────────
New chat                You      → left
Chats                   Vaxx     → right
Settings
Memory                  Message composer
Debug
```

Assistant messages are rendered progressively instead of appearing as a complete block immediately.

## Debug mode

Open **Debug** to inspect backend startup, process state, stderr, request events, and exit/crash information. API keys are not included in the debug output.

## Project structure

```text
Project/
├── cpp/
│   ├── app_main.cpp
│   └── ChatBubble.hpp
├── src/
│   ├── backend_bridge.py
│   ├── chat.py
│   ├── config.py
│   ├── providers.py
│   └── sessions.py
├── CMakeLists.txt
└── Makefile
```

## Commands

```sh
make run
make native-build
make check
make test
make clean
```
