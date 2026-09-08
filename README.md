# Fairo

A guitar-effects plugin that emulates the **[Black Arts Toneworks Pharaoh](https://www.blackartstoneworks.com/pedal/pharaoh/)** fuzz pedal. A Ram's Head-era Big Muff
Pi variant with switchable clipping and the dual Tone/High tonestack.

[**Download**](https://niallgiblin.github.io/fuzzyband/) — VST3 / AU / Standalone.

I've only tested this on my own setup (one guitar, macOS, REAPER). Other DAWs,
operating systems, and gear still need checking. Let me know if you have any feedback or would like to contribute to the project in any way, development is ongoing.

Special thanks to the hard work of the researchers listed below that allowed me to skip a lot of the upfront work and frustrations.

## Signal path

```
guitar → Hi/Lo → Fuzz → Clip 1 (fixed silicon)
       → Clip 2 (silicon / germanium / bypass)
       → Tone + High → Volume → out
```

## Controls

| Control   | Range                        | Effect                    |
| --------- | ---------------------------- | ------------------------- |
| Fuzz      | 0-1                          | drive into clip stage 1   |
| Volume    | 0-1                          | output level              |
| Tone      | 0-1                          | bass ↔ treble balance    |
| High      | 0-1                          | treble restore            |
| Hi/Lo     | Lo / Hi                      | ~15 dB input-level change |
| Clip mode | Silicon / Germanium / Bypass | clip stage 2              |

## Research

The circuit numbers and pitfalls that change the sound come from published analysis
and schematic traces of the pedal.

My main source is Troels Lunde Hagensen's 2017 AAU master's thesis,
[*Analog Emulation of the Black Arts Toneworks Pharaoh Fuzz Guitar Effect
Pedal*](https://projekter.aau.dk/projekter/files/259990799/Analog_Emulation_of_the_Black_Arts_Toneworks_Pharaoh_Fuzz_Guitar_Effect_Pedal___Troels_Lunde_Hagensen.pdf). In which the pedal circuitry is rigorously analysed and an analog emulation of the pedal is created. Without this very comprehensive study I would have had to experiment a lot more.
Two findings from it are why Fairo is built the way it is:

1. **Tone and High are one network.** Measuring each knob on its own and
   then cascading two filters matches the frequency curves but the study found that it still didn't sound correct. The pots share an RC stack, so Fairo solves that stack as one
   coupled network rather than two filters in a row.
2. **A static waveshaper creates weaker sustain.** A fixed transfer function can
   copy waveform shape and still miss the attack punch and the long
   level-dependent sustain of real diode clipping (the thesis notes sustain
   of up to ~25 s with only 3 dB decay). The clippers in Fairo use a per-sample
   Shockley diode model instead of tanh / atan.

The thesis also supplied the ~15 dB Hi/Lo input-level shift, the three clip
characters (silicon harsher / brighter, germanium asymmetric ~0.3 V vs
~0.6 V, bypass hard transistor clipping), and the Tone/High topology.

Component values come from Kit Rae's **Pharaoh Big Muff Clone** schematic on
[bigmuffpage.com](http://www.bigmuffpage.com/Big_Muff_Pi_versions_schematics_part4.html). OCR-checked against the schematic image
and cross-checked against [Coda Effects](https://www.coda-effects.com/2016/04/black-arts-toneworks-pharaoh-fuzz-clone.html),
[Effects Layouts](http://effectslayouts.blogspot.com/2016/02/black-arts-toneworks-pharaoh.html),
[Tagboard Effects](https://tagboardeffects.blogspot.com/2012/02/black-arts-toneworks-pharaoh.html), and the [Rullywow
King Tut (Pharaoh-clone) BOM](https://rullywow.com/product/king-tut-fuzz-pharoh-clone-pcb/?doing_wp_cron=1788871354.4885849952697753906250). Calls that affect the sound:

- **Hi/Lo** — 39 kΩ / 390 kΩ series input resistors into a 10 µF coupling cap
- **Clip 1** — fixed antiparallel 1N914s
- **Clip 2** — 1N4001 silicon, three 1N34A germaniums wired asymmetrically,
  or diodes out
- **Coupling caps** — 47 nF, as on the trace. Some DIY layouts use 470 nF;
  that variant is not used here
- **Tonestack** — C9 10 nF, R5 470 kΩ, High 25 kΩ, C8 22 nF, Tone 250 kΩ:
  the AMZ-style dual stack that keeps mids and restores treble when Tone is
  bass-heavy

Those values can be found in `src/dsp/CircuitValues.h`.

## Building

Requires CMake ≥ 3.22, a C++20 compiler, and Xcode CLT on macOS.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release   # add -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/juce to reuse a checkout
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure   # unit tests
```

Formats: VST3 everywhere, AU on macOS, optional Standalone
(`-DFA_BUILD_STANDALONE=OFF` to skip).

## Offline sandbox

`python/` contains NumPy/SymPy prototypes of the DSP math and exports golden
data into `tests/data/`, which the C++ tests validate against:

```sh
python3 python/diode_clipper.py   # transfer curves -> tests/data/diode_curves.csv
python3 python/tonestack.py       # impulse responses -> tests/data/tonestack_impulse.csv
```

## Neural capture (not in the plugin)

`training/` and `src/inference/` are the SPICE→WaveNet→ONNX pipeline,
kept in case a real-pedal capture is worth revisiting. The plugin does not
load models or currently depend on ONNX Runtime. To compile the parked inference tests:

```sh
cmake -B build -DFA_ENABLE_ONNX=ON -DONNXRUNTIME_ROOT=/path/to/onnxruntime
```
