#!/usr/bin/env python3
"""
Generates the ngspice netlist of the Pharaoh fuzz, from the schematic-trace
values and the node-by-node wiring read from the bigmuffpage schematic crops
(research/crops/A-E, research/report.md).

The real topology (verified from the schematic images — the earlier
transcribed "collector/emitter" assignments were flipped):

  Input  jack -> R2 39k (Hi) | R27 390k (Lo) -> C1 10u -> Q4 (MPSA18) base
         Q4: R13 10k coll, R22 1k em, R14 100k base->gnd, R9 470k + C10 470pF
         coll<->base; Q4 coll -> C4 0.47u -> DIST pot R24 100k (lug3 in,
         lug1 -> R23 1k -> gnd, wiper -> C5 47n -> R19 6.2k -> Q3 base)
  Clip 1 Q3 (2N5089): R18 10k coll -> +9V, R21 100R em -> gnd, R20 100k base->gnd
         coll<->base: R17 470k || C12 470pF || C6 47n || D3/D4 1N914 antiparallel
         Q3 coll -> C13 47n -> R12 6.2k -> Q2 base
  Clip 2 Q2 (2N5089): R11 10k coll, R10 100R em, R16 100k base->gnd
         coll<->base: R15 470k || C11 470pF; C7 47n -> SPDT On/Off/On:
           Si: D1/D2 1N4001 antiparallel -> coll
           Ge: D5 single 1N34A (one direction) vs D6+D7 series 1N34A (other)
           Bypass: direct wire
  Tone   Q2 coll -> C9 10n -> A -> R5 470k -> gnd
         A -> R8 HIGH 25k rheostat -> B -> C8 22n -> gnd
         R25 TONE 250k: lug1 -> A, lug3 -> B, wiper -> C3 47n -> Q1 base
  Out    Q1 (2N5089): R6 10k coll, R4 2.2k em, R7 470k coll<->base
         coll -> C2 10u -> R26 VOL 100k (lug3 in, wiper -> out, lug1 -> gnd)

Pots are linear tapers; all values are the schematic-trace values.
"""

DIODE_MODELS = """
* RS (series resistance) is essential: it softens the exponential at high
* current and keeps the adaptive timestep from collapsing during hard clip.
.model 1N914 D(IS=2.52n N=1.752 RS=0.5 CJO=2p)
.model 1N4001 D(IS=14.11n N=1.984 RS=0.04 CJO=30p)
.model 1N34A D(IS=2.0u N=1.5 RS=0.5 CJO=1p)
.model MPSA18 NPN(IS=3e-14 BF=1600 VAF=150 CJE=20p CJC=15p TF=250n)
.model 2N5089 NPN(IS=2e-14 BF=700 VAF=180 CJE=25p CJC=15p TF=300n)
"""


def stage2_branches(mode: str) -> str:
    """The three-way switch wiring, per mode."""
    if mode == "silicon":
        return (
            "* Si branch: D1/D2 1N4001 antiparallel\n"
            "D1 sw q2c 1N4001\n"
            "D2 q2c sw 1N4001\n"
        )
    if mode == "germanium":
        return (
            "* Ge branch: single D5 vs series pair D6+D7 (1N34A, asymmetric)\n"
            "D5 sw q2c 1N34A\n"
            "D6 q2c qx 1N34A\n"
            "D7 qx sw 1N34A\n"
        )
    if mode == "bypass":
        return "* Bypass: direct wire (transistor-only clipping)\nRb sw q2c 0.01\n"
    raise ValueError(mode)


