# Pharaoh Fuzz — Circuit Research Report (component values + topology, with sources)

**Scope.** Values below were extracted from (1) the AAU master's thesis *Analog Emulation of the Black Arts Toneworks Pharaoh Fuzz Guitar Effect Pedal* (Troels Lunde Hagensen, AAU Copenhagen, June 2 2017) — full text read; (2) the circuit schematic the thesis is built on, Kit Rae's trace on **bigmuffpage.com** ("Pharaoh Big Muff Clone Schematic", reproduced as thesis Figure 1.3/Appendix A) — read by OCR of the schematic image at 3400 px (all values double-verified with 4–8× zoom crops); (3) **coda-effects.com** Pharaoh clone build page; (4) **effectslayouts.blogspot.com** Pharaoh layout + comments; (5) **tagboardeffects.blogspot.com** Pharaoh layout + comments; (6) **Rullywow "King Tut" v1.1 build doc PDF** (the Pharaoh-clone PCB BOM).

**Refdes note (important).** The thesis, the bigmuffpage trace and the clone builds all use *different* reference designators for the same parts. The thesis's own SPICE redraw (its Fig. 3.3 / Appendix B) relabels the parts — e.g. it calls the Lo switch resistor "R2" and the Fuzz pot "R7", which collides with the bigmuffpage trace where R2 = 39 kΩ (Hi), R27 = 390 kΩ (Lo), R24 = Fuzz pot. Transistor/diode numbering is also reversed from signal flow on the bigmuffpage schematics: "Q1–Q4 and D1–D4 are labeled in reverse order from input to output" (bigmuffpage site note). **All refdes below are Kit Rae's bigmuffpage trace refdes (the thesis's source), unless stated otherwise.**

Signal flow on the bigmuffpage trace: Q4 → Q3 → Q2 → Q1 (Q4 = input, MPSA18; Q3/Q2 = the two clipping stages, 2N5089; Q1 = output, 2N5089).

---

## A. Complete component list (bigmuffpage trace; values OCR-verified from the schematic)

### Resistors (25 + 4 pots)
| Ref | Value | Where it sits in the signal path |
|---|---|---|
| R1 | 2 MΩ | Input pulldown (jack → ground). (Also added as a "2M pulldown" by tagboard/effectslayouts builders; the trace shows it) |
| R2 | 39 kΩ | **Hi** position of the input Hi/Lo switch — series input resistor, input jack → C1 |
| R27 | 390 kΩ | **Lo** position of the Hi/Lo switch — series input resistor, input jack → C1 |
| R9 | 470 kΩ | Input stage (Q4) bias network (sits between Q4 and the Fuzz pot cell; with C10) |
| R14 | 100 kΩ | Q4 collector load |
| R13 | 10 kΩ | Q4 emitter |
| R24 | 100 kΩ pot ("DIST") | **Fuzz control** — between input stage and first clipping stage; controls signal strength into the clippers |
| R17 | 470 kΩ | First clipping stage (Q3) base bias |
| R18 | 10 kΩ | Q3 emitter |
| R19 | 6.2 kΩ | Q3 stage input/bias network (exact node UNKNOWN — see §D note) |
| R20 | 100 kΩ | Q3 stage bias/collector-side network (exact node UNKNOWN — see §D note) |
| R21 | 100 kΩ | Q3 collector load |
| R22 | 1 kΩ | Input-stage/Fuzz-pot cell (exact node UNKNOWN; 47 Ram's Head has 1.2 kΩ here) |
| R23 | 1 kΩ | Input-stage/Fuzz-pot cell (exact node UNKNOWN) |
| R15 | 470 kΩ | Second clipping stage (Q2) base bias |
| R11 | 10 kΩ | Q2 emitter |
| R12 | 6.2 kΩ | Q2 stage input/bias network (exact node UNKNOWN — see §E note) |
| R16 | 100 kΩ | Q2 stage bias/collector-side network (exact node UNKNOWN) |
| R10 | 100 kΩ | Q2 collector load |
| R5 | 470 kΩ | Tonestack high-pass resistor (C9–R5 path to ground) |
| R8 | 25 kΩ pot ("HIGH") | Tonestack low-pass resistor (replaces fixed R8; with C8) |
| R25 | 250 kΩ pot ("TONE") | Tonestack blend pot |
| R3 | 100 kΩ | Output stage (Q1) collector load |
| R4 | 2.2 kΩ | Q1 emitter |
| R7 | 470 kΩ | Q1 bias network (with R6) |
| R6 | 10 kΩ | Q1 bias/emitter-side network |
| R26 | 100 kΩ pot ("VOLUME") | Output level pot |

Schematic note on the trace: *"Silicon transistors • Metal film resistors • Linear taper potentiometers"* (all pots linear per the trace; tagboard recommends 100K log for Sustain & Level, 25K lin Highs, 250K lin Tone).

### Capacitors (13)
| Ref | Value | Position |
|---|---|---|
| C1 | 10 µF tantalum (polarized) | Input coupling cap, after the Hi/Lo switch ("Tantalum capacitors (polarized) at C1, C2" — trace note) |
| C2 | 10 µF tantalum (polarized) | Output coupling cap (Q1 collector → Volume pot) |
| C3 | 0.047 µF | Tonestack output coupling → Q1 base |
| C4 | 0.047 µF | Fuzz-pot cell (exact node UNKNOWN — see §B) |
| C5 | 0.047 µF | Coupling into first clipping stage (Q3 base) |
| C6 | 0.047 µF | First clipping stage feedback cap (collector→base) |
| C7 | 0.047 µF | Second clipping stage feedback cap (collector→base) |
| C8 | 0.022 µF | Tonestack low-pass cap (with HIGH pot R8). **Trace shows 0.022 µF**; bigmuffpage page text says "enlarged to 2.2µF" — inconsistent with the trace, tagboard (22n) and King Tut BOM (22n); 22 nF used here |
| C9 | 0.01 µF | Tonestack high-pass coupling cap (from Q2 collector, with R5) |
| C10 | 470 pF | Input stage (Q4) compensation/bias cap |
| C11 | 470 pF | Second clipping stage cap (feedback/diode network) |
| C12 | 470 pF | First clipping stage cap (feedback/diode network) |
| C13 | 0.047 µF | Coupling between clipping stages (Q3 → Q2) |

Trace note: "Film caps at C1,C3,C4,C5,C6,C7,C8,C9,C13 • Ceramic caps at C10,C11,C12" (C1/C2 tantalum as above).

### Diodes
| Ref | Part | Position |
|---|---|---|
| D3, D4 | **1N914** ×2 | **First clipping stage** antiparallel silicon clipper (thesis §3.3; trace note "Silicon diodes at D1/D2 – 1N4001", with D3/D4 being the 1N914 pair by trace numbering) |
| D1, D2 | **1N4001** ×2 | **Second clipping stage** silicon branch of the 3-way switch (trace note: "Silicon diodes at D1/D2 – 1N4001"; thesis §3.4.2) |
| D5, D6, D7 | **1N34A** ×3 | **Second clipping stage** germanium branch (trace note: "Germanium diodes at D5/6/7 – 1N34A") |

(Polarity-protection diode — 1N5817 on effectslayouts, D8 1N4001 on King Tut — is outside the audio path; the bigmuffpage trace omits power filtering.)

### Transistors
| Ref | Part | Stage |
|---|---|---|
| Q4 | **MPSA18** (NPN, high-hFE) | Input stage ("Both use MPSA18 transistors in Q4 and 2N5089 in Q1-3" — bigmuffpage; "Q1 is MPSA18 on the original" — effectslayouts, input-first numbering) |
| Q3 | **2N5089** (NPN) | First clipping stage |
| Q2 | **2N5089** (NPN) | Second clipping stage |
| Q1 | **2N5089** (NPN) | Output (make-up gain) stage |

Thesis on the first stage: "The first clipping segment comprises a bipolar transistors (2N5089 NPN) in common emitter configuration and a diode clipper configuration based on two 1N914 silicon diodes" (§3.3).

### Cross-check — King Tut v1.1 BOM (Rullywow Pharaoh-clone PCB)
Resistors: R1 2M, R2 39k, R3 390k, R4/R11/R17/R19/R23 470k (×5), R5/R10/R15/R20 100k, R6/R8 1k, R7/R12/R18/R22 10k, R9/R14 6k2, R13/R16 100R, R21 2k2. Caps: C1/C13 10uF tant, C2/C5/C8 470pF, C3 470n, C4/C6/C7/C9/C12 47n, C10 10n, C11 22n. Diodes: D1/D2 1N914, D3/D4 1N4001, D5/D6/D7 1N34A, D8 1N4001. Pots: DIST 100kB, HIGH 25kB, TONE 250kB, VOL 100kB. Q1 MPSA18, Q2–Q4 2n5089. (KT numbering differs from the trace; values agree with the trace except KT's 100R pair and one fewer 100 kΩ.)

---

## B. Input stage — exact topology & values

- **Hi/Lo switch (SPDT on-on):** Input jack → switch center → **R2 = 39 kΩ (Hi)** or **R27 = 390 kΩ (Lo)** → **C1 = 10 µF tantalum** → Q4 base. Coda: "Signal arrives to the center of the switch, and goes through a resistor, 39k ('high' setting), or 390k ('low' setting)". Thesis §1.1.1: "In the input segment a switch between two resistors allow for a selection between to gain level - high or low. The switch however, also affects the low cut filtering." and "A coupling capacitor is placed after the switch to filter away DC current." bigmuffpage: "The input resistor in the first stage has been converted to a hi (39k) and low (390k) gain switch, affecting the low cut filtering in the input stage, but unfortunately also affecting the input signal level, causing a large volume difference between the two sides."
- **Q4 = MPSA18** common-emitter: collector load **R14 = 100 kΩ**, emitter **R13 = 10 kΩ**, bias network **R9 = 470 kΩ + C10 = 470 pF**. (Per the trace layout; the exact node of R9/C10 — +9 V→base vs collector→base feedback — could not be read from the schematic image; the standard BMP input stage uses a 470 kΩ base-bias resistor and 470 pF HF rolloff cap.)
- **Fuzz pot R24 = 100 kΩ ("DIST")**, between the input stage and the first clipping stage. Thesis §3.2.2: "The 'Fuzz' knob is also located in the input segment. Its functionality is very similar to that of the 'Hi-Lo' switch, but with added fine tuning of a variable resistor. The potentiometer can be used to in- or decrease the signal strength within an interval. This will affect the degree of clipping in the following clipping segments." Thesis Fig. 3.3 caption (their SPICE redraw): "R7 is the potentiometer connected to the 'Fuzz' knob, here set at minimum resistance"; "Note that R7 was set to 1 Ohm during the simulation." So the Fuzz pot is a variable signal-strength control between Q4 and the first clipper (the BMP "sustain" function) — it is **not** an emitter/feedback resistor of any transistor. The 0.047 µF cap C4 sits in the Fuzz-pot cell on the trace; its exact node is UNKNOWN (either the pot-output coupling or an HF cap in the pot cell; the dedicated Q3 input coupling is C5).

---

## C. Hi/Lo level difference

Thesis §3.2.1 (SPICE AC sweep of the input segment, 20 Hz–22 kHz, fuzz pot at 1 Ω): "Noting the different intervals on the y-axis makes it clear that a large drop in signal strength is happening when switching from 'Hi' to 'Lo'. Using the SPICE analysis tools the difference of the two graphs were found to be **approximately 15 dB**." The thesis also notes the real-pedal recording shows a smaller drop (transistor gain follows): "Taking a look at the dB scales reveals a signal strength drop but nothing like the simulated results, which makes sense as no transistor gain was included in the simulated results." Frequency response figures: thesis Fig. 3.4 (SPICE: Hi top, Lo bottom).

---

## D. Clip stage 1 (Q3, 2N5089) — topology

- Common-emitter 2N5089; input coupling **C5 = 0.047 µF** → base; collector load **R21 = 100 kΩ**; emitter **R18 = 10 kΩ**; base bias **R17 = 470 kΩ**; feedback cap **C6 = 0.047 µF** (collector→base); **C12 = 470 pF**; stage network also shows **R19 = 6.2 kΩ** and **R20 = 100 kΩ** (exact nodes UNKNOWN from the schematic image — candidates: input/base series resistor and second bias resistor; the 47 Ram's Head parent stage has the same two extra parts, 7.5 kΩ + 100 kΩ).
- **Clipper: D3/D4 = 1N914 antiparallel pair in the collector circuit** (to ground), giving symmetric clipping. Thesis §3.3: "the diodes receive the same bias from the 9 volt DC source. The result is symmetric clipping of the positive and negative wave cycle." Thesis §1.1.3 (on both clip stages): "The components are connected in feedback loops, which saturate the sound." Thesis §3.3.1: "The first clipping segment comprises a bipolar transistors (2N5089 NPN) in common emitter configuration and a diode clipper configuration based on two 1N914 silicon diodes."
- No knobs/switches affect this stage (thesis §3.3: "The first clipping segment can not be manipulated directly with any of the external control knobs or switches.").
- Bias: transistor stages are 9 V powered; "the diodes receive the same bias from the 9 volt DC source" (thesis §3.3). Note the trace's Fig. 3.6 caption: "Section of the SPICE modeled first clipping segment. The top input is connected to a 9V DC source."

---

## E. Clip stage 2 (Q2, 2N5089) — 3-way switch topology

- Same CE architecture as stage 1: coupling in **C13 = 0.047 µF**, collector load **R10 = 100 kΩ**, emitter **R11 = 10 kΩ**, base bias **R15 = 470 kΩ**, feedback **C7 = 0.047 µF**, **C11 = 470 pF**, plus **R12 = 6.2 kΩ** and **R16 = 100 kΩ** (nodes UNKNOWN, same caveat as §D). Thesis §3.4: "The second clipping stage is build around the same diode clipper feedback loop as the first clipping segment, but with the added option for the user to switch between germanium or silicon diodes or bypass these."
- **Switch: SPDT On/Off/On** (bigmuffpage; effectslayouts "3 position DPDT"; King Tut "DPDT (center off)"), wired into the collector diode circuit.
- **Silicon branch: D1/D2 = 1N4001 antiparallel pair** (symmetric). Thesis §3.4.2: "The last option is a silicon diode clipper much like the one found in the first clipping segment, but with 1N4001 diodes instead of 1N914." SPICE sim with 10 kΩ load: "The 1N4001 starts to clip a little earlier than the 1N914, but the knees are very alike" (thesis §3.4.2).
- **Germanium branch: D5/D6/D7 = 1N34A ×3, asymmetric.** Thesis §3.4.1: "three 1N34A germanium diodes are arranged in parallel, two of which are in series, oriented in the opposite direction of the last diode... The forward voltage of the series arrangement is therefore 2 times 0.3 volts = 0.6 forward voltage (approximately). This means that the positive half cycle of the waveform will be clipped at approximately half the forward voltage (0.3 V) of the negative wave cycle (0.6 V)." i.e. one branch = single 1N34A (clips at ~0.3 V), other branch = two 1N34A in series, reversed (clips at ~0.6 V). bigmuffpage: "A third Ge diode was added to the Ge clipping loop to help bring the output level back up, but there is still a volume drop when engaged."
- **Bypass (center):** no diodes in circuit; clipping done by the transistors only. Thesis §4.2.2: "From the recordings of the different 'Fuzz' settings played in the bypass setting it was found that the clipping was very hard and sudden." (The bypass state also lets you isolate stage 1 — thesis §3.3.1.)

---

## F. Tonestack — exact network, values, and the thesis's analysis

**Node-by-node (per the trace + thesis §6.2):**
```
Q2 collector ── C9 (0.01 µF) ──┬── node A ── R5 (470 kΩ) ── GND      (fixed high-pass path)
                               │
                               └── R8 (HIGH pot, 25 kΩ, rheostat, lugs 1-2) ── node B ── C8 (0.022 µF) ── GND   (variable low-pass path)
R25 (TONE pot, 250 kΩ):  lug1 ── node A ; lug3 ── node B ; wiper ── C3 (0.047 µF) ── Q1 base
```
- Thesis §3.5.3: "the 'High' filter is a RC low-pass filter e.g. (component R8 and C8 in the tonestack, see Appendix A)."
- Thesis §6.2: "A RC filter acts as a fixed high-pass filter, which bypasses the 'High' filter. … the C9-R5 and R8-C8 configurations respectively … implemented and connected in parallel. Both filters are connected to the 'Tone' potentiometer (R25), which weighs the balance between the two RC filters."
- bigmuffpage: "A 25k pot was added to the tone stage in place of the R8 low pass resistor, yet another variant on the AMZ tone/presence control, allowing a boost to the highs. The low pass cap at C8 was enlarged to 2.2µF [trace shows 0.022 µF] to balance the low pass filtering. The large cap and resistor at C9 and R5 on the high pass sides allow for less mid range scoop than the '47' Ram's Head." (Stock 47 Ram's Head: C9 = 0.004 µF, R5 = 22–39 kΩ, R8 = 22–33 kΩ fixed, C8 ≈ 0.004–0.012 µF.)
- **Coda build instruction for the High pot:** "you just have to replace R8 (tonestack resistor) by a 25k potentiometer. Just connect the lug 1 and 2 to each pad of the R8 resistor" (rheostat).
- **Thesis's filter analysis (what it actually did):** it did **not** publish SPICE poles/zeros of the tonestack. It measured the pedal's response by exciting with white noise (Bela/Pure Data) and designed digital filters from the recordings:
  - Tone knob (Fig. 3.12): "The 8 o'clock bass-heavy setting lets through 9dB more of the lowest frequency content compared to the treble-heavy 4 o'clock setting… The frequencies around 10kHz are therefore 24dB lower in the 8 o'clock setting."
  - High knob (Fig. 3.13): "the filter actually boost the entire frequency range, but with much more effect around 1kHz and above"; also removes low frequencies at high settings (Fig. 4.8) — behaviour they chose not to model (§4.3.2).
  - PD implementation: Tone = two parallel 1st-order high-passes whose cutoffs track the pot exponentially — germanium 0→~110 Hz and 0→~360 Hz; silicon 0→~70 Hz and 0→~240 Hz; bypass: single HP 0→35 Hz. High = simple 1st-order low-pass 500 Hz→10 kHz (§4.3).
  - RC formula used (Eq. 3.1): fc = 1/(2πRC).
- **Computed cutoffs (from the component values; my calculation, not the thesis's):** C9–R5 high-pass fc = 1/(2π·470 kΩ·10 nF) ≈ **34 Hz**; R8–C8 low-pass fc = 1/(2π·R8·22 nF) = **289 Hz at R8 = 25 kΩ** (pot maximum), rising with lower resistance — i.e. the High control sweeps the low-pass corner upward as you turn it down.
- Thesis §6.2 verdict on the real tonestack: cascading simple filters sounded wrong; the real stack "is a bit more complex than cascading the 'Tone' and 'High' filter" because of the parallel RC structure.

---

## G. Coupling caps between stages — the 47 nF vs 470 nF question

- **The thesis** does not discuss this numerically; its source schematic (bigmuffpage trace, Appendix A) shows **0.047 µF (47 nF)** for every interstage coupling and feedback cap (C3, C4, C5, C6, C7, C13), with **10 µF** tantalums at input (C1) and output (C2).
- **bigmuffpage** page text: "The low pass cap at C8 was enlarged to 2.2µF" (trace actually shows 0.022 µF); coupling caps in the trace are all .047 µF.
- **effectslayouts** layout uses 470 nF for all coupling + feedback caps; comment (BC, Oct 2017): *"I wonder about the clipping caps values. The schematic from bug muff page have 47nF instead of 470nF here. Have you noticed that?"*; comment (V, Apr 2019): *"There is a big difference if you use 470nf or 47nf. Original Rams head had 47nf's. There are conflicting schems of Pharoah circulating with both 470 or 47nf."*
- **coda-effects** comment (V, Feb 2019): *"I am finding that there are two versions of this pedal floating around on the net. One with 47nf coupling and feedback caps (as in yours and Rullywows). And the other version has all caps changed to 470nf. This will make a HUGE difference in the tone of the entire pedal."*
- **tagboardeffects** comment: *"all coupling caps incl feedback caps are shown as 470nf here. Considering Pharoah was a modded clone of Ram's head, which had one 470nf right after Q1 and all the rest were 47nf's. King Tut from Rullywow shows the same."*
- **King Tut v1.1 BOM:** one 470 nF (C3) + 47 nF elsewhere (C4, C6, C7, C9, C12), 470 pF (C2/C5/C8), 10 nF (C10), 22 nF (C11).
- **Verdict:** the trace the thesis used says 47 nF everywhere; the 470 nF variant circulates in build layouts (effectslayouts, tagboard) and some King Tut versions. No gutshot evidence resolves it publicly; the thesis does not state a preference.

---

## H. SPICE diode models / netlists

**None published in the thesis text.** No Is/N/Rs parameters, no netlist appears in the text; Appendix B ("SPICE schematic") is a schematic image, and the thesis says only: "running a very simple SPICE simulation of the diode clippers with a 10k resistor load and a AC voltage source" (§3.4.2, Fig. 3.11); "The circuit design was structured according to the bigmuffpage circuit" (§3.2.1) using "OrCAD Capture". Diode behaviour is described physically: silicon forward drop 0.6–0.7 V vs germanium 0.2–0.4 V (thesis §3.3.1, citing [3]), Ge clips at ~0.3 V and Si at ~0.6–0.7 V. For SPICE modelling you must supply your own diode parameters (e.g. stock 1N914/1N4001/1N34A models); the thesis's digital emulation used a waveshaping transfer function V1/(V2+|V1|) instead of SPICE-derived models (§4.1, Eq. 4.1), with static distortion factors 0.905 (1N914), 0.75 (1N4001), 0.3 (1N34A) — these are *emulation* parameters, not SPICE diode model parameters.

---

## I. Output stage — make-up gain transistor + Volume pot

- **Q1 = 2N5089** common emitter; collector **R3 = 100 kΩ**, emitter **R4 = 2.2 kΩ**, bias **R7 = 470 kΩ + R6 = 10 kΩ** (per trace; nodes per the standard BMP output stage). Thesis §1.1.5: "A final transistor is used to amplify the sound to make up for the lost volume due to the clipping diodes. A potentiometer is used to control the volume of the output signal."
- Input to Q1 via **C3 = 0.047 µF** from the tone pot wiper; output via **C2 = 10 µF tantalum** → **R26 = 100 kΩ Volume pot** → output jack. Thesis §3.6: "The output segment is the simplest of the five segments… only the volume knob is connected to this segment. The volume control is located at the output of the pedal. The signal is not manipulated further after this point." (Polarity of C2 per effectslayouts: negative leg toward the volume-pot side.)

---

## J. Supply voltage & audio measurements

- **Supply: 9 V DC** (standard pedal supply; the bigmuffpage trace omits power filtering, but builds add 220 µF electrolytic + 1N5817 polarity protection — effectslayouts — or D8 1N4001 — King Tut).
- Measured/stated in the thesis:
  - **Hi/Lo level difference ≈ 15 dB** (SPICE, §3.2.1).
  - **Tone knob:** +9 dB more low-frequency content at 8 o'clock vs 4 o'clock; ~24 dB lower at 10 kHz at 8 o'clock (§3.5.1).
  - **Sustain:** "the sustain phase can be upwards of 25 seconds with only 3 dB signal drop, followed by a release over 5 seconds with 12 dB signal drop" (§6.1).
  - Guitar pickup signal measured: open D2 with both humbuckers, "just below 130mV" peak at the cable plug (§3.3.1).
  - **No THD numbers** and no formal Bode plots of the pedal are given; frequency-response data is qualitative (white-noise recordings, Figs. 3.12/3.13, 4.8). Germanium diodes: "An unbiased germanium diode will start clipping at around 0.3 volts" (§3.4.1); recordings at 48 kHz via Behringer U-Phoria UM2 (§3.1).

---

## Limitations / what could not be obtained

1. **Thesis figures are not machine-readable for wiring**: the thesis's Appendix A (the schematic) and Appendix B (their SPICE redraw) are images; the thesis text gives no complete netlist or SPICE diode parameters. The values above come from OCR of the bigmuffpage schematic (the thesis's cited source) — every value was verified at ≥4× zoom, but **wire/node connections inside the bias networks** (R9/C10, R19/R20, R12/R16, R22/R23, C4, R6/R7, and exact pot lug wiring) could not be read from a raster schematic and are marked UNKNOWN above.
2. **freestompboxes.org is Cloudflare-blocked** (print and mobile views); the Wayback Machine has no snapshot of the Pharaoh thread (only an unrelated 2014 thread). The community thread data above comes from tagboardeffects, effectslayouts, coda-effects and diystompboxes-linked King Tut threads instead.
3. **Pot tapers** conflict between sources: trace note says all linear; tagboard recommends log for Sustain/Volume. Both are stated above.
4. The bigmuffpage page text's "2.2µF" for C8 contradicts the trace (0.022 µF), tagboard (22n) and King Tut (22n) — treated as a typo; 22 nF used.

## Files saved in /Users/ng/Desktop/fairo/research/
- `pharaoh_thesis.pdf` / `pharaoh_thesis.txt` (full text extracted)
- `bigmuffpage_part4.html` (+`.txt`), `coda_pharaoh.html` (+`.txt`), `effectslayouts_pharaoh.html` (+`.txt`), `tagboard_pharaoh.html` (+`.txt`)
- `pharaoh_schematic_bmp.jpg` (the trace; 3× upscale `pharaoh_schematic_3x.png`), `ramhead47_schematic.jpg` (47 Ram's Head reference), `coda_schematic.jpg`, `effectslayouts_pharaoh.png`, `tagboard_layout.png`
- `kingtut.pdf` / `kingtut.txt` (Rullywow BOM), `ocr.swift`, `tile_ocr.py`, `tile_map*.txt` (OCR dumps), `crops/`, `tiles/`, `kt_images/`
