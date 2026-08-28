
`MIDIbutton` is a plugin to send MIDI messages from the Elgato Streamdeck.


# Description

`MIDIbutton` is a plugin to send MIDI messages (Note On/Off, CC and MMC) from the Elgato Streamdeck. The messages can be customised, and the MIDI port can be renamed. Uses the RtMidi library for MIDI communication.


# Features

- code written in C++
- runs on macOS and Linux (via [OpenDeck](https://github.com/nekename/OpenDeck)), with plans for a windows port


![](screenshot.png)


# Installation

In the Release folder, you can find the file `co.uk.clarionmusic.midibutton.streamDeckPlugin`. If you double-click this file on your machine, Stream Deck will install the plugin.


# Linux (OpenDeck)

On Linux the plugin runs under [OpenDeck](https://github.com/nekename/OpenDeck). To build it:

```
sudo apt install cmake g++ libasound2-dev   # Debian/Ubuntu
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cp build/midibutton co.uk.clarionmusic.midibutton.sdPlugin/midibutton-linux
```

Then copy the `co.uk.clarionmusic.midibutton.sdPlugin` folder into `~/.config/opendeck/plugins/` (without `sudo` — the folder must stay user-owned) and restart OpenDeck. The MIDI layer uses the ALSA sequencer; enabling *Use virtual port* in the plugin's global settings creates a `Streamdeck MIDI` port that other applications (DAWs, mixer remote apps such as Mixing Station, etc.) can connect to.

See **[docs/OPENDECK.md](docs/OPENDECK.md)** for a full reference aimed at humans *and* AI agents: adding buttons by editing OpenDeck profiles directly (no UI), exactly which MIDI bytes each action sends on key press/release, and how to bridge the ALSA sequencer port to applications that only see rawmidi devices (e.g. Java apps like Mixing Station).

# Source code

The Sources folder contains the source code of the plugin.
