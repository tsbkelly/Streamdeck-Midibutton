# Notes for AI agents (and curious humans)

This is a Stream Deck MIDI plugin (C++), with Linux/OpenDeck support on the
`linux-support` branch.

**To add or modify MIDI buttons on an OpenDeck profile programmatically, read
[docs/OPENDECK.md](docs/OPENDECK.md) first.** It contains the exact on-disk
profile format (including the non-obvious required `states` field, and why a
bad edit silently empties the profile), a per-action reference of exactly
which MIDI bytes are sent on key press/release, and the ALSA
seq→rawmidi bridge needed for Java apps to receive the plugin's output.

Ground rules learned the hard way:

- Never edit `~/.config/opendeck/profiles/**` while OpenDeck is running; stop
  it first (`pkill -x opendeck`), back up the profile, edit, restart.
- Never install/copy the plugin with `sudo` — the plugin directory must stay
  user-owned or OpenDeck can't spawn the binary.
- The plugin's virtual MIDI port only exists once a profile key using the
  plugin becomes visible (willAppear), and only in the ALSA *sequencer* layer.
- Use `pkill -x`/`pgrep -x` (exact match) when managing `opendeck` or
  `mixing-station` processes.

Build (Linux): `cmake -B build && cmake --build build` — output binary
`midibutton-linux` goes into `co.uk.clarionmusic.midibutton.sdPlugin/`.