def build_netlist(mode="silicon", fuzz=0.5, tone=0.5, high=1.0, vol=0.5,
                  hilo=0, src="sine", amp=1.0, freq=1000.0, tfinal=0.1,
                  tstep=None, title="Pharaoh fuzz (Fairo)"):
    """Returns the full ngspice deck.

    src: 'sine' (amp*freq) or 'pulse' (for step response) or 'dc'.
    hilo: 0 = Hi (39k, louder), 1 = Lo (390k).
    fuzz/tone/vol: 0..1 pot positions; high: 0..1 (1 = 25k = brighter).
    """
    if tstep is None:
        tstep = 1.0 / 44100.0

    hilo_r = f"{39e3 * (1 - hilo) + 390e3 * hilo:g}"

    # Pot splits: fuzz=1 -> wiper at lug3 (full signal).
    # Tone: t=1 -> wiper at node A (BRIGHT/full-range side), t=0 -> node B (dark).
    # (FIX vs. origin: r25a/r25b were swapped, so tone=1 selected the dark node B
    #  and the knob read backwards vs. the real Pharaoh. Matches ToneStack.cpp.)
    r24a = max(100e3 * (1 - fuzz), 1.0)
    r24b = max(100e3 * fuzz, 1.0)
    r25a = 250e3 * (1 - tone)
    r25b = 250e3 * tone
    r8 = 25e3 * high

    if src == "sine":
        source = f"VIN in 0 DC 0 SIN(0 {amp:g} {freq:g})"
    elif src == "dc":
        source = f"VIN in 0 DC {amp:g}"
    else:  # pulse: rising edge through 10 ms, then holds
        source = f"VIN in 0 PULSE(0 {amp:g} 0 1n 1n {max(tfinal, 0.02):g} {2 * max(tfinal, 0.02):g})"

    return f"""* {title}
* mode={mode} fuzz={fuzz:.3f} tone={tone:.3f} high={high:.3f} vol={vol:.3f} hilo={'Hi' if hilo == 0 else 'Lo'}
.tran {tstep:g} {tfinal:g}
.print tran v(in) v(out) v(q3b) v(q2b) v(q3c)
.option method=gear
.option reltol=1e-3
.option trtol=7
.option temp=27
VCC 9 0 DC 9
{source}
R1 in 0 2M
RW0 node1 in {hilo_r}
C1 node1 q4b 10u
Q4 q4c q4b q4e MPSA18
R9 q4c q4b 470k
C10 q4c q4b 470p
R13 q4c 9 10k
R14 q4b 0 100k
C4 q4c fj3 0.47u
R24A fj3 fw {r24a:g}
R24B fw fj1 {r24b:g}
R22 q4e 0 1k
R23 fj1 0 1k
C5 fw q3a 0.047u
R19 q3a q3b 6.2k
R20 q3b 0 100k
R18 q3c 9 10k
R21 q3e 0 100
R17 q3c q3b 470k
C12 q3c q3b 470p
C6 q3c q3b 0.047u
D3 q3c q3b 1N914
D4 q3b q3c 1N914
Q3 q3c q3b q3e 2N5089
C13 q3c q2a 0.047u
R12 q2a q2b 6.2k
R16 q2b 0 100k
R11 q2c 9 10k
R10 q2e 0 100
R15 q2c q2b 470k
C11 q2c q2b 470p
C7 q2b sw 0.047u
{stage2_branches(mode)}
Q2 q2c q2b q2e 2N5089
C9 q2c ta 0.01u
R5 ta 0 470k
R8 ta tb {r8:g}
C8 tb 0 0.022u
R25A ta tw {r25a:g}
R25B tb tw {r25b:g}
C3 tw q1b 0.047u
Q1 q1c q1b q1e 2N5089
R3 q1b 0 100k
R7 q1c q1b 470k
R6 q1c 9 10k
R4 q1e 0 2.2k
C2 q1c ov3 10u
R26A ov3 out {100e3 * vol:g}
R26B out 0 {100e3 * (1 - vol):g}
{DIODE_MODELS}
.end
"""


if __name__ == "__main__":
    import sys
    mode = sys.argv[1] if len(sys.argv) > 1 else "silicon"
    print(build_netlist(mode=mode))
