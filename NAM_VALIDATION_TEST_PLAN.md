# Fairo — Pharaoh NAM Validation & Stress-Test Plan

**Purpose.** Confirm that Fairo's DSP matches the reference **NAM captures** of the
real Black Arts Toneworks Pharaoh, knobs-for-knobs, across every capture the
author published — and that the plugin's *level structure* and *clip/tonestack*
behavior also line up with the AAU thesis analysis on the pedal.

**What this doc is.** A precise capture→setting map plus a step-by-step
recording protocol. You record one render per setting through Fairo **and** one
through the NAM reference, then hand me the batch. I'll do the numeric/spec
pass (level, spectrum, harmonic/sustain, dropouts) and report a per-setting
pass/fail & gap list. This is a *validation/measurement* run, not a blind-listen
run — we both get numbers and I flag where Fairo is off.

> Source of truth for every knob setting below: the TONE3000 capture page
> (https://www.tone3000.com/tones/black-arts-toneworks-pharaoh-fuzz-77705),
> captures by **@baab**, sweep method, Palmer TRave reamp → Millenium DI-E →
> Behringer UMC404, 200 epochs, A2-Full format, 48 kHz.

---

## 1. The capture → Fairo setting map

The 21 `.nam` files in `Desktop/Black Arts Toneworks Pharaoh Fuzz/` encode three
independent controls in the filename plus two "hard-set" knobs. Mapping (from
the capture page description):

| Filename token | Meaning | Fairo parameter |
|---|---|---|
| `GER` / `SIL` / `NO` | Diode mode: **GER**=asymmetric Germanium, **SIL**=Silicon ("Silicone" in caption), **NO**=no diodes (bypass) | Clip mode → Germanium / Silicon / Bypass |
| `HI` / `LO` | Input stage Hi/Lo switch | Hi/Lo → Hi / Lo |
| `FUZZ_1` / `_2` / `_3` | Fuzz knob: **1 = Noon**, **2 = 2 o'clock**, **3 = Max** | Fuzz → 0.50 / 0.70 / 1.00 |
| `MAXED_*` | "Maxxed out" captures | Fuzz=1.00, Tone=1.00, High=1.00, Volume=1.00 |
| *(not in name)* | Tone & High are **hard set to 2 o'clock** for all non-MAXED captures | Tone=0.70, High=0.70 |
| *(not in name)* | Volume at noon (grid) / max (MAXED) | see §4 |

**Assumptions to confirm before you record** (they only shift Tone/High/Fuzz
by a few percent, but confirm so the numbers line up):

- **"2 o'clock" ≈ 0.70** on the plugin's 0–1 linear pots (pot travel 7→5
  o'clock, noon = 0.5, +2h = 0.7). If your pot convention differs, tell me and
  adjust Fuzz/Tone/High in §4 accordingly (0.65–0.70 is the window).
- **Volume for the FUZZ grid** — the capture page doesn't specify it. I assume
  the captures were taken at **Volume ≈ noon (0.5 = unity)**. If you know
  otherwise, say so.
- **MAXED captures** — "maxxed out" = Fuzz/Tone/High/Volume all max. Input
  switch isn't named; I assume **Hi** (the natural "maxed" setting). Confirm.

### Full grid (21 captures → one Fairo preset each)

| # | Clip | In | Fuzz | Tone | High | Vol | Fairo | NAM ref loudness (dB) |
|---|---|---|---|---|---|---|---|---|
| 1 | GER | HI | 0.50 | 0.70 | 0.70 | 0.50 | `PHARAOH_GER_HI_FUZZ_1` | −17.99 |
| 2 | GER | HI | 0.70 | 0.70 | 0.70 | 0.50 | `PHARAOH_GER_HI_FUZZ_2` | −16.75 |
| 3 | GER | HI | 1.00 | 0.70 | 0.70 | 0.50 | `PHARAOH_GER_HI_FUZZ_3` | −14.70 |
| 4 | GER | LO | 0.50 | 0.70 | 0.70 | 0.50 | `PHARAOH_GER_LO_FUZZ_1` | −27.11 |
| 5 | GER | LO | 0.70 | 0.70 | 0.70 | 0.50 | `PHARAOH_GER_LO_FUZZ_2` | −22.34 |
| 6 | GER | LO | 1.00 | 0.70 | 0.70 | 0.50 | `PHARAOH_GER_LO_FUZZ_3` | −17.38 |
| 7 | SIL | HI | 0.50 | 0.70 | 0.70 | 0.50 | `PHARAOH_SIL_HI_FUZZ_1` | −15.65 |
| 8 | SIL | HI | 0.70 | 0.70 | 0.70 | 0.50 | `PHARAOH_SIL_HI_FUZZ_2` | −15.28 |
| 9 | SIL | HI | 1.00 | 0.70 | 0.70 | 0.50 | `PHARAOH_SIL_HI_FUZZ_3` | −14.49 |
| 10 | SIL | LO | 0.50 | 0.70 | 0.70 | 0.50 | `PHARAOH_SIL_LO_FUZZ_1` | −17.62 |
| 11 | SIL | LO | 0.70 | 0.70 | 0.70 | 0.50 | `PHARAOH_SIL_LO_FUZZ_2` | −16.79 |
| 12 | SIL | LO | 1.00 | 0.70 | 0.70 | 0.50 | `PHARAOH_SIL_LO_FUZZ_3` | −14.70 |
| 13 | NO | HI | 0.50 | 0.70 | 0.70 | 0.50 | `PHARAOH_NO_HI_FUZZ_1` | −13.65 |
| 14 | NO | HI | 0.70 | 0.70 | 0.70 | 0.50 | `PHARAOH_NO_HI_FUZZ_2` | −13.48 |
| 15 | NO | HI | 1.00 | 0.70 | 0.70 | 0.50 | `PHARAOH_NO_HI_FUZZ_3` | −13.10 |
| 16 | NO | LO | 0.50 | 0.70 | 0.70 | 0.50 | `PHARAOH_NO_LO_FUZZ_1` | −15.73 |
| 17 | NO | LO | 0.70 | 0.70 | 0.70 | 0.50 | `PHARAOH_NO_LO_FUZZ_2` | −14.60 |
| 18 | NO | LO | 1.00 | 0.70 | 0.70 | 0.50 | `PHARAOH_NO_LO_FUZZ_3` | −13.19 |
| 19 | GER | HI | 1.00 | 1.00 | 1.00 | 1.00 | `PHARAOH_MAXED_GER` | −13.01 |
| 20 | SIL | HI | 1.00 | 1.00 | 1.00 | 1.00 | `PHARAOH_MAXED_SIL` | −16.02 |
| 21 | NO | HI | 1.00 | 1.00 | 1.00 | 1.00 | `PHARAOH_MAXED_NO` | −12.51 |

> The "NAM ref loudness" column is the integrated loudness stored in each
> `.nam`'s metadata. It's a *relative* target, not an absolute one — the captures
> were made from a sweep signal, not `fairo_di.wav`. Use it for the **level
> ordering** sanity check (§5), not as a hard dB target.

---

## 2. Thesis behaviors these settings should reproduce

From the AAU thesis (`research/pharaoh_thesis.txt`), these are the specifics
your recording needs to be able to confirm or reject:

1. **Hi→Lo ≈ 15 dB level shift** before clipping (thesis §3.2.1). It shows up in
   the *input drive*, not proportionally in output loudness — because once the
   stages clip, output loudness **converges**. So the **output-loudness gap**
   between HI and LO should **shrink as Fuzz increases** (drive → saturation).
   Verify against the table in §1: GER HI−LO = +9.1 dB @fuzz1 → +2.7 dB @fuzz3;
   SIL ≈ +2.0 → +0.2 dB; NO ≈ +2.1 → +0.1 dB.
2. **The Hi signal is more square-wave-like, richer in overtones; Lo is more
   sine-like** (thesis §3.2.1). The Hi render should show stronger upper
   harmonics and more compression; Lo should be rounder and less saturated.
3. **Fuzz = drive /**-increase signal strength (thesis §3.2.2), same family as
   Hi/Lo but fine-grained. Fuzz 1→3 should add harmonics/sustain, not just level.
4. **Large volume drop in Germanium** (thesis §1.1.3). This is the single most
   diagnostic setting: **GER + LO + Fuzz 1** is nearly dead in the capture
   (gain≈0, loudness −27.1 dB). Fairo must reproduce a similarly large, dramatic
   drop there, or it's wrong.
5. **Tonestack is one coupled network** (thesis §6.2; PLAN pitfall #1). Tone
   changes brightness across a wide band without the classic Muff mid-scoop;
   High restores treble. At Tone=High=0.7 (grid) both are active together.

---

## 3. Test rig (use the setup already in Reaper)

- **DAW:** REAPER. Open `~/Desktop/plugin.RPP` — it already has Fairo in the
  chain and `RENDER_PATTERN fairo_di.wav` set.
- **Input DI:** `~/Desktop/fairo/fairo_di.wav` (or `man_di.wav`) is the DI you
  render through BOTH Fairo and the NAM model. Use the **same** file for both
  legs of every A/B pair.
- **Project sample rate: 48 kHz** (the `.nam` files are 48 kHz; don't resample
  one leg but not the other). Mono, and **render/export at 24-bit float or
  24-bit PCM** (avoid 16-bit truncation of the quiet GE/LO setting).
- **NAM reference player:** any NAM host (NAM standalone, the TONE3000 plugin,
  a DAW NAM loader, or `nam` CLI) set to load the matching `.nam`. Make sure it's
  set to **48 kHz** and **not** auto-normalizing (match the file's stored gain).

---

## 4. Recording protocol — one A/B pair per setting

For **each of the 21 settings** (rows in §1) produce **two** renders:

| Leg | Signal path | Output file |
|---|---|---|
| **A — Fairo** | `fairo_di.wav` → **Fairo** (setting per §1) → record | `PLG_<row#>.wav` |
| **B — NAM** | `fairo_di.wav` → **NAM model** (`PHARAOH_*.nam`) → record | `REF_<row#>.wav` |

`<row#>` is the row number in §1 (1–21). The **NAM model filename** is the exact
`.nam` in the same row (e.g. row 12 → `PHARAOH_SIL_LO_FUZZ_3.nam`).

### Steps (repeat per row; make it a reusable template if you like)

1. Set the project to 48 kHz. Insert **Fairo**. Set knobs to the row's values:
   - **Fuzz** = 0.50 / 0.70 / 1.00 (Fuzz 1/2/3); **Tone** = 0.70, **High** = 0.70
     for rows 1–18; **Tone = High = 1.00** for rows 19–21.
   - **Clip mode** = Germanium / Silicon / Bypass for GER / SIL / NO.
   - **Hi/Lo** = Hi / Lo.
   - **Volume** = 0.50 for rows 1–18; 1.00 for rows 19–21.
2. **Render/record** `fairo_di.wav` through Fairo from a fixed start point.
   Save as `PLG_<row#>.wav`.
3. **Load the NAM model** for that row (leave everything else — input file,
   sample rate, start point — identical). Record the same range. Save as
   `REF_<row#>.wav`.
4. Leave ~0.3 s of silence at the head and tail of each render (lets me measure
   noise floor / DC decay, and pads the reverb tail for GE/LO).
5. Keep Fairo **bypassable but OFF the chain** while recording the REF leg, and
   vice-versa — you don't want the pedal and the capture both in the signal path.

### Naming / where to drop them

Put all 42 files in one folder, e.g. `~/Desktop/Black Arts Toneworks Pharaoh Fuzz/NAM_validation/`:

```
PLG_01.wav … PLG_21.wav
REF_01.wav … REF_21.wav
```

Include row→file as a header comment in a small `MANIFEST.txt` (or just keep the
§1 table as the index). I'll map from the row numbers.

Drop the folder anywhere and tell me the path. Do **not** rename the `.nam`
files.

---

## 5. What I'll compute & the pass/fail criteria

For each of the 21 pairs I'll report:

1. **Level match** — RMS/peak/LUFS of `PLG_*` vs `REF_*`, plus the loudness
   **ordering** across all 21 (does the Fairo set rise and fall the same way).
   Target: within ±2 dB RMS per setting; the **relative** curve should be within
   ~±1.5 dB. I'll flag any setting where Fairo is >3 dB off the reference.
   **Watch: GER+LO+Fuzz1** — Fairo must be dramatically quieter, not near-unity.
2. **Spectral match** — narrowband FFT difference (dB vs freq). Target: mean
   |Δ| < ~3 dB below 5 kHz and no +/− > 6 dB band in the fundamentals/harmonics
   region. I'll report where Fairo is consistently brighter or darker.
3. **Top-end / high-harmonic balance** — *this is the flagged weak area*. See §6.
   I'll measure the harmonic tilt above ~2–4 kHz and compare against both the
   reference **and** the real-pedal expectation from the creation notes.
4. **Sustain & compression** — note RMS envelope decay from the attack through
   the tail (thesis's "punch & sustain" gap). I'll measure the decay constant
   and the peak-to-sustain RMS ratio so the "level-dependent clipping" behavior
   is quantified, not just eyeballed.
5. **Aliasing/noise** — high-frequency content above ~16 kHz (the 2× oversampling
   should keep this low) and the noise floor (for the quiet GE/LO setting).
6. **Continuity/stability** — no NaN, clicks, or zipper noise at any setting.

**Pass =** level within ±2 dB, spectral mean |Δ| < 3 dB below 5 kHz, and no
single setting in the 6 dB+ structural-gap list below.

---

## 6. Known gap hypotheses to focus on (from code review — not yet verified)

These are the places I expect trouble, based on `src/dsp/` and the capture
metadata. Test these deliberately:

- **High-frequency presence.** Fairo has an explicit post-clip high-shelf:
  `kPresenceShelfFc = 3000 Hz`, `kPresenceShelfGainDb = +6 dB`
  (`CircuitValues.h`). It was added because Fairo measured **−7 to −14 dB dark
  vs the MAXED_SIL capture**. So the reference is bright relative to baseline
  Fairo, and the shelf is a partial fix.
  **But the capture creator says the cap the wrong way:** the captures are
  *themselves* dark ("passive DI took some high frequencies out... not close to
  the original, especially in the higher frequencies"). So matching the capture
  and matching the real pedal pull in *opposite* directions. **Decision point for
  you:** target the NAM capture (reference), or target the real pedal (brighter)?
  I'll compute the Fairo-vs-NAM delta either way; you choose the target. If it's
  the real pedal, we may *reduce* or re-tune the presence shelf.
- **Germanium volume drop.** Verify rows 4–6: Fairo's GE drop must be as large
  and as *monotonic* as the reference (thesis §1.1.3; capture GER_LO_Fuzz1 is
  near-silent).
- **Hi/Lo gap that shrinks with Fuzz.** The capture shows the HI−LO output-
  loudness gap collapsing as Fuzz rises (see §2). Fairo's input divider gives a
  fixed ~15 dB, but the clippers should make the *output* gap converge with more
  drive. Confirm Fairo does this (if it doesn't, the clipping isn't saturating
  enough — the thesis's real "punch/sustain" gap).
- **MAXED_SIL being quieter than MAXED_NO/GER.** Capture loudness: MAXED_SIL
  −16.02, MAXED_NO −12.51, MAXED_GER −13.01. If Fairo's MAXED_SIL is *louder*
  than MAXED_NO, that's a flag — the Silicon square/limit should be the one that
  flattens hardest.
- **Fuzz-pot taper.** Fairo maps Fuzz 0→1 to a `1.0→12.0×` gain
  (`kGainStage1Min/Max`). Is 12× the right ceiling, and does 0.5/0.7 intermediate
  land where the real knob does? The rows will tell.

---

## 7. Reference level ordering (quick visual check before you export)

Reading the stored NAM loudness (relative, same input assumed):

```
LOUDEST ▲ MAXED_NO  −12.51
         MAXED_GER  −13.01
         NO_HI_1..3 −13.65 → −13.10      (bypass is the loudest family)
         MAXED_SIL  −16.02   ← *quieter* than NO/GER MAXED — expected?
         GER_HI_1..3 −17.99 → −14.70
         SIL_*      −14.49 … −17.62
         NO_LO_1..3 −15.73 → −13.19
         GER_LO_3   −17.38
         GER_LO_2   −22.34
         GER_LO_1   −27.11   (near-silent) ▼
```

Two things stand out: **(a)** MAXED_SIL is quieter than MAXED_NO/GER, and
**(b)** GER+LO collapses to near-nothing. If Fairo doesn't show the same story,
that's the signal.

---

## 8. Recording log (fill each row)

| Row | Clip | In | Fuzz | Tone | High | Vol | `.nam` used | Input file | Plugin knobs confirmed? | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | GER | HI | .50 | .70 | .70 | .50 | `PHARAOH_GER_HI_FUZZ_1` | fairo_di.wav | | |
| … | | | | | | | | | | |
| 21 | NO | HI | 1.0 | 1.0 | 1.0 | 1.0 | `PHARAOH_MAXED_NO` | fairo_di.wav | | |

If a row's knobs didn't land exactly (e.g. your pot isn't where the % says),
note it in "Notes" — I'll fold it in rather than discard the row.

---

## 9. What I need back from you

1. The `NAM_validation/` folder with `PLG_*.wav` + `REF_*.wav` (42 files, 48 kHz).
2. Confirmation or correction of the three assumptions in §1 (2 o'clock ≈ 0.70,
   grid Volume ≈ 0.50, MAXED input = Hi).
3. For the presence-shelf question in §6: **match the capture, or match the real
   pedal?** (This is the one decision I can't make for you.)

Then I'll do the numeric pass per §5 and return a per-row verdict, a ranked gap
list, and (if you want) specific `CircuitValues.h` / `FuzzEngine.cpp` changes to
make.
