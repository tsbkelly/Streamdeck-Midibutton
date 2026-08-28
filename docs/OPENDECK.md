# Using this plugin with OpenDeck on Linux — and adding buttons programmatically

This guide is written for both humans and AI agents. It documents everything
needed to add working MIDI buttons to an [OpenDeck](https://github.com/ninjadev64/OpenDeck)
profile **by editing files directly** (no UI clicking), plus the Linux MIDI
routing needed for other applications to actually receive the plugin's output.

Everything below was verified against OpenDeck 2.13.1 and this plugin's
`linux-support` branch. Source references are to this repository unless noted.

---

## 1. Installation rules (Linux / OpenDeck)

- Plugin directory: `~/.config/opendeck/plugins/co.uk.clarionmusic.midibutton.sdPlugin/`
- **The plugin directory must be owned by the user running OpenDeck.** OpenDeck
  runs `fs::set_permissions(0o755)` on the plugin binary at *every* spawn
  (OpenDeck `plugins/mod.rs`). If the directory is root-owned (e.g. you copied
  it with `sudo`), the plugin dies with *"Permission denied (os error 13)"*.
  Fix: `chown -R $USER: ~/.config/opendeck/plugins/co.uk.clarionmusic.midibutton.sdPlugin`
- Pre-seed the global settings so the plugin opens its virtual MIDI port
  without any UI interaction — write
  `~/.config/opendeck/settings/co.uk.clarionmusic.midibutton.sdPlugin.json`:

  ```json
  { "useVirtualPort": true }
  ```

- The virtual port is only opened after the first `willAppear` event
  (`WillAppearForAction` → `InitialSetup` → `GetGlobalSettings` →
  `DidReceiveGlobalSettings` → `InitialiseMidi`, see
  `Sources/StreamDeckMidiButton.cpp`). **If no key on the current profile uses
  this plugin, no MIDI port exists.**

## 2. OpenDeck profile disk format

Profiles live at
`~/.config/opendeck/profiles/<device-id>/<ProfileName>.json`
(device id example: `sd-A00NA53831YSOD`).

**Edit profiles only while OpenDeck is stopped.** OpenDeck keeps profiles in
memory and writes them back on exit — a running instance will silently
overwrite your edit.

```bash
pkill -x opendeck          # stop (also stops plugins)
# ... edit the JSON ...
setsid nohup opendeck &    # restart (add DISPLAY=:0 or :1 if needed)
```

Top level (OpenDeck `store/simplified_profile.rs`, struct `DiskProfile` — this
is *not* the same struct as the runtime `Profile` in `shared.rs`):

```json
{ "keys": [ <key or null> × 32 ], "sliders": [...], "infobars": [...] }
```

Each key is a `DiskActionInstance`:

| field | meaning |
|---|---|
| `action` | static action definition (copied from the plugin manifest) |
| `context` | `"Keypad.<position>.<key index>"` — 3 segments |
| `states` | **displayed** state(s): title text, colours, image |
| `current_state` | index into `states` |
| `settings` | the plugin's per-button settings (see §3) |
| `children` | `null` unless it's a folder/multi action |

⚠️ **The top-level `states` field is required and is distinct from
`action.states`.** `action.states` holds the manifest defaults; the top-level
`states` is what the deck actually renders (titles live here). If any key is
missing a required field, **the whole profile fails to parse and OpenDeck
saves back an EMPTY profile on exit** — always keep a backup:
`cp Default.json Default.json.bak`.

Image/property-inspector paths are relative to `~/.config/opendeck/`
(e.g. `plugins/co.uk.clarionmusic.midibutton.sdPlugin/midibuttoncc.png`).
A state `image` starting with a digit refers to a custom image under
`images/<device>/<profile>/<context>/`.

### Complete working key (momentary CC button)

```json
{
  "action": {
    "controllers": ["Keypad"],
    "disable_automatic_states": false,
    "encoder": null,
    "icon": "plugins/co.uk.clarionmusic.midibutton.sdPlugin/midibuttoncc.png",
    "name": "MIDI CC",
    "plugin": "co.uk.clarionmusic.midibutton.sdPlugin",
    "property_inspector": "plugins/co.uk.clarionmusic.midibutton.sdPlugin/propertyinspector/index.html",
    "states": [ { "alignment": "middle", "background_colour": "#000000",
      "colour": "#FFFFFF", "family": "Liberation Sans",
      "image": "plugins/co.uk.clarionmusic.midibutton.sdPlugin/midibuttoncc.png",
      "image_scale": 100, "name": "", "show": true, "size": 17,
      "stroke_colour": "#000000", "stroke_size": 3, "style": "Regular",
      "text": "", "underline": false } ],
    "supported_in_multi_actions": true,
    "tooltip": "Send a MIDI CC message",
    "uuid": "uk.co.clarionmusic.midibutton.cc",
    "visible_in_action_list": true
  },
  "children": null,
  "context": "Keypad.0.0",
  "current_state": 0,
  "states": [ { "alignment": "middle", "background_colour": "#000000",
    "colour": "#FFFFFF", "family": "Liberation Sans",
    "image": "plugins/co.uk.clarionmusic.midibutton.sdPlugin/midibuttoncc.png",
    "image_scale": 100, "name": "", "show": true, "size": 17,
    "stroke_colour": "#000000", "stroke_size": 3, "style": "Regular",
    "text": "DCA 1", "underline": false } ],
  "settings": {
    "ccMode": 1, "statusByte": 180, "dataByte1": 12,
    "dataByte2": 127, "dataByte2Alt": 0,
    "fadeCurve": 0, "fadeTime": 0, "toggleFade": false
  }
}
```

For a two-state action (`cctoggle`, `noteontoggle`) supply **two** entries in
both `action.states` and the top-level `states` (manifest defines 2 states).

## 3. Action reference — exactly what each button sends

All actions read their settings from the same fields
(`StoreButtonSettings`, `Sources/StreamDeckMidiButton.cpp:564-776`):
`statusByte`, `dataByte1`, `dataByte2`, `dataByte2Alt`, `dataByte5`,
`noteOffMode`, `ccMode`, `fadeTime`, `fadeCurve`, `toggleFade`.

`statusByte` = MIDI status: `0xB0 + channel` for CC (so **180 = 0xB4 = CC on
channel 5**, 1-based), `0x90 + channel` for notes, `0xC0 + channel` for
program change.

| action UUID (`uk.co.clarionmusic.midibutton.*`) | key press sends | key release sends |
|---|---|---|
| `.cc`, `ccMode: 0` "Single Value" | `[statusByte, dataByte1, dataByte2]` | nothing |
| `.cc`, `ccMode: 1` "Momentary Value" | `[statusByte, dataByte1, dataByte2]` | `[statusByte, dataByte1, dataByte2Alt]` |
| `.cc`, `ccMode: 2/3` "Momentary with Fade In/Out" | timer streams CC values `dataByte2`⇄`dataByte2Alt` over `fadeTime` | reverse fade |
| `.cctoggle` | alternates per press: `dataByte2` (state 0) / `dataByte2Alt` (state 1) | **nothing, ever** |
| `.noteon`, `noteOffMode: 0` | note-on `[statusByte, dataByte1, dataByte2]` | nothing |
| `.noteon`, `noteOffMode: 1` "On Push" | note-on, then immediately velocity-0 note-off | nothing |
| `.noteon`, `noteOffMode: 2` "On Release" | note-on | velocity-0 note-off |
| `.noteontoggle` | alternates: note-on / velocity-0 note-off | nothing |
| `.programchange` | `[statusByte, dataByte1]` (2 bytes) | nothing |
| `.mmc` | SysEx `F0 7F 7F 06 <dataByte5> F7` | nothing |

(Key handling: `KeyDownForAction` :778-1068, `KeyUpForAction` :1070-1121 —
only `.noteon` and `.cc` do anything on release; every other action returns.)

Incoming MIDI that matches a `cctoggle`/`noteontoggle` button's
`statusByte`+`dataByte1` flips that button's displayed state
(`HandleMidiInput`, :285-302) — useful for state feedback from the target app.

### Choosing between `cc` and `cctoggle` for toggle-style targets

Many applications (e.g. Mixing Station's "click" button mode) treat a control
like a physical button: they need a **press+release pair** (value >0 followed
by value 0, within a short time window) to register one click. For those, use
`.cc` with `ccMode: 1` (127 on press, 0 on release) — **one tap = one click**.
`cctoggle` sends only one message per press, so such apps see a complete
click only every *two* presses ("double-click bug").

## 4. Linux MIDI routing — making other apps see the port

The plugin's virtual port ("Streamdeck MIDI", RtMidi) exists only in the ALSA
**sequencer** layer. Java applications (`javax.sound.midi` — e.g. Mixing
Station) enumerate only ALSA **rawmidi** hardware devices (OpenJDK
`PLATFORM_API_LinuxOS_ALSA_MidiUtils.c`) and can never see it. Bridge with the
`snd-virmidi` kernel loopback, which is both a rawmidi card and a seq client:

```bash
# /etc/modules-load.d/virmidi.conf
snd-virmidi
# /etc/modprobe.d/virmidi.conf
options snd-virmidi index=2 midi_devs=1
```

Then connect plugin → virmidi by client *name* (client numbers change on
every restart) and re-fire on every ALSA announce event so it survives
OpenDeck restarts. Run as a service:

```bash
SRC='RtMidi client'; DST='Virtual Raw MIDI'
wire() { aconnect "$SRC" "$DST" 2>/dev/null || true; }
wire
stdbuf -oL aseqdump -p 'System:announce' | while read -r _; do wire; done
```

The receiving app then opens the rawmidi device `VirMIDI [hw:2,0,0]`.
Java caches its device list at startup — (re)start the receiving app *after*
virmidi is loaded.

## 5. Verification / debugging playbook

```bash
# 1. plugin alive + port open? (expect 'RtMidi client' connected to virmidi)
aconnect -l | grep -A2 'RtMidi client'

# 2. keys loaded? (expect one WillAppearForAction per plugin key)
grep 'WillAppearForAction' ~/.local/share/opendeck/logs/opendeck.log | tail

# 3. key presses reaching the plugin?
grep 'SendMidiMessage' ~/.local/share/opendeck/logs/opendeck.log | tail

# 4. simulate the deck without touching it: inject events into the virmidi
#    seq port (24:0 here) — the receiving app can't tell the difference.
aplaymidi -d 1 -p 24:0 test.mid
```

A profile edit that "did nothing" usually means OpenDeck was still running
(it saved over your edit), or the JSON failed to parse (check that every key
has the top-level `states` field — and restore from your backup, because the
parse failure empties the profile on next save).
