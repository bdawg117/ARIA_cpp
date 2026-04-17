# ARIA - Autonomous Reactive Intelligent Assistant

A small passion project — a ROS2-based AI assistant that listens for voice commands, processes them through a local LLM, and speaks back to the user. Eventually this will be the brain of a physical robot as part of a larger robotics project.

This is a C++ rewrite of an earlier Python prototype (Now deprecated and private, although I can share if you are interested). It was too slow with the Python implementation.

This will be the offical development version going forward.

## What it does

- Listens for a wake word ("aria") via microphone
- Sends the recognized command to a local LLM (Phi-4-mini-instruct via llama.cpp)
- Speaks the response back using Piper TTS
- Can execute basic ROS2 commands (list topics, list nodes, broadcast messages, control servos and LEDs)

## Dependencies

- **ROS2 Jazzy**
- **llama.cpp** — local LLM inference, built from source
- **Phi-4-mini-instruct** (`Phi-4-mini-instruct.BF16.gguf`) — runs locally via llama.cpp
- **Vosk** (`vosk-model-en-us-0.22-lgraph`) — offline speech recognition
- **Piper** — local text-to-speech (`en_US-libritts_r-medium.onnx` voice model)
- **PortAudio** — microphone input (`sudo apt install libportaudio2 libportaudio-dev`)

## Building

```bash
cd ~/cpp_robotics/aria_ws
colcon build
source install/setup.zsh
```

## Running

Each node runs in its own terminal:

```bash
ros2 run aria_cpp aria          # AI brain (LLM)
ros2 run aria_cpp aria_tts      # Text-to-speech
ros2 run aria_cpp aria_ears     # Microphone / wake word
ros2 run aria_cpp aria_terminal # Optional: type commands manually
```

## Wake word

Say **"aria"** followed by your command. Examples:

- `aria how are you`
- `aria what topics do you see`
- `aria broadcast hello world`
- `aria turn the LED on`