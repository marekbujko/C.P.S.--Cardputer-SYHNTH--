/*
 * C.P.S. (CardPuter Synth) - v1.0
 * -------------------------------------------------------
 * A DIY synthesizer app for the M5Stack Cardputer family.
 * Runs on both CardputerADV (full feature set, incl. IMU) and the
 * original Cardputer (auto-detected at boot — see "Original Cardputer"
 * section below for what's different).
 *
 * Features:
 *   - Both EZ and Pro Mode use the same two physical key rows, each
 *     spanning as many octaves as needed to fit 13 notes of the active
 *     scale with no gaps or overlaps (computed automatically per scale):
 *     "1234567890-=" + Backspace (row 1) and "qwertyuiop[]\" (row 2,
 *     some octaves below row 1). Switch modes in SETTING > Play Mode.
 *   - EZ Mode (default): always the Major scale — simple and
 *     predictable, but with a wide 2-row range so octave-spanning
 *     melodies don't require manually shifting octaves
 *   - Pro Mode: choose any scale, including "Chromatic" (the default,
 *     giving full black-key access — see Scale below)
 *   - Monophonic: last key pressed wins
 *   - Notes can be played on every screen except the Patch Bank (VCO/
 *     VCF/VCA/LFO/SETTINGS/CATEGORY all only use ;/./,// for their own
 *     navigation, so playing while editing lets you hear tone/filter/
 *     LFO changes live)
 *   - ';' / '.' keys: octave shift (-2 to +2) [CardputerADV; see below
 *     for original Cardputer]
 *   - ',' / '/' keys: transpose (-12 to +12 semitones), independent
 *     of octave shift [CardputerADV; see below for original Cardputer]
 *   - 'k' / 'l' keys: volume control (0-100%, 5% steps)
 *   - 'Z' key: bend down  /  'X' key: bend up
 *     Guitar-chording style: slow pitch rise on press,
 *     fast return on release (asymmetric attack/release).
 *   - 'C' key: portamento ON/OFF toggle (PLAY mode)
 *   - 'A' key: IMU/PAD X axis hold toggle
 *   - 'S' key: IMU/PAD Y axis hold toggle
 *   - 'D' key: note hold toggle
 *   - 'H' key: hold to show HELP overlay on MAIN screen
 *   - MAIN screen shows "(HOLD)" next to an IMU/PAD axis readout when
 *     that axis is held
 *   - ADSR envelope with retrigger support
 *   - Biquad filter: LPF/HPF/BPF/Notch/None, with its own filter envelope
 *   - Oscillator: Timbre morphs through a user-configurable ORDERED
 *     SUBSET (4-6 slots) of a growing waveform library — currently Sine/
 *     Triangle/Sawtooth/Square/Wavefolder/Half-Sine (v0.983 added
 *     Wavefolder, v0.9831 added Half-Sine, both appended to the library
 *     so existing patches/settings keep the same meaning). SETTING >
 *     Morph (v0.984) is where the active chain is chosen: a library
 *     list (;/. to move, Enter to add/remove the highlighted waveform,
 *     subject to the 4-6 slot range) plus a graphical strip of the
 *     current chain's order (,// reorders the highlighted entry within
 *     it) — the morph knob's range always matches however many slots
 *     are currently active. Wavefolder drives a sine hard enough to
 *     fold back on itself several times per cycle (rich "West Coast"
 *     texture); Half-Sine is a half-wave-rectified sine (only the
 *     positive half plays), re-centered after rectifying so it doesn't
 *     push a DC bias into the filter/mix downstream.
 *     Shape (v0.985, SETTING menu entry stays "Morph" but the VCO tab's
 *     old "PWM" knob is now called "Shape", 0-100%): each waveform in the
 *     library has a second table representing its Shape=1 extreme —
 *     Sine gets a touch of 2nd-harmonic bend, Triangle skews toward a
 *     Sawtooth-like ~90/10 rise/fall, Sawtooth becomes a "double saw"
 *     (two ramps per cycle), Square becomes a narrow ~10%-duty pulse
 *     (this IS what PWM used to be, just reframed as Square's own Shape
 *     extreme instead of a separate control), Wavefolder gets a higher
 *     drive (more folds), Half-Sine gets a narrower positive lobe. The
 *     live Shape knob interpolates each waveform between its own Shape=0
 *     and Shape=1 tables — for whichever two waveforms the Timbre morph
 *     is currently blending between — before the morph blend itself
 *     runs, so Shape and Timbre compose cleanly no matter where either
 *     one is set. This fully replaces the old Square-only PWM special
 *     case with one uniform per-waveform mechanism; the ImuTarget/
 *     LfoTarget PWM enum entries were renamed to SHAPE in place (same
 *     underlying position/integer, so old saved settings still resolve
 *     correctly) rather than added anew.
 *     v0.9831 also fixed a real bug: the PLAY screen's waveform preview
 *     (lastModMorph/lastModShape) was only ever updated inside audioTask's
 *     per-sample loop, which is skipped entirely whenever no note is
 *     sounding — so changing Timbre on the VCO tab while idle and
 *     returning to PLAY kept showing the OLD waveform until the next
 *     key press refreshed it. Fixed by also updating those two values
 *     from the current base parameters in the idle branch itself.
 *     v0.9841 fixed two more bugs in the same family, found via IMU
 *     testing: (1) timbreMorph's exponential smoothing toward its target
 *     mathematically never exactly arrives, just gets asymptotically
 *     closer — invisible for continuous parameters, but timbreMorph
 *     feeds a discrete (int) waveform-index lookup for both the audio
 *     blend and the display label, so without an explicit snap the
 *     chain's LAST waveform could never actually be fully reached or
 *     correctly labeled even at full IMU deflection — fixed by snapping
 *     to the target once within 0.01 of it. (2) The idle branch only
 *     updated lastModMorph/lastModShape from timbreMorph, but timbreMorph
 *     ITSELF was still frozen while idle (its own smoothing lives in the
 *     per-sample loop this branch skips) — meaning IMU tilts updating
 *     the target while no note played never actually moved the real
 *     value at all. Fixed by snapping timbreMorph straight to its target
 *     in the idle branch too (no audible smoothing needed with nothing
 *     sounding anyway).
 *     v0.986 added 2 more waveforms to the library (Parabolic — a
 *     rounded-corner Triangle/arch shape; ESaw — an exponential-curve
 *     Sawtooth that accelerates through its ramp instead of Sawtooth's
 *     constant linear rate) — MAX_MORPH_SLOTS stays at 6 by design
 *     (Morph's whole point is picking a capped subset from a growing
 *     library), so these two aren't in anyone's default chain until
 *     manually added via SETTING > Morph. Both are Shape=0 only for now
 *     (their Shape=1 table is the same as Shape=0 — no effect yet — kept
 *     simple/low-risk this round; dedicated Shape curves for these two
 *     can follow later the same way the original 6 got theirs). Neither
 *     is band-limited via additive synthesis the way Saw/Square/
 *     Wavefolder are, so (like the pre-v0.983 Saw/Square) they may show
 *     some aliasing on extreme high/octave-shifted-up notes — a
 *     conscious simplification for this round rather than an oversight.
 *     Also fixed a real, simple bug from the same v0.984 feature: Half-
 *     Sine's abbreviated name ("HalfSin", 7 chars) overflowed the Morph
 *     screen's 36px-wide chain box — shortened to "HSin" (4 chars,
 *     matching the brevity of the other abbreviations). The Morph
 *     screen's library list now scrolls (7 rows visible, centered on
 *     the cursor) now that the library is larger than comfortably fits
 *     in the available space at once — same scrolling pattern already
 *     used elsewhere (Pattern Bank, Song), so this won't need revisiting
 *     as the library grows further.
 *     v0.9861 fixed a real bug found on hardware: the LFO screen's
 *     Sawtooth/Square previews had visibly turned wavy/rippled compared
 *     to before v0.983. Root cause: LFO's `lfoTableSample()` was reading
 *     the SAME band-limited (additive-synthesis) Sawtooth/Square tables
 *     the main oscillator uses for anti-aliasing — correct for audio
 *     playback, but the Gibbs-phenomenon ripple a truncated harmonic
 *     series produces near a discontinuity has no benefit for an LFO
 *     (0.1-20Hz, nowhere near audio rates) and instead just makes
 *     whatever it modulates subtly less steady. Fixed by having LFO's
 *     Sawtooth/Square compute the clean/naive formula directly instead
 *     (same math as before v0.983), leaving the oscillator's own
 *     band-limited tables untouched.
 *     v0.9862 added 4 more waveforms (12 total now), all via simple
 *     phase-distortion/waveshaping formulas (a sine warped by a second
 *     sine, or pushed through tanh) rather than harmonic-series
 *     summation — cheap, boot-time-only, and each gives a genuinely
 *     distinct character: Squeeze (sine phase-distorted by itself, an
 *     asymmetric "squeezed" shape), ESquare (sine through tanh, a
 *     smoothly-rounded square-like shape with no hard edges), Saw2 (sine
 *     phase-distorted by its own 2nd harmonic, an alternate asymmetric
 *     ramp distinct from both Sawtooth and ESaw), Square2 (phase-
 *     distortion + tanh combined, a squared-off shape with a different
 *     asymmetry than ESquare). v0.9863 completed Shape support across
 *     the whole 12-waveform library — added proper Shape=1 tables for
 *     the 6 that were still missing one (Parabolic, ESaw, Squeeze,
 *     ESquare, Saw2, Square2), each using the SAME formula family as its
 *     own Shape=0 table with the defining parameter intensified (the
 *     same "turn the same knob further" approach Wavefolder's own
 *     Shape=1 already used) — e.g. ESaw's exponential curve gets
 *     steeper, Squeeze's self-distortion gets stronger, ESquare's tanh
 *     drive gets harder. Shape now works uniformly everywhere in the
 *     library, per the user's explicit request.
 *   - Portamento: glide speed & on/off (SETTING > Portamento sub-screen)
 *   - SETTING screen is a category launcher: Patch / Morph / IMU(PAD) /
 *     Bend / Portamento / Play Mode each open their own dedicated
 *     sub-screen (Tab goes back one level, same pattern as the Patch
 *     Bank screen)
 *   - Sub oscillator, noise blend, bit-crusher
 *   - General-purpose LFO (Sine/Triangle/Sawtooth/Square/Sample&Hold) that
 *     can modulate Pitch / Volume / Timbre / Filter cutoff / Shape.
 *     Fully independent from the existing Vibrato and Tremolo LFOs.
 *     Sample&Hold (v0.982) draws a fresh random value once per LFO cycle
 *     and holds it steady until the next — the draw happens at the one
 *     authoritative point with true per-sample phase tracking (the
 *     phase-wrap check inside audioTask's per-sample loop), and
 *     `lfoTableSample()` itself just reads the currently-held value with
 *     no side effects, so the once-per-buffer filter-cutoff calculation
 *     and the LFO screen's waveform preview can both sample it freely
 *     without ever disturbing the real audio-rate sequence (an earlier
 *     draft kept this state as a `static` local inside `lfoTableSample()`
 *     itself, which broke exactly that — the screen's own preview sweep
 *     shared, and fought over, the same state as the live audio).
 *   - FX tab (v0.987, new — 7th tab in the Tab/Shift+Tab cycle: PLAY/SEQ
 *     -> VCO -> VCF -> VCA -> LFO -> FX -> SETTINGS -> PLAY/SEQ, shared
 *     between PLAY and SEQ same as every other tab; tab bar width
 *     shrunk 40px->34px per tab to fit 7 across 240px): first effect is
 *     a Ring Modulator (Ring Rate 20-2000Hz, Ring Mix 0-100%, Mix=0 is
 *     fully off with no separate enabled flag needed) — multiplies the
 *     post-oscillator/sub/noise signal by an audio-rate carrier (its own
 *     phase accumulator reusing sineTable via lookup, not a fresh sinf()
 *     call, so it stays cheap), placed right before the Bit-crusher
 *     stage. No IMU/LFO targets for it yet (kept this first FX delivery
 *     focused) — can follow later using the same base+offset+offsetTarget
 *     pattern already used for every other IMU-mappable parameter, at
 *     which point layering it under IMU control adds no meaningful cost
 *     either (IMU only ever adds an offset to one already-computed
 *     parameter, never re-runs a whole effect stage). More effects
 *     (chorus/delay next, per an explicit plan) will follow as their own
 *     items on this same tab rather than one tab each.
 *     Also fixed a real bug found while implementing this: LFO wave
 *     load/restore still checked "< 4" from before Sample&Hold (the 5th
 *     LfoWave value, added v0.982) existed — a saved Sample&Hold LFO
 *     selection would silently fail to restore on reboot, quietly
 *     reverting to Sine. Fixed to "< 5".
 *     v0.9871 redesigned the FX tab into two levels, per user's request
 *     for a more discoverable UI once multiple effects stack up: a pad
 *     selector (one pad per effect, filled when on / outline when off,
 *     `,`//` moves the cursor, Enter toggles on/off right from the pad)
 *     and each pad's own parameter screen (`.` drills in, reusing the
 *     same SettingItem-list pattern as VCO/VCF/VCA/LFO; Tab returns to
 *     the pad selector instead of advancing to the next tab while
 *     inside a param screen — intercepted at the top of the Tab-cycle
 *     handler specifically for this state, everywhere else FX stays a
 *     normal member of the cycle). Toggling a pad off always zeroes
 *     that effect's own mix/amount for an unambiguous "off"; toggling
 *     on restores a reasonable default instead of whatever fractional
 *     value it was left at. Extensible by design — FxEffect/
 *     FX_EFFECT_NAMES/fxIsOn()/fxToggle()/fxGetParamItems() are the only
 *     4 places a future effect (soft limiter, Chorus, Delay) needs a new
 *     entry, the pad UI itself doesn't change. Confirmed with the user
 *     up front that running multiple effects simultaneously is fine
 *     cost-wise — each effect's own Mix parameter already gates whether
 *     its processing runs at all, so an unused effect costs nothing, and
 *     nothing here forces effects to be mutually exclusive the way some
 *     hardware historically limited "2 effects at a time".
 *     v0.9872 added the 2nd effect, a Soft Limiter (Drive 1.0-5.0x,
 *     Mix 0-100%, same off-when-zero convention) — needed a genuinely
 *     different implementation approach than everything else in this
 *     file: a limiter has to respond to each sample's INSTANTANEOUS
 *     value, so unlike control-rate parameters it can't be computed
 *     once per buffer, and calling a transcendental function (tanhf)
 *     every single sample would violate the project's own established
 *     "no expensive math in the per-sample loop" rule. Used the classic
 *     x/(1+|x|) rational soft-clip curve instead — smooth, tanh-like
 *     saturation shape from one fabsf() and one division, cheap enough
 *     to run every sample safely. Applied at the very end of the signal
 *     path, on the fully-mixed sample right before it's written to the
 *     output buffer (after volume/tremolo/envelope, not before).
 *     v0.9873 added the 3rd effect, Chorus (Rate 0.1-5Hz, Depth 0-20ms,
 *     Mix 0-100%, same off-when-zero convention) — a ~2048-sample
 *     (~46ms @44100Hz) circular delay buffer, written every sample and
 *     read back from a fractional position that swings around a ~10ms
 *     base delay by the LFO (sineTable lookup, same cheap approach as
 *     Ring Mod's carrier); the two nearest buffer samples are linearly
 *     interpolated since the read position isn't sample-aligned, giving
 *     a smooth result instead of a stair-stepped one. Placed right
 *     after the Filter stage, before volume/tremolo/envelope — timbral
 *     effects belong earlier in the chain than final loudness shaping.
 *     v0.9874 added the 4th and (per the original plan) final regular
 *     FX pad, Delay/Echo (Time 50-800ms, Feedback 0-90%, Mix 0-100%,
 *     same off-when-zero convention) — a ~35280-sample (800ms @44100Hz)
 *     circular buffer holding dry+feedback*delayed, so repeat echoes
 *     emerge and decay naturally from that recirculation without
 *     needing to explicitly track multiple echo taps. Unlike Chorus,
 *     the read offset is a plain integer (not fractional/interpolated)
 *     since Delay time doesn't need to sweep smoothly the way Chorus's
 *     modulated delay does. Placed right after Chorus in the signal
 *     path (Phaser/Reverb remain deliberately held for later, per the
 *     original feasibility discussion).
 *     Also fixed a real gap found by the user: Patch Reset zeroed
 *     timbreMorph (the knob position) but never restored the Morph
 *     CHAIN itself (morphChain[]/morphChainLen — which waveforms are
 *     active and in what order) to its default — so a customized chain,
 *     or an unusual waveform a beginner selected by accident, survived
 *     a "reset the tone" action instead of being cleared along with
 *     everything else. performPatchToneReset() now also resets the
 *     chain to its original Sine/Triangle/Sawtooth/Square/Wavefolder/
 *     Half-Sine default.
 *     v0.9875 fixed a click ("jiri") heard on every key press once
 *     Delay was switched on, and the related problem of echo tails
 *     being cut off dead the instant a note ended. Both came from the
 *     same root cause: audioTask's envPhase==IDLE branch skips the
 *     whole per-sample loop, so applyChorus()/applyDelay() stopped
 *     being called while nothing was sounding — the delay line froze
 *     mid-stride with the previous note still sitting in it, and the
 *     next note-on read that stale audio straight out at full level,
 *     a hard step from silence that reads as a broadband click. (Same
 *     class of bug as v0.9831's waveform preview and v0.9841's
 *     timbreMorph: anything that has to stay live while idle needs its
 *     own handling in that branch.) Fixed by giving the idle branch a
 *     proper FX-tail path — it keeps generating and playing buffers,
 *     feeding silence through the FX chain, until the tail decays below
 *     an audible threshold, then clears the buffers and drops back to
 *     the cheap sleep. Chorus and Delay also moved from mid-chain to
 *     after the VCA (the conventional VCO->VCF->VCA->FX order), since
 *     sitting before the envelope meant echoes were being gated by it
 *     a second time. NOTE: this genuinely changes how existing patches
 *     sound — echoes now ring out properly instead of being clipped.
 *     Also in v0.9875: Patch Reset now switches all four FX off and
 *     clears their buffers, and resets Bit-crusher (previously left
 *     untouched, so a forgotten Bit-crusher setting survived a "reset
 *     the tone" the same way the Morph chain used to). The default IMU
 *     Y-axis target changed from Volume to Shape — Volume as a default
 *     means simply holding the device at an angle can silence it, which
 *     reads as broken hardware to a first-time user; Shape always keeps
 *     the sound audible. The two places that set IMU defaults had also
 *     drifted apart (first-boot said Vibrato Depth, Patch Reset said
 *     Volume); both now agree on Shape. Finally, the audioTask timing
 *     diagnostic now stops measuring at the end of DSP instead of after
 *     playRaw() — including playRaw() meant the figure converged on the
 *     buffer period (~23220us) as soon as the speaker queue filled,
 *     reporting playback rate rather than CPU cost and making the
 *     "over budget" counter fire on buffers that were never late.
 *     v0.9876 made the FX parameters modulatable, closing the gap left
 *     deliberately open since v0.987. Seven of them gained IMU and LFO
 *     targets: Ring Mod Rate/Mix, Limiter Drive, Chorus Depth/Mix, Delay
 *     Feedback/Mix. They use the same base+offset+offsetTarget pattern as
 *     every other modulatable parameter here, with the offsets smoothed
 *     at buffer rate and resolved by updateFxEffective() once per buffer
 *     into fxEff* values that the apply* functions read — so modulation
 *     adds no per-sample cost at all. Both enums were APPENDED to, never
 *     inserted into, since settings.json stores target selections
 *     numerically. Two parameters were left out on purpose. Delay Time,
 *     because applyDelay() reads its buffer at an integer offset, so
 *     sweeping the time would step the read position a whole sample at a
 *     time (zipper noise) and re-pitch echoes already recirculating —
 *     doing it properly needs interpolated reads like Chorus has, which
 *     changes how the existing Delay sounds and so belongs in its own
 *     change. Chorus Rate, because modulating one LFO's rate with another
 *     reads as drift rather than as an effect. Mix modulation is gated on
 *     the effect's base Mix being non-zero, so an effect can never become
 *     audible while its FX pad still shows as off (fxIsOn() tests the base
 *     value); fading up from near-silence still works by turning the pad
 *     on and setting a low base Mix.
 *     v0.9877 fixed a crackle on Chorus that got worse the higher Mix
 *     was set, and that carried on for seconds after playing stopped.
 *     Cause: clearFxBuffers() reset chorusLfoPhase to 0. That phase sets
 *     the chorus read distance, so zeroing it teleported the read
 *     position by up to ~900 samples in one step, and a step in read
 *     position is a step in the output waveform — a click, scaled by Mix,
 *     which is exactly how it presented. clearFxBuffers() runs at the end
 *     of every FX tail, so with an arpeggio it fired in the gap between
 *     notes and clicked on every one; Delay then captured those clicks
 *     and echoed them for the length of its tail, which is why the noise
 *     outlasted the playing. The LFO is a free-running modulator and
 *     nothing about wiping the delay line requires restarting it, so it
 *     is simply left alone now. applyChorus() additionally smooths its
 *     read distance with a one-pole filter at sample rate, which closes
 *     the same hole from the other direction: fxEffChorusDepthMs is
 *     resolved once per buffer, so once Chorus Depth became an IMU/LFO
 *     target in v0.9876 it could step every ~23ms. The ~11ms time
 *     constant is well short of a chorus LFO cycle (200ms at the fastest
 *     5Hz setting), so the sweep is unaffected and only steps are
 *     rounded off. This is the same defect class that kept Delay Time off
 *     the modulation list in v0.9876 — it applied to Chorus Depth too,
 *     which was missed at the time.
 *     v0.9878 fixed the click that v0.9877 left exposed: with the loud
 *     per-note crackle gone, a quieter one on key press became audible.
 *     This one was always there, just masked. When the chorus line is
 *     empty and a note starts, the read pointer spends the first
 *     delay-time worth of samples inside the empty region and then
 *     crosses into the freshly recorded note in a single sample — so the
 *     wet signal goes from silence to the note's full attack in one step,
 *     scaled by Mix. It never happened during an arpeggio because the
 *     line never empties between fast notes, and that difference is what
 *     pointed at it. applyChorus() now holds the wet path at zero until
 *     the read pointer has cleared the boundary and then fades it in over
 *     ~28ms, so the wet signal always arrives smoothly; the chorus is
 *     simply absent for the first few ms of a note after silence, which
 *     is inaudible next to the click it replaces. Note this was NOT
 *     caused by clearFxBuffers() zeroing the chorus buffer: three tail
 *     buffers write more zeros than the 2048-sample line holds, so it is
 *     empty by then either way — removing the memset would not have
 *     helped.
 *     v0.9879 added the 5th FX pad, Reverb — the one effect held back from
 *     the original lineup as "hardest to keep light". The measured
 *     headroom made it viable: once the v0.9875 timing fix showed real
 *     DSP cost sitting near 58% of budget rather than the 88% previously
 *     estimated, a full Schroeder bank was affordable. Topology is
 *     Freeverb's: 8 parallel comb filters, each with a one-pole lowpass in
 *     its feedback path (that lowpass IS the Damping control), summed and
 *     then passed through 4 series allpass stages that flatten out the
 *     discrete echoes into something diffuse. The delay lengths are
 *     Freeverb's own, mutually prime at 44100Hz so the combs cannot
 *     reinforce into a ringing pitch. Parameters are Room / Damping / Mix,
 *     with Room and Mix additionally available as IMU and LFO targets;
 *     Damping is left off that list as a set-and-forget tone control.
 *     Two deliberate departures from the rest of the FX code. The buffers
 *     are float rather than int16: comb feedback runs near 0.84, so buffer
 *     contents recirculate at a steady-state gain around 6x with eight
 *     combs summing on top, which would put int16 quantization noise back
 *     around -65dBFS — audible as hiss exactly where a reverb tail is most
 *     exposed. That costs ~50KB instead of ~25KB. And the buffer wrap is
 *     an if-compare rather than a modulo, because a modulo is an integer
 *     division and there are twelve wraps per sample here. Everything else
 *     follows the established extension points: FxEffect + FX_EFFECT_NAMES
 *     + fxIsOn/fxToggle/fxGetParamItems, with the pad selector itself
 *     needing no changes — though five pads at 44px+4px gap come to
 *     exactly 240px, so a sixth effect will need that layout revisited.
 *     v0.988 optimized the reverb rather than adding anything. Measured on
 *     hardware it cost ~5800us per buffer, about 25% of the budget and
 *     roughly 3x the estimate; with everything on, peaks reached ~95% of
 *     budget. Nothing dropped out, but that left no room for a sixth
 *     effect. The clue was the ratio: ~1360 cycles/sample for ~70 float
 *     ops is ~19 cycles apiece, which is memory traffic rather than FPU
 *     work. So applyReverb() now hoists its three coefficients into locals
 *     (they are globals resolved once per buffer, but the float writes
 *     into the delay lines meant the compiler had to reload them on every
 *     comb of every sample) and the comb and allpass banks are unrolled,
 *     which turns each line's base address and wrap length into
 *     compile-time constants instead of loads through a pointer array.
 *     platformio.ini additionally switches from the Arduino core's default
 *     -Os to -O2, which applies to the whole firmware. Neither change
 *     touches the algorithm, so the reverb should sound identical — if it
 *     does not, that is a bug, not a tuning change. If more is needed the
 *     next lever is block processing: applyReverb() reloads twelve indices
 *     and eight damping states per sample purely because it is called per
 *     sample, and a whole-buffer pass would amortize that away.
 *     v0.9881 reverted the -O2 half of v0.988 and kept the unrolling.
 *     Measured, the two changes pulled in opposite directions: the FX-tail
 *     path (almost pure effects, so the cleanest read on reverb cost)
 *     dropped from ~7700us to ~6390us, meaning the reverb itself went from
 *     ~5500us to ~4190us — the unrolling did roughly what it was meant to.
 *     But per-buffer peaks rose from ~22000us to ~23350us and one buffer
 *     went over budget, the first in this project's logs. Peak variance
 *     roughly tripled while the average fell. That pattern points at the
 *     instruction cache rather than at the arithmetic: this chip runs code
 *     from flash through a cache, -O2 inlines and unrolls more, the hot
 *     path grows, and a miss costs more than the instructions saved. -Os
 *     is often genuinely faster on a large ESP32 firmware for that reason.
 *     A single 126us overrun was never audible — the speaker queue holds
 *     8x512 samples, about 93ms of cushion — but it means the margin is
 *     thin, and thin margin is worth more than a slightly lower average.
 *     v0.989 gave Vibrato, Tremolo and the Bit-crusher menu controls. All
 *     three dated from the earliest IMU work, before an FX tab existed,
 *     and could only ever be reached by tilting the device — which had
 *     become the odd one out now that everything else has a menu. They did
 *     not simply lack a menu: the IMU wrote their value DIRECTLY rather
 *     than adding an offset, so a menu control would have been overwritten
 *     on the next IMU update. All three now use the base+offset model the
 *     rest of the file uses, so menu and tilt work together, and all three
 *     are saved (they never were before, which also means no existing
 *     patch carries a value and nothing changes sound on upgrade).
 *     The two IMU gates are gone with them. Vibrato and Tremolo only
 *     applied when an axis was actually assigned to them, which made sense
 *     when tilt was the only way to set them at all — a leftover depth
 *     would otherwise have been unreachable dead modulation — but with a
 *     menu control it would do the opposite of what a user expects: set
 *     Depth, hear nothing, no way to tell why.
 *     Where each one went is deliberate. The Bit-crusher is a signal
 *     degradation effect and became the 6th FX pad. Vibrato and Tremolo
 *     are modulation sources, not effects: Vibrato is an LFO on pitch and
 *     Tremolo an LFO on amplitude, and the general LFO already routes to
 *     both via LfoTarget::PITCH/VOLUME. Putting them in an effects menu
 *     would have sat them next to Reverb while duplicating the LFO tab, so
 *     Vibrato went to VCO and Tremolo to VCA, where a hardware synth puts
 *     them. Also fixed while here: applyBitcrush() called powf() on every
 *     sample, which this file's own rule about the per-sample loop
 *     forbids — the amount can only change once per buffer, so the powf()
 *     moved to updateFxEffective() and the level count is passed in.
 *     The pad selector went to two rows of three with rounded corners and
 *     labels centred vertically as well as horizontally. Five 44px pads
 *     had filled the 240px width exactly, so the 6th had nowhere to go;
 *     shrinking them would have cut labels to five characters and lost
 *     "RingMod". At 72px across two rows the names stay readable and there
 *     is room for a 7th and 8th. ,// still walks all six in order — the
 *     grid is visual only, so navigation was untouched. The VCA tab needed
 *     splitting into two columns for the same class of reason: a 5th item
 *     in one column would have landed past the nav line.
 *     v0.9891 made Square's Shape a real pulse-width sweep again. Since
 *     v0.985 folded PWM into Shape it had been a crossfade between a
 *     50%-duty band-limited square and a hard-edged 10% pulse — and a
 *     crossfade cannot produce a duty sweep. Mixing those two gives their
 *     SUM: a stepped three-level waveform, not a square of intermediate
 *     width, which is exactly what showed up on hardware anywhere between
 *     the extremes. Square now rebuilds its table instead, using the
 *     identity that a pulse is the difference of two sawtooths offset in
 *     phase by the pulse width — pulse(x,w) = saw(x) - saw(x-w). Because
 *     sawtoothTable is already band-limited so is the result, and there is
 *     no per-sample cost at all: 256 table entries rebuilt only when the
 *     value actually changes is cheaper than 1024 extra lookups per buffer
 *     would have been. oscWaveformTableB() returns the same table for
 *     Square so the generic Shape crossfade becomes a no-op, and the old
 *     squareTableB is gone. Verified on the host: Shape 0/50/100% give
 *     49.6/31.2/12.1% duty against 50/30/10% intended, at an identical
 *     peak level for every width. Normalization measures the actual peak
 *     rather than deriving it, since a pulse's two levels are asymmetric
 *     and band-limiting adds Gibbs overshoot an analytical figure misses.
 *     Rebuilt from the idle branch as well as the buffer-rate block —
 *     otherwise the VCO preview would freeze at the last width until a
 *     note was played, the same idle-branch trap as v0.9831 and v0.9841.
 *     One thing this does NOT change: the ripple visible along the flat
 *     parts. That is the Gibbs phenomenon, the unavoidable signature of a
 *     truncated harmonic series, and it is what makes the waveform
 *     alias-free. A perfectly flat square would be the naive one, which
 *     aliases badly on high notes.
 *     v0.9892 corrected the width mapping. v0.9891 fixed the SHAPE of the
 *     sweep but not its range: it ran 50% duty down to 10%, symmetrical at
 *     Shape 0. Photos of the pre-Shape firmware showed the original PWM
 *     control swept both ways — 10% at one end, symmetrical at the middle,
 *     90% at the other — so half the range had been missing, and the
 *     default Shape of 0.5 sat on a 30% pulse rather than a plain square.
 *     Now 0 -> 10%, 0.5 -> 50%, 1 -> 90%, linear across the whole knob so
 *     neither end is a dead zone. Verified on the host: 12.1/31.2/49.6/
 *     68.8/87.9% measured duty at Shape 0/25/50/75/100%, identical peak
 *     level throughout.
 *     v0.9893 stopped the FX pads discarding your Mix setting. Switching
 *     an effect off has to zero its Mix — that IS "off" everywhere in this
 *     file, and fxIsOn() reads the same value to decide how to draw the
 *     pad — but switching back on restored a fixed default rather than
 *     what had been there, so setting Delay to 50%, toggling off and on,
 *     and getting 40% back looked like the pad quietly throwing the
 *     setting away. fxToggle() now saves the level before clearing it and
 *     restores that. Bit-crusher's control is Amount rather than Mix but
 *     shares the same zero-means-off convention, so it goes through the
 *     same mechanism; the six cases collapsed into one path via
 *     fxLevelPtr(). Note the memory is per session: the level itself is
 *     saved, but an effect saved in the off state comes back with its
 *     default, since zero is all that was stored.
 *     v0.9894 fixed two things the hardware turned up. Square's pulse
 *     visibly shrank as the width moved away from 50%, because the table
 *     normalized peak rather than peak-to-peak — a DC-free pulse is
 *     asymmetric about zero, so holding the larger side fixed lets the
 *     span collapse. Peak-to-peak is now held instead, sized so the
 *     10%/90% extremes peak at 26000 (ESQUARE_AMP, the loudest waveform
 *     here), which puts the symmetrical square at about +/-14400 instead
 *     of +/-18000. That is roughly 1.9dB quieter — under the ~3dB that
 *     reads as a clear step, and only on this one waveform — and in
 *     exchange the loudness change across a PWM sweep drops from 2.6x to
 *     1.67x in RMS, which is much closer to what a sweep should sound
 *     like. Second, the Bit-crusher needed to be near 90% before anything
 *     was audible. The cause was range, not a changed formula: 16 bits
 *     down to 3 sounds like a full sweep, but the source is already 16-bit
 *     so nothing is audible until below about 10 bits, which the old curve
 *     did not reach until ~45%. Half the control did nothing. It now runs
 *     10 bits down to 2, so 30% lands on 7.6 bits where it used to give
 *     12.1.
 *     v0.990 opens the UI/UX pass. Three things, all about how the thing
 *     feels to operate rather than what it can do.
 *     Filter Cutoff stepped by a fixed 100Hz. Pitch is perceived
 *     logarithmically, so that is an enormous jump at the 100Hz end and
 *     almost nothing at 8000Hz, and crossing the range took 79 presses on
 *     the parameter people touch most. It now multiplies by 1.15, the
 *     same factor LFO Rate and Ring Mod Rate already used — every press
 *     moves the same musical interval, low-end resolution goes from 100Hz
 *     steps to 15Hz, and the full sweep is 31 presses.
 *     Held keys now auto-repeat. Every menu action here was edge
 *     triggered, one press one step, which did more to make editing feel
 *     slow than any individual step size did. 400ms before repeat starts,
 *     then ~14 steps/second; the delay matters as much as the rate, since
 *     a tap has to stay a single step or fine adjustment becomes
 *     impossible. Deliberately NOT applied to: Enter on an FX pad and the
 *     FX drill-in key (repeating those just flickers an effect or
 *     re-enters a screen), and value editing on the SETTINGS and CATEGORY
 *     screens — several category items are binary toggles bound to both
 *     inc and dec, and a toggle repeating 14 times a second lands wherever
 *     you happened to release. List movement repeats everywhere, which is
 *     the part that helps on a long list.
 *     The FX pads became a grid in v0.989 but ,// kept walking all six in
 *     a line, so moving from the top row to the one below took three
 *     presses where the eye says one. ';' now moves a row, wrapping.
 *     Purely additive — ,// still walks the sequence and . still drills
 *     in — and written as "up one row" rather than "swap rows" so it still
 *     makes sense if a 7th effect ever makes it three rows.
 *     v0.9901 fixed a fault in that repeat logic. menuKeyFire() only ever
 *     recorded a hold timestamp on the initial press, and only the branch
 *     for the current screen runs each frame — so a key held across a
 *     screen change arrived carrying a timestamp from minutes earlier, and
 *     the first frame in the new screen saw a key that looked like it had
 *     been held forever and repeated immediately. Pressing '.' on the FX
 *     pad selector to drill into a parameter screen is exactly that case.
 *     Hold timestamps are now cleared for any key that is not down, before
 *     the per-screen dispatch so it happens regardless of which branch is
 *     active, and a zero timestamp means "this hold did not start here"
 *     and cannot repeat.
 *     v0.9902 fixed the IMU screen's Shape readout sitting still unless a
 *     note was sounding, while Timbre's moved freely. Shape's IMU offset is
 *     smoothed in the buffer-rate block, which the idle branch skips, so it
 *     froze with nothing playing. It is snapped in that branch now, exactly
 *     as timbreMorph already was — v0.9841 fixed this for timbreMorph and
 *     missed the parameter sitting next to it, making this the fourth
 *     appearance of the same idle-branch trap.
 *     Also fixed a genuine overflow in applyBitcrush(): rounding pushes
 *     values near full scale UP past it, so at 4 levels an input of 30000
 *     rounds to 32768 — one above int16's maximum — and the cast wrapped it
 *     to -32768. A loud note hit that on most peaks. Now clamped before the
 *     cast. This is a real defect but NOT an explanation for the effect
 *     being inaudible, which remains unexplained: the quantization maths
 *     verifies correct on the host at every setting. A temporary [bitcrush]
 *     diagnostic prints base, offset and level count whenever the value
 *     changes, to find out what the hardware actually has.
 *     v0.9903 makes modulation visible. Until now every menu showed its
 *     base value and nothing else, so with an IMU axis or the LFO pointing
 *     at a parameter the sound changed with no indication of why. Rows now
 *     carry a marker: '*' when an IMU axis drives that parameter, '~' when
 *     the LFO does, both when both. SettingItem gained two target fields
 *     to support it, declared WITHOUT in-class default initializers on
 *     purpose — this is a C++11 build, where a default initializer makes
 *     the struct non-aggregate and would break every existing
 *     {"Name",inc,dec,label} in the file. Without one, aggregate
 *     initialization value-initializes them to NONE, so untagged arrays
 *     keep working untouched. The markers go after the value rather than
 *     in a column of their own, since the name field is already 8
 *     characters and several names use all 8.
 *     Also fixed the PLAY waveform preview crawling from the old shape to
 *     the new one after editing Timbre or Shape in VCO. The preview is
 *     smoothed so that modulation animates instead of stepping frame to
 *     frame, which is right for continuous motion but wrong for a discrete
 *     edit — it showed a waveform the synth was not actually set to. It
 *     now snaps when arriving back on the screen or when the value jumps
 *     by more than 0.05, a threshold far above what modulation moves
 *     between two frames and far below any menu step, and keeps smoothing
 *     everything else.
 *     v0.9904 fixed debris along the bottom nav line on every screen
 *     except PLAY, which accumulated as you cycled through the tabs. The
 *     nav row was never cleared: the per-screen clear in the item-list
 *     screens is fillRect(0,76,240,50), which stops at y=125, and the nav
 *     text sits at y=126. So that row was only ever overwritten, never
 *     erased. Nav strings differ per screen and are centred, so they begin
 *     at different x and leave different gaps, and fragments of the
 *     previous screen's text showed through the gaps in the current one.
 *     Only a full redraw cleaned it up, which is why it appeared after a
 *     lap of the tab cycle rather than immediately. drawItemList() now
 *     clears y=125-134 before drawing. The other two draws at y=126 were
 *     checked and are already covered by a fillRect(0,12,240,123).
 *     v0.991 opens work toward a second oscillator, starting by making
 *     room for one rather than by adding it. getMorphedSample() called
 *     oscWaveformTable()/oscWaveformTableB() four times per SAMPLE. Both
 *     are switch statements over the waveform enum, so that was four
 *     dispatches plus four pointer loads 44100 times a second to reach
 *     addresses that only change when the Morph chain is edited — the one
 *     place this file's own "expensive work at buffer rate" rule had never
 *     been applied. The pointers are now resolved into a flat per-slot
 *     array once per buffer (12 switch calls per 1024 samples instead of
 *     4096) and the inner loop does plain indexed loads. Output is
 *     bit-identical: same tables, same order, only the lookup moved.
 *     Refreshed from the buffer-rate block, from the idle branch (the
 *     Morph chain is edited with nothing sounding, so the preview would
 *     otherwise keep drawing the old chain — the same idle-branch trap
 *     this project keeps meeting) and at boot. Whatever this frees goes
 *     straight into the budget for oscillator two.
 *     Measured afterwards, that optimization made no difference at all —
 *     the average was unchanged. The reasoning was sound (four switch
 *     dispatches per sample is real work) but it was not the bottleneck;
 *     the compiler or the instruction cache was evidently already handling
 *     it. The code is bit-identical and harmless so it stays, but it did
 *     not free the headroom it was written to free. What the same run DID
 *     establish is the number that mattered: with all FX off, a sounding
 *     voice costs ~9,700us, 42% of budget — not the ~12,000us/52% that had
 *     been estimated. So the room for a second oscillator was already
 *     there.
 *     v0.9911 adds oscillator 2. It has its own position along the Morph
 *     chain, its own Shape, its own Detune/Fine/Octave, and a Mix that
 *     crossfades between the two — at 0 you hear only oscillator 1, so
 *     every existing patch sounds exactly as it did and nothing needs
 *     migrating. It shares the Morph chain, filter, envelope and VCA,
 *     because those are one signal path; duplicating them would make it a
 *     second voice rather than a second oscillator. Skipped entirely at
 *     Mix 0, so an unused oscillator costs nothing, and its pitch ratio
 *     resolves once per buffer since it needs a powf().
 *     The UI is a second page inside the VCO tab rather than a new tab —
 *     the tab bar is already seven entries at 34px and an eighth would not
 *     fit. An "Osc" row at the top of each page flips between them with
 *     ',' and '/', so the switch is where you are already looking. That row
 *     is bound to both inc and dec, which means it must NOT auto-repeat, or
 *     it would flip pages ~14 times a second and land wherever you released
 *     — the same hazard as the CATEGORY screen's toggles, detected here by
 *     testing whether a row's two handlers are the same function. Mix
 *     appears on both pages deliberately: it is what you reach for while
 *     balancing the two, and having to change page to hear the balance move
 *     would be backwards. The waveform preview follows the page on screen,
 *     so you see the oscillator you are about to edit.
 *     v0.9912 fixed the VCO page-1 left column running into the nav line.
 *     Adding the page-flip row and a Mix row took that page to 11 items, so
 *     6 rows at the old fixed 13px pitch put the last at y=122 and its
 *     glyphs at 130 — through the nav text at 126. drawItemList()'s row
 *     pitch is a parameter now, and that page alone uses 12px from y=56,
 *     which lands the lowest row at 116 and ends it at 124. Every other
 *     screen keeps 13.
 *     v0.9913 stopped saving to the SD card from disrupting the audio.
 *     Writing settings.json was ~100 separate f.printf() calls straight to
 *     the File, each reaching the FAT layer and the SPI driver on its own.
 *     On the buffer where a save landed, audioTask's worst case went from
 *     the usual ~22-23ms to 31ms — well past the 23.22ms budget — and the
 *     surrounding second logged several late buffers. The text is now
 *     assembled in a RAM buffer and written once, so the card is touched a
 *     single time instead of a hundred. The pattern save got the same
 *     treatment: it runs on leaving SEQ, so it fires often enough to
 *     matter. If the buffer ever fills, the write is refused outright
 *     rather than producing a truncated file. Song saves still write
 *     directly, but they only happen on an explicit user action.
 *     v0.9914 gave oscillator 2 the whole waveform library. It had been
 *     picking a position along the Morph chain, which meant it could only
 *     reach waveforms that happened to be in that chain — and the chain
 *     exists purely so IMU and LFO can sweep continuously between
 *     waveforms, which a fixed layer never needs. So the restriction was
 *     costing reach and buying nothing: with osc 1 morphing the six
 *     defaults you could not put an ESquare underneath it. It now selects
 *     one of all twelve outright. Simpler as well — one table pair read
 *     directly, no morph interpolation, resolved at buffer rate like
 *     everything else. LFO->Shape still reaches it, LFO->Timbre no longer
 *     does: there is no Timbre to sweep, but Shape still gives the layer
 *     movement, which is the part worth having. Persisted as osc2_wave
 *     replacing osc2_morph.
 *     v0.9915 moved the reverb and the soft limiter to whole-buffer
 *     passes. Called per sample, applyReverb() reloaded twelve buffer
 *     indices and eight damping states from memory, used each once, and
 *     wrote them all back — 1024 times per buffer. None of that is the
 *     reverb; it is the cost of the call boundary. The state is now
 *     hoisted into locals for the length of the buffer, so the load/store
 *     happens once instead of 1024 times, with the arithmetic, the
 *     coefficients and the order all untouched. Verified bit-identical
 *     against the old path over 44100 samples. The limiter followed for
 *     the same reason and so the two share one pass. Note this changed
 *     WHERE they sit: the per-sample loop now finishes at the Delay and
 *     writes to buf, and the two passes run over that buffer afterwards.
 *     The FX-tail path needed reordering to match, since its fade and
 *     peak test read what the reverb produces and the peak is what decides
 *     when the tail ends.
 *     Worth recording why the obvious alternative does not work: lowering
 *     the reverb's Room, Damping or Mix does not reduce its cost at all.
 *     Those are coefficients, not amounts of work — all eight combs and
 *     four allpasses run every sample regardless. Mix=0 is the sole
 *     exception, since that returns early. The reverb is all-or-nothing,
 *     which is why this had to be attacked as bookkeeping.
 *     v0.992 is UI work. The IMU target picker had the Bit-crusher in a
 *     section of its own, left from when it predated the FX tab entirely;
 *     it has been the 6th FX pad since v0.989, so it now sits with the
 *     other FX entries, in the same order as the pads themselves. While
 *     there, the picker's sections gained names shown in the title bar:
 *     the list is 29 rows deep with 7 on screen, so dividers alone left
 *     you scrolling with no idea which part of the synth you were in. The
 *     title follows the cursor rather than only redrawing on entry.
 *     Square's Shape row is now labelled "PWM" whenever the waveform in
 *     play is Square, on either oscillator page. That is what the control
 *     does there, and calling it "Shape" cost real confusion once: a user
 *     comparing against the pre-Shape firmware had no way to tell from the
 *     screen that it was the same control.
 *     Single-parameter effects are edited in place on the pad selector
 *     instead of opening a screen for one number — the Bit-crusher's
 *     Amount is its only control. '.' toggles inline editing on such a
 *     pad, ',' and '/' then change the value instead of moving the pad
 *     cursor, and '.', Enter or ';' leaves. The value shows on the info
 *     line either way, and effects with more than one parameter open their
 *     screen exactly as before.
 *     v0.9921 covers three things asked for after playing with it.
 *     The help overlay can now be latched open with Shift+H, while H alone
 *     stays momentary. Making H itself a toggle was considered and
 *     rejected: holding it can never strand you, because letting go always
 *     closes it, whereas a toggle on a key sitting in the top row can be
 *     hit by accident mid-performance and the overlay covers the whole
 *     screen. Shift+H gives the hands-free reading without giving up that
 *     property.
 *     Each IMU axis can be switched off with Shift+A / Shift+S, the same
 *     shape as Shift+V for the arpeggiator; unshifted stays the hold
 *     toggle. Disabling an axis also clears its offset and releases its
 *     hold — otherwise whatever it was contributing would freeze at the
 *     tilt you happened to be at, leaving the sound altered by a control
 *     that reads as off. The enable flag is separate from target==NONE so
 *     that switching an axis off and back on keeps its assignment, and it
 *     is saved. The axis readout shows "-- OFF --"; the level bar itself
 *     is deliberately left drawn, since blanking it would hide what the
 *     axis is still pointing at.
 *     The arpeggiator's state is shown in the top-right of the waveform
 *     area ("ARP", or "ARP L" with latch on). It was previously only in
 *     the help overlay, so you had to press a key to find out. The flag is
 *     part of the dirty test, so toggling it redraws at once instead of
 *     waiting for the waveform to move — with a static patch that could
 *     have been a long wait.
 *     v0.9922 fixes two faults in the previous version.
 *     Shift+A/S/H did nothing. The keyboard reports the SHIFTED character,
 *     so a shifted A arrives as 'A' rather than as 'a' with s.shift set —
 *     testing s.shift alone could never fire. Shift+V already carried a
 *     note about precisely this along with a defensive uppercase test, and
 *     that lesson was not carried across. Both forms are accepted now.
 *     The second was the display corruption after using the help overlay:
 *     nav line blanked, help text left in the waveform area, and SEQ's
 *     orange bleeding into PLAY after switching modes — all intermittent.
 *     The overlay covers the whole screen while PLAY and SEQ redraw
 *     through several partial sprites, so closing it must be followed by a
 *     FULL redraw or leftover pixels survive wherever no dirty region
 *     happens to land. That request was computed fresh each frame and
 *     consumed in the same frame: if that frame was throttled
 *     (canForceRedraw false, MIN_REDRAW_MS not elapsed) it was dropped,
 *     and lastHelpVisible had already been updated, so the next frame no
 *     longer knew a full redraw was owed — the 100ms fallback then redrew
 *     only the dirty regions. Whether it corrupted depended on where in
 *     the throttle window the key release landed, which is exactly why it
 *     came and went at random. The request is latched now and cleared only
 *     once a full redraw has actually been issued, and it also forces the
 *     frame to happen so a throttled frame cannot swallow it twice.
 *     modeChanged is latched the same way for the same reason: it is true
 *     for one frame only, and PLAY and SEQ differ in accent colour, so a
 *     throttled mode-change frame left the old colour behind.
 *     v0.9923: the Y axis still read "0%" when switched off while X read
 *     "-- OFF --". The v0.9921 edit was written against the X readout's
 *     variable name, and the Y readout uses a different one, so it matched
 *     X's two sites and silently missed Y's two — the sort of thing a
 *     find-and-replace does quietly. Both axes agree now.
 *     v0.9924 is four pieces of tidying, all user-spotted.
 *     Pro Style's scale readout said "Chromatic: Chromatic", since
 *     Chromatic is the only member of its own category. A category whose
 *     name matches the scale's now prints once.
 *     The SETTING list showed "Select>" on every row. The word was
 *     identical everywhere, so it carried no information, and because the
 *     row name prints in a fixed-width field it sat in a different column
 *     for "Portamento" and "Play Style" than for the shorter names — a
 *     ragged edge earned for nothing. Just the arrow now, which still says
 *     "this opens something" and lines up.
 *     "Play Mode" became "Play Style", and EZ/Pro became EZ Style/Pro
 *     Style. When that setting was written there was no MODE in this
 *     firmware; PLAY / SEQ / SONG arrived later and took the word, leaving
 *     two unrelated things both called Mode. All four menu variants and
 *     the category title were updated together.
 *     The sequencer's step outlines are coloured in groups of four —
 *     red, orange, yellow, cream — the way the TR-808 tinted its sixteen
 *     step buttons so you could see which beat of the bar you were on.
 *     (The 909 dropped it; it deserved better.) Outline only: the fill
 *     still carries velocity and accent, and cursor and playhead still
 *     override, so nothing that already meant something was displaced.
 *     v0.9925 made those beat colours actually visible. With notes entered
 *     they disappeared, and the cause was not the outline but the fill:
 *     the velocity bar used the same bright orange the outlines are drawn
 *     from, so the two were close in both hue and brightness and the
 *     border sank into the bar. Two changes, neither of which costs any
 *     information. The bar is drawn in a darkened orange (and a darkened
 *     red when accented) used for nothing else — bar HEIGHT still carries
 *     velocity and the accent is still a distinct hue, so both read as
 *     before, while the beat colour now sits clearly on top. And the fill
 *     is inset by the outline thickness, leaving a black gap so the two
 *     never touch; that does about as much for legibility as the dimming.
 *     The playhead stays full white and still wins over everything.
 *     Dimming the whole SEQ theme was considered and rejected: the problem
 *     exists only inside the step boxes, and darkening the accent colour
 *     globally would have made every readout and border on the screen
 *     harder to read in order to fix it.
 *     v0.9926 re-picked those colours and fixed a real performance fault.
 *     Darkening the orange had taken it straight through brown — 120,66,0
 *     IS a dark brown — so on the panel the steps read as muddy rather
 *     than dim. Pulling the hue toward amber and keeping saturation high
 *     gives the same drop in brightness against the outlines while still
 *     looking like a colour; the accent bar moves to deep pink for the
 *     same reason, and stays clearly distinct from the red beat-1 outline
 *     above it. The beat palette itself swaps the 808's muted cream for
 *     cyan and warms beat 1 toward pink, which reads better on a small
 *     backlit panel than a faithful copy of a 1980 silkscreen does.
 *     The Pattern bank grid crawled under the cursor keys, and opening it
 *     during playback dragged the sequencer's tempo down. Cause:
 *     drawPatternBankScreen() called patternSlotExists() for all 64 cells,
 *     so every redraw meant 64 SD.exists() calls — 64 card transactions,
 *     several times a second, blocking long enough to interfere with the
 *     audio task. Same mechanism as the settings-save spike fixed in
 *     v0.9913, in a place nobody thought to look because nothing appeared
 *     to be writing. Occupancy is cached now, read once on entering the
 *     screen and refreshed after a save or delete — the only things that
 *     can change it. The Enter and Delete handlers were hitting the card
 *     on every press for the same answer and now use the cache too.
 *     v0.9927 gave up on a coloured fill for the sequencer steps. Every
 *     version of a dimmed ORANGE stayed in the same hue family as the beat
 *     outlines — beat 1 and beat 2 especially — so the border kept sinking
 *     into the bar no matter how the brightness was tuned. A neutral has
 *     no hue to collide with: near-white separates from all four beat
 *     colours equally, and it is the brightest thing available, which
 *     suits the element that should read first.
 *     That forced the other two fills to move, since white was already
 *     spoken for. Accent is violet — the one direction no beat colour
 *     occupies, since they run red -> orange -> lime -> cyan — so it can
 *     never be mistaken for an outline. The playhead is green, and remains
 *     the most conspicuous thing on the screen because it is the only part
 *     that moves. Beat 4's cyan was deepened, since a pale cyan was the
 *     single beat colour close enough to a near-white fill to blend with
 *     it. The cursor outline stays white and still reads, because the fill
 *     is inset by the outline thickness and there is always a black gap
 *     between the two.
 *     v0.9928 takes the beat colours to a palette the user supplied —
 *     primary red, orange, green, blue. Four clearly separated hues suit a
 *     small backlit panel better than the 808's warm gradient did: the
 *     yellow previously on beat 3 was too close to the near-white fill to
 *     read across it, which was the last remaining collision.
 *     Two deliberate departures from the palette as given. Its blue,
 *     1005EB, is very dark, and a dark outline on a black background is
 *     close to invisible here — the hue is kept and the luminance raised
 *     until it reads. And with a green and a blue now among the beats,
 *     every candidate for the playhead sat next to one of them, so the
 *     fill drops from near-white to mid grey — still neutral, still
 *     colliding with no beat hue — which frees white to be the playhead
 *     again, as it was before v0.9927. Accent moves to magenta, the
 *     direction none of the four occupies.
 *     v0.9929 replaced that magenta with teal on preference. The choice is
 *     more constrained than it appears: the beat outlines sit at roughly
 *     0 deg (red), 30 (orange), 110 (green) and 235 (blue), leaving only
 *     two hue gaps wide enough to be unambiguous — around 175 (cyan/teal)
 *     and around 300 (purple/magenta). Teal is the better of the two
 *     because it is ~60 deg from both neighbours, where purple is only
 *     ~50 from blue and would be a fill sitting inside a blue outline.
 *     Kept fully saturated rather than lightened so it reads as a strong
 *     colour rather than a pastel.
 *     v0.993 swaps the ordinary fill back to white and moves the playhead
 *     to purple, on preference — the white bars look better on the panel.
 *     Worth recording what this trades away, since it may want reverting.
 *     Brightness is the strongest signal available, and white bars moving
 *     through grey ones made the playhead unmissable. Marking it by hue
 *     instead is weaker, and weakest on a low-velocity step where the bar
 *     is only a few pixels tall. Purple is the only candidate left: the
 *     beat outlines leave exactly two usable gaps in the hue circle and
 *     teal has the other. The cursor outline stays white and still reads
 *     over a white fill thanks to the inset gap from v0.9927.
 *     Swapping the two outright — a grey playhead over white fill — was
 *     considered and rejected: it would have made the two things that most
 *     need to stand out, the playhead and the cursor, the two dimmest
 *     things on the screen.
 *     v0.9931 covers three requests.
 *     Randomize Patch had quietly stopped covering the synth. Everything
 *     added since it was written — oscillator 2, Vibrato, Tremolo, the
 *     Bit-crusher and all five FX — was simply never touched by it, so a
 *     "random patch" was random only in the parts that existed in 2024.
 *     Audited in one pass and all of it included. The probabilities are
 *     not uniform on purpose: oscillator 2 appears about a third of the
 *     time (one on every patch would make them all thick in the same way,
 *     and it leans toward musical intervals rather than an arbitrary
 *     semitone offset), Vibrato and Tremolo are occasional and shallow,
 *     the Bit-crusher is rarer still and capped well below its top, and
 *     each FX pad is rolled independently so they cannot all land at once
 *     — which would both bury the patch and put every random patch at the
 *     top of the CPU budget. The existing Noise line was left exactly as
 *     it was, having already been tuned deliberately.
 *     Oscillator 2 gained a Semitone control, -12..+12, which is what it
 *     needed to play harmony: Octave is too coarse and Detune is in cents,
 *     so a third or a fifth could only be asked for as 400 or 700 cents on
 *     a +-50 control. Labelled by interval as well as number ("+7 5th"),
 *     since that is how you reach for one. The VCO 2 page is a tidy 4+4
 *     with it.
 *     The IMU target picker is two levels now, like the scale picker: 28
 *     targets in one flat list meant scrolling four screens to reach the
 *     far end. It also opens with VCO at the top rather than pitch and
 *     volume — Timbre and Shape move the waveform on screen as you tilt,
 *     which is what this synth is for; pitch was first only because it was
 *     what existed when the list was written. The NONE entry is gone,
 *     since Shift+A / Shift+S switch an axis off outright and that says it
 *     better than a row you have to scroll to. The enum is untouched, so
 *     settings that stored NONE still load. Sections are derived from the
 *     group labels rather than listed separately, so there is one list to
 *     keep in step instead of two.
 *     v0.9932 fixed patch loading, which had been quietly merging rather
 *     than replacing. The settings parser only ever ASSIGNS keys it finds
 *     and leaves anything absent alone — correct for settings.json, which
 *     this firmware always writes complete, but wrong for a patch file
 *     saved by an older version. A patch from before oscillator 2, the
 *     Morph chain or the FX tab existed has no keys for any of them, so
 *     loading it gave you that patch's tone with whatever oscillator 2,
 *     Morph chain and effects happened to be set at the time. Not a
 *     consequence of changing the format, as it might look — the parser
 *     simply never had a notion of "absent means default". Patch loads now
 *     reset the tone to defaults first, so an absent key means default
 *     rather than keep, and old patches load sounding as they did when
 *     they were saved. settings.json keeps the old behaviour, since it is
 *     always current and complete.
 *     v0.9933 sets the default Morph chain back to the four waveforms the
 *     synth shipped with — Sine, Triangle, Sawtooth, Square. It had grown
 *     to six, picking up Wavefolder and Half-Sine as they were added, but
 *     the default is what a beginner meets and what a tone reset returns
 *     to, and that should be the plain starting point; the other eight are
 *     there to be chosen. MIN_MORPH_SLOTS is 4, so this is the smallest
 *     valid chain. Safe to change because the twelve-waveform library has
 *     not shipped publicly yet — patches saved against a six-slot default
 *     only exist on the developer's own device.
 *     Note the reset path also has to refresh the cached morph table
 *     pointers: they still describe the previous chain, and the audio task
 *     would otherwise read the old slots until the next buffer-rate
 *     refresh happened to catch up.
 *     v0.9934 covers three more IMU items.
 *     The picker's sections follow the TAB BAR now — VCO, VCF, VCA, LFO,
 *     FX, then Pitch and ARP/SEQ, which have no tab of their own. FX had
 *     been near the top because its effects are the easiest to hear, but
 *     since the list was split into sections there is no long scroll left
 *     to save anyone from, so matching the tabs is worth more than
 *     ordering by impact — one order to learn instead of two.
 *     Osc Mix joined the VCO section, and it is bipolar on purpose:
 *     tilting one way brings oscillator 1 forward and the other brings
 *     oscillator 2, so at full deflection you get one or the other alone.
 *     That makes it a crossfade you can play rather than a level you fade
 *     up, which is the point of having two oscillators. Osc 2's Shape came
 *     along with it, since oscillator 1's Shape was already there and the
 *     asymmetry was hard to justify.
 *     Finally, a disabled axis now shows an empty level track. It had been
 *     showing a half-full bar for some targets and an empty one for
 *     others, which was never a decision — getImuNorm() returns each
 *     target's current value and where zero sits differs per target
 *     (Shape's neutral is mid-range, the Bit-crusher's is the bottom), so
 *     the bar simply reflected whatever the maths gave. Off now looks the
 *     same everywhere.
 *     v0.9935 fixes both of the previous version's new controls.
 *     Osc Mix only ever moved toward oscillator 1, whichever way the
 *     device was tilted. It inherited the axis's own bipolar setting, and
 *     with that off the caller takes fabsf() of the tilt — so both
 *     directions produced the same positive value. Direction IS the
 *     control for a crossfade, so it is forced bipolar now, alongside
 *     Pitch Bend and the ARP targets.
 *     Oscillator 2's Shape did nothing on Square and appeared to be the
 *     only waveform where it DID work. Both symptoms are the same cause:
 *     Square's Shape is a duty sweep baked into its table rather than a
 *     crossfade between two tables, and there was only one such table,
 *     rebuilt from oscillator 1's Shape. Oscillator 2 therefore ignored
 *     its own Shape and displayed oscillator 1's duty — which is exactly
 *     what looked like it responding. Oscillator 2 has its own table now,
 *     built by a shared routine so the maths cannot drift apart, used by
 *     both the audio path and the VCO 2 preview.
 *     v0.9936 adds UI themes — a feature planned since early on, when this
 *     was a single-mode synth and one colour was the whole of it.
 *     A theme sets the accent for all THREE home modes at once rather than
 *     one colour, and they are presets rather than free RGB. Both follow
 *     from the same thing: uiColor is not purely decorative. PLAY, SEQ and
 *     SONG are told apart by it, and that is load-bearing — it is how the
 *     v0.9922 "SEQ orange left behind in PLAY" bug was noticed. Three
 *     independent colour pickers would let someone choose three similar
 *     colours and quietly lose the distinction, and a picker on a 240x135
 *     panel would just as happily produce dark grey on black. Every preset
 *     here keeps the three modes apart AND keeps each legible on black.
 *     Five: Classic (the existing scheme, still the default), Ember (red
 *     for PLAY, as asked for, with SEQ amber and SONG violet so the three
 *     stay separated), Ice, Mono, and Access. Access is blue / yellow /
 *     white, which avoids the red-green axis that the common forms of
 *     colour vision deficiency affect, and separates the three by
 *     brightness as well as hue so they remain distinguishable even if hue
 *     does not read at all. Mono is brightness-only for the same reason,
 *     and doubles as the one that survives direct sunlight.
 *     What a theme does NOT touch: the sequencer's step colours, the level
 *     bars, cursor white. Those carry meaning — beat position, accent,
 *     playhead — rather than decoration, and took six versions to balance.
 *     Changing theme forces a full redraw, since uiColor is read all over
 *     and the partial-redraw paths would otherwise bring the new accent in
 *     piecemeal.
 *     v0.9937 moved themes into a DISPLAY category alongside a new
 *     brightness control, and gave the theme its own picker.
 *     As a cycling value in a list that had grown long, Theme was hard to
 *     tell apart from the rows that open a screen. It is a list now, with
 *     each theme's three mode accents drawn as swatches beside its name —
 *     so the colours can be compared before being applied. That also fixes
 *     Ice and Access looking alike: cycling only ever let you compare a
 *     theme against the one before it, and side by side they are plainly
 *     different. A '*' marks the theme actually in use so it stays
 *     findable once the cursor moves off it, and the swatches are outlined
 *     in grey rather than uiColor so a white or pale swatch cannot merge
 *     into a pale accent border.
 *     Ember's red went from vermilion to crimson — dropping green to near
 *     zero is what makes a red read as deep rather than orange-ish. Ice
 *     moved teal-ward to put more distance between it and Access's blue.
 *     Brightness stops at 32/255 rather than 0: the display is the only
 *     way to see what the synth is doing, and a setting that can black it
 *     out completely is one someone will hit by accident and then be
 *     unable to see well enough to undo.
 *     v0.9938 fixes two things from that version.
 *     The build broke on the new SettingsCategory enumerator. It was
 *     called DISPLAY, and M5Unified defines Display as a macro — so it was
 *     substituted before the compiler saw the enum, which then failed to
 *     parse. The error is reported on the enum line itself, which makes it
 *     look like an ordinary syntax mistake rather than a name collision.
 *     Renamed to SCREEN. (Same family as the PS/PAD_SIZE collision already
 *     noted in this file: short, generic identifiers are the ones that
 *     collide with platform headers.)
 *     Changing theme also left the tab bar on the old accent. The tab bar
 *     is only painted on a FULL redraw, and the settings and category
 *     screens compute their own full-redraw condition — the uiThemeDirty
 *     latch was only being consulted by the PLAY/SEQ path. Both screens
 *     now include it, and whichever repaints first clears it.
 *     v0.9939 fixes the ordering that broke the build. The theme picker's
 *     state and the Display menu's items had been written next to the
 *     other menu items, but updateThemePicker() runs with the other picker
 *     handlers far earlier in the file, and getCategoryItems() needs the
 *     item array before that point. Single-translation-unit C++ means
 *     definition order IS declaration order, so both had to move up: the
 *     picker state now sits with the theme it belongs to, and the theme
 *     setters plus displayMenuItems sit immediately above
 *     getCategoryItems. This is the same trap the file has hit before —
 *     new code lands where it reads best rather than where the compiler
 *     needs it.
 *     v0.994 gives the SETTING list two columns and drops the Portamento
 *     ON/OFF row.
 *     Eight entries in one column walked the eye down the screen for no
 *     reason; VCO and VCA already use two. The split is at 6 rather than
 *     down the middle, because the name field is 8 characters and
 *     "Portamento" and "Play Style" are the only entries that fill or
 *     exceed it — the right column starts at x=123 and must fit name plus
 *     value inside 117px, so the long names stay left where they have room
 *     to overrun. Arp and Display, both short, go right.
 *     Portamento's ON/OFF row is gone: it toggles from a performance key
 *     already, exactly as the IMU axes do, and a menu row duplicating a
 *     shortcut is one more thing to scroll past. Speed and Reset stay,
 *     since neither has a key. While removing it, the key handler turned
 *     out to be repeating portaToggle()'s body rather than calling it —
 *     with the menu row gone it is the only caller, so it calls the helper
 *     now. Two copies of the same two lines is how they drift apart.
 *     v0.9941 adds Analog Drift — the deliberate instability of an old
 *     analog synth. Pitch wanders a few cents, the filter breathes, the
 *     level creeps, all independently: if they moved together it would
 *     read as one tremolo rather than as circuitry. It needed no new
 *     machinery, because base values and effective values are already
 *     separate throughout this file — drift is just one more offset, and
 *     it resolves once per buffer, so the per-sample cost is zero.
 *     The menus keep showing the base value. That was the request and it
 *     is also what a real synth does: the knob does not move, the sound
 *     does.
 *     Ranges are not uniform. Pitch is held to about nine cents at full
 *     amount — past that it stops sounding like an old synth and starts
 *     sounding out of tune. Cutoff takes far more before it reads as
 *     wrong, and level the least, amplitude wobble being the most
 *     obviously artificial of the three. Switching it off eases back to
 *     neutral rather than snapping, so it cannot click mid-note.
 *     It lives on the Pro Style page with Scale, since deliberately losing
 *     stability is the opposite of what EZ Style is for, and a warning
 *     appears below the list while it is on — "Drift ON" alone does not
 *     tell you the tuning is about to wander, and this is exactly the
 *     setting someone enables, forgets, and later reports as a tuning bug.
 *     The warning shows only when it is on; a permanent caution line would
 *     become part of the furniture.
 *     v0.9942 made that drift actually audible. At full amount the pitch
 *     wander was barely there, and simulating the walk showed why: it
 *     reached 0.86 of full scale at its peak but only 0.29 RMS, so the
 *     typical detune was under 3 cents while the audible moments were rare
 *     enough to look like nothing was happening. The cause was the pair of
 *     walk constants — a new target was picked (3% per buffer) long before
 *     the previous one had been travelled to (0.010 per buffer), so the
 *     value spent its life crawling around the middle. Targets are chosen
 *     less often and travelled toward faster now, which lifts RMS to 0.43,
 *     and the ceilings roughly doubled on top of that. The time constant
 *     is still about a second, so it stays a wander rather than a wobble.
 *     v0.9943 fixed IMU Shape travelling in only one direction. Same fault
 *     as OSC_MIX in v0.9935: with the axis's own bipolar setting off the
 *     caller takes fabsf() of the tilt, so both directions produced the
 *     same positive offset. Shape's neutral is the MIDDLE of its range
 *     rather than an end — on Square, 0.5 is the symmetrical square and
 *     the two directions widen or narrow the pulse — so it is forced
 *     bipolar now, along with oscillator 2's Shape. Harder to spot than
 *     the OSC_MIX case because the sound still changed; it just never went
 *     the other way. The readout also showed the raw offset, a signed
 *     number with no obvious relation to what you hear, and shows the
 *     effective value now like the other targets do.
 *     v0.9944 widened Shape's IMU range from +-0.4 to +-0.5, so that from
 *     the natural centre of 50% a full tilt reaches 0% and 100% exactly.
 *     Worth being explicit about what remains by design: the offset is
 *     relative to the VCO's Shape setting, exactly as every other IMU
 *     target is relative to its own base. With Shape parked at 0% there is
 *     no room below it, so only one direction does anything and the
 *     readout sits at 0 at rest — which looks like the control is broken
 *     but is the base value showing through. Shape at 50% is what makes
 *     both directions live.
 *     v0.995 adds patch morphing. Ten slots, reached with Shift+1..0,
 *     crossfade the current sound into a stored patch rather than
 *     switching to it — the point being to change sound WHILE playing,
 *     with an arpeggio latched or a pattern running, so the change is part
 *     of the performance instead of a pause in it.
 *     Slots are assigned on their own screen under SETTING > Patch >
 *     Morph, not by extending the patch bank: the bank is for saving and
 *     loading, this is a separate idea, and keeping it separate makes
 *     "which patches are held in RAM" an explicit list rather than
 *     something implied by the bank's contents. Empty slots are allowed
 *     and do nothing when pressed.
 *     Each slot holds a full snapshot, built once at boot and again when
 *     an assignment changes. Nothing reads the card while morphing —
 *     reads block exactly as writes do, and that has interfered with the
 *     audio twice already here (v0.9913, v0.9926).
 *     Two things are deliberately not interpolated. Discrete state —
 *     waveforms, the Morph chain, filter type, octave and semitone
 *     offsets — switches once at the START, since there is no halfway
 *     between a saw and a square and a chain changing slot by slot
 *     mid-morph would sound like a fault. And morphFrom is captured from
 *     the LIVE sound at the start of every morph rather than being the
 *     previously selected slot, so pressing a new slot part-way through
 *     continues from wherever the sound actually is instead of jumping
 *     back. That is what makes rapid slot changes playable, which is the
 *     whole point.
 *     Morph time is on the same screen, 0 to 10s; 0 switches instantly.
 *     The morph advances from loop(), which runs far faster than the ear
 *     needs and leaves audioTask untouched.
 *     Shift+1..0 arrives as '!' through ')' — the keyboard reports the
 *     SHIFTED character, the same lesson as Shift+A arriving as 'A' in
 *     v0.9922. Both that and digit-with-shift are accepted. Plain digits
 *     remain note keys, and the trigger is ignored while a text field is
 *     open, where a shifted digit is a character being typed.
 *     v0.9951 fixes the build breaks v0.995 shipped with. Three separate
 *     use-before-declare/scope issues, all the same root cause: new code
 *     was written next to what it conceptually belonged with rather than
 *     where the compiler needed it.
 *     morphSlotPatch[] and the rest of the slot-screen state were placed
 *     ABOVE the NUM_MORPH_SLOTS constant that sizes the array, so nothing
 *     using them — including saveSettingsToFile() and parseSettingLine(),
 *     far below — could see the declaration. Moved to just after
 *     NUM_MORPH_SLOTS.
 *     updateMorphSlotScreen() called menuKeyFire() and reused the menu's
 *     shared hold-timer globals, but both live with the rest of the picker
 *     machinery much further down the file, past this screen's handler.
 *     Given its own morphIncHeldMs/morphDecHeldMs pair instead of sharing,
 *     and a forward declaration for menuKeyFire() itself.
 *     The Shift+1..0 block had been spliced into the middle of the
 *     Shift+H if/else chain, which left a dangling else with no if. It is
 *     now its own standalone statement after that chain ends, which is
 *     what it always should have been — it does not participate in that
 *     chain's logic at all.
 *     v0.9952 fixes patches disappearing from Load/Save after boot. The
 *     boot sequence had gained an unnecessary scanPatches() call ahead of
 *     morphLoadAllSlots() — added reflexively, without checking that
 *     morphLoadAllSlots() needs it. It does not: it reads each slot's
 *     saved patch NAME and checks SD.exists() on the file directly, never
 *     touching patchNames[]. That extra scan opened the Patch folder once
 *     at boot and closed it, then the Load/Save screen opened it again —
 *     and the ESP32 SD library does not always reset a directory handle's
 *     internal state cleanly on a second open of the same path, so the
 *     second scan came back empty. Removed. Nothing else in this codebase
 *     opened that folder twice per session, which is why the fault was
 *     new here rather than pre-existing.
 *     v0.9953 chases the same fault further: after v0.9952 the card was
 *     not mounting AT ALL, so nothing loaded — theme, patches, every
 *     setting back to default — and saves failed with "File system is not
 *     mounted".
 *     Two changes, and the diagnostics to tell which mattered. The morph
 *     snapshots moved from internal DRAM to PSRAM. Internal DRAM here is
 *     already very heavily committed (the reverb network is ~50KB of
 *     float, the delay line ~70KB) and SD.begin() allocates its buffers
 *     from what is left, so several KB of new static array does not merely
 *     use memory — it can push the mount over the edge. PSRAM is where
 *     this data belonged anyway: it is touched when a slot is assigned or
 *     a morph starts, never per sample, so the slower access costs
 *     nothing. If the allocation fails, morphing disables itself rather
 *     than the firmware misbehaving.
 *     And initSDCard() now retries, stepping 25 -> 16 -> 4MHz. One attempt
 *     at 25MHz was the whole of it before, and cards vary in how quickly
 *     they will negotiate right after power-on. The retry costs a few
 *     milliseconds in the bad case and nothing in the good one.
 *     Boot now also prints free heap, largest allocatable block and free
 *     PSRAM either side of the mount, because "every setting is back to
 *     default" is a symptom several causes share and guessing between them
 *     has already cost a version.
 *     Recorded from that diagnostic run, because it changes what the fix
 *     WAS: this hardware reports "PSRAM chip not found" at boot and
 *     psramInit() fails, despite BOARD_HAS_PSRAM being set. So the move of
 *     the morph snapshots to PSRAM did nothing — the fallback to ordinary
 *     calloc is what runs, which is no better than the static array it
 *     replaced. What actually fixed the mount was the retry with the
 *     stepped-down clock. Worth knowing before anything else here is ever
 *     designed around PSRAM being available.
 *     v0.9954 fixes two things found once morphing was usable.
 *     The slot screen had nothing to assign on a fresh boot: the list of
 *     patches comes from patchNames[], which is only filled on entering
 *     the Load/Save browser, so the morph screen had to be visited AFTER
 *     Load at least once. It scans on entry now, the same as that browser
 *     does. This is not the boot-time scan removed in v0.9952 — that one
 *     ran before any other open of the folder and left the handle in a
 *     state the next scan came back empty from; scanning when a screen is
 *     entered is the pattern that has always worked here.
 *     And morphing did not carry the IMU mapping. A patch stores which
 *     parameter each axis drives, so a morph that leaves that behind
 *     changes the sound while tilt keeps doing whatever the previous patch
 *     said — the opposite of loading that patch. The mapping is discrete,
 *     so it switches at the start with the waveforms, and the outgoing
 *     target's offset is cleared first exactly as the IMU picker does, or
 *     whatever it was contributing stays frozen into the sound.
 *     v0.9955 fixes settings coming back wrong after every reboot — Play
 *     Style stuck on Pro/Hirajoshi, Drift stuck ON at 100% — and patches
 *     changing the output volume.
 *     Both come from morphLoadSlot(). To snapshot a slot it loads that
 *     patch over the live state and then puts the live state back, but it
 *     was only restoring params, the filter and the ADSR. A patch file
 *     also carries Play Style, Scale, Drift, key volume, the IMU mapping,
 *     the LFO, and the arp and portamento settings — all of which were
 *     left holding whatever the LAST slot's patch said. The next settings
 *     save then wrote that to settings.json, making a morph slot's patch
 *     permanently the synth's startup state. Backed up and restored in
 *     full now, via an explicit struct rather than a growing list of
 *     assignments, so a future key added to the parser is one place to
 *     remember rather than several.
 *     Output volume is now skipped entirely when loading a patch. Level is
 *     a property of the session rather than of the sound: a patch that
 *     jumps the volume is startling, and one storing a low level looks
 *     like it failed to load. settings.json still restores it, so the
 *     level the synth was left at comes back.
 *     v0.9956 extends that to Play Style, Scale and Drift, which a patch
 *     load was still applying: those change how the instrument PLAYS
 *     rather than how it sounds, and a patch silently flipping the
 *     keyboard into Pro Style on a different scale, or switching the
 *     tuning instability on, is not what loading a sound should do.
 *     settings.json still carries them.
 *     It also fixes the synth going completely silent during a morph,
 *     with audio still being generated and the screen still animating.
 *     v0.9954 cleared an axis's offset only when the morph CHANGED that
 *     axis's target. If the target stayed the same but the incoming patch
 *     had the axis disabled, whatever offset it had been contributing was
 *     frozen with nothing left to update it — and with the axis on Volume
 *     and the device tilted, that freezes the output at zero until a
 *     reboot. Both axes are now cleared unconditionally before repointing,
 *     any hold is released, and an axis arriving disabled is cleared again
 *     afterwards. Clearing unconditionally costs nothing, since
 *     resetParamToDefault() only zeroes an offset and live tilt is about
 *     to drive the axis again anyway.
 *     v0.9957 removes the PSRAM flags from platformio.ini and the code
 *     written around them. There is no PSRAM on this hardware and never
 *     was: both boards use an ESP32-S3FN8 (StampS3 / StampS3A), where
 *     "FN8" means 8MB flash and no psram — a part with it reads R8.
 *     -DBOARD_HAS_PSRAM made the SDK hunt for a chip that is not fitted
 *     and fail at boot with "PSRAM chip not found", which reads like a
 *     hardware fault and is not one; the owner of this device reasonably
 *     took it for a symptom of a power problem it already has.
 *     -mfix-esp32-psram-cache-issue went with it and was doubly
 *     irrelevant, being an original-ESP32 erratum workaround that does not
 *     apply to the S3. ps_calloc() for the morph slots is now plain
 *     calloc(), which is what it was resolving to anyway; the heap
 *     allocation stays, since it lets morphing disable itself cleanly if
 *     the memory is not there rather than the array existing
 *     unconditionally and squeezing the card mount. The boot diagnostic
 *     drops its PSRAM figure, and a comment claiming the canvas lived in
 *     PSRAM was corrected.
 *     The numbers that DO matter, measured: free heap ~59KB before
 *     SD.begin() and ~31KB after, so mounting the card costs about 27KB on
 *     its own. That ~31KB is the real ceiling on any future feature
 *     wanting a large buffer — MIDI stacks especially — and running out
 *     presents as the card silently failing to mount, which in turn looks
 *     like every setting having reset to default.
 *     v0.996 begins MIDI, and begins it away from the risky part. This
 *     version adds only the MESSAGE layer: bytes in, note events out. It
 *     is deliberately not attached to any transport, because USB MIDI
 *     needs a build-time change to the USB mode — and with it the serial
 *     log this project debugs by — so getting the message handling right
 *     first keeps that change small and keeps this code useful whichever
 *     transport ends up carrying it, USB or the DIN sockets on a MIDI
 *     unit. Nothing in this version alters existing behaviour: the parser
 *     simply has no source feeding it yet.
 *     What is handled: Note On, Note Off, and Pitch Bend, with running
 *     status, which real keyboards rely on. Velocity 0 is treated as Note
 *     Off — the convention nearly every keyboard uses, and forgetting it
 *     leaves notes stuck on forever. Notes go on a small stack rather than
 *     a single slot, so releasing one key while another is still held
 *     falls back to that note instead of cutting off; overlapping notes
 *     are constant on a real keyboard. Last-note priority, matching how
 *     the built-in keys already behave. Velocity lands on
 *     seqVelocityMult, the same scaling the sequencer's per-step velocity
 *     uses, and pitch bend maps onto the existing bend range, so a wheel
 *     and the local bend keys reach the same place — both reuse controls
 *     that already exist rather than adding parallel ones.
 *     Omni for now: every channel is accepted. A channel setting can come
 *     later; Omni is what makes "plug it in and it plays" true, which is
 *     the point of a first version.
 *     v0.9961 connects that layer to USB. This is the part that had to be
 *     kept separate, because ARDUINO_USB_MODE is a BUILD-TIME choice with
 *     no runtime switch: mode 1 is the hardware USB Serial/JTAG that
 *     carries the log and flashing, mode 0 is TinyUSB. Mode 0 is used now,
 *     which lets CDC and MIDI be presented together as one composite
 *     device — so the serial log survives instead of being traded away for
 *     MIDI, which matters given how much of this project has been debugged
 *     from it.
 *     CPS_USB_MIDI in platformio.ini is the single switch that undoes all
 *     of it: comment it out, set ARDUINO_USB_MODE back to 1, and the
 *     firmware builds exactly as v0.996 did. TinyUSB on ESP32-S3 under
 *     PlatformIO is known to be fussy, and the message layer is
 *     transport-independent, so a failure here costs the transport and
 *     nothing else — a MIDI unit's DIN socket feeds the same parser.
 *     usbMidi.begin() runs before Serial.begin(), since the two interfaces
 *     enumerate together and MIDI has to exist before the host is told
 *     what this board is. It names itself "C.P.S." rather than appearing
 *     as a generic descriptor in a host's device list.
 *     Polling happens in loop(), not audioTask: MIDI timing is in
 *     milliseconds and the audio path should not take on work it does not
 *     need. Each call drains at most 256 bytes so a controller sweeping a
 *     wheel cannot stall the UI.
 *     Expect the port to disappear on reset in this mode; BOOT may need
 *     holding to flash. That is TinyUSB behaving normally, not a fault.
 *     v0.9962 corrects that platformio.ini, which would not resolve: the
 *     TinyUSB library was asked for as ^2.4.3, a version that does not
 *     exist. Pinned to 3.3.4 exactly, and deliberately without a caret —
 *     versions above it are reported to fail to link on ESP32-S3 under
 *     PlatformIO, and a range would quietly pull one in on some later
 *     dependency update.
 *     Three things were also missing from the known-good combination.
 *     build_unflags = -DARDUINO_USB_MODE=1, because the board definition
 *     already defines it and adding =0 alongside leaves both present
 *     rather than replacing it. -DUSE_TINYUSB=1, which is what the library
 *     itself gates on. And lib_archive = no, so TinyUSB's objects are
 *     linked directly instead of from an archive, where the USB
 *     descriptors get dropped as apparently-unreferenced.
 *     v0.9963 turns USB MIDI back off. It enumerated — the host saw a
 *     TinyUSB device rather than the usual USB Serial/JTAG one, so the
 *     descriptors and the composite device were right — but the board
 *     would not stay up.
 *     The measurement explains it. Free heap before SD.begin fell from
 *     ~59KB to ~40KB, so the TinyUSB stack costs about 19KB of internal
 *     DRAM. SD.begin needs roughly 27KB. What was left was not enough:
 *     the card stopped mounting, and the board went into a reset loop on
 *     top of that. This is exactly the ceiling flagged before the attempt
 *     started, now with a number attached — there is not enough internal
 *     DRAM for the USB stack and the SD card together as things stand.
 *     Nothing is lost from the code: the MIDI message layer is
 *     transport-independent and is still compiled in, waiting for a
 *     source. Re-enabling is four commented lines in platformio.ini.
 *     What would make it fit is freeing DRAM, and the two candidates are
 *     both audio buffers: the delay line is 35280 int16 (~70KB) for its
 *     800ms maximum, so halving that maximum would free ~35KB outright,
 *     and the reverb is ~50KB of float that was deliberately not int16
 *     because the noise floor showed. Either is a real trade against a
 *     feature that already works, which is why neither was made
 *     unilaterally.
 *     v0.9964 fixes Delay often producing no audible echo at all when it
 *     was the only effect on — and working once Reverb was switched on
 *     alongside it, which is the detail that gave it away.
 *     The FX tail ends when two consecutive buffers come back quiet. But
 *     immediately after a note stops, the delay line's READ position is
 *     still inside the silent stretch that precedes the first echo, so the
 *     output IS genuinely quiet: the test fired after about 46ms and
 *     clearFxBuffers() wiped the line before the echo ever came round.
 *     Reverb masked it by keeping the level above the threshold long
 *     enough for the echo to arrive.
 *     The tail now requires enough quiet buffers to cover a full delay
 *     time before it believes it is finished, so the first echo is always
 *     given the chance to appear. The 8-second cap still bounds it.
 *     v0.9965 trades delay length for USB MIDI, deliberately and
 *     reversibly. The maximum delay time goes from 800ms to 400ms, which
 *     shrinks its buffer from ~70KB to ~35KB of internal DRAM — comfortably
 *     more than the ~19KB the TinyUSB stack needs, which is what left too
 *     little for SD.begin's ~27KB and put the board in a reset loop in
 *     v0.9962. USB MIDI is switched back on.
 *     This is a real loss and not a free win: 400ms is short for anything
 *     you would call a long delay, and the owner said as much after
 *     listening to it. It is being tried to find out whether USB MIDI
 *     earns it. DELAY_MAX_MS is now a single constant that the buffer
 *     size, the menu limit, the settings clamp and the randomizer all
 *     derive from, so reversing this is one number rather than four
 *     places to remember.
 *     v0.9966 reverses that trade and shelves USB MIDI. DELAY_MAX_MS is
 *     back to 800ms and the TinyUSB build flags are off again.
 *     The delay cut DID solve what it was aimed at: with ~35KB freed the
 *     card mounted, every patch loaded, the IMU came up and audio started
 *     — all of which had failed in v0.9962. So the memory ceiling was read
 *     correctly and the arithmetic was right. But the board still reset in
 *     a loop, now from somewhere AFTER audio was running, which makes it a
 *     second and unrelated fault rather than the same one. TinyUSB
 *     coexisting with the core's CDC on this core version is the obvious
 *     suspect and would be its own investigation.
 *     Stopping here is the owner's call, made in advance: try it once with
 *     the delay shortened, and if it does not work, drop USB MIDI rather
 *     than keep paying for it. Nothing is wasted — the MIDI message layer
 *     stays compiled in and transport-independent, so a MIDI unit's DIN
 *     socket feeds the same parser with no USB stack, no RAM cost, and
 *     none of this.
 *     v0.997 prepares patches to be shared between people, after a user
 *     asked about posting them for others to drop into /CPS/Patch. That
 *     already works mechanically — scanPatches() lists whatever .json it
 *     finds, and since v0.9932 a missing key means default, so old files
 *     load safely. The problem is what a patch CONTAINS.
 *     Save writes the whole synth state to settings.json and to patch
 *     files through one function, so every patch carried the sequencer's
 *     sixteen steps, the morph-slot assignments, the UI theme and the
 *     screen brightness. Auditioning someone else's patch therefore
 *     replaced the pattern you were working on, filled your morph slots
 *     with names of patches you do not have, and repainted your synth.
 *     All of those are now ignored on a patch load, joining volume, Play
 *     Style, Scale and Drift. The rule that has emerged: a patch carries
 *     the SOUND; anything describing the session, the device or a
 *     separate document stays out. Patterns already have their own bank.
 *     Patch files also gained a cps_format stamp, because until now a file
 *     said nothing about what wrote it — worth fixing BEFORE files start
 *     circulating, since a format added later cannot appear in patches
 *     already shared. A reader finding a higher number logs that it is
 *     ignoring settings it does not understand, rather than refusing to
 *     load: unknown keys are skipped anyway and known ones still mean what
 *     they say, so refusing would be the worse failure. It is a format
 *     number rather than the firmware version, changing only when the
 *     meaning of the keys does.
 *     MAX_PATCHES went from 32 to 64. Collecting other people's files
 *     passes 32 easily, and over the limit scanPatches() silently stopped
 *     adding — the extras just did not appear, with nothing to say why.
 *     v0.9971 stops WRITING into patches what v0.997 stopped reading from
 *     them. Ignoring a key on load but still saving it left morph-slot
 *     assignments, the UI theme, brightness, the sequencer's sixteen steps
 *     and the session's volume and playing style sitting inside every
 *     patch file — about 2KB of a file people are about to share, and an
 *     obvious question for anyone who opens one: why are someone else's
 *     morph slots in a sound? If a reader ignores it, writing it was
 *     pointless.
 *     savingPatch mirrors loadingPatch on the write side, set by a
 *     savePatchToFile() wrapper around the shared writer. Two separate
 *     flags rather than one "patch mode", since loading and saving happen
 *     at different moments and conflating them is a bug waiting to happen.
 *     Existing patches are unaffected: the extra keys they already contain
 *     are ignored on load, and a key that is now absent means default —
 *     the same property that has made every earlier format change safe.
 *     v0.998 adds the serial MIDI transport, for the M5Stack Unit MIDI
 *     arriving tomorrow. The unit is a plain UART bridge to a pair of DIN
 *     sockets, so this costs one HardwareSerial and nothing else: no USB
 *     stack, none of the ~19KB of DRAM that made USB MIDI fail in v0.9962,
 *     and no reset loop. It starts unconditionally, since nothing happens
 *     when no unit is plugged in.
 *     Two details from M5's documentation are worth having written down.
 *     The DIP switch must be in BYPASS — M5 state explicitly that only
 *     then does the controller's RX pin receive the INPUT socket's signal;
 *     in Separate that pin does nothing, which from the outside is
 *     indistinguishable from a wrong pin or a bad cable. And the baud rate
 *     is 31250, the MIDI 1.0 standard, despite M5's spec table saying
 *     31520; that is a typo of theirs, and an unhelpful one, because the
 *     0.9% error it implies is within what a UART tolerates — the wrong
 *     figure could appear to work.
 *     The pins default to Grove G1/G2 as RX/TX and are #defines, because
 *     which of the pair is which is the one thing that cannot be confirmed
 *     from documentation alone. If nothing arrives, swapping them is the
 *     first thing to try.
 *     Which is why there is a byte counter: [MIDI] rx prints once a second
 *     while data flows and [MIDI] idle when it stops. Whether BYTES are
 *     arriving separates a wiring, pin or DIP-switch problem from a
 *     parsing one, and from the outside those look identical — silence.
 *     The pins are overridable from platformio.ini for a reason beyond
 *     that: there is only one Grove port, and the ToF unit for the
 *     theremin idea wants it as well. The EXT 2.54-14P header carries its
 *     own UART on G13/G15, so moving MIDI there lets both live at once.
 *     v0.9981: nothing arrived on the first hardware test, so the RX pin
 *     now auto-swaps. Which of the Grove pair is RX cannot be settled from
 *     documentation, and getting it wrong presents identically to every
 *     other failure here — silence. Rather than edit a constant, rebuild
 *     and reflash to test the other option, it alternates between the two
 *     every three seconds until a byte actually arrives, then locks onto
 *     whichever pin delivered it. Free once data is flowing, and it turns
 *     a guess-and-reflash cycle into waiting a few seconds.
 *     The startup checklist also now names the Grove 5V DIRECTION switch.
 *     Cardputer ADV can either power a unit from that port or be powered
 *     through it, and set the wrong way the Unit MIDI receives no power at
 *     all — silence again, from a switch on the case rather than anything
 *     in software.
 *     v0.9982 adds a raw byte dump, because the second hardware test
 *     changed the question. Bytes arrived — four a second, on RX=1,
 *     whether or not a key was pressed. That is Active Sensing and nothing
 *     else: 0xFE roughly every 250ms, which is what a Roland sends to say
 *     it is still connected. Which means the cable, the unit, the DIP
 *     switch, the pin and the baud rate are all now PROVEN correct, and
 *     the notes are simply not being transmitted.
 *     So the dump filters out 0xFE and prints everything else in hex. The
 *     log then stays silent until something real arrives, and pressing one
 *     key either prints bytes or prints nothing — which separates "the
 *     keyboard is not sending notes" from "the parser is not understanding
 *     them" without any guessing. This is temporary and comes out once the
 *     answer is known; it would flood the log under real playing.
 *     v0.9983: the dump did its job and is gone. With the PCR's own
 *     setting corrected, notes arrived and were textbook — 90 4F 52 then
 *     80 4F 10, Note On and Note Off correctly paired. So the parser was
 *     never the problem. What was silencing them sat in the main loop.
 *     resolveFreqFromKeys() returns 0 when no LOCAL key is down, and its
 *     result was assigned to currentFreq unconditionally, so any
 *     keyChanged event wiped out a MIDI note instantly. keyChanged fires
 *     for octave, volume, Tab and every other key too, which is exactly
 *     why the symptom was notes cutting off or not sounding seemingly at
 *     random rather than never working at all. With the arpeggiator on,
 *     updateArpTiming() did the same thing every loop instead of only on
 *     key events.
 *     Both now leave currentFreq alone while midiNoteActive. Local keys
 *     still win whenever one is actually held: whoever pressed last is
 *     playing, which is the last-note priority both inputs already use on
 *     their own.
 *     v0.9984 connects MIDI to the arpeggiator, which had been ignoring it
 *     entirely — updateArpHeldNotes() read the built-in keyboard and
 *     nothing else. This is the combination most worth having: the local
 *     keyboard manages three keys at once and an arpeggio wants more, so
 *     an arpeggiator that only listens to the local keys is limited by
 *     exactly the thing MIDI IN was added to escape.
 *     Held MIDI notes now join the chord alongside local keys. Note there
 *     is no octave/transpose multiplier on them: a MIDI note number is
 *     already an absolute pitch, and applying the local shift would move
 *     the keyboard out from under the player.
 *     Latch needed its own MIDI list. Latch means "keep playing what I
 *     pressed after I let go", so it cannot use midiHeldNotes — that
 *     empties as fingers lift, which is the very thing Latch exists to
 *     survive. Pressing a latched note again removes it, the same toggle
 *     the local keys use, and clearing the latch clears both halves.
 *     Also: a local key taking over from a sounding MIDI note now
 *     retriggers the envelope. The test was currentFreq==0, so pressing a
 *     local key while MIDI held a note changed the pitch but left the
 *     envelope mid-note — which reads as the built-in keyboard being
 *     ignored, and was reported as exactly that.
 *     v0.9985 fixes the build: the arpeggiator sits above the MIDI code,
 *     so it could not see the note lists or midiNoteToHz(). Forward
 *     declared rather than moved — they belong with MIDI and the arp is
 *     the borrower, and this file has gone wrong more than once by
 *     relocating code to satisfy the compiler instead of declaring it. A
 *     static_assert ties the forward-declared array bound to the real one,
 *     so changing one without the other fails at compile time rather than
 *     as a confusing linker error.
 *     v0.9986: feeding MIDI into the arp's chord was necessary but not
 *     sufficient, because of WHERE that chord is rebuilt. It happens
 *     inside the keyChanged branch of the main loop — only when a LOCAL
 *     key event occurs. Which explains all three reported symptoms at
 *     once: MIDI alone never started an arpeggio, adding any local
 *     keypress swept the already-held MIDI notes in, and letting go left
 *     those notes arpeggiating as though latched, because nothing ever
 *     asked for another rebuild.
 *     MIDI now sets a dirty flag on every note change and the rebuild runs
 *     from the poll, outside that gate, guarded by the same conditions so
 *     MIDI cannot start an arpeggio on a screen where local keys could
 *     not.
 *     Also: releasing the last MIDI note no longer silences a note the
 *     local keyboard is still holding — it zeroed currentFreq
 *     unconditionally, which was reported as the built-in key's sound
 *     vanishing when the MIDI hand lifted.
 *     Not a bug, for the record: with a chord held, the last-pressed note
 *     sounds until every key is released. The built-in keyboard has always
 *     behaved that way, this is a monophonic synth, and MIDI now matches
 *     it.
 *     v0.99861 adds Program Change, the sustain pedal and the modulation
 *     wheel — the three that cost least and are expected most.
 *     Program Change selects a morph slot, so an external keyboard or a
 *     host changes sound and it MORPHS rather than switching, which is
 *     what this synth does. Program 0 is slot 1, lining up with the
 *     Shift+1..0 keys.
 *     The sustain pedal defers note-offs into a pending list and releases
 *     them together when it lifts, which is what a piano pedal does. The
 *     check happens before the note stack is touched, or the note would
 *     already be gone by the time the pedal came up.
 *     The modulation wheel drives vibrato depth via CC1, the near-
 *     universal default, so a wheel does something sensible with no setup.
 *     It writes the vibrato OFFSET rather than the base, so the menu's own
 *     setting stays put and the wheel adds to it — the same arrangement
 *     the IMU targets use, and the same reuse-don't-duplicate approach as
 *     velocity and pitch bend.
 *     All Notes Off and All Sound Off are honoured too: that is what a
 *     host sends when someone hits panic, and ignoring it leaves notes
 *     stuck with no way out short of a reboot.
 *     One parser detail worth stating: Program Change carries ONE data
 *     byte where everything else here carries two. Getting that wrong
 *     would not merely lose the message — the parser would sit waiting for
 *     a byte that never arrives and swallow whatever came next.
 *     v0.99862 fixes the build: the sustain arrays were written up with
 *     the new CC constants, which sit ABOVE the MIDI_NOTE_STACK constant
 *     that sizes them. Moved down beside the note stack itself. Same trap
 *     as v0.9985, two versions apart — in this file, a declaration's
 *     natural home and its legal home are often not the same place.
 *     v0.99863 makes the sustain pedal hold notes played on the BUILT-IN
 *     keyboard too. Previously it only deferred MIDI note-offs, which was
 *     not a design decision so much as a consequence of where the code
 *     sat: a pedal is a performance control and should hold whatever is
 *     being played, not only what arrived over a cable.
 *     It drives the existing noteHeld/heldFreq mechanism — the H key's
 *     Hold — rather than a parallel one, so both routes share the same
 *     state and the H:ON readout stays truthful. A flag records whether
 *     the pedal is the reason Hold is on, because lifting it must not
 *     cancel a Hold the player set with the H key themselves; anything
 *     else that clears Hold clears that flag too.
 *     v0.99871 sends the IMU out as MIDI CC — the first thing this
 *     firmware transmits rather than receives. Tilt the synth and an
 *     external instrument responds, which points the feature people
 *     actually noticed at release outward instead of inward.
 *     Throttling is the whole engineering problem. A MIDI cable carries
 *     about 3125 bytes a second and a CC message is three of them, so
 *     sending on every IMU update would be roughly a thousand messages a
 *     second per axis — enough to swamp the link and delay the notes
 *     sharing it. Values go out only when the 7-bit value CHANGES, and
 *     never closer together than 15ms. Change alone would not be enough:
 *     a hand shaking gently across a boundary would transmit
 *     continuously.
 *     The values sent are the same ones the synth is using, so what leaves
 *     the wire matches what is heard, and transmission is gated on the
 *     axis being live — a disabled or held axis stops sending rather than
 *     freezing a receiver at its last value.
 *     Defaults are CC1 on X and CC74 on Y: modulation is what a receiver
 *     is most likely to have mapped, and 74 is filter cutoff by
 *     convention. Both are settable across the full 0-127, with the
 *     near-universal names shown beside the number so the common choices
 *     are recognisable rather than bare figures.
 *     This lives in a new MIDI category rather than four more rows on the
 *     IMU page, which is already eleven items over two columns — and it is
 *     where CC receive and clock sync will go. The channel is stored 0-15
 *     and displayed 1-16, as every piece of hardware labels it.
 *     v0.99872 sends notes as well, which turns the Unit MIDI's own
 *     SAM2695 chip into a second voice. Reading M5's mode description
 *     closely is what makes this the right move: in Bypass the
 *     controller's TX reaches that chip WHILE the RX pin still receives
 *     the INPUT socket, so an external keyboard can play C.P.S. and a GM
 *     instrument together with no second piece of hardware anywhere. In
 *     Separate the same bytes leave the OUTPUT socket for external gear
 *     instead, but MIDI in stops working — the unit genuinely cannot do
 *     both, which is worth knowing before planning around it.
 *     Notes are derived from what the synth is actually SOUNDING rather
 *     than from the key handlers. Local keys, MIDI in, the arpeggiator and
 *     the sequencer all end up setting currentFreq, so watching that one
 *     value covers every source at once instead of needing a hook in each
 *     — and the arpeggiator and sequencer come along for free.
 *     GM instruments are chosen by family rather than by numbered
 *     program: sixteen names on a menu row beat 128 numbers, and stepping
 *     picks the first program of each family. The program is sent when
 *     Note Out is switched on, so the chip is playing the chosen sound
 *     from the first note rather than whatever it defaulted to, and
 *     switching Note Out off releases any note still held — otherwise the
 *     receiver would sound it forever.
 *     v0.99873 fixes two things found once the send path was audible.
 *     Retargeting a CC left the old one still applied: a CC is a value a
 *     receiver LATCHES, not a momentary command, so moving X from Chorus
 *     to Reverb left the chorus at whatever depth it was last sent and the
 *     two stacked, with no way back except steering the CC to that number
 *     again and winding it down by hand. A 0 is now sent to the outgoing
 *     controller before switching, and to both when CC out is switched
 *     off. The last-sent value resets too, so the new controller gets an
 *     update immediately rather than waiting for the tilt to cross into a
 *     different 7-bit step.
 *     The second was patches losing their IMU enable state when morphed
 *     to. performPatchToneReset() never set imuXEnabled/imuYEnabled, and
 *     patch loads reset first and then parse — so a key the file does not
 *     contain means "default", and these two had no default: they simply
 *     kept whatever was live. Patches saved before v0.9921 carry no
 *     imu_x_en at all, so loading one inherited the previous patch's
 *     state and morphing through several could leave an axis off with
 *     nothing on screen explaining it. The report pinned it exactly — the
 *     two patches that DO carry the key loaded with the IMU on, the six
 *     older ones did not. Both flags now default to on in the reset,
 *     which is what the rest of the reset already does for every other
 *     parameter.
 *     v0.99874 fixes two more, one of them mine from the version before.
 *     Releasing a controller by sending 0 is right for an effect depth,
 *     where 0 means none. It is catastrophic for CC7 (Channel Volume) and
 *     CC11 (Expression), where 0 means SILENCE — and with nothing sending
 *     them again the receiver stays mute until power-cycled. Reported
 *     exactly so: moving off either killed the sound until reset. Each
 *     controller is now released to ITS idle value — full for the two
 *     volume controls, centre for pan, zero for the rest.
 *     And the LFO was missing from PatchSnapshot altogether, so morphing
 *     changed the tone while the previous patch's modulation carried on.
 *     An LFO is as much part of a sound as the filter. Wave and target are
 *     discrete and switch at the start with the waveforms — there is no
 *     halfway between a sine and a square, or between modulating pitch and
 *     modulating the filter — while rate and depth interpolate.
 *     v0.99875 sends pitch bend, which the note-out path needed rather
 *     than merely wanted. midiHzToNote() rounds to the nearest semitone,
 *     so every bend, glide, vibrato, detune and drift was thrown away on
 *     the MIDI side — and worse than lost: Analog Drift reaches +-22
 *     cents, so near a semitone boundary the note number flipped back and
 *     forth and the receiver retriggered repeatedly, a chattering with no
 *     relation to what was being played. Portamento did the same on every
 *     boundary it crossed.
 *     The note number is fixed at note-on now and everything after it
 *     goes out as bend, over the General MIDI default range of +-2
 *     semitones — which the SAM2695, and practically every receiver,
 *     assumes without being told. Beyond that the deviation cannot be
 *     expressed so the note is retriggered, which is right anyway: a glide
 *     of more than a whole tone is a new note musically. The bend is
 *     centred before each note-on, or it would inherit whatever the
 *     previous note was bent to, and it is throttled exactly like the CC
 *     path — a 5Hz vibrato would otherwise emit hundreds of messages a
 *     second.
 *     v0.99876: none of that reached the receiver, because the bend was
 *     computed from currentFreq — which is only the TARGET note.
 *     Everything that makes the pitch expressive is applied inside
 *     audioTask and nowhere else: portamento's glide, key bend, vibrato,
 *     detune, Analog Drift. So the deviation was always about zero and
 *     nothing was ever sent. The sounding pitch is published from the
 *     audio loop now (playF*pr, the value the oscillator actually uses)
 *     and both the bend and the retrigger test read it — testing one
 *     against currentFreq while bending from the other would let the two
 *     disagree about when a new note is due. Sampled once per buffer,
 *     since the MIDI side is throttled to 15ms and per-sample accuracy
 *     would only be discarded.
 *     v0.9988 receives CC: two assignable slots, each mapping an incoming
 *     controller number onto one of the existing IMU targets.
 *     Reusing ImuTarget instead of inventing a parallel list is the whole
 *     of the design. Every target already knows how to apply itself
 *     through applyImuValue(), already has a name for the menu, already
 *     has a sensible range, and already writes to an OFFSET so the patch's
 *     own setting remains the base. A separate CC-target enum would have
 *     meant duplicating all of that and then keeping the two in step
 *     forever. imuBipolarAuto() decides the mapping too, so an incoming CC
 *     behaves exactly like a tilt of the same target rather than needing
 *     rules of its own.
 *     The destination steps through IMU_PICKER_ORDER rather than the raw
 *     enum, so the choices appear in the grouping and order the IMU picker
 *     established and unassignable entries never come up. Changing a
 *     destination clears the outgoing target's offset first: leaving it
 *     would freeze that parameter at whatever the knob last sent, the same
 *     trap the morph IMU handover fell into.
 *     Assigned slots are checked BEFORE the fixed meanings of CC1 and
 *     CC64, so pointing a slot at either overrides the built-in behaviour
 *     rather than fighting it.
 *     Worth stating plainly: an external knob and the IMU write to the
 *     same offset, so aiming both at one target means whichever moved last
 *     wins. That is the honest result of two controls wired to one
 *     parameter, and it is why the assignments live in a menu rather than
 *     being hidden.
 *     v0.99881: overwriting a patch left the morph slots holding the OLD
 *     sound. Snapshots were built at boot and when an assignment changed,
 *     and nowhere else — so saving over a patch updated the file, Load
 *     read the new version, and morphing replayed what had been captured
 *     before the edit. The two disagreeing about the same patch name reads
 *     as corruption rather than as a missing refresh, which is what makes
 *     it worth fixing rather than documenting.
 *     Any slot pointing at that name is rebuilt after a successful save,
 *     matched by name because one patch can occupy several slots.
 *     The boot rebuild also logs each slot's cutoff and attack now. If a
 *     slot still reports pre-edit numbers after a REBOOT then the fault is
 *     in reading the file rather than in when the snapshot was taken —
 *     from the outside those two look the same, and this separates them.
 *     v0.99882 answers two things noticed while testing CC in.
 *     Turning an external knob changed the sound but nothing on the VCF
 *     page moved: both the Hz readout and the response curve were drawn
 *     from filterParams.cutoffHz, the knob position, while a CC (or a
 *     tilt) writes filterCutoffOffset. A control that works but shows
 *     nothing reads as a control that is not connected. Both now use
 *     effectiveCutoffHz(), which mirrors the audio path's own scaling
 *     exactly — positive offset pulls the cutoff down by up to 90%. The
 *     screen already redraws on a 100ms tick, so it follows.
 *     And the CC destinations stepped through thirty targets one key
 *     press at a time. They open the IMU picker now, which already
 *     presents those targets in named sections — the IMU page moved away
 *     from cycling for exactly this reason. The picker gained two more
 *     "axis" codes rather than a second picker being written: one list to
 *     keep in step instead of two, and the outgoing target's offset is
 *     cleared on assignment just as it is for an axis.
 *     v0.99883 finishes what the previous version half-did. Cutoff was
 *     switched to the effective value; resonance, the cutoff MARKER and
 *     the frequency label on the graph were all left reading the knob
 *     position. So a CC or a tilt moved the curve while the yellow marker
 *     stayed put — the page disagreeing with itself, which is worse than
 *     the original problem of nothing moving at all. All four now use the
 *     same effective values the audio path does.
 *     Worth noting the marker is not a reference line for the base
 *     setting: it marks where the filter IS, so it has to track.
 *     v0.9989 receives MIDI clock, and first puts the diagnostic logs
 *     behind build flags — which is why that came first rather than being
 *     tidying for its own sake. Clock arrives 24 times per BEAT, so the
 *     per-second byte counter would never fall silent again, and a log
 *     that always says something says nothing. CPS_LOG_AUDIO, CPS_LOG_MIDI
 *     and CPS_LOG_PATCH each default to on: these logs have found real
 *     faults (the SD mount failure, the stale morph snapshots, whether
 *     MIDI bytes were arriving at all) and deleting them would mean
 *     rewriting them after the next one.
 *     The clock itself measures the INTERVAL between clocks and derives a
 *     BPM, rather than counting to 24 and stepping. Counting would lock
 *     the resolution to a quarter note and drift whenever a clock was
 *     dropped; deriving a tempo means the existing timing code carries on
 *     unchanged, simply reading a number that now comes from outside.
 *     Nothing in the sequencer had to be touched.
 *     Averaged over one beat, because a single interval is far too noisy —
 *     serial jitter alone swings the reading by several BPM, and a tempo
 *     display flickering that much looks broken even when the timing is
 *     fine. Intervals outside a generous window are treated as a restart
 *     rather than a tempo.
 *     Start, Continue and Stop drive the sequencer transport, so pressing
 *     play on the master starts this too, which is the point of syncing.
 *     Realtime bytes are handled without touching midiStatus or
 *     midiDataIdx: they can arrive BETWEEN the data bytes of another
 *     message, and treating them like an ordinary status byte would
 *     corrupt whatever note was mid-transmission.
 *     The menu shows the received tempo rather than just ON, since ON
 *     alone cannot distinguish a working link from a silent cable.
 *     v0.99891 covers two things found once the clock worked.
 *     SEQ step entry ignored an external keyboard:
 *     seqResolveFreqExcludingDel() reads the built-in keys only. Not a
 *     decision — and the wrong one to leave, since entering steps is where
 *     playing the pitch you want in the octave you want matters most and
 *     where the built-in three-key limit matters least. MIDI notes enter
 *     steps now, with local keys taking precedence when both are down (the
 *     rule the note path already uses), and the MIDI note's velocity
 *     becomes the step's velocity — a keyboard that sends velocity is
 *     saying exactly what that step should be.
 *     And the PLAY screen shows tempo and rate while the arpeggiator is
 *     running, on the line the frequency readout uses when it is off. That
 *     line is unused with the arp on, the note list above already says
 *     WHAT is playing, so the missing information is how fast. An external
 *     clock's tempo appears there too, marked with a tilde — otherwise
 *     that value is visible only inside the MIDI menu.
 *     v0.99892 lets MIDI switch things on and off, and splits the MIDI
 *     menu to make room.
 *     Two more CC slots, but for things that TOGGLE rather than sweep:
 *     portamento, hold, the arpeggiator and its latch. These could not use
 *     the existing CC destinations, because that list is built from
 *     ImuTarget and ImuTarget by definition holds continuous parameters —
 *     a knob mapped to a toggle is useless. They are treated as momentary
 *     switches: 64 or above is pressed, below is released, which is what
 *     pedals and pads send, and the toggle fires on the press only so
 *     holding a pad down does not flip the setting over and over. Switch
 *     slots are checked before the continuous ones, since aiming a switch
 *     at a number is the more specific intent.
 *     Hold goes through a callable that mirrors what the H key does,
 *     capturing the frequency rather than just flipping the flag — a Hold
 *     with no note captured does nothing at all.
 *     The MIDI page had reached twelve rows and this would not fit, so it
 *     is now MIDI > Out and MIDI > In. Room is the occasion rather than
 *     the reason: sending and receiving are two different jobs that were
 *     sharing a page only because they share a word. Channel stays with
 *     Out, being the one direction it applies to — reception is Omni.
 *     v0.99893: a latching button needed two presses per change, and the
 *     cause was an assumption rather than the controller. Controllers send
 *     these two ways and neither is wrong. A momentary pad or pedal sends
 *     127 while held and 0 on release — the press is the event and the
 *     release means nothing. A button in LATCH mode sends 127, then 0 on
 *     the next press: the value IS the state and both edges are events.
 *     Acting only on the press discarded the 0, so only every other press
 *     of a latching button did anything. Assuming the opposite would have
 *     been wrong in the other direction, letting a pad's release switch
 *     things off.
 *     So the mode is per slot — a sustain pedal on one and a panel button
 *     on the other is an ordinary setup, not a corner case. Latch mode
 *     compares the incoming state against the current one and calls the
 *     same toggle function when they differ, rather than setting the flag
 *     directly: that keeps every side effect those functions carry
 *     (capturing the held frequency, clearing portaFreq, resetting the
 *     arp's step) instead of duplicating them. Changing mode forgets the
 *     last level, since the two read it differently and a stale one would
 *     swallow the next press.
 *     v0.99894: switching Arp Latch off over MIDI updated the display but
 *     left the latched chord playing until any key was pressed. Turning it
 *     off with the local key worked, which is the tell.
 *     arpLatchToggle() empties the latched lists, but the notes the
 *     arpeggiator is actually PLAYING live in arpHeldFreqs[], and that is
 *     only rebuilt inside the keyChanged branch of the main loop. From a
 *     key, that branch runs immediately; from MIDI nothing asked for it.
 *     Exactly the shape of the v0.9986 fault — the arp's chord has two ways
 *     in and only one of them triggered a rebuild — which is worth noting
 *     because it means the rebuild's placement, not the latch, is the thing
 *     that keeps being wrong.
 *     The MIDI switch handler now requests a rebuild after any of the four
 *     functions, not just the latch: they all change what should be
 *     sounding, and the rebuild costs nothing when the chord has not
 *     changed.
 *     v0.99895 sends MIDI clock, the counterpart to receiving it and the
 *     last thing missing from the send direction: other gear follows
 *     C.P.S.'s tempo instead of setting it, and Start/Stop travel with it
 *     so pressing play here starts the other machine — the same courtesy
 *     clock in already extends to us.
 *     Two details decide whether a generated clock is usable. The deadline
 *     advances by exactly one interval rather than being reset to now, or
 *     every late call would push the tempo permanently flat. And if it
 *     falls far behind — a long redraw, a card write — the missed clocks
 *     are abandoned rather than fired as a burst, which would arrive as a
 *     stumble.
 *     It refuses to generate while following an incoming clock: two
 *     masters on one wire is not a tempo but a fight, and clock in wins
 *     because something explicitly asked for it. Switching clock out off
 *     sends Stop, rather than leaving the other machine running with
 *     nothing arriving.
 *     Worth stating the hardware limit again: this only reaches external
 *     gear with the unit's DIP in Separate. In Bypass the controller's TX
 *     goes to the SAM2695 alone, which ignores clock — harmless, just
 *     pointless.
 *     v0.99896 fixes clock out reaching a sequencer only intermittently
 *     and never carrying tempo. The cause was the RX pin auto-search from
 *     v0.9981, which had no way to stop.
 *     It was written to find which Grove pin is RX and then get out of the
 *     way, and it does stop the moment a byte arrives. But nothing ever
 *     arrives with the unit's DIP in Separate — that is the mode where the
 *     RX pin is not connected at all, and it is also the only mode where
 *     output reaches the OUTPUT socket. So the two features needed each
 *     other's opposite, and the search swapped forever, every three
 *     seconds. Each swap calls midiSerial.end()/begin(): the UART is torn
 *     down mid-send and the TX pin moves with it, so transmission worked
 *     in roughly half of alternating three-second slices. Which is exactly
 *     how it presented — responding sometimes, never following tempo. The
 *     log said so plainly in hindsight, alternating RX=1 and RX=2 forever.
 *     Two limits. It never runs while any send feature is enabled, since
 *     tearing down the UART to look for input is not worth breaking output
 *     for. And it gives up after thirty seconds, settling on G1 as RX,
 *     which is what this hardware actually uses.
 *     v0.999 adds the theremin: a VL53L1X on the Grove port plays pitch by
 *     hand height, the way a theremin's pitch antenna does, with volume
 *     staying on the IMU tilt so one unit is enough — the constraint this
 *     was designed around from the start.
 *     It drives currentFreq and the envelope exactly as the MIDI note path
 *     does, so the filter, the effects, the arpeggiator and note-out all
 *     follow with no special cases: the sensor is simply another way of
 *     saying "play this pitch".
 *     Wire1 rather than Wire, because the keyboard controller already owns
 *     the default bus at SDA=8/SCL=9 and a 50Hz ranging loop in the middle
 *     of key scanning would be a poor trade. Short distance mode: less
 *     reach than the sensor can manage, but far better immunity to ambient
 *     light and a faster update, and a theremin is played within arm's
 *     reach.
 *     Moving the hand AWAY lowers the pitch, as on the real instrument.
 *     Out of range stops the note rather than holding the last pitch —
 *     taking your hand away should silence it. Pitch can be continuous or
 *     snapped to semitones: continuous is authentic, but a theremin is
 *     famously hard to play in tune and the snap makes it usable alongside
 *     the rest of the instrument. Light smoothing either way, since
 *     untreated sensor jitter reads as a warble.
 *     The range is set in millimetres with the live reading shown on the
 *     same page, so the far limit can be set by holding a hand where you
 *     want it rather than by guessing.
 *     v0.9991: the sensor was not found. Which Grove pin is SDA cannot be
 *     settled from documentation any more than the MIDI RX pin could, so
 *     both orders are tried — but unlike that case this runs ONCE at boot,
 *     with no search left running to tear the bus down later.
 *     The bus is also scanned and every address logged, because "not
 *     found" has several causes that look identical from the outside:
 *     wrong pins, no power, a sensor at an unexpected address. Any device
 *     responding means the wiring is right and the address is the problem;
 *     an empty bus means it is not. Guessing between those without the
 *     scan is what cost several versions on the MIDI side.
 *     v0.9992: that scan never appeared in the log at all. It ran beside
 *     the MIDI UART setup, milliseconds into boot, before a serial monitor
 *     could attach — the same trap the SD and MIDI diagnostics fell into,
 *     and the third time a diagnostic has been unreadable for want of
 *     timing luck.
 *     Two changes so it does not depend on luck. The scan runs after the
 *     SD work, late enough to catch. And its result is shown on the
 *     Theremin page: the row says "no i2c device" when nothing answered on
 *     the bus at all — wiring or power — or the address and "init fail"
 *     when something is there but is not this sensor. Those two need
 *     different fixes and previously looked identical. A Rescan row
 *     reports how many devices were seen and repeats the search, so a unit
 *     can be plugged in without rebooting.
 *     v0.99903 (the two before it should have been numbered v0.99901 and
 *     v0.99902) fixes a scan that was reading the wrong bus entirely. It
 *     reported six devices with nothing plugged in: three addresses seen
 *     twice, and those three were the board's own 0x18/0x34/0x69. M5
 *     Cardputer initialises Wire1 during its own startup, and a later
 *     begin() with different pins does NOT move a bus that has already
 *     started — so the scan was walking the internal I2C bus. Wire.end()
 *     first forces the reconfiguration.
 *     It now looks for 0x29, the VL53L1X's own address, rather than for
 *     any device at all. That would have made the fault obvious at once
 *     instead of reading as a wiring problem, and it distinguishes an
 *     empty bus from a bus carrying only the board's own chips — two
 *     cases needing different fixes. The count also resets per scan, which
 *     is why the row climbed by six on every press.
 *     Four pin pairs are tried, Grove in both orders and then the EXT
 *     header's, since neither could be confirmed from documentation.
 *     v0.99904 moves the sensor to I2C port 0 and stops touching Wire1 at
 *     all. Wire1 is the board's OWN bus — keyboard and IMU at SDA=8/SCL=9,
 *     which is precisely why the earlier scan found 0x18, 0x34 and 0x69 on
 *     it. Repointing it at the Grove pins took the bus away from them, and
 *     M5's library reads the IMU every frame, so it claimed the bus
 *     straight back: the sensor read once after a rescan and then never
 *     again until the next one. The comment above thereminBegin() had said
 *     Wire1 was chosen to avoid disturbing the keyboard, which was exactly
 *     backwards.
 *     Readings are also checked against range_status now. With nothing in
 *     front of it the sensor still returns a number — large and wandering
 *     — and turning those into notes is what produced random pitches out
 *     of an empty room.
 *     v0.99905 fixes the pitch freezing and the stray high notes.
 *     The measurement takes 20ms and the repeat interval was also 20ms, so
 *     there was no gap: the sensor was asked for a new reading before it
 *     had finished the last, and returned stale or invalid data. Only the
 *     first measurement of each pass was real, which is exactly why the
 *     pitch locked to whatever height the hand first appeared at. The
 *     interval is 33ms now.
 *     The base note also had an octave added to it, putting the whole
 *     range an octave above where the synth was playing — a "1 oct"
 *     setting produced notes far higher than the octave it was meant to
 *     span. Removed.
 *     And a single reading is no longer enough evidence in either
 *     direction: one spurious valid-looking sample could fire a note out
 *     of an empty room, one dropout could cut a note being played.
 *     Agreement across three readings costs about 50ms of response, well
 *     under where a theremin starts to feel laggy, and removes both.
 *     v0.99906 finds why the pitch still would not move: the theremin was
 *     writing currentFreq, and the oscillator does not read it.
 *     audioTask copies currentFreq into playingFreq at the moment a note
 *     ATTACKS, and playingFreq is what the oscillator uses. Every other
 *     input retriggers per note, so latching at attack is exactly right
 *     for them. A theremin is the opposite case — one note that never
 *     stops while its pitch moves continuously — so it was changing a
 *     value nothing was listening to any more. That is why Reading tracked
 *     the hand smoothly while the pitch stayed wherever the hand first
 *     entered: the two symptoms were the same fault seen from both ends.
 *     playingFreq is written directly now, and portaFreq alongside it when
 *     portamento is on.
 *     Reading also clears when the sensor sees nothing, instead of holding
 *     the last distance — an empty sensor looked like a held hand.
 *     v0.99907: with the hand away the pitch wandered on its own, and
 *     pressing a key set it looping. Stopping the theremin cleared
 *     currentFreq and the smoothed pitch but not portaFreq — and
 *     portamento glides portaFreq toward currentFreq, so leaving it at the
 *     last pitch while currentFreq went to zero made it slide down to
 *     nothing by itself. A keypress restarted the glide from wherever it
 *     had got to, which is the loop.
 *     Stopping now happens in one function rather than being spelled out
 *     at each of its three call sites, which is how one of the four values
 *     came to be missed. Zero is this codebase's "unset" for portaFreq —
 *     the note path tests portaFreq<=0 before seeding it — so that is what
 *     it is set to.
 *     v0.99908 rewrites the decision instead of patching it again.
 *     "Is this a real measurement" and "is the hand inside the playing
 *     window" were separate tests with separate early exits, and only the
 *     first reset the good-reading counter. At the edges the two disagreed
 *     several times a second, so the note stopped and started repeatedly —
 *     and every restart is an ATTACK at whatever pitch the next reading
 *     happened to give. That is where the bursts of high notes came from,
 *     and why they appeared exactly when hovering at the far limit or
 *     closer than the near one: those are the boundary.
 *     A reading is now simply usable or not, and it takes several in a row
 *     to change state either way. Between states the pitch is left alone
 *     rather than recomputed from a reading already judged unusable.
 *     The EXT header's pins are also no longer guessed at. They were tried
 *     in case a unit was attached there, but configuring I2C on pins that
 *     may be wired to something else is a real risk for no benefit — the
 *     sensor is on Grove, and a board with NO sensor was misbehaving.
 *     v0.99909 stops touching the I2C bus unless asked, which is what that
 *     last sentence should have led to immediately.
 *     Probing means reconfiguring an I2C peripheral, and one of the two
 *     belongs to M5's library, carrying the keyboard and the IMU. Taking
 *     it away makes the IMU read nonsense — heard as parameters moving on
 *     their own, the waveform changing, and notes appearing with nothing
 *     in front of the sensor. Every one of those was reported with no
 *     sensor attached, which rules the theremin's own code out and points
 *     squarely at the probe. Nothing that reconfigures shared hardware
 *     should happen unasked at every startup.
 *     Turning Theremin on, or pressing Rescan, probes; boot does so only
 *     if it was left on. A failed probe hands the peripheral back rather
 *     than sitting on it with the wrong pins. And the bus is selectable,
 *     since which peripheral is free cannot be settled from documentation
 *     and getting it wrong is this exact failure.
 *     Far defaults to 250mm rather than 400: Short mode reaches much
 *     further in darkness but only about 280mm in room light, and a window
 *     wider than the sensor can see is a window whose far half is silence.
 *     v0.99910 finds the real fault, which every prior theremin version
 *     missed: the Grove port has exactly two signal pins, and
 *     CPS_TOF_SDA_PIN/SCL_PIN are the SAME physical pins as
 *     CPS_MIDI_RX_PIN/TX_PIN. MIDI serial starts on those pins
 *     unconditionally at boot regardless of what is actually plugged in,
 *     and configuring I2C on top of a running UART on the same GPIOs is a
 *     genuine electrical conflict, not a software race. Corrupted bytes on
 *     the MIDI side read as random Note On/Off and CC messages — which is
 *     where the phantom notes, the runaway pitch, and parameters moving on
 *     their own actually came from, including with no ToF unit attached at
 *     all, since the UART alone was enough to misbehave once I2C
 *     reconfigured the pins under it. Every symptom reported across
 *     v0.99906 through v0.99909 is consistent with this single cause;
 *     none of those fixes were wrong, they were addressing a bus that was
 *     never going to behave while something else held its pins.
 *     The two are made to take turns rather than sharing silently.
 *     Enabling Theremin suspends the MIDI UART before touching the pins;
 *     disabling it hands the UART back. The RX-pin auto-search also checks
 *     that the UART is actually running before it does anything, so a
 *     suspended UART cannot be searched out from under the Theremin
 *     either.
 *     v0.99911 closes a gap the previous fix left open: thereminBegin() is
 *     the function that actually touches GPIO1/2, but only
 *     thereminToggle() suspended MIDI before calling it. Called directly
 *     from boot when a saved setting restored Theremin as already on, the
 *     UART was never suspended and the same conflict happened again —
 *     which is consistent with the report of it working only after a
 *     second reset: a peripheral left in a bad state by a live pin
 *     conflict is the kind of thing a full power cycle clears and a soft
 *     reset may not. It also explains the stray waveform changes settling
 *     on Sine at 50% Shape, the firmware's own default — corrupted MIDI
 *     bytes landing as something close to a patch reset.
 *     Suspending now happens inside thereminBegin() itself, so boot, the
 *     toggle and Rescan are all covered by the one place that owns the
 *     pins, rather than by each caller remembering to do it first. Failing
 *     to find a sensor gives MIDI back immediately, since there is no
 *     reason to hold it suspended for a bus nothing is using.
 *     Separately: the first several readings after (re)starting continuous
 *     ranging are now discarded. VL53L1X datasheets note those can be
 *     unreliable while the sensor settles, and a stray one at connect time
 *     is a stray note — which matches the 1-2 seconds of pitch reported
 *     with nothing in front of the sensor. Counted down inside
 *     thereminUpdate() rather than delayed in thereminBegin(), so boot is
 *     not blocked waiting for it.
 *     v0.99912 fixes two more.
 *     Semitone mode barely sounded different from Smooth, because
 *     smoothing was applied to the ALREADY-quantized pitch: the output was
 *     always gliding toward whichever semitone had just been picked and
 *     never actually landed on it, so Semitone sounded like a mildly
 *     stepped Smooth rather than real steps. Smoothing now runs on the raw
 *     continuous pitch, quantizing happens after — once the smoothed
 *     value crosses a semitone boundary the output jumps straight there.
 *     Toggling Theremin on or off froze the UI for several seconds. The
 *     cause was thereminBegin() sweeping all 126 I2C addresses every time
 *     it ran, including every ON toggle — each address is a transaction
 *     carrying the platform's I2C timeout when nothing answers, which adds
 *     up fast. The sensor's address is known (0x29), so the normal path
 *     now checks only that one address; the full 126-address sweep is
 *     reserved for an explicit Rescan, which is the one place someone is
 *     actually reading the result.
 *     v0.99913 addresses two things from real hardware photos and reports
 *     rather than guessing further at code already changed twice.
 *     The Cap LoRa-1262's Grove port silkscreen reads G8 SDA / G9 SCL —
 *     the exact pins M5's library already uses for the keyboard and IMU.
 *     It is not a second bus, it is a tap on the same internal one, which
 *     I2C's multi-drop wiring supports and UART never could. Selecting it
 *     no longer calls begin()/end() at all: that bus is already running,
 *     and reconfiguring it is precisely the mistake v0.99904 fixed for the
 *     Grove port. The sensor is simply attached to what is already there.
 *     Bus selection is now labelled Grove / Cap G8-9 rather than raw
 *     peripheral names, since which one to pick is a wiring fact rather
 *     than an implementation detail.
 *     Separately, the SD card failing to mount only on a direct cold boot
 *     with the Cap attached, while mounting fine through the Launcher,
 *     points at power rather than at the card: those two differ in
 *     exactly whether 3.3V has already stabilised by the time SD.begin()
 *     runs, and a LoRa module's inrush current is the kind of load that
 *     dips a rail right at power-on. A 150ms wait is added before the
 *     existing SD retry, but only when esp_reset_reason() reports a
 *     genuine POWERON_RESET — a Launcher-triggered soft reset does not
 *     wait at all, since power is already settled by then.
 *     And the toggle freeze, still present after the fast-probe fix,
 *     is timed rather than patched again: a probe under a millisecond
 *     does not explain several seconds on its own, so thereminToggle()
 *     now logs how long it actually took, to find the real remainder
 *     instead of trading one guess for another.
 *     v0.99914 has the answer, from the log: the toggle itself measured
 *     0ms — it was never the source — and what followed was an unbounded
 *     stream of "i2cRead returned Error 263" with no way out short of a
 *     reboot. The sensor's per-call timeout drops from 200ms to 50ms, and
 *     thirty consecutive failures now disables Theremin outright rather
 *     than retrying forever: the menu reports "lost connection", distinct
 *     from the boot-time reasons, since this means the sensor was working
 *     and then stopped — a wiring, power, or interference question during
 *     use rather than at startup. A single success resets the count, so
 *     ordinary transient glitches never approach the limit.
 *     Separately, the Cap's own Grove port confirmed working — sharing
 *     M5's bus rather than reconfiguring it was the right call.
 *     And the SD-under-Cap theory from v0.99913 turned out to be
 *     incomplete: crc errors recur through the whole retry sequence on
 *     that hardware, well past any startup transient a 150ms wait could
 *     smooth over, which looks like continuous interference on the SPI
 *     lines rather than a one-off dip. A slower final rate and a slightly
 *     longer gap between attempts are added, on the honest expectation
 *     that they may only help rather than fully fix it — persistent
 *     interference is a wiring or shielding question no retry loop can
 *     solve outright.
 *     v0.99915 corrects an error in the version before it, found by doing
 *     the arithmetic properly instead of jumping to a bigger theory. The
 *     failure interval in the freeze log was about 2 seconds, and thirty
 *     of them — the v0.99914 threshold — takes roughly a minute to reach.
 *     The captured log only spanned 16-20 seconds. Auto-disable had not
 *     failed to fire; it had not had time to. A one-minute stall before
 *     recovering is still bad even once it ends, so the threshold drops
 *     to six, resolving in about twelve seconds.
 *     Worth recording what the interval itself says: a genuine per-poll
 *     failure would produce an error every 15-50ms, not one every two
 *     seconds. Roughly one failure in over a hundred read attempts, with
 *     pitch tracking otherwise reported as working, reads as a marginal
 *     connection — a Grove cable or connector seated imperfectly — rather
 *     than as another peripheral contending for the same bus.
 *     v0.99916 answers the SD-under-Cap question properly, prompted by
 *     the owner noting the Launcher mounts the card fine with the Cap
 *     attached on the very same cold boot — which the v0.99913/v0.99914
 *     "continuous interference" theory could not explain, since Launcher
 *     runs on identical hardware under identical conditions. If the
 *     problem were genuinely electrical, Launcher would see it too.
 *     M5's own Cap LoRa868/1262 tutorial gives the SX1262's pins directly:
 *     NSS (chip select) is GPIO5. This firmware never touched it, so with
 *     the Cap attached it sat floating — and SPI is a shared bus by
 *     design, where every device's chip select must be held deselected or
 *     it can answer for someone else. A floating CS reading as asserted,
 *     with the LoRa chip responding to SD commands, matches "GO_IDLE_STATE
 *     failed" and crc errors from the very first command far better than
 *     interference does. GPIO5 is now driven HIGH before SD.begin() runs,
 *     costing nothing when no Cap is attached.
 *     v0.99917 finds why the freeze's auto-disable never fired, prompted
 *     by the owner questioning whether Grove-only support was really a
 *     large undertaking — which turned the search toward a bug rather
 *     than another architecture theory.
 *     Pololu's own VL53L1X source shows did_timeout is set only inside
 *     read(true)'s blocking wait loop. This code calls read(false)
 *     specifically so a stalled sensor cannot block the main loop, and
 *     that path skips the loop entirely — did_timeout is never touched
 *     here no matter what the underlying I2C transaction did. The
 *     consecutive-failure counter across v0.99914 and v0.99915 was
 *     watching a flag that call was never going to set. It was not that
 *     failures were rare; none of them were being counted.
 *     Which means the Grove-vs-Cap bus-conflict theory this was chasing
 *     may never have been the real story: an occasional glitch on an
 *     external Grove cable, ordinary and expected, is enough on its own
 *     once failures go uncounted forever. Cap's internal connection being
 *     more reliable doesn't require Wire being claimed elsewhere to
 *     explain the difference — a shorter, direct connection glitching
 *     less than an external cable does not need a second explanation.
 *     Detection is time-based now: how long since a reading last
 *     succeeded, tracked independently of any internal flag. A result of
 *     0 or an invalid range_status counts as a miss even when read()
 *     reports no timeout, since a failed transaction can still hand back
 *     a stale or garbage value without setting anything. Three seconds
 *     without a good reading disables Theremin and reports "lost
 *     connection" — a dedicated flag now, cleared on every fresh probe,
 *     rather than inferred from other state that happened to line up.
 *     v0.99918 makes Semitone mode snap to the active Pro Style scale
 *     instead of flat chromatic steps. Chromatic snapping meant every
 *     scale sounded identical through the theremin, which missed the
 *     point of Semitone mode: it exists to make the instrument playable
 *     in tune, and "in tune" should mean whatever scale the rest of the
 *     synth is set to, not every semitone regardless of it. EZ Style has
 *     no Scale setting to draw on, so it keeps the chromatic snap this
 *     replaces.
 *     thereminQuantizeToHz() searches every degree of the active scale
 *     across a couple of octaves either side of the theremin's range and
 *     keeps the closest — the same brute-force approach
 *     recomputeKeyNotes() already uses to build the keyboard rows. At
 *     under a dozen scale degrees and a handful of octaves this is cheap
 *     enough for every 33ms reading.
 *     v0.99919 fixes "lost connection" firing constantly on BOTH Grove
 *     and Cap, which is what gave this one away — Cap's shared bus had
 *     never produced a single error before, so a bus-specific cause was
 *     ruled out immediately and the fault had to be in logic common to
 *     both.
 *     It was: "still talking to the sensor" and "a target is currently in
 *     range" got conflated in v0.99917. A RangeStatus other than Valid —
 *     nothing detected, a weak signal — is an entirely ordinary result,
 *     exactly what a working sensor reports whenever nothing is in front
 *     of it, which happens constantly during normal playing: a hand
 *     lifted between notes, a pause, adjusting position. Feeding that
 *     into the same watchdog as a genuine communication failure meant a
 *     few seconds of ordinary silence disabled Theremin outright,
 *     regardless of bus, and no amount of reseating a cable was ever
 *     going to touch it.
 *     dataReady() returning true is what actually shows the connection is
 *     alive — a fresh measurement read successfully over I2C this cycle,
 *     whatever it turned out to say. The watchdog resets on that alone
 *     now. Whether the measurement is usable for a note remains the
 *     separate question it always was, decided afterward and unchanged.
 *     Also: Rescan's count on the Cap's shared bus legitimately includes
 *     the keyboard controller, the IMU, and whatever else already lives
 *     there — not a miscount. Grove's bus carries only the sensor and
 *     still shows a plain count; Cap's now reads "N shared" so a higher
 *     number there doesn't look like an error.
 *     v0.9992 continues the Theremin phase, since it did not wrap up in
 *     v0.9990x — two more fixes reported after real playing.
 *     Toggling Theremin on with nothing in front of the sensor could
 *     still trip "lost connection" within a second or two, on Grove only,
 *     and only before the first real reading arrived — once ANY target
 *     had been detected once, the connection stayed solid until the next
 *     toggle. That is a settling-time pattern, not a fault: right after
 *     startContinuous(), dataReady() can go a beat longer than usual
 *     before it first trips, and Grove's marginally different electrical
 *     path made 3 seconds occasionally not enough of one. Cap never
 *     showed it, plausibly because that bus is already running
 *     continuously rather than having just been reconfigured. A 2.5s
 *     grace window now runs before the watchdog starts counting at all.
 *     And Range is no longer tied to the keyboard's live octave/transpose.
 *     Deriving the theremin's top note from params.octaveShift meant the
 *     playable range moved whenever the keyboard's own octave did, and
 *     reaching a higher theremin range meant pushing the keyboard itself
 *     out of a comfortable register at the same time — there was no way
 *     to have both. Top is its own setting now, a plain semitone offset
 *     from C4 (thereminTopSemis), shown as a note name and stepped an
 *     octave at a time since semitone precision at the ceiling isn't the
 *     point. The existing octave-count setting becomes Span, the width
 *     below that fixed top note. Unaffected by anything else in the
 *     synth, which is what a "how high can this go" control should be.
 *     Two fixes applied on top without a version bump, per the owner's
 *     request to save version numbers for confirmed-working states.
 *     thereminLastGoodMs was declared below thereminBegin(), which uses
 *     it — moved above, next to thereminLastMm. And tofBusIndex (Grove vs
 *     Cap) was never saved or loaded at all, so a reboot always came back
 *     to Grove regardless of what had been selected; it now persists
 *     under "thr_bus".
 *     v0.99921: Transpose is reapplied to the theremin's top note.
 *     Octave was deliberately dropped in v0.9992 because it dragged the
 *     whole theremin range along with the keyboard's own register —
 *     Transpose is a different kind of setting, a small deliberate
 *     key-of-the-song shift, and a scale locked to C regardless of it
 *     defeated the point of Semitone mode's scale-following added in
 *     v0.99918: the scale should move with the song's key the same way it
 *     does for the keyboard, not sit fixed on C.
 *     v0.99922 fixes two things from a combined MIDI-unit-on-Grove,
 *     ToF-on-Cap test: MIDI receiving and sending nothing at boot, and a
 *     harder look at the "just keep going like Cap" question for Grove's
 *     lost-connection handling.
 *     thereminBegin() suspended MIDI unconditionally, on either bus.
 *     Only Grove actually shares GPIO1/2 with the MIDI UART; Cap's bus is
 *     G8/9 and never touches those pins at all. Restoring a saved
 *     Theremin-ON state on the Cap bus at boot therefore silenced MIDI
 *     for a conflict that could never happen. What made it look fixable
 *     by toggling Theremin off and on was a second, unrelated bug: OFF
 *     never cleared tofPresent, so the ON toggle's `if(!tofPresent)`
 *     guard skipped thereminBegin() entirely — which incidentally also
 *     skipped re-suspending MIDI, restoring it as a side effect. The
 *     suspend call is now gated on tofBusIndex==0, so Cap-based Theremin
 *     never touches MIDI in the first place.
 *     On the lost-connection question: Cap's tolerance isn't something
 *     Grove can safely copy outright, because Cap's bus is kept alive by
 *     unrelated keyboard/IMU traffic, while Grove's dataReady() is the
 *     only signal that bus has — silently ignoring failures there would
 *     also hide a genuine unplug forever, which the original cable-pull
 *     test relied on catching. So instead of disabling on loss and
 *     waiting for a manual toggle, thereminUpdate() now retries the exact
 *     probe automatically every 500ms while lost, via thereminBegin()
 *     itself — cheap on a miss (one transaction on Grove, none on Cap).
 *     A transient hiccup clears itself the moment a reading succeeds
 *     again, with nothing for the player to do; a genuine outage simply
 *     stays silent for as long as it lasts, which is the correct outcome
 *     either way.
 *     v0.9993 closes the Theremin phase and fixes a longstanding envelope
 *     bug it happened to surface: notes that never stopped ringing on the
 *     Piano, Pluck and Bells starter patches unless Release was exactly 0.
 *     What those three share is sustainLevel=0 — decay-only, percussive
 *     envelopes. RELEASE's decrement was `dt/releaseTime*sustainLevel`,
 *     which is zero whenever sustainLevel is zero, regardless of what
 *     envLevel actually is. Releasing a key mid-DECAY, before it reached
 *     sustainLevel, put envLevel at whatever DECAY had reached so far —
 *     and with the decrement permanently zero, it stayed there forever.
 *     Release=0 took a separate branch that skipped this entirely, which
 *     is why only nonzero Release hung. The filter envelope carried the
 *     exact same formula and the exact same bug, just less audible: a
 *     stuck-open filter rather than a note that never stops.
 *     Fixed by capturing the level RELEASE actually starts from —
 *     envReleaseStartLevel / filterEnvReleaseStartLevel, set once on
 *     entry to RELEASE from whichever phase preceded it — and decrementing
 *     proportionally to THAT instead of to sustainLevel. Releasing from a
 *     held SUSTAIN reduces to exactly the old formula, since the captured
 *     level equals sustainLevel there, so existing patches that release
 *     normally are timed identically to before; only release interrupting
 *     ATTACK or DECAY behaves differently, which is precisely the case
 *     that was broken.
 *     This was never Theremin-specific — any input releasing mid-decay on
 *     a zero-sustain patch would have hit it, keyboard included, and the
 *     Theremin phase closes here, with the ARP-tidying phase (v0.9993x)
 *     starting from the same version number.
 *     v0.99931 begins the ARP-tidying phase promised there, taking option
 *     B: one funneled entry point rather than a rebuild on every loop.
 *     Every previous bug in this area (v0.9986, v0.99891, v0.99894) had
 *     the same shape — a function that read local keys AND rebuilt the
 *     chord welded into one, called from two unrelated places (a
 *     keyChanged branch, and a midiNotesDirty flag polled from loop()),
 *     so whichever call site existed when a new feature was added often
 *     did not know to trigger the other.
 *     updateArpHeldNotes() is split into two. updateArpLatchEdges() is
 *     the part that is genuinely keyChanged-only: detecting a NEW
 *     physical keypress by diffing against the previous frame, which only
 *     means anything at the instant a key transitions, and stays called
 *     from that branch alone. rebuildArpChord() is everything else —
 *     building arpHeldFreqs[]/arpSortedFreqs[] from whichever state is
 *     current — and is now the one place every path that changes what
 *     should be sounding calls directly: the local key branch, MIDI note
 *     on/off, a switch toggling Latch, MIDI panic, and Latch's own
 *     toggle function. The midiNotesDirty flag and its loop()-polling
 *     consumer are gone entirely, since nothing sets a flag for later
 *     any more — each caller just calls rebuildArpChord() when it has
 *     something to report.
 *     The screen/mode eligibility check (arpEnabled, not mid-SEQ, not on
 *     PATCH/SEQ/PATTERN/SONG/TIMBRE) moved inside rebuildArpChord() itself
 *     rather than being duplicated at each call site — the local-key path
 *     and the loop()-poll path had each grown a slightly different copy
 *     of this over time, which is exactly the kind of drift a single
 *     entry point is meant to prevent.
 *     v0.99932: MIDI clock's received BPM is now smoothed across beats,
 *     not applied raw. Averaging over one beat (24 clocks) already
 *     rejects jitter within it, but consecutive beats can still disagree
 *     slightly — and a real tempo knob's own physical wobble and small
 *     variance in exactly when this firmware gets to process a byte both
 *     show up the same way from the clock stream alone; there is no way
 *     to tell them apart. Blending reduces either without needing to
 *     know which one it is. The first lock still snaps straight to the
 *     first reading rather than easing up from the startup default.
 *     v0.99933 raises the smoothing from 0.3 to 0.6: it removed the wobble
 *     but made a deliberate tempo change feel slow to follow, since the
 *     same filter cannot tell a real change from beat-to-beat jitter — it
 *     can only trade how much of each gets through. 0.6 leans toward
 *     following: a full jump settles in roughly 3-4 beats instead of 7-8,
 *     while still averaging enough to keep a held tempo from visibly
 *     wobbling.
 *     v0.99934 adds Swing to the PLAY screen's arp display, on its own
 *     line under tempo/rate rather than crowded onto that one — the note
 *     list above already says WHAT is playing and tempo/rate said how
 *     fast; Swing was the remaining piece SETTING > Arp shows that this
 *     screen did not. Resolved as base plus its IMU/CC offset, clamped to
 *     the same range the ARP timing itself uses, so the number shown is
 *     what is actually shaping the groove rather than just the menu's
 *     stored value — the same treatment tempo already got.
 *     v0.99935 corrects v0.99934, which assumed the row below tempo/rate
 *     was free. It was not — the Octave/Transpose status line (O:+0 T:+0)
 *     is drawn there unconditionally, arp running or not, and the two
 *     overlapped into unreadable garbage, visible in a photo of the
 *     actual screen. Swing now shares the tempo/rate line instead:
 *     "%c%.0f %s%+.0f" with no fixed-width padding on the rate label and
 *     no percent sign on swing, since a 73px-wide box has no room to
 *     spare for separators that are not load-bearing. Every realistic
 *     combination fits; the rare case of a triplet rate at extreme tempo
 *     and swing simultaneously clips at the box edge, accepted rather
 *     than shrinking the common case further to guard against it.
 *     v0.99936, per the owner's own observation of where the actual spare
 *     room was: the note list above rarely fills its full three rows in
 *     ordinary play, and that gap was better spent on Swing than the
 *     tempo/rate line was on cramming it in. The note list is capped to
 *     two rows now rather than three, specifically to GUARANTEE that
 *     third row's space is free regardless of how many notes happen to
 *     be held — a large chord growing into it would only recreate the
 *     same collision that hit the Octave/Transpose line in v0.99934, just
 *     moved one row up. Swing gets that guaranteed row back to itself,
 *     and tempo/rate returns to the simpler single-line form.
 *     v0.99937 swaps the two lines' order: Tempo/Rate on top, Swing below,
 *     matching the order SETTING > Arp already lists them in rather than
 *     the order they happened to get coded in.
 *     v0.99938 fixes two things visible in a photo of the actual screen:
 *     the tempo/rate and swing lines touching, and tempo/rate sitting
 *     noticeably right of every other row's left edge.
 *     The touching was a spacing bug carried through v0.99936 and
 *     v0.99937's edits: every other row in this box — the note list,
 *     O:+0/T:+0, P:off/H:off — is spaced 9px apart, but these two were
 *     only 6px apart, close enough for character descenders to visibly
 *     meet on the real screen even though nothing was technically out of
 *     the canvas. Moved to the same 9px cadence (local y=23, 32), which
 *     lands exactly on O:+0 at y=40 with no wasted gap, since the note
 *     list is already capped to two rows to guarantee y=23 is free.
 *     The rightward offset was the '~' external-clock marker's column
 *     being reserved as a literal space even when not external, pushing
 *     the whole line one character right of every other row's flush x=4
 *     start. It only prints now when it actually applies.
 *     v0.9994 opens the UI/UX final-pass phase with two of the items
 *     raised there: an explicit "+" on additive IMU/CC offsets, and Tap
 *     Tempo.
 *     Nine displays showed a raw offset — ARP_TEMPO, ARP_SWING, PITCH_BEND,
 *     DETUNE, NOISE, SUB_LEVEL, RESONANCE, LFO_RATE, LFO_DEPTH — as a bare
 *     number with no sign for a positive value, exactly the confusion
 *     reported for ARP Tempo: "15" read as "playing at 15bpm" rather than
 *     "15 over whatever tempo is set." BEND_UP already carried an explicit
 *     "+", which is what made the omission on the rest visible once named.
 *     imuSign() supplies "+" for a positive value and nothing otherwise,
 *     since the numeric formatting already contributes "-" on its own.
 *     Tap Tempo lives on Shift+Enter, tracked from updateOctaveAndVolume()
 *     — which already runs on every screen except Patch/Pattern/Timbre —
 *     rather than being added to a menu, so it costs no key PLAY or SEQ
 *     had spare. It sets whichever tempo is actually in use, seqTempoBpm
 *     while SEQ plays and arpTempoBpm otherwise, matching how the two are
 *     already independent everywhere in this firmware. Consecutive taps
 *     blend into a running average rather than each replacing the last —
 *     a human tapping a beat is never perfectly even, and averaging the
 *     last few intervals is what a hardware tap-tempo button does. A gap
 *     over two seconds starts a fresh average instead of blending against
 *     a stale one, indistinguishable otherwise from deciding to tap a new,
 *     much slower tempo.
 *     v0.99941 finds the cause of the morph-start artifact reported as
 *     sounding like a stray bitcrush or phaser, on patches using neither.
 *     It was a real bug, not the sound a waveform change is expected to
 *     make.
 *     timbreMorph is a float POSITION inside morphChain[] — which slot of
 *     the chain to read the current waveform from — and morphChain[]
 *     itself switches discretely to the target patch's chain at the very
 *     start of morphStart(), by design: there is no halfway between two
 *     different SETS of waveforms. But timbreMorph was left to interpolate
 *     continuously in morphApply(), starting from the OUTGOING patch's
 *     value. For as long as that interpolation took to catch up, the
 *     synth was reading a position meant for the old chain out of the
 *     chain that had already become the new one — an unrelated blend of
 *     whatever waveforms happened to sit at that slot in the new set,
 *     which is what produced the harsh, unintended sound at the start of
 *     nearly every morph.
 *     timbreMorph now snaps to the target patch's own value in
 *     morphStart(), on the same line that switches morphChain[], and is
 *     no longer touched by morphApply() at all. A chain switch was always
 *     discrete; the position inside it has to be discrete with it, for
 *     the same reason every other discrete parameter here snaps instead
 *     of interpolating.
 *     v0.99942 brings the gradual sweep back, after the owner pointed out
 *     v0.9995's instant snap removed something they liked: the waveform
 *     visibly changing over the course of a morph, not just jumping.
 *     Re-examining the mechanism showed the sweep itself was never the
 *     problem — getMorphedSample() already clamps every index it uses to
 *     morphChainLen, so nothing was ever misreading memory or a stale
 *     pointer. The actual fault was narrower: interpolation used to start
 *     from the OUTGOING patch's timbreMorph value, a position that
 *     belonged to a chain which had already stopped existing the instant
 *     morphChain[] switched — an arbitrary jump with no relationship to
 *     the new chain, not a sweep through it.
 *     The fix keeps the sweep and removes only that mismatch: it now
 *     starts from an END of the chain that is already active — whichever
 *     end sits farther from this patch's own timbreMorph target, for the
 *     longest and most audible sweep — and interpolates toward the
 *     target in lockstep with morphApply()'s own t, over the whole
 *     morph's duration. Every frame of that sweep reads consecutive
 *     waveforms of the SAME currently-active set, so it is coherent audio
 *     throughout rather than a jump followed by a sweep.
 *     Deliberately NOT routed through the separate timbreMorphTarget/SM
 *     smoothing used for live IMU tweaks: that mechanism resolves in a
 *     fraction of a second, on a schedule meant for a tilt sensor, and
 *     would have collapsed a multi-second morph into an instant snap
 *     again from a different starting point. morphTimbreStart holds the
 *     fixed reference morphApply() sweeps from on every tick.
 *     v0.99943 addresses both the residual noise and the intermittent
 *     silence together, since a video of the actual behaviour and the
 *     "no fixed repro" description together pointed at the same
 *     underlying cause rather than two separate ones.
 *     morphChain[]/morphChainLen/morphTblA[]/morphTblB[] are shared
 *     between loop() (APP core) and audioTask (PRO core — a genuinely
 *     different physical core on this chip, not merely a different task),
 *     and nothing in this file had ever synchronised access to them.
 *     morphStart() rewrites all of them across several separate
 *     statements; audioTask's getMorphedSample() reads them on every
 *     sample, and separately calls refreshMorphTablePtrs() itself,
 *     unconditionally, once per buffer — meaning there were genuinely TWO
 *     writers to morphTblA/morphTblB (audioTask's own routine refresh and
 *     whichever UI action just changed the chain), on two different
 *     cores, with no coordination. A read landing mid-update could pair a
 *     just-updated chainLen with pointer slots the array copy had not
 *     reached yet, or catch both writers mid-overlap — read as a stray
 *     waveform in the wrong position (heard as noise) or, if a pointer
 *     was caught still null, worse. No fixed reproduction is exactly what
 *     an unsynchronised cross-core race looks like from outside it.
 *     A single portMUX_TYPE spinlock (morphChainMux) now guards every
 *     site that touches this state: morphStart(), the Morph chain
 *     editor's live add/remove/swap, the slot-restore and tone-reset
 *     defaults, and refreshMorphTablePtrs() itself — which takes the lock
 *     internally, so every caller is protected automatically rather than
 *     each having to remember to. ESP32's portMUX spinlocks are
 *     re-entrant by the same core, so a caller that already holds the
 *     lock nests into refreshMorphTablePtrs()'s own lock safely.
 *     Cost is deliberately not uniform: refreshMorphTablePtrs()'s body is
 *     a handful of array-pointer copies, not real work, so locking every
 *     call to it — including the one every audio buffer makes
 *     unconditionally — is negligible next to the ~23ms it has to run in.
 *     getMorphedSample()'s own hot per-sample lookup is NOT locked,
 *     deliberately: at up to 44100 calls/sec a spinlock there would cost
 *     real CPU for a case that essentially never needs it, so only its
 *     rare null-pointer fallback takes the lock, the same as every other
 *     rare writer here.
 *     Not covered: patch loading writes morph_chain* keys one at a time
 *     across many separate calls as a file is parsed line by line, which
 *     would need locking the whole multi-line load rather than one
 *     assignment, a larger change than this pass covers. Loading a patch
 *     while a note is actively sounding remains a known gap.
 *     v0.99944 responds to two reports. The morph silence got WORSE and
 *     now persists until a full power cycle — a materially different,
 *     more serious symptom than before, and one this reads as consistent
 *     with a genuine hang rather than a transient glitch. Every
 *     portENTER_CRITICAL/portEXIT_CRITICAL pair from v0.99943 was checked
 *     by hand for an early return or break that could skip the exit —
 *     none found. Rather than guess a sixth time, audioTask now writes a
 *     timestamp every buffer (audioTaskHeartbeatUs), and loop() — on its
 *     own independent clock, deliberately not relying on anything
 *     audioTask itself would need to be alive to trigger — logs plainly
 *     if that timestamp stops advancing, and separately logs if the
 *     heartbeat is current but envPhase disagrees with what should be
 *     sounding. The next occurrence should say definitively whether
 *     audioTask has stopped outright or is running but stuck in a
 *     different way, rather than requiring another guess between them.
 *     Separately: the ToF connection-lost retry from v0.99917 had no
 *     upper bound, so a unit left unplugged with Theremin still on from a
 *     saved setting retried every 500ms forever, printing "no sensor
 *     found" endlessly with no way to stop it short of the SETTING menu.
 *     40 attempts (~20s) is long enough to catch a real reconnect; past
 *     that it disables outright, the same outcome as the lost-connection
 *     path, and needs an explicit switch back on. Resets on any
 *     successful find, so a genuine reconnect is never blocked by a
 *     stale count from an earlier session.
 *     v0.99945 fixes two things found once the silence and the residual
 *     noise were no longer in the way of noticing them clearly.
 *     Re-morphing to the same patch — or to a different patch that
 *     happens to share the same chain — still swept through every
 *     waveform in it, which should have been an audible no-op. The
 *     "start from a far end of the chain" logic from v0.99942 ran
 *     unconditionally, with nothing checking whether the chain had
 *     actually changed at all; by the time it ran, morphChain[] had
 *     already been overwritten, so there was nothing left to compare
 *     against even if it had tried. The comparison now happens BEFORE
 *     that overwrite: when the incoming chain is byte-for-byte identical
 *     to the one already active, the sweep starts from wherever the
 *     sound already is instead of a chain end — which, for an unchanged
 *     chain, means no audible change, exactly what re-selecting the same
 *     patch should do.
 *     Separately, the VCO screen's Timbre readout jumped straight to the
 *     morph's target the instant nothing was playing, and only revealed
 *     the true in-progress value once a key was pressed — seemingly
 *     backwards from what a display should do. The cause was older code,
 *     written to fix IMU tilt not reaching params.timbreMorph while idle,
 *     that snaps it straight to its target every buffer whenever
 *     envPhase is IDLE. That snap is exactly right for the IMU case it
 *     was written for, but the morph sweep now drives this same pair on
 *     its own independent timeline via morphApply(), deliberately
 *     regardless of note state — so the old snap fought it every single
 *     buffer while idle, and the display never had a chance to show
 *     anything but the final value until a note interrupted the snap by
 *     leaving IDLE. Gated on !morphActive now, so the snap only runs when
 *     nothing is actually mid-sweep — unaffected for its original IMU
 *     purpose, out of the way during a morph.
 *     v0.99946 finds the actual source of the phaser sound reported on
 *     re-morphing to the same patch — worst on Lead, Bass, Brass and
 *     Pluck specifically, which was the clue: those four share nothing
 *     in their FX, but they DO all carry a large filter envelope depth,
 *     and that turned out to be exactly the variable that mattered.
 *     filterParams.cutoffHz/resonanceQ are shared between audioTask (PRO
 *     core) and morphCapture() (loop(), APP core, called from
 *     morphStart() with no synchronisation). The per-buffer envelope/LFO/
 *     tracking/drift modulation used to WRITE the fully-modulated dynamic
 *     value directly into those shared fields, call
 *     updateFilterCoefficients() (which read them back out with no
 *     parameters of its own), then immediately restore the saved static
 *     value — a real window, however brief, during which the shared
 *     fields held an envelope-inflated value rather than the patch's true
 *     one. Landing in that window meant morphCapture() recorded the
 *     inflated value as the morph's starting cutoff; morphApply() then
 *     swept audibly from there back down to the patch's real value (the
 *     same value, for a same-patch remorph) over the whole morph
 *     duration. A large fenv_depth makes the captured value dramatically
 *     different from the true one, which is exactly why only patches
 *     built around a big filter-envelope swing made this obvious.
 *     updateFilterCoefficients() now takes the dynamic cutoff/Q as
 *     optional parameters, defaulting to the previous behaviour (reading
 *     filterParams directly) for every other caller. The per-buffer
 *     modulation path passes its dynamic values straight through instead,
 *     and never touches filterParams.cutoffHz/resonanceQ at all any
 *     more — removing the race outright rather than narrowing it with a
 *     lock, since there is nothing left to catch mid-update.
 *     v0.99947: the filter-cutoff race fix did not resolve the residual
 *     phaser on same-patch remorphing, and a new, RELIABLY reproducible
 *     silence appeared (Strings<->Pluck, both directions, preceded by
 *     several clicks). Neither audioTaskHeartbeatUs nor the envPhase/
 *     currentFreq mismatch check fired, ruling out both a hung audioTask
 *     and that specific envelope contradiction — the cause is something
 *     else the existing diagnostics do not cover.
 *     Two things worth separating on the phaser: same-patch morphing was
 *     never guaranteed to be silent to begin with. morphCapture() records
 *     whatever the LIVE sound is doing at that instant, offsets included —
 *     if IMU tilt, vibrato, or any other live modulation had the sound
 *     away from the patch's own stored baseline at the moment of the
 *     morph, resetting those offsets back to baseline (which any morph
 *     does, including to the same patch) is audible movement and not a
 *     bug. Whether that explains what was heard depends on whether
 *     anything was actively modulating at the time.
 *     A second look for the same anti-pattern that caused the filter race
 *     — a shared field temporarily overwritten with a dynamic value, used,
 *     then restored — found nothing else matching it in the file.
 *     For the reproducible silence, morphTick() now logs envPhase,
 *     envLevel, currentFreq, playingFreq, adsr.sustainLevel,
 *     filterEnvLevel and filterParams.cutoffHz every ~150ms while a morph
 *     is active, behind CPS_LOG_PATCH. Reproducible cases are exactly
 *     what this kind of logging is for — the next run should show what
 *     state things are actually in when the clicks and the silence
 *     happen, rather than requiring another guess.
 *     v0.99948: it did not happen — v0.99947 stopped reproducing the
 *     click/silence entirely. Not fixed, avoided: Serial.printf() is slow
 *     enough that calling it every 150ms shifted loop()'s timing just
 *     enough to dodge whatever narrow window causes this, which is itself
 *     useful confirmation that the cause is timing-sensitive rather than
 *     a plain logic error — a live-printing diagnostic cannot observe a
 *     race it is itself perturbing out of existence.
 *     Recording now goes into a small in-memory ring buffer
 *     (morphDiagLog[], a handful of array writes, no I/O) during the
 *     morph, with the whole captured sequence printed in one batch only
 *     after the morph finishes — real-time pressure is over by then, so
 *     the dump cannot influence the window it is reporting on. Same
 *     fields as before (envPhase, envLevel, currentFreq, playingFreq,
 *     adsr.sustainLevel, filterEnvLevel, filterParams.cutoffHz), just
 *     delivered after the fact instead of live.
 *     v0.99949: a same-patch Lead remorph, held via Hold the whole time,
 *     produced exactly the "murky/gurgling" sound reported — and the log
 *     shows why it looked odd. envLevel and filterEnvLevel both rose well
 *     above their sustain levels (0.75 and roughly 0.5) for a stretch
 *     around 300-1000ms into the morph before settling back down, with
 *     envPhase briefly reporting DECAY (2) in the middle of what should
 *     have been an unbroken SUSTAIN — there is no path from SUSTAIN back
 *     to DECAY in advanceEnvelope() without an ATTACK in between, so
 *     something retriggered the envelope mid-morph despite Hold never
 *     changing.
 *     Neither morphStart() nor morphApply() touches envPhase or
 *     currentFreq directly — checked by hand across both functions — so
 *     the actual cause is not yet found. What the log COULD NOT show:
 *     filterEnvPhase is a separate state machine from envPhase
 *     (advanceEnvelope() runs two independent switch blocks for them),
 *     and the diagnostic had only ever recorded the amp one — a
 *     filterEnvLevel excursion could be the filter envelope retriggering
 *     entirely on its own, invisible until logged directly. Both
 *     filterEnvPhase and noteHeld are recorded now, which should show
 *     whether Hold's own state ever moves (ruling a real key-state change
 *     in or out) and whether the two envelopes retrigger together or
 *     independently — the next capture should narrow this considerably.
 *     v0.99950: three consistent captures (filterEnvPhase/noteHeld now
 *     included) all show the same shape — noteHeld staying 1 throughout,
 *     yet both envelopes retrigger at almost exactly the same relative
 *     point in the morph every time. Every legitimate retrigger source —
 *     ARP, Theremin, MIDI, SEQ — was confirmed off/disconnected for these
 *     captures, ruling out five of the six known trigger sites. A broader
 *     search for any envPhase/filterEnvPhase=ATTACK assignment, not just
 *     the exact pattern already known, found no seventh site — the
 *     remaining candidate is loop()'s own local-keyboard handler, the one
 *     path that does not require any of those four systems.
 *     That handler only retriggers when resolveFreqFromKeys() reports a
 *     held note key (nf>0). The owner isn't touching a note key during
 *     this test — they are pressing Shift+<number> to reselect the same
 *     Morph slot, which is a keyChanged event this handler runs on too.
 *     This keyboard's Shift handling has produced exactly this class of
 *     quirk before elsewhere in this file — a transitional scan catching
 *     an unshifted character before Shift's effect is reflected — so a
 *     plausible mechanism exists for nf to read briefly nonzero during
 *     that specific keypress.
 *     Rather than fix a guess, the retrigger site itself now logs nf and
 *     the raw key buffer at the exact moment it fires, so the next
 *     capture shows directly whether this is what is happening, and
 *     which character is responsible if so.
 *     v0.99951: the log is conclusive — every capture, three times, read
 *     "shift=0 word=\"3\"" at the exact instant of retrigger. Pressing
 *     Shift+3 to reselect Morph slot 3 (Lead) genuinely produces one scan
 *     where the keyboard driver reports Shift not yet active alongside
 *     the bare digit — the two physical edges do not land in perfectly
 *     the same instant — and "3" happens to be a valid unshifted note key
 *     on this layout (E4, 329.6Hz, matching every capture exactly), so
 *     that transitional scan was genuinely indistinguishable from someone
 *     pressing it. This was never really about Morphing at all; any
 *     Shift+<note-mapped-digit> combo elsewhere in the firmware would hit
 *     the same one-scan artifact, and Morph slot selection was simply the
 *     first place it got tested enough to notice.
 *     Fixed by debouncing the RETRIGGER decision specifically, not
 *     resolveFreqFromKeys()'s result itself — nf still drives
 *     currentFreq, pitch tracking and note-off exactly as before, so
 *     releasing a key stays immediate. Only the ATTACK-phase retrigger
 *     now requires the SAME nf value on two consecutive keyChanged reads
 *     before it is allowed to fire. A one-scan blip fails that
 *     immediately (the very next read never matches, since it was never a
 *     real key). A genuine press, physically held for far longer than a
 *     couple of scan cycles, satisfies it on the next read — an
 *     imperceptible added latency next to how this instrument is
 *     actually played.
 *     v0.99952 reverts v0.99951's fix, which was wrong in a way severe
 *     enough to silence the instrument from boot onward, and replaces it
 *     with one scoped to what the race actually requires.
 *     keyChanged is edge-triggered — it fires once when the pressed-key
 *     SET changes, not repeatedly while a key is simply held — so
 *     requiring a SECOND keyChanged carrying the same nf had no natural
 *     way to ever arrive for an ordinary single press: there is no
 *     further state change to report while a finger just sits on one
 *     key. nfConfirmedForRetrigger was false for essentially every real
 *     note, not just the Shift+digit glitch it targeted.
 *     The replacement re-checks within the SAME frame instead of waiting
 *     on a second event, and only when it is actually needed: Row 1 (the
 *     number row, ROW1_KEYS) is what doubles as Shift+1..0 for Morph slot
 *     select, so only a note resolved from a Row-1 character gets a
 *     short delay and a direct re-read of Shift's own state — Row 2
 *     (letters) has no such combo and is never delayed at all. If Shift
 *     reads true once the race has had a moment to settle, this was a
 *     Shift+digit combo, not a note, and the retrigger is skipped. A
 *     genuine Row-1 note press, unaccompanied by Shift, pays a few
 *     milliseconds and proceeds exactly as before.
 *     v0.99953: the retrigger fix is confirmed working — three clean
 *     captures, envPhase never leaving SUSTAIN — and yet the reported
 *     "blurry, reverb-like, clears up right as the morph finishes"
 *     character persisted across those same clean runs. That means the
 *     retrigger was never actually causing this; it only co-occurred
 *     with a louder, more dramatic symptom that happened to mask it.
 *     Analog Drift was also ruled out — the effect persists with it off.
 *     Code review of reverb, chorus and LFO — the obvious candidates for
 *     a blur/wash character — found none of the same live-overwrite
 *     anti-pattern the filter had; morphCapture()'s wholesale `d.p=params`
 *     copy reads them safely, and nothing writes back into them the way
 *     the old filter code did. Rather than guess further, reverbMix,
 *     chorusMix, chorusDepthMs and lfo.depth are now logged alongside
 *     everything morphTick() already tracks, so the next same-patch
 *     capture shows directly whether any of them move when they should
 *     not, instead of staying constant like a true same-patch remorph
 *     should produce throughout.
 *     v0.99954 reverts that struct growth outright: SD failed to mount
 *     on every boot immediately after v0.99953, no Cap attached, on both
 *     boot paths, with nothing else changed. setup() (where SD mounts)
 *     completes entirely before loop() is first called, so morphTick()'s
 *     own code could not have run yet at the point of failure — the
 *     connection has to be a memory-layout side effect from the array
 *     growing large enough to expose an out-of-bounds write elsewhere
 *     that happens to land on whatever the VFS layer needs, not a direct
 *     causal one. The exact culprit elsewhere is not found; reverting the
 *     one thing that changed is the safe, testable move while it isn't.
 *     The reverb/chorus/LFO question this was investigating remains open
 *     — it will need a different, smaller way to gather the same data
 *     next time, not a repeat of what just broke SD.
 *     v0.99955 fixes a genuine crash — Guru Meditation, StoreProhibited,
 *     a write near address 0 — that followed a burst of the v0.99952
 *     Row-1 retrigger recheck firing in quick succession, and struck on
 *     an unrelated Tab press shortly after. The most likely mechanism:
 *     delay(5) yields to the FreeRTOS scheduler, and calling it
 *     synchronously inside loop()'s keyChanged handling — on every single
 *     Row-1 note-on — opened a scheduling window that let some other,
 *     previously-latent race actually fire, rather than the delay itself
 *     being the direct fault.
 *     No delay() call exists anywhere in this replacement. A Row-1
 *     retrigger is now deferred rather than decided on the spot: the note
 *     is stashed in pendingRow1Nf/pendingRow1MidiActive/pendingRow1SetMs,
 *     and a separate check — outside the keyChanged gate, reached on
 *     every ordinary loop() pass regardless of further key events —
 *     finalises it once 5ms have genuinely elapsed, with a fresh
 *     keysState() read at that point. loop() runs many times within 5ms
 *     on its own, so nothing needs to wait for anything.
 *     v0.99956: the crash recurred on v0.99955 too, with the identical
 *     EXCVADDR=0x00000004 and the same backtrace shape, and — critically
 *     — the owner confirmed it was NOT tied to any number-key activity
 *     this time. That rules out the delay()/scheduling theory v0.99955
 *     was built on: this is a separate, pre-existing bug that happens to
 *     manifest on a Tab press, unrelated to the Row-1 retrigger work.
 *     Without the actual ELF and symbol table, this file's own analysis
 *     cannot decode the backtrace addresses to function names. Tab's own
 *     handler is a large, sprawling switch covering every screen, so a
 *     diagnostic print right where Tab starts being processed — logging
 *     appMode, shift, morphActive, morphDiagCount and currentFreq before
 *     any of that switch's branches can run — should at minimum narrow
 *     which screen/state combination is responsible the next time this
 *     is reproduced, rather than guessing among many candidate branches.
 *     v0.99957: the diagnostic worked immediately — "[tab] appMode=0
 *     ..." printed successfully (confirming Tab's handler itself runs
 *     fine, right at its very top) and the crash followed regardless,
 *     with no number-key activity, on a totally fresh boot. G0 (SEQ)
 *     crashed identically, with the same EXCVADDR=0x00000004, without
 *     ever reaching this diagnostic at all — meaning the fault is not
 *     inside Tab's own switch statement, but somewhere both paths funnel
 *     into afterward.
 *     That somewhere turned out to be `delay(5);return;` — a longstanding
 *     pattern already present at the end of every non-PLAY screen's
 *     branch (VCO, VCF, VCA, LFO, FX, SETTINGS, CATEGORY), not something
 *     added this session. It is the one thing every screen Tab or G0 can
 *     lead to has in common. delay() yields to the FreeRTOS scheduler,
 *     and something added earlier this session — most plausibly the
 *     morphChain[]/filterParams locking work — most likely introduced a
 *     race that only manifests when a yield lands in the wrong window;
 *     this delay was simply the first place reliable enough to hit it,
 *     the same mechanism already confirmed for the earlier Row-1
 *     retrigger crash.
 *     Removed from all twelve matching sites, plus the equivalent one at
 *     the very end of loop() for the PLAY-screen fallthrough — PLAY has
 *     not shown this crash, but that is not proof it is immune to the
 *     same underlying race. The actual per-screen redraw throttling
 *     (MIN_REDRAW_MS, canForceRedraw, the 100ms checks) is untouched;
 *     only the artificial per-iteration pause is gone. The race itself
 *     remains unidentified — this removes the yield point these paths
 *     shared, not the root cause.
 *     v0.99958 fixes a genuine silence bug found while chasing the
 *     "juwa-juwa" texture: a log showed currentFreq becoming nonzero
 *     (440.0, then 415.3) while envPhase stayed IDLE the whole time, with
 *     the v0.99944 mismatch diagnostic confirming it directly. The cause
 *     is v0.99955's redesign — a single pending-retrigger slot that a
 *     newer Row-1 key press can overwrite before an older one's 5ms
 *     window finalises, silently dropping the older retrigger. This
 *     surfaced during rapid Morph-slot A/B testing (repeatedly pressing
 *     number keys to switch patches), exactly the kind of fast repeated
 *     Row-1 activity that creates the overlap.
 *     Rather than making the single pending slot perfectly race-proof for
 *     every overlap, a fast, unconditional check now runs every loop()
 *     pass: if currentFreq is set and envPhase is IDLE, start the
 *     envelope right there. This resolves the symptom directly regardless
 *     of which path produced the mismatch, and is a much faster-acting
 *     safety net than the existing 1-second diagnostic, which only
 *     reports the problem rather than fixing it.
 *     v0.9996 opens a pass over HELP overlay content, the first item in
 *     the UI/UX final-check phase's own backlog. Three real, working
 *     shortcuts had no mention anywhere: Morph slot select (Shift+1..0),
 *     Tap Tempo (Shift+Enter, added v0.9994), and holding G0 for SONG
 *     mode — a player would have no way to discover any of these except
 *     by reading the firmware itself. PLAY's HELP is re-laid-out to fit
 *     all three within its existing 10-line budget, trading the
 *     redundant "hold H" half of "H/S+H:Help hold/latch" (self-evident
 *     from already being on this screen) for the room. SEQ's HELP gains
 *     the Tap Tempo mention it was missing too — it works there as well
 *     — trading the explicit "G0:PLAY" mention, the same key that got
 *     there in the first place and the natural inverse of the Space/G0
 *     toggle already documented. SONG's HELP is unchanged: Tap Tempo was
 *     built for PLAY and SEQ specifically, not SONG's own playback.
 *     v0.99963: Portamento and Arp (on/off, type, Tempo/Swing/Rate) are
 *     no longer part of a patch at all, on request — grouped with Bend
 *     as performance/operational settings the player controls directly,
 *     not something a patch should carry or overwrite on load. Save and
 *     load both gate every one of these keys on savingPatch/loadingPatch,
 *     matching the existing pattern already used for UI theme, MIDI CC
 *     assignments, and everything else this mechanism was built for.
 *     Randomize's own additions of these from a few versions ago are
 *     withdrawn to match — it randomizes a PATCH, and these are not part
 *     of one any more.
 *     Separately: PLAY's HELP now shows IMU X/Y's live on/off state
 *     inline on the existing "C:Porta A:IMU-X S:IMU-Y" line — asked for
 *     right after the Latch/Arp reorder, but the box has no spare row, so
 *     this replaces the static line rather than adding one.
 *     v0.99964 corrects that line and gives it proper room: A/S is IMU
 *     axis HOLD (freeze the current value), Shift+A/Shift+S is the axis
 *     ENABLE toggle — two different actions the single line conflated,
 *     labelling A/S as if they controlled on/off. The owner checked the
 *     actual hardware and found real spare space below the box (13+99=112
 *     against a 135px display), so rather than compress both meanings
 *     into one line again, the shared box grows 99->117px and a second
 *     row is inserted: "A:XHold S:YHold" plus a new "Sh+A:XEn(..)
 *     Sh+S:YEn(..)" showing live enable state with the correct keys.
 *     Named XHold/YHold here and NoteHold for D below, since both are
 *     called "Hold" for genuinely different things and sitting three
 *     lines apart made that easy to conflate before. SEQ and SONG's
 *     HELP screens share the same box and did not need the extra room;
 *     both still fit comfortably inside it.
 *     v0.99965: two follow-ups on the same HELP work. The shared box
 *     stayed at 117 for every screen after growing to fit PLAY's new IMU
 *     row, leaving SEQ and SONG — whose content still ends at y=98 — with
 *     a visible empty gap at the bottom. The box height is now chosen per
 *     screen: 99 for SEQ/SONG (their original size, still exactly enough),
 *     117 for PLAY, which is the only one that needed the extra room.
 *     Separately, SONG's HELP only supported holding H, not the Shift+H
 *     latch PLAY and SEQ both have — asked for to match them. Added the
 *     same detection (Shift+H vs plain h) and toggle SONG's own key
 *     handler was missing, reusing the same helpLatched/
 *     prevHelpLatchPressed globals PLAY/SEQ already use — a latch set on
 *     one screen now carries over to any of the three, which is the
 *     unified behaviour actually being asked for rather than three
 *     independent latches. SONG's HELP text gained a "Sh+H:Latch" mention
 *     to match.
 *     v0.99966: two follow-ups after checking on real hardware.
 *     PLAY's box still had about a line of slack, so its height is
 *     tightened 117->109 — 13+109=122, content ending at 116, the same
 *     ~6px bottom margin SEQ/SONG already have at their own 99.
 *     Separately: switching modes while H was held or latched (PLAY to
 *     SEQ, specifically going from the taller box to a shorter one) left
 *     the old box's bottom strip un-erased, visible underneath the new,
 *     shorter one — a real glitch introduced by v0.99965's per-mode
 *     sizing, since the two heights no longer matched, so the same
 *     region no longer got fully overwritten every draw. Fixed by always
 *     clearing the MAXIMUM footprint (109, PLAY's own height) to black
 *     first, unconditionally, before drawing the mode-appropriate
 *     smaller bordered box on top — the tight-to-content look for
 *     SEQ/SONG stays, but nothing from a taller previous frame can show
 *     through any more.
 *     Also confirmed as intentional, not a bug: Help only exists on
 *     PLAY/SEQ/SONG. Tab-ing to a screen without it (VCO, SETTINGS,
 *     etc.) while H is held just shows nothing for it there — those
 *     screens never call drawHelpOverlay() at all — and it reappears on
 *     returning to a screen that does.
 *     v0.99967: ARP/SEQ/SONG's Tempo and Swing all moved 5-unit steps to
 *     1-unit, on request — timing controls, too coarse to dial in
 *     precisely at 5 per tap. A bare 1-unit step makes a large change
 *     tedious on its own, so all three gained hold-to-repeat to cover
 *     that: ARP's, reached through the shared CATEGORY settings dispatch,
 *     now checks onIncrement!=onDecrement to tell a genuine two-direction
 *     value from a toggle (a toggle's shared single function is never
 *     unequal to itself) — repeating the former the same way VCF/VCA/
 *     LFO/FX already do, while leaving toggles (X Invert, X Curve, ...)
 *     exactly as edge-triggered as the comment there already explained
 *     they need to stay. SEQ's and SONG's live in their own separate
 *     handlers, not this shared dispatch, so each got the same
 *     menuKeyFire() treatment directly instead. All three reuse the same
 *     menuIncHeldMs/menuDecHeldMs globals CATEGORY's own value-editing
 *     already shares across VCF/VCA/LFO/FX — safe here too, since
 *     SETTINGS, SEQ and SONG are never the active screen simultaneously.
 *     v0.99968 fixes two things found testing v0.99967.
 *     SEQ's and SONG's Tempo/Swing did not actually repeat while held —
 *     the reused menuIncHeldMs/menuDecHeldMs were the WRONG pair. Those
 *     track '/' and ',' (updateMenuNavigation()'s own mI/mDe), not the
 *     ';'/'.' that SEQ's vInc/vDec and SONG's left/right actually are.
 *     updateMenuNavigation() runs unconditionally every frame regardless
 *     of appMode and clears menuIncHeldMs whenever '/' is not down —
 *     which, while holding ';' for SEQ or SONG's Tempo, is always — so it
 *     zeroed the repeat timer out from under this on literally the next
 *     frame: the initial press fired (menuKeyFire's own !prev branch),
 *     nothing after did, matching exactly what was reported. Repaired by
 *     using the pair that actually shares each key: menuUpHeldMs (';')
 *     and menuDownHeldMs ('.').
 *     Separately, Volume gained the same step-5%->1% and hold-to-repeat
 *     treatment as Tempo/Swing, on request. keyVolume's 'l'/'k' handling
 *     used to live inside updateOctaveAndVolume(), which only runs
 *     inside loop()'s keyChanged gate — the same reason SEQ/SONG's
 *     Tempo needed extracting in the first place, since a function only
 *     invoked when the key SET changes never runs again while a key is
 *     simply held steady. Pulled out into its own updateVolumeRepeat(),
 *     called unconditionally every loop() pass; the rest of
 *     updateOctaveAndVolume() is untouched. 'k'/'l' get their own
 *     dedicated volUpHeldMs/volDownHeldMs rather than reusing anything
 *     from CATEGORY's set — deliberately, to not repeat the exact mistake
 *     just found and fixed above.
 *     v0.99969: SEQ's per-step Velocity gets the same step 5->1 and
 *     hold-to-repeat treatment, on request — it uses the same ';'/'.'
 *     keys (vInc/vDec) as the Tempo/Swing case in this same function, so
 *     the same menuUpHeldMs/menuDownHeldMs pairing applies here too.
 *     v0.9997 responds to a serious new report: total input lockup after
 *     rapidly re-morphing (redirecting to a new target before the
 *     previous one finished) while ARP was playing — ARP would not stop,
 *     Latch would not release, no key did anything, yet audioTask kept
 *     logging normally throughout and afterward. That last part matters:
 *     it means audioTask itself was never the stuck one, and the hang has
 *     to be in loop() — which, being the thing that is stuck, has no way
 *     to report its own hang from inside it.
 *     A mirror of the existing audioTaskHeartbeatUs is added in the
 *     other direction: loopHeartbeatMs, set at the very top of every
 *     loop() pass (before anything that could hang, so a hang partway
 *     through still leaves proof loop() reached that point), checked
 *     from audioTask — genuinely guaranteed to keep running independently
 *     even if loop() is stuck — inside diagRecordBuffer(), the same place
 *     that already tracks its own per-second timing window.
 *     Separately, the log from the report showed morphDiagLog capped at
 *     exactly 48 samples — MORPH_DIAG_CAP — confirming a gap noted but
 *     left unaddressed when that diagnostic was built: redirecting to a
 *     new target before a previous morph finished never reset
 *     morphDiagCount, so repeated redirects (exactly what was being done
 *     when this happened) kept appending to the same buffer across all of
 *     them until it filled and stopped recording anything further.
 *     morphStart() now resets the counter on every call, including
 *     redirects, so each attempt gets its own clean window.
 *     Neither change is confirmed as the actual cause of the lockup — the
 *     heartbeat exists to find out what is, on the next occurrence,
 *     rather than guessing further from one log.
 *     v0.99971 finds an actual cause, from a much more useful report: no
 *     loop() heartbeat failure this time (confirming loop() itself was
 *     never stuck — the screen kept updating, IMU indicator included),
 *     but one specific note stuck sounding continuously, displayed the
 *     same way a Latched note is even with Latch off, ARP on, no morph
 *     involved this time at all.
 *     Traced to the v0.99958 self-healing safety net running unqualified.
 *     ARP's own triggerArpStep() sets currentFreq and envPhase=ATTACK
 *     together atomically, so there was never a gap there — but a gate
 *     shorter than 100% deliberately leaves a silent gap BETWEEN steps:
 *     envPhase legitimately reaches IDLE via its own release while
 *     currentFreq still holds the note that just finished, since nothing
 *     zeroes it for that gap on purpose. The safety net had no way to
 *     tell that gap apart from a genuine stuck note, so it re-attacked
 *     the stale frequency every single loop() pass — fighting ARP's own
 *     timing outright and getting stuck on whichever note happened to be
 *     playing the moment it first fired.
 *     Now excluded whenever arpEnabled or seqPlaying — both already have
 *     complete, correct control over currentFreq/envPhase on their own,
 *     including deliberate silent gaps this check was never meant to
 *     compete with. It was built for one specific case that has nothing
 *     to do with either: ordinary keyboard play's Row-1 pending-retrigger
 *     slot getting overwritten by a newer press before its 5ms window
 *     finalises. That case is untouched — this narrows where the net
 *     applies, not what it catches.
 *     v0.99972 responds to a follow-up report on v0.99971: a different
 *     symptom this time — total silence rather than a stuck note, "---"
 *     shown for the current note, screen still updating (no loop()
 *     heartbeat failure), ARP on. Possible that the v0.99971 narrowing
 *     unmasked a separate, pre-existing ARP issue that the old
 *     unqualified safety net had been accidentally papering over by
 *     force-retriggering something, however wrongly.
 *     Rather than guess at ARP's chord logic further, a lightweight
 *     diagnostic logs its own view once a second while arpEnabled:
 *     arpHeldCount, currentFreq, envPhase, appMode, and the raw keyboard
 *     word. Deliberately no array this time, learning from the earlier
 *     SD-mount regression a growing diagnostic array caused — this is a
 *     single periodic print with no state beyond a timestamp. If
 *     arpHeldCount reads 0 while real keys are visibly in the logged
 *     word, rebuildArpChord() is not seeing them — a different bug from
 *     anything fixed so far. If arpHeldCount is nonzero but nothing
 *     sounds, the fault is further down in updateArpTiming() or
 *     triggerArpStep() instead. Either way, the next occurrence's log
 *     should say which, rather than requiring another guess.
 *     v0.99973 corrects a gap in that same diagnostic, found from its own
 *     first capture: envPhase, currentFreq, heldCount and the raw
 *     keyboard word all stayed EXACTLY identical across four consecutive
 *     one-second samples — sound frozen mid-decay rather than silent this
 *     time — which is consistent with updateArpTiming() simply never
 *     running at all. But the diagnostic only checked arpEnabled, while
 *     the real call site requires the stricter
 *     !seqPlaying&&notesAllowed&&arpEnabled — so it kept firing and
 *     looking normal regardless of whether that actual condition was
 *     being met, hiding exactly the thing that would explain a freeze.
 *     seqPlaying and notesAllowed are now logged alongside everything
 *     else. If seqPlaying reads stuck true despite the player never
 *     intentionally using SEQ — plausible from the same class of
 *     keyboard-scan quirk already found and fixed for Shift+digit,
 *     Space being the key that toggles it — that would fully explain
 *     updateArpTiming() going silent while every other loop() pass
 *     continued normally.
 *     v0.99974: the seqPlaying theory was wrong — the follow-up log read
 *     seqPlaying=0, notesAllowed=1 throughout, meaning updateArpTiming()
 *     really was being called every frame, yet heldCount/cur/envPhase/
 *     the raw keyboard word all stayed frozen identically for 9+ seconds
 *     regardless. The stall is inside updateArpTiming() itself.
 *     ARP_RATES[arpRateIndex] is read with no bounds check, and a garbage
 *     .mult from an out-of-range index would explain an effectively
 *     infinite step interval — but every one of arpRateIndex's four write
 *     sites turned out to already be safely guarded except
 *     restoreGlobals()'s, which is now bounds-checked too (defensive;
 *     that restore only runs once at boot, unlikely to be this bug's
 *     actual mechanism given the freeze happens well into a live
 *     session). Nothing confirms the theory outright, so rather than ship
 *     a fix for an unconfirmed cause, updateArpTiming() now prints its
 *     own live values once a second: now, arpLastStepMs, the elapsed
 *     difference, stepMs, bpm, arpRateIndex, and whether the step
 *     condition evaluates true. The next freeze's log should show
 *     directly whether these are sane or garbage, and whether the
 *     elapsed-time condition ever reads true again once stuck.
 *     v0.99975 finds the actual root cause, and it is not in this file.
 *     The follow-up log showed stepMs/bpm/arpRateIndex all perfectly
 *     healthy throughout — updateArpTiming()'s own math was never the
 *     problem. What stayed frozen was the raw keyboard word itself: the
 *     same single key, reported held, for over a minute straight, with
 *     the player confirming NOTHING responded to any key during that
 *     window (not just the one that looked stuck). The Cardputer ADV
 *     reads its keyboard through a TCA8418 I2C keypad controller (a
 *     separate chip from the BMI270 IMU, which is why the IMU indicator
 *     kept moving throughout — a different device on the bus, unaffected).
 *     Independent reports from engineers working with the same TCA8418
 *     describe exactly this: I2C communication to the chip can go
 *     completely dead, most reliably triggered by fast key presses,
 *     recoverable only by pulling the chip's own RESET line — something
 *     M5Cardputer's library does not expose a way to do from software.
 *     This is a documented hardware/library-level quirk of the chip
 *     itself, not a C.P.S. bug, and not something fixable by editing
 *     this file's own logic.
 *     Without access to reset the chip directly, updateKeyboardWatchdog()
 *     is the best available mitigation: if the reported keyboard state is
 *     non-empty and does not change at all for 30 real seconds, the board
 *     restarts itself. An idle keyboard (nothing held) is explicitly
 *     exempt — that is normal, not stuck. What this cannot distinguish is
 *     a genuinely frozen TCA8418 from a player deliberately holding one
 *     long, unchanging note or chord for the same span of time; those
 *     look identical from software. The threshold is chosen to make that
 *     collision rare, not impossible — see the accompanying README/manual
 *     note this pairs with.
 *     v0.99976: the watchdog never fired — a 20+ second log window showed
 *     the same frozen "3i" throughout (heldCount and the arp itself
 *     confirming the two keys never actually changed) and no restart. The
 *     suspected cause: s.word's character ORDER is not guaranteed stable
 *     between reads of the exact same held key set — a check reading "3i"
 *     one second and "i3" the next would make a raw strcmp see those as
 *     different, resetting the timer every single time and never
 *     accumulating enough consecutive unchanged time to reach 30 seconds.
 *     Both buffers are now sorted before comparing, so the check depends
 *     only on WHICH keys are held, not what order the library happened to
 *     report them in. Confirmation logging added alongside it — one line
 *     when a change is detected, one every 5s while unchanged, showing
 *     the running duration against the threshold — so the next occurrence
 *     shows directly whether this was the actual cause rather than
 *     assuming the sort fixed it blind.
 *     v0.99977 is the UI/UX phase's diagnostic-log re-audit. Retired every
 *     investigation-specific diagnostic whose bug is now confirmed fixed
 *     — each had gone from useful to just serial-log noise for ordinary
 *     use: [tab] (the Tab-crash investigation, fixed v0.99957), [arp] and
 *     [arpTiming] (the TCA8418 freeze investigation — root cause found,
 *     not in this file; the watchdog is the mitigation, not these), and
 *     [retrigger] (the Shift+digit Row-1 race, fixed v0.99952 — this one
 *     fired on nearly every note played, Row 1 being the main octave).
 *     Also retired: [bitcrush], an even older diagnostic (v0.9902) whose
 *     own comment said to remove it once the cause was known, and
 *     bitcrush has since been exercised extensively with no further
 *     reports. The keyboard watchdog's per-change/every-5s confirmation
 *     logging is retired too, now that a real recovery has been observed
 *     firing correctly — left running it would print on nearly every
 *     keypress and every 5s of any held note; only the actual restart
 *     notice remains, unconditional, genuinely rare and worth knowing
 *     about. [Morph] (boot-time slot verification) and [morph] (the
 *     per-morph ring-buffer dump) stay — both still have ongoing
 *     diagnostic value and neither spams continuously. CPS_LOG_PATCH's
 *     own comment is updated to match what is actually left under it.
 *     v0.9998 adds a proper animated splash screen — the last item on
 *     the v1.0 list — replacing the single line of plain boot text that
 *     sat here since the very first version. The mark is a ring with a
 *     crosshair reaching to its edge (the finalized logo design: a
 *     circle, a thin inner ring, and four spokes to the outer edge —
 *     the same visual language as the IMU X/Y axis control this synth
 *     is built around), built once into a small sprite (splashLogo, a
 *     new global — an M5Canvas destructor would free the sprite buffer
 *     on function return otherwise, while the animation is still using
 *     it) so it can be rotated as one unit via pushRotateZoom() rather
 *     than recomputing rotated geometry by hand every frame. The mark
 *     starts tilted (-28 degrees, as if just picked up) and eases to
 *     level over ~900ms (cubic ease-out — fast start, gentle stop, the
 *     one-shot version of the exponential-approach shape used
 *     elsewhere in this file for continuous smoothing, here driven by
 *     elapsed wall-clock time instead of a per-buffer constant since it
 *     only runs once). Once level, "CARDPUTER SYNTH", the version, and
 *     a credit line ("100% vibe-coded with Claude") appear underneath,
 *     all hand-centered the same way every other text draw in this file
 *     already is — no text-datum API is used anywhere else here, so
 *     this doesn't introduce one either.
 *     This call sits at exactly the position the old single-line text
 *     did, right before buildWaveTables() and the SD mount — both still
 *     slow enough to want covering, and both still run with the
 *     finished splash on screen, same as before, just with more to look
 *     at while they do.
 *     vTaskDelay(1) between animation frames, not delay() — this
 *     project's own hard-won rule about delay() is specifically about
 *     loop()'s input-handling path (see the v0.99956-57 history), and
 *     neither risk that rule addresses applies here: this runs once in
 *     setup(), before loop() exists and before any input handling is
 *     possible. vTaskDelay(1) is used anyway simply because yielding to
 *     the scheduler for free is never worse than a bare spin-wait.
 *     UNVERIFIED on hardware — this is the project's first use of
 *     pushRotateZoom anywhere in the file; the call signature
 *     (dst_x, dst_y, angle, zoom_x, zoom_y) is confirmed correct against
 *     the M5GFX/LovyanGFX docs, but the visual timing, sizing, and
 *     legibility of the three text lines under the settled mark all
 *     need a real screen to judge.
 *     v0.9998, hardware-tested fixes: (1) SD card failed to mount —
 *     traced to the sprite creation and its sustained pushRotateZoom()
 *     activity happening before the mount, the same class of bug this
 *     project hit once before (v0.99953-54, a diagnostic struct growing
 *     by a few floats shifting memory layout enough to break the mount,
 *     never fully explained, just reliably fixed by reverting). The
 *     splash is now split: drawSplashBootText() is the original cheap
 *     plain-text version, called in the original early position before
 *     canvas/etc., buildWaveTables(), and the SD mount; the full rotating
 *     animation moved into drawSplashLogoAnimation(), now called AFTER
 *     the SD mount has already succeeded or failed. (2) Flicker during
 *     rotation — the per-frame fillScreen()+pushRotateZoom() were two
 *     separate, unbracketed display-bus writes, so the panel could show
 *     the intermediate all-black frame between them. Wrapped both in
 *     startWrite()/endWrite() so each frame is one atomic bus
 *     transaction, the same technique LovyanGFX's own rotation examples
 *     use. (3) The subtitle/version/credit lines were drawn back-to-back
 *     with nothing between the three drawString() calls, so despite the
 *     staggered reveal described when this was designed, nothing in the
 *     code actually paused between them — they always appeared together.
 *     Real vTaskDelay(pdMS_TO_TICKS(...)) pauses between each line fix
 *     that.
 *     v0.99981: the animation itself went missing on the next hardware
 *     test — the fallback text path (green "C.P.S." top-left, matching
 *     uiColor) confirmed the sprite allocation was now failing outright,
 *     even moved to after the SD mount. The likely reason: this board
 *     has no PSRAM at all (see the comment on the free-heap print right
 *     before the SD mount — internal DRAM is the only pool anything
 *     here draws from, measured at only ~31KB free right after the
 *     mount), and the previous fix still placed the sprite creation
 *     AFTER ensureCpsFolder()/loadSettings()/morphLoadAllSlots() — the
 *     last of which reads up to 10 patch files off the card, a
 *     reasonable next suspect for eating further into or fragmenting
 *     whatever the mount left free. Two changes: the sprite shrinks
 *     100x100 -> 70x70 (20,000 bytes -> 9,800, under half the ask), and
 *     its creation moves to immediately after initSDCard() itself,
 *     before that heavier loading block, giving it first claim on
 *     whatever memory survived the mount rather than whatever survives
 *     everything after it too. A Serial.printf on allocation failure
 *     now reports free heap and largest block, so if this still fails,
 *     the next log carries an actual number instead of another guess.
 *     v0.99982, a much more serious hardware report: no sound at all,
 *     with the boot log showing I2S DMA-buffer allocation failing in a
 *     tight repeating loop from the moment a key was first pressed.
 *     splashLogo was a global M5Canvas that createSprite() allocated but
 *     nothing ever freed — it sat there permanently, for the rest of the
 *     program's life, occupying part of the same small internal-DRAM
 *     pool (no PSRAM on this board at all) that the speaker's own I2S
 *     driver later needs to claim DMA-capable buffers from. The
 *     previous round's fix (shrinking the sprite) was solving the wrong
 *     half of the problem — the actual fix is not holding onto it past
 *     the moment this function is done with it: splashLogo.deleteSprite()
 *     now runs at the end, returning that memory before setup() moves on
 *     to whatever needs it next.
 *     Separately, the same report described the ring appearing to swing
 *     across the screen ("drawn from bottom-left toward top-right")
 *     rather than spin in place — consistent with pushRotateZoom()
 *     rotating around the sprite's top-left corner instead of its
 *     center, since no setPivot() call had ever been made and every
 *     LovyanGFX example that uses this function calls it explicitly
 *     rather than assuming a center default. splashLogo.setPivot(35,35)
 *     (the sprite's actual center) is now set right after creation.
 *     v0.99983: the pivot fix didn't hold up — a second hardware round
 *     showed the exact same swing/arc rendering. Rather than continue
 *     debugging pushRotateZoom blind, on request, the rotation is
 *     removed entirely: the finished mark is drawn once, statically,
 *     via the plain pushSprite() every other sprite in this file
 *     already uses successfully, and the sequential subtitle/version/
 *     credit reveal underneath — already confirmed working across every
 *     round so far — now carries the whole splash on its own. If motion
 *     is wanted again later, a brightness fade-in was discussed as the
 *     natural next thing to reach for: no per-pixel resampling, just
 *     scaling the same static draw's color values a few times, cheaper
 *     than a rotation transform and in the same risk category as
 *     everything already proven safe here.
 *     v0.99984, two follow-ups after the static logo was confirmed
 *     working. The stray boot text shown during the SD-mount window
 *     ("C.P.S. CardPuter Synth", flashing briefly before the real splash
 *     replaced it) is gone, on request — drawSplashBootText() now just
 *     fills the screen black for that window instead. Separately, the
 *     staggered per-line reveal from two rounds ago felt too fast; it is
 *     replaced with the fade-in floated as an option back when the
 *     rotation was first cut: the logo and all three text lines are now
 *     drawn together as one finished frame, dim, and the SCREEN
 *     BACKLIGHT itself ramps from UI_BRIGHT_MIN up to the user's
 *     configured brightness via setBrightness() over ~900ms, then holds
 *     at full brightness for another ~900ms so the finished screen reads
 *     clearly for close to 2 seconds total, not just the fade. No new or
 *     unproven API — setBrightness() is the same call
 *     applyUiBrightness() already uses elsewhere in this file, so this
 *     sidesteps pushRotateZoom entirely, the API every hardware surprise
 *     in this feature so far has come from.
 *     v0.99985, cosmetic follow-ups once the fade-in itself was working:
 *     the logo sprite grows 70x70 -> 90x90 (16,200 bytes, still under the
 *     original 100x100's 20,000 — safe now that it's freed right after
 *     use rather than held for the program's life, and this position
 *     after a successful SD mount was already hardware-confirmed at the
 *     smaller size). logoCy moves up 55->45 so the larger mark still
 *     clears the subtitle line below it. Separately, the fade read as
 *     too fast on real hardware: duration doubles 900ms->1800ms and the
 *     linear ramp is replaced with smoothstep (3p²-2p³), easing in and
 *     out at both ends instead of changing brightness at a constant
 *     rate — the gentler curve that was asked for.
 *     v0.99986, two follow-ups from the same hardware round: the 90x90
 *     logo sprite failed to allocate — falling back to the plain
 *     "C.P.S." text path — so this reverts to the 70x70 size already
 *     confirmed working across multiple earlier rounds, rather than
 *     guessing at a bigger size again without a failure-log number to
 *     size against.
 *     Separately, a saved brightness preference was found to apply the
 *     moment loadSettings() parsed it — landing in the middle of the
 *     splash's own fade-in and abruptly overriding it with whatever
 *     level was last saved, dim or otherwise, well before PLAY even
 *     appeared. applyUiBrightness() now checks a new
 *     bootBrightnessDeferred flag and skips the actual hardware call
 *     while it's set; setup() clears the flag and applies the real
 *     value exactly once, as its very last line — right as PLAY is
 *     about to appear, which is the moment being asked for. The splash's
 *     own fade is unaffected either way, since it calls
 *     M5Cardputer.Display.setBrightness() directly rather than through
 *     applyUiBrightness().
 *     v0.99987: trying bigger again, on request, but more cautiously
 *     than last time — 90x90 (v0.99985) failed outright, and there's
 *     still no failure-log number to size confidently against. 80x80 is
 *     12,800 bytes, roughly a third more than 70x70's 9,800 rather than
 *     90x90's ~65% more — an incremental step instead of jumping
 *     straight back to the size that just failed. If this also fails,
 *     the existing [Splash] alloc-FAILED print will finally give an
 *     actual free-heap number to size the next attempt against.
 *     v0.99988: that log arrived, and it changes the picture. Free heap
 *     after the SD mount was 24,528 bytes, but the LARGEST CONTIGUOUS
 *     BLOCK was only 11,764 — the heap is fragmented after SD.begin(),
 *     not simply short on total free memory, so the real ceiling for a
 *     single allocation like this sprite is the block size, not the
 *     free-heap figure. 80x80 (12,800 bytes) exceeded that block by
 *     just over 1,000 bytes, exactly explaining the failure. Sized to
 *     75x75 (11,250 bytes) — about 500 bytes of margin under the
 *     measured ceiling, since fragmentation can vary slightly boot to
 *     boot and 76x76 (11,552) would leave almost none. Going bigger
 *     than this would mean addressing the fragmentation itself, not
 *     picking a larger number — a different problem than the one this
 *     round's log answered.
 *     v0.99989: the credit line ("100% vibe-coded with Claude") is
 *     removed, on request — it read as too prominent sitting right
 *     below the version number. The mark, subtitle, and version remain.
 *     v1.0: the official release. No functional change from v0.99989 —
 *     this is the version number itself moving, marking every feature,
 *     fix, and piece of documentation built across the v0.9.x line
 *     (waveform library, Shape, Osc2, the FX tab, Morphing, MIDI,
 *     Theremin, Drift, the splash screen, and the round after round of
 *     hardware-tested fixes behind all of it) as done.
 *     v0.99961: PLAY's HELP line listed Latch before Arp
 *     ("D:Hold V:Latch Sh+V:Arp"), backwards from how they actually
 *     relate — Latch presupposes Arp is already running, so reads more
 *     naturally the other way around. Swapped to "D:Hold Sh+V:Arp
 *     V:Latch". No key binding changed, display order only.
 *     SETTING menu hierarchy reviewed as the next UI/UX backlog item —
 *     category list, per-category item counts, and the one existing
 *     two-level split (MIDI Out/In) all checked out with nothing to
 *     restructure; the owner had no specific friction to point at
 *     either, so this one closes without a code change.
 *     v0.99962: Randomize's own v0.993 audit comment already flagged
 *     that features added after it was written kept slipping past it —
 *     Portamento (porta_enabled/porta_speed) and Arp's own
 *     Tempo/Swing/Rate turned out to be the next ones, found while
 *     checking Randomize's coverage for the UI/UX pass. All are saved as
 *     part of a patch, same as everything else this function already
 *     covers, so the omission was real.
 *     Neither joins the general "randomize freely" treatment. Portamento
 *     is an on/off decision first — most patches do not want a permanent
 *     glide — and only gets a speed when it lands on. Tempo is left alone
 *     entirely: retuning the whole instrument's tempo out from under
 *     whatever the player was doing would be a far bigger surprise than
 *     a random filter cutoff, and tempo is something the player already
 *     controls directly far more often than they author it into a patch
 *     — including via Tap Tempo, added this same phase.
 *   - IMU (BMI270) tilt-to-parameter mapping across 17 targets (customizable,
 *     hold state and frozen value persist across save/load), selected via
 *     a scrollable picker grouped by category (Pitch/Volume/Timbre/Filter/LFO/Effect)
 *   - Per-axis IMU fine control: Sensitivity, Invert, response Curve
 *     (Linear/Exponential), Deadzone, and a Calibrate ON/OFF toggle
 *     (ON opens a confirm dialog and re-zeros to current tilt, OFF resets)
 *     [CardputerADV only — see below for original Cardputer]
 *   - IMU Volume target is now a relative multiplier of the current
 *     volume (0-100%), so it can only attenuate, never exceed the set level
 *   - Patch Bank: save/load full synth state (incl. IMU mapping/hold
 *     state and portamento) as named patches under /CPS/Patch.
 *     Rename, duplicate and delete are available from the Patch Bank
 *     screen (SETTING menu). The app never navigates outside this
 *     dedicated folder.
 *   - Pattern Bank (Phase 4): save/load Sequencer patterns (16 steps +
 *     Tempo/Swing) to/from a grid of 8 lettered banks (A-H) x 8 numbered
 *     slots (1-8), stored under /CPS/Pattern as one file per slot
 *     (e.g. /CPS/Pattern/A1.json). Accessed via SETTING > Pattern >
 *     Save/Load, a grid browser (own AppMode::PATTERN, own Tab handling
 *     like Patch Bank) — navigate with ;/./,//,  confirm with Enter,
 *     Backspace clears the selected slot (with a Y/N confirm, same for
 *     overwriting an occupied slot on Save). Reuses the exact same
 *     seq_tempo/seq_swing/seqN_* field names the main settings file
 *     already uses for the Sequencer, via a shared writeSeqPatternFields()
 *     helper (save) and the existing parseSettingLine() (load) — a
 *     pattern file is really just a tiny settings file scoped to only
 *     those fields. SETTING > Pattern > Random generates a fresh 16-step
 *     pattern in-key with the current scale (picked from the same
 *     row1Freqs/row2Freqs tables note entry uses), including Tie/Accent/
 *     Slide, not just pitch/velocity — behind the same confirm dialog as
 *     Randomize Patch. Tempo/Swing are left untouched (pattern-level, not
 *     part of "randomize the steps"). Randomize Patch's own Noise amount
 *     is deliberately rare (15% chance) and subtle (5-20%) when it does
 *     apply — full 0-100% randomization made pitch clarity noticeably
 *     worse even at seemingly-low rolls. SETTING > Pattern is hidden
 *     while PLAY is the active home mode (only meaningful from SEQ),
 *     the mirror image of Arp being hidden from SEQ — needed splitting
 *     original Cardputer's settings list into PLAY/SEQ variants too,
 *     which it didn't have before (Pattern showed regardless of home
 *     mode there).
 *   - Song mode (Phase 4 continued, v0.95-v0.953): arranges saved
 *     Pattern Bank patterns into a sequence and plays them back. Its own
 *     AppMode::SONG, entered via a LONG press (500ms) of G0 — short
 *     press keeps the existing PLAY<->SEQ toggle unchanged, now resolved
 *     on release instead of press-down so the two can be told apart.
 *     Has its own fixed UI color (cyan) rather than inheriting PLAY's
 *     green or SEQ's orange, since it's a distinct third mode — `uiColor`
 *     is computed once per loop() iteration, AFTER all of that frame's
 *     mode-changing input (G0, Tab-cycle, and each mode's own Tab
 *     handling) has already been processed, not at the very top of
 *     loop() — computing it too early meant a mode change and its color
 *     update landed on different frames, causing a stale/mixed-color
 *     flash right at the moment of switching (fixed in v0.954). Each song
 *     entry references a Pattern Bank slot (bank+slot) plus its own
 *     Transpose (semitones, chromatic — consistent with how Octave/
 *     Transpose already works elsewhere, not a scale-aware diatonic
 *     shift) and Repeat count (times through before advancing) — a new
 *     entry inherits the Bank/Slot of whichever entry the cursor was on
 *     (handy for building similar-pattern sequences), but Transpose/
 *     Repeat reset to defaults rather than also carrying over. Two
 *     global toggles: whether each entry uses its own saved pattern's
 *     Tempo/Swing ('I' key, on by default — patterns already save these,
 *     so this comes for free) or a dedicated Song-level Tempo/Swing
 *     instead throughout, and whether the song loops back to the start
 *     after the last entry or stops ('O' key, loop by default). Editor
 *     has two focuses (like SEQ's STEP/PATTERN split): 'f' toggles
 *     between Entry focus (,// moves the entry cursor — matching the
 *     horizontal timeline's own layout, swapped from SEQ's convention in
 *     v0.961 per user feedback — 'g' cycles which field — Bank/Slot/
 *     Transpose/Repeat — ;/. adjusts it) and Song
 *     focus ('g' cycles Tempo/Swing instead, ;/. adjusts it) — the
 *     Song-level Tempo/Swing and Volume are always shown regardless of
 *     focus, matching how SEQ always shows every value rather than
 *     hiding the non-selected ones. Volume (k/l) and other non-
 *     conflicting performance keys work here too, same "usable anywhere
 *     except Patch" principle as elsewhere — Shift+S/Shift+L are
 *     reserved for Save/Load specifically in SONG, so the plain
 *     IMU-Y-hold/Volume-up meanings of 's'/'l' are skipped there, same
 *     pattern as SEQ's Shift+C/Shift+X reservations. Enter inserts a new
 *     entry after the cursor, Backspace deletes the selected entry,
 *     Space plays/stops the song, Shift+S/Shift+L open a Save/Load slot
 *     picker (8 numbered slots, no lettered banks — fewer songs expected
 *     than patterns) under /CPS/Song, Tab returns to whichever of
 *     PLAY/SEQ was home.
 *     Visual design (v0.96): rather than a plain text list, entries show
 *     as a horizontal timeline of fixed-width blocks colored by Bank
 *     letter (A-H each get a distinct fixed color, songBankColors[]) —
 *     the playing entry's block turns white, the edit cursor's block
 *     gets a white outline instead (so both can show distinctly even on
 *     the same block); a thin bar under each block is proportional to
 *     that entry's Repeat count. Below the timeline, a small step-grid
 *     preview (mirroring SEQ's own grid look, but simplified — filled=
 *     note, half-height=Tie, no velocity/accent detail) shows the actual
 *     shape of whichever pattern is currently playing, or the cursor's
 *     entry when stopped — read via a new loadPatternPreview() into a
 *     dedicated songPreviewSteps[] buffer (cached by bank+slot) so
 *     browsing entries in the editor never disturbs the live seqSteps[]
 *     that may actually be playing.
 *     Playback engine reuses SEQ's own step-timing engine (
 *     updateSeqTiming(), seqPlaying) entirely: loading an entry
 *     (songLoadEntry()) pulls that pattern in via the existing Pattern
 *     Bank load function, applies the entry's Transpose as a multiplier
 *     on newly-triggered pitches, and completing a full 16-step pass
 *     (songAdvanceOnPassComplete(), called from updateSeqTiming()) counts
 *     against the entry's Repeat count before advancing (or looping/
 *     stopping at the end of the song). Deliberately simple/no pre-fetch
 *     — each entry transition does a synchronous SD card read; verified
 *     on hardware with no audible hiccup at pattern boundaries so far.
 *     SEQ also has step Copy/Cut/Paste: 'V' marks/confirms/clears a
 *     step-range selection (shown as a yellow strip above the selected
 *     steps), extended live by ,// while marking; Shift+C copies the
 *     selection, Shift+X cuts it (clears the source to rests), Enter
 *     pastes the clipboard starting at the cursor (truncated if it would
 *     run past step 16). Reusing 'V'/Shift+C/Shift+X this way meant
 *     freeing them from their usual meanings specifically within SEQ:
 *     plain 'V' no longer toggles Arp Latch there (Shift+V still toggles
 *     Arp on/off everywhere, unchanged), and Shift+C/Shift+X no longer
 *     also fire Portamento-toggle/Bend-up there — chosen because Arp
 *     Latch has no audible effect during SEQ anyway (Arp is suppressed
 *     while a pattern plays), same reasoning as hiding SETTING > Arp
 *     from SEQ.
 *   - Reset to default: Patch category resets VCO/VCF/VCA/LFO/IMU to a
 *     simple starting patch (behind a confirm dialog); Bend and
 *     Portamento categories each have their own separate reset
 *   - Randomize: Patch category can also randomize every tone parameter
 *     (incl. filter type, LFO wave/target, IMU targets) behind a confirm
 *     dialog — a quick way to discover new sounds
 *   - Play Mode (SETTING > Play Mode): EZ Mode (default) vs Pro Mode,
 *     see above
 *   - Scale (SETTING > Play Mode > Scale, Pro Mode only — EZ Mode is
 *     always Major): choose from 49 scales across 9 categories
 *     (Chromatic, Classical, Symmetrical, Pentatonic, Japan, China,
 *     India, Middle East, Europe) via a 2-level picker (v0.954 added 16
 *     more — Harmonic/Neapolitan Major, Lydian Augmented/Dominant,
 *     2 Messiaen modes, 2 more pentatonics, Ahir Bhairav/Marva/Purvi/
 *     Charukeshi, Nikriz/Persian, Romanian Minor/Hungarian Major — all
 *     appended after the existing entries rather than inserted, so any
 *     already-saved currentScaleIndex stays pointing at the same scale).
 *     New scales are always appended, never inserted, for this reason.
 *     The per-category index buffer used by the picker was bumped from
 *     16 to 32 slots to leave headroom for further additions — Classical
 *     is the largest category so far at 13. Selecting a
 *     scale takes effect immediately, so holding a note key while
 *     scrolling previews it live. Current scale (with category) is
 *     shown on the MAIN screen.
 *   - Arpeggiator (SETTING > Arp, CardputerADV only): hold up to 6 notes
 *     at once as a chord; Up / Down / Up-Down / As Played / Random
 *     patterns, adjustable Tempo (40-240 BPM), Rate (note length per
 *     step, 1/1 to 1/32 incl. two triplets), and Swing (-100% to +100%:
 *     positive delays the off-beat step for a classic swung feel,
 *     negative pushes it earlier for a pushed/anticipated feel). Tempo
 *     and Swing can also be assigned as IMU targets — both are always
 *     bipolar (+/- the base value) regardless of the axis's own
 *     Invert/bipolar setting, same as Pitch Bend. Each step
 *     force-retriggers the envelope for a percussive, stepped feel.
 *     'V' key toggles Latch mode: each note-key press toggles that
 *     note's membership in the chord, instead of needing continuous
 *     physical holds (easier on a small keyboard). Shift+'V' toggles
 *     the Arpeggiator on/off, usable on any screen except the Patch
 *     Bank (the redundant SETTING > Arp on/off toggle was removed once
 *     this covered every screen). PLAY screen lists
 *     every held note (press order) with the currently-sounding one
 *     highlighted, in place of the normal single note-name display.
 *     Since notes now work on every screen (see above), this preview
 *     works everywhere too — including while adjusting Arp's own
 *     settings, without needing to back out to PLAY first.
 *   - Step Sequencer: a 16-step, TB-303-style pattern. Each step has a
 *     Note (or Rest), Velocity, and three performance flags: Tie
 *     (extends the previous note instead of retriggering — chaining
 *     consecutive Tie steps is how a note's length varies, instead of a
 *     Gate percentage), Slide (glides from the previous pitch to this
 *     one instead of retriggering, via its own lightweight portamento-
 *     style glide independent of the global Portamento toggle), and
 *     Accent (boosts this step's Velocity and gives the filter cutoff a
 *     temporary boost, for the classic TB-303 "punch"). Has its own
 *     independent Tempo and Swing (separate from the Arpeggiator's —
 *     PLAY and SEQ are treated as distinct performance modes with their
 *     own timing), though assigning the Arpeggiator's Tempo/Swing IMU
 *     targets controls the Sequencer's instead whenever SEQ is the
 *     active home mode — one target per axis works contextually for
 *     both. Works on both boards (unlike the Arpeggiator) since
 *     entering one note per step doesn't need multi-key rollover.
 *     Accessed via its own SEQ mode (see G0 button below) rather than
 *     the Tab cycle, since it's conceptually a second "home" screen
 *     alongside PLAY, not another editor tab. The Arp SETTING entry is
 *     hidden while SEQ is the active home mode, since the Arpeggiator
 *     needs live chord-holding that SEQ playback suppresses.
 *     Orange accent color applies to the ENTIRE UI (not just the SEQ
 *     screen itself) whenever SEQ is the active home mode — even while
 *     viewing VCO/VCF/etc — so it's always visible at a glance which
 *     mode you're in; a first step toward user-customizable UI colors,
 *     planned for later.
 *     The SEQ screen otherwise mirrors PLAY's layout exactly (no
 *     waveform, but the same IMU/PAD block, gauge bars, Bend meter, and
 *     Scale name display on the right/bottom, working identically):
 *     step grid where the waveform would be — each step shows Velocity
 *     as a bottom-aligned bar (taller = louder); a run of Tie-connected
 *     steps merges into one shape (thick outer border, no internal
 *     vertical line at the join) so it visibly reads as one sustained
 *     note, while each step's own bar segment still shows (using the
 *     run's starting velocity) so the cursor/playhead can still pick out
 *     individual steps within it; Accent turns the bar red instead of
 *     the usual orange (a shape-based indicator — a triangle top — was
 *     tried first but proved hard to distinguish at 13px step width;
 *     color reads reliably at any size). Slide gets a small diagonal
 *     notch at the bottom-left corner. Velocity carries over from
 *     whatever was last explicitly set to the next note you enter,
 *     instead of resetting to the 100 default each time. Then step/
 *     tempo/swing/octave/transpose/portamento/hold details on the left,
 *     all always visible regardless of what's currently selected to
 *     edit. Full key reference is in the HELP overlay ('H', same as
 *     PLAY) rather than fixed on-screen text.
 *     The Bend meter (used by
 *     both PLAY and SEQ) fits its UP/DWN labels within the same
 *     vertical footprint as the other side blocks now, instead of
 *     spilling into the waveform/step-grid area above and below the box.
 *     Sequencer Play/Stop (Space) works from any screen except Patch,
 *     not just from SEQ itself, so playback can be started/stopped
 *     while tweaking VCO/VCF/etc.
 *     Controls: ','/'/' move the step cursor, note keys assign that
 *     pitch to the selected step (playing a brief preview so you can
 *     hear it, and auto-advancing the cursor to the next step — use
 *     ,// instead of a note key to skip a step and leave it as a rest),
 *     Backspace clears the selected step entirely (note + Tie/Slide/
 *     Accent) back to a plain rest, Shift+Backspace clears the whole
 *     16-step pattern at once, Space starts/stops playback. ';'/'.'
 *     adjusts one of two separate
 *     "focuses" (kept conceptually distinct, not one flat list): STEP
 *     focus cycles through the selected step's Velocity (adjusted
 *     numerically), Tie, Slide, and Accent (each toggled on/off by
 *     either ';' or '.'); PATTERN focus adjusts the whole sequence's
 *     Tempo or Swing. 'f' toggles which focus is active, 'g' cycles
 *     which of the current focus's values ';'/'.' affects. All the
 *     step's values, plus Tempo/Swing, Octave/Transpose, and Portamento/
 *     Hold status (both usable here too, same as PLAY), are always
 *     shown on screen regardless of focus — only a single "Ed:" label
 *     indicates which one is currently adjustable. Shift +
 *     ';'/'.'/','/'/ ' adjusts Octave/Transpose on CardputerADV,
 *     mirroring PLAY's unshifted keys for muscle-memory consistency
 *     (original Cardputer's J/N/B/M don't need Shift, since they don't
 *     collide with SEQ's own keys).
 *     While playing, the Sequencer keeps looping even on other screens
 *     (VCO/VCF/etc, but not SEQ's own editing), so tone/filter changes
 *     can be heard against the pattern — normal note-triggering and the
 *     Arpeggiator are suppressed while it's playing, to avoid fighting
 *     over which note is currently sounding. Volume ('k'/'l', shown on
 *     screen too) now works on every screen except the Patch Bank, same
 *     as note-triggering. Assigning the same "ArpTempo"/"ArpSwing" IMU
 *     targets used by the Arpeggiator instead controls the Sequencer's
 *     own Tempo/Swing whenever SEQ is the active home mode — one target
 *     per axis works contextually for both, no separate assignment
 *     needed per mode. Switching between PLAY and SEQ via G0 silences
 *     whatever was sounding (sequencer playback, an Arp chord, or a
 *     held note via Note Hold), since they're treated as distinct modes.
 *   - G0 button (physically separate from the keyboard, so it can't be
 *     hit by accident while playing/typing): toggles between PLAY and
 *     SEQ from anywhere. Whichever was toggled to last becomes the
 *     "home" position that Tab cycling returns to after SETTINGS.
 *   - Tab key cycles: PLAY/SEQ (whichever is current) -> VCO -> VCF ->
 *     VCA -> LFO -> SETTINGS -> back to PLAY/SEQ. Shift+Tab cycles the
 *     same chain backward (v0.97) — same edge-tracker, just a reversed
 *     switch statement when Shift is held. PATCH/PATTERN/SONG each
 *     handle their own Tab (always "back", no forward/reverse
 *     distinction there) and aren't part of this cycle.
 *   - Auto-save / auto-load via SD card (/CPS/settings.json)
 *   - /CPS folder created automatically on first boot
 *
 * Original Cardputer (no IMU, GPIO-matrix keyboard):
 *   Board type is auto-detected at boot (M5.getBoard()). On original
 *   Cardputer:
 *   - Octave shift moves to 'J' (up) / 'N' (down); Transpose moves to
 *     'M' (up) / 'B' (down) — freeing ';' '.' ',' '/' for PAD control
 *   - IMU is replaced by a key-driven "PAD": ';' / '.' move a virtual
 *     Y axis up/down, ',' / '/' move a virtual X axis left/right.
 *     Moves toward the extreme while held, springs back to center on
 *     release — unless that axis's Hold ('A'/'S', unchanged) is on, in
 *     which case it stays wherever it was instead of springing back.
 *     Everywhere the UI said "IMU" now says "PAD" instead.
 *   - Deadzone and Calibrate are hidden from the PAD sub-menu — neither
 *     concept applies to a clean key-driven signal (no sensor noise to
 *     filter, no physical zero-point to correct)
 *   - Arpeggiator is not available: it needs multi-key chord holding,
 *     which the original's 3-key rollover limit can't reliably support
 *   - IMPORTANT: the original Cardputer's GPIO-matrix keyboard only
 *     reliably supports 3 simultaneous key presses. Pressing a 4th key
 *     at the same time can cause "ghosting" (incorrect/missing key
 *     detection) — this is a hardware limitation of the original
 *     Cardputer itself and cannot be fully corrected in software, since
 *     the ambiguity already exists by the time a key press reaches this
 *     app. Keep this in mind with combinations like note + PAD + Bend.
 *
 * Display rendering: every screen draws into a single off-screen canvas
 * (M5Canvas, 240x135, in internal DRAM — there is no PSRAM on this
 * hardware, whatever this comment used to claim) and pushes the frame to the
 * display in one single transfer, instead of many small direct draws —
 * this eliminates a diagonal tearing/flicker artifact that was visible
 * whenever several UI elements updated in the same frame. Shared drawing
 * helpers (drawTabBar, drawWaveform, drawBendMeter, drawImuPad,
 * drawHelpOverlay) take a LovyanGFX& target parameter, a holdover from
 * when PLAY/SEQ were converted first and other screens still drew
 * directly to the display; now that everything uses the canvas, they
 * could be simplified to call it directly, but the parameter is
 * harmless to keep.
 *
 * PLAY screen specifically also uses a dirty-rect split into four
 * independently pushed canvases: canvasTop (y=0-54: tab bar + waveform,
 * skipped when the waveform hasn't meaningfully changed), canvasName
 * (x=0-73, y=55-112: note info, always pushed since that's what changes
 * on every note keypress), canvasImu (x=73-240, y=55-112: bend meter,
 * IMU pad+readout, volume — skipped when none of those values actually
 * changed, which is the common "IMU=None, just playing notes" case),
 * and canvasNav (y=113-134: scale name + nav text, rarely changes).
 * This was needed because even DMA-based SPI transfers still consume
 * shared memory-bus bandwidth long enough to occasionally overlap
 * audioTask's real-time budget on Core 0, audible as an intermittent
 * crackle correlated with key presses; reducing push FREQUENCY alone (a
 * redraw-rate throttle) didn't fully resolve it, since each full push
 * still took its fixed transfer time whenever it did happen, and even
 * the first (2-way) split didn't help much since canvasBottom back then
 * still bundled note info together with IMU/bend/volume, so it still
 * pushed on every note regardless of whether IMU/bend/volume changed.
 * drawBendMeter/drawImuPad take optional yOff/xOff parameters (default
 * 0, unused by SEQ/HELP which still use the single full canvas) so the
 * same functions work against canvasImu's offset coordinate space too
 * (PLAY, yOff=-55, xOff=-73).
 *
 * Required library: M5Cardputer (uses M5Unified / M5GFX internally)
 */

#include "M5Cardputer.h"
#include <math.h>
#include <string.h>   // memset() — clearFxBuffers() (v0.9875)
#include <stdarg.h>   // va_list — sbAppend() (v0.9913)
#include <Wire.h>
#include <VL53L1X.h>   // theremin distance sensor (v0.999)
#include <esp_system.h>   // esp_reset_reason() (v0.99913)

// ---- Diagnostic logging (v0.9989) ----
//
// These logs have found several real faults — the SD-mount failure, the
// stale morph snapshots, whether MIDI bytes were arriving at all — so they
// are kept rather than deleted. But MIDI clock arrives 24 times per BEAT,
// which at 120bpm is 48 bytes a second of nothing but timing, and the
// per-second byte counter would then never fall silent. A log that always
// says something says nothing.
//
// So each group can be switched off independently at build time. Default
// is on, because this is still pre-1.0 and the next fault has not happened
// yet; the point is that they can now be silenced without deleting the
// code that would have to be rewritten to investigate the fault after it.
#ifndef CPS_LOG_AUDIO
#define CPS_LOG_AUDIO 1     // [audioTask] buffer timing
#endif
#ifndef CPS_LOG_MIDI
#define CPS_LOG_MIDI 1      // [MIDI] rx byte counts
#endif
#ifndef CPS_LOG_PATCH
// [Morph] slot loads (boot-time patch-snapshot verification), [morph]
// (per-morph diagnostic ring buffer, dumped on completion). A UI/UX-phase
// pass (v0.9997x) retired several investigation-specific diagnostics that
// had done their job — [tab], [arp], [arpTiming], [retrigger], [bitcrush],
// and the watchdog's per-change/per-5s confirmation logging — since each
// was fixing a since-resolved bug and had gone from useful to just noise
// for ordinary use.
#define CPS_LOG_PATCH 1
#endif
#if CPS_USB_MIDI
#include <Adafruit_TinyUSB.h>   // composite CDC+MIDI device (v0.9961)
#endif
// Settings file is ~3KB today; 8KB leaves room for the Morph chain and
// sequencer fields to grow without needing another look.
constexpr int SETTINGS_BUF_SIZE = 8192;
#include <SPI.h>
#include <SD.h>

// Off-screen canvas (sprite buffer): every screen draws into this fully
// in memory, then pushes the finished frame to the display in one single
// SPI transfer via pushSprite(). This eliminates the tearing/flicker
// that came from drawing many small regions directly to the display one
// at a time (visible as a diagonal "wipe" while the panel's own scan-out
// caught mid-update content). Sized to the full physical display
// (240x135) and created in setup().
M5Canvas canvas(&M5Cardputer.Display);
// Splash-screen logo sprite (v0.9998) — built once in
// drawSplashLogoAnimation(),
// rotated as a whole unit via pushRotateZoom() during the boot animation.
// Global (not local to that function) because M5Canvas's destructor would
// otherwise free the sprite's buffer the moment the function returns,
// while the animation is still running.
M5Canvas splashLogo(&M5Cardputer.Display);
// PLAY-only dirty-rect split (v0.937, further split in v0.9372 — see
// canvasName/canvasImu/canvasNav below): PLAY is redrawn far more often
// than any other screen (every note key press), so its 63KB full-canvas
// push was the main contributor to an audible crackle correlated with
// key presses — SPI-DMA bus time for a ~65KB transfer occasionally
// overlapped audioTask's real-time budget on Core 0. canvasTop (tab bar
// + waveform, y=0-54, no coordinate offset needed — already 0-based) is
// pushed independently from the rest, and skipped entirely when nothing
// is modulating Timbre/PWM. Other screens (SEQ/VCO/etc) and the HELP
// overlay still use the single full-size `canvas` above, unchanged.
M5Canvas canvasTop(&M5Cardputer.Display);
// v0.9372: the original single "everything below the waveform" canvas
// still transferred on EVERY note keypress even with IMU=None, since it
// bundled note info (changes every note) together with IMU/bend/volume
// (often static). Split further into canvasName (note info only, ~8KB,
// still pushed every note) and canvasImu (bend+IMU pad+readout+volume,
// ~27KB, only pushed when THAT content actually changes) so playing
// notes with IMU=None only transfers canvasName. canvasNav (scale name +
// nav text, rarely changes) split out too so it doesn't need to ride
// along on every canvasName/canvasImu push.
M5Canvas canvasName(&M5Cardputer.Display);
M5Canvas canvasImu(&M5Cardputer.Display);
M5Canvas canvasNav(&M5Cardputer.Display);
constexpr int BOTTOM_Y_OFFSET=55; // shared by canvasName/canvasImu (both start at absolute y=55)
constexpr int IMU_X_OFFSET=73;    // canvasImu starts at absolute x=73
constexpr int NAV_Y_OFFSET=113;   // canvasNav starts at absolute y=113

// ---------------------------------------------------------
// SD card pin configuration
// ---------------------------------------------------------
#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12
static const char *CPS_FOLDER_PATH = "/CPS";

// ---------------------------------------------------------
// Audio settings
// ---------------------------------------------------------
static constexpr int SAMPLE_RATE     = 44100;
static constexpr int WAVE_TABLE_SIZE = 256;

int16_t sineTable[WAVE_TABLE_SIZE];
int16_t triangleTable[WAVE_TABLE_SIZE];
int16_t sawtoothTable[WAVE_TABLE_SIZE];
int16_t squareTable[WAVE_TABLE_SIZE];
int16_t wavefolderTable[WAVE_TABLE_SIZE]; // v0.983: 5th morph waveform
int16_t halfSineTable[WAVE_TABLE_SIZE];   // v0.9831: 6th morph waveform

// v0.985: Shape control. Each waveform has a second table representing
// its "Shape=1" extreme; the live Shape knob (0-1) interpolates each
// table sample between its A (Shape=0) and B (Shape=1) variant BEFORE
// the morph blend runs, the same interpolate-by-index approach already
// used for morphing between waveforms. This absorbs the old PWM control
// entirely — Square's own Shape variants ARE what PWM used to be
// (duty cycle 50%->10%), just reframed as one instance of the same
// per-waveform mechanism every other waveform now also gets.
int16_t sineTableB[WAVE_TABLE_SIZE];
int16_t triangleTableB[WAVE_TABLE_SIZE];
int16_t sawtoothTableB[WAVE_TABLE_SIZE];
int16_t wavefolderTableB[WAVE_TABLE_SIZE];
int16_t halfSineTableB[WAVE_TABLE_SIZE];
// v0.986: 2 more waveforms. Shape=1 variants for these two are simply
// the same as Shape=0 for now (Shape has no effect on them yet) — kept
// simple/low-risk for this round; dedicated Shape curves for these can
// follow later the same way the original 6 got theirs.
int16_t parabolicTable[WAVE_TABLE_SIZE];
int16_t esawTable[WAVE_TABLE_SIZE];
// v0.9862: 4 more waveforms, all built via simple phase-distortion /
// waveshaping formulas (a sine whose phase or amplitude is warped by a
// second sine) — a classic, cheap (boot-time only) way to get distinct
// characters without needing full harmonic-series summation each time.
int16_t squeezeTable[WAVE_TABLE_SIZE];
int16_t esquareTable[WAVE_TABLE_SIZE];
int16_t saw2Table[WAVE_TABLE_SIZE];
int16_t square2Table[WAVE_TABLE_SIZE];
// v0.9863: Shape=1 tables for the 6 waveforms that didn't have one yet.
// Each uses the SAME formula family as its Shape=0 table, just with the
// defining parameter intensified — the same "turn the same knob further"
// approach already used for Wavefolder's own Shape=1 (higher drive).
int16_t parabolicTableB[WAVE_TABLE_SIZE];
int16_t esawTableB[WAVE_TABLE_SIZE];
int16_t squeezeTableB[WAVE_TABLE_SIZE];
int16_t esquareTableB[WAVE_TABLE_SIZE];
int16_t saw2TableB[WAVE_TABLE_SIZE];
int16_t square2TableB[WAVE_TABLE_SIZE];

// ---------------------------------------------------------
// Timbre Morph Order (v0.984): the oscillator's Timbre knob morphs
// between a user-chosen, user-ordered SUBSET of the waveform library
// (4-6 slots), rather than a fixed sequence through all of them. This
// keeps each added waveform "affordable" (the morph range grows with
// the whole library otherwise) and lets each person pick a set that
// makes musical sense together. `morphChain[]`/`morphChainLen` hold the
// active order; the full library (`OscWaveform`/names/tables) grows
// independently as more waveforms are added later.
// ---------------------------------------------------------
enum class OscWaveform : uint8_t { SINE, TRIANGLE, SAWTOOTH, SQUARE, WAVEFOLDER, HALFSINE, PARABOLIC, ESAW, SQUEEZE, ESQUARE, SAW2, SQUARE2 };
constexpr int NUM_OSC_WAVEFORMS = 12; // grows as the waveform library expands
constexpr int MAX_MORPH_SLOTS = 6;
constexpr int MIN_MORPH_SLOTS = 4;
const char *OSC_WAVEFORM_NAMES[NUM_OSC_WAVEFORMS] = {"Sine","Triangle","Sawtooth","Square","Wavefolder","HalfSine","Parabolic","ESaw","Squeeze","ESquare","Saw2","Square2"};
// Default chain: all 6 in their original order, so existing behavior
// (and existing saved Patches, which only ever stored a plain morph
// float) is unaffected until someone actually opens SETTING > Timbre
// and changes it.
// The default chain is the four waveforms the synth shipped with —
// Sine, Triangle, Sawtooth, Square (v0.9933). It had been six, picking up
// Wavefolder and Half-Sine when those were added, but the default ought to
// be the plain starting point a beginner meets and the thing a tone reset
// returns to; the other eight are there to be chosen. MIN_MORPH_SLOTS is
// 4, so this is the smallest valid chain.
OscWaveform morphChain[MAX_MORPH_SLOTS] = {
    OscWaveform::SINE,OscWaveform::TRIANGLE,
    OscWaveform::SAWTOOTH,OscWaveform::SQUARE};
int morphChainLen = 4;

int16_t *oscWaveformTable(OscWaveform w){
    switch(w){
        case OscWaveform::SINE:       return sineTable;
        case OscWaveform::TRIANGLE:   return triangleTable;
        case OscWaveform::SAWTOOTH:   return sawtoothTable;
        case OscWaveform::SQUARE:     return squareTable;
        case OscWaveform::WAVEFOLDER: return wavefolderTable;
        case OscWaveform::HALFSINE:   return halfSineTable;
        case OscWaveform::PARABOLIC:  return parabolicTable;
        case OscWaveform::ESAW:       return esawTable;
        case OscWaveform::SQUEEZE:    return squeezeTable;
        case OscWaveform::ESQUARE:    return esquareTable;
        case OscWaveform::SAW2:       return saw2Table;
        case OscWaveform::SQUARE2:    return square2Table;
        default:                      return sineTable;
    }
}
int16_t *oscWaveformTableB(OscWaveform w){
    switch(w){
        case OscWaveform::SINE:       return sineTableB;
        case OscWaveform::TRIANGLE:   return triangleTableB;
        case OscWaveform::SAWTOOTH:   return sawtoothTableB;
        // Square deliberately returns its Shape=0 table here (v0.9891).
        // Its Shape is a duty-cycle sweep, which a crossfade between two
        // fixed tables physically cannot produce — mixing a 50% square
        // with a 10% pulse gives their SUM, a stepped three-level shape,
        // not a 30% square. Returning the same table both sides makes the
        // generic blend a no-op, and updateSquareDuty() rebuilds the table
        // itself instead.
        case OscWaveform::SQUARE:     return squareTable;
        case OscWaveform::WAVEFOLDER: return wavefolderTableB;
        case OscWaveform::HALFSINE:   return halfSineTableB;
        case OscWaveform::PARABOLIC:  return parabolicTableB;
        case OscWaveform::ESAW:       return esawTableB;
        case OscWaveform::SQUEEZE:    return squeezeTableB;
        case OscWaveform::ESQUARE:    return esquareTableB;
        case OscWaveform::SAW2:       return saw2TableB;
        case OscWaveform::SQUARE2:    return square2TableB;
        default:                      return sineTableB;
    }
}

// Per-slot table pointers for the current Morph chain (v0.991).
//
// getMorphedSample() used to call oscWaveformTable()/oscWaveformTableB()
// four times for EVERY sample. Both are switch statements over the
// waveform enum, so that was four dispatches plus four pointer loads
// 44100 times a second just to arrive at addresses that only change when
// the user edits the Morph chain. That is exactly the "expensive work at
// buffer rate, cheap work per sample" rule this file applies everywhere
// else — it had simply never been applied here.
//
// Resolving them into a flat array turns the inner loop into plain
// indexed loads. The output is bit-identical: same tables, same order,
// only the lookup moved. Refreshed once per buffer, which is 12 switch
// calls per 1024 samples instead of 4096.
// Cross-core guard for morphChain[]/morphChainLen/morphTblA/morphTblB
// (v0.99943). morphStart() (loop(), APP core) rewrites all three across
// several separate statements, and getMorphedSample() (audioTask, PRO
// core — a genuinely different physical core, not just a cooperative
// task) reads them on every sample with no synchronisation at all before
// this. A read landing mid-update could see, for instance, the new
// chainLen paired with pointer slots the chain-array copy hadn't reached
// yet — read as either a stale waveform in the wrong position (heard as
// noise) or, if the still-null-pointer path is hit, a race between
// audioTask's own defensive refresh and morphStart()'s refresh running on
// the other core at the same time, each partially overwriting the same
// arrays (a very plausible source of the intermittent silence reported —
// no fixed repro is exactly what an unsynchronised cross-core race looks
// like from the outside).
//
// Deliberately only guards the RARE paths — morphStart() itself, and
// getMorphedSample()'s fallback refresh when a pointer is still null —
// not the hot per-sample lookup. A spinlock on every one of ~44100
// calls/sec would cost real CPU for no benefit; the common case never
// touches morphTblA/morphTblB while they are mid-update, so it needs no
// protection at all.
portMUX_TYPE morphChainMux=portMUX_INITIALIZER_UNLOCKED;

int16_t *morphTblA[MAX_MORPH_SLOTS]={nullptr};
int16_t *morphTblB[MAX_MORPH_SLOTS]={nullptr};
void refreshMorphTablePtrs(){
    // Self-locking (v0.99943) so every caller — writers right after
    // editing the chain, and audioTask reading it on the other core —
    // is protected without each having to remember to take the lock
    // itself. ESP32's portMUX spinlocks are re-entrant by the same core,
    // so a caller that already holds the lock (see morphStart()) nests
    // safely rather than deadlocking. The body is a handful of array
    // copies, not real work, so locking every call — including the one
    // audioTask makes unconditionally each buffer — costs nothing
    // measurable next to the ~23ms it has to do it in.
    portENTER_CRITICAL(&morphChainMux);
    for(int i=0;i<morphChainLen&&i<MAX_MORPH_SLOTS;i++){
        morphTblA[i]=oscWaveformTable(morphChain[i]);
        morphTblB[i]=oscWaveformTableB(morphChain[i]);
    }
    portEXIT_CRITICAL(&morphChainMux);
}
// Where a given waveform currently sits in the active chain, or -1 if
// it isn't included right now.
int morphChainSlotOf(OscWaveform w){
    for(int i=0;i<morphChainLen;i++)if(morphChain[i]==w)return i;
    return -1;
}

constexpr float SINE_AMP     = 32000.0f;
constexpr float TRIANGLE_AMP = 30000.0f;
constexpr float SAWTOOTH_AMP = 22000.0f;
constexpr float SQUARE_AMP   = 18000.0f;
constexpr float WAVEFOLDER_AMP = 24000.0f;
constexpr float HALFSINE_AMP   = 28000.0f;
constexpr float PARABOLIC_AMP  = 30000.0f;
constexpr float ESAW_AMP       = 24000.0f;
constexpr float SQUEEZE_AMP    = 28000.0f;
constexpr float ESQUARE_AMP    = 26000.0f;
constexpr float SAW2_AMP       = 26000.0f;
constexpr float SQUARE2_AMP    = 24000.0f;

// ---------------------------------------------------------
// Note-key layout — shared physical layout for EZ and Pro Mode
// ---------------------------------------------------------
// Both modes use the same two physical rows of 13 keys each:
//   Row 1 (number row): "1234567890-=" + Backspace (13th key — not a
//   printable character, so it's detected via KeysState.del instead of
//   .word; see resolveFreqFromKeys())
//   Row 2 (qwerty row, some octaves below Row 1): "qwertyuiop[]\"
// EZ Mode always uses the Major scale (see MAJOR_SCALE below); Pro Mode
// can pick any scale from SCALES[], including "Chromatic" (the default,
// reproducing the original Pro Mode note layout exactly).
constexpr char ROW1_KEYS[12] = {'1','2','3','4','5','6','7','8','9','0','-','='};
constexpr char ROW2_KEYS[13] = {'q','w','e','r','t','y','u','i','o','p','[',']','\\'};
float row1Freqs[13]; // index 12 = the Backspace/del key
float row2Freqs[13];

// ---------------------------------------------------------
// Scales
// ---------------------------------------------------------
// Each scale is a set of semitone offsets from the root (fixed at C, since
// Transpose already covers changing key). The 13 keys of a row are mapped
// onto the scale degrees in order, wrapping into the next octave once the
// scale's own note count is exceeded — e.g. a 5-note pentatonic scale
// spans keys 1-5 (octave 1), 6-10 (octave 2), 11-13 (start of octave 3).
// Row 2 uses the same scale, shifted down by just enough octaves that its
// own 13th key lands at or below Row 1's root (see computeRow2OctaveShift).
struct ScaleDef { const char *name; uint8_t category; int8_t intervals[12]; uint8_t length; };
const ScaleDef MAJOR_SCALE = {"Major",0,{0,2,4,5,7,9,11},7}; // EZ Mode is always this

const char *SCALE_CATEGORY_NAMES[] = {
    "Chromatic","Classical","Symmetrical","Pentatonic","Japan","China","India","Middle East","Europe"
};
constexpr int NUM_SCALE_CATEGORIES = sizeof(SCALE_CATEGORY_NAMES)/sizeof(SCALE_CATEGORY_NAMES[0]);

const ScaleDef SCALES[]={
    // Chromatic — Pro Mode's default (reproduces the original layout)
    {"Chromatic",     0,{0,1,2,3,4,5,6,7,8,9,10,11},12},
    // Classical / church modes
    {"Major",         1,{0,2,4,5,7,9,11},   7},
    {"Natural Minor", 1,{0,2,3,5,7,8,10},   7},
    {"Dorian",        1,{0,2,3,5,7,9,10},   7},
    {"Phrygian",      1,{0,1,3,5,7,8,10},   7},
    {"Lydian",        1,{0,2,4,6,7,9,11},   7},
    {"Mixolydian",    1,{0,2,4,5,7,9,10},   7},
    {"Locrian",       1,{0,1,3,5,6,8,10},   7},
    {"Harmonic Minor",1,{0,2,3,5,7,8,11},   7},
    {"Melodic Minor", 1,{0,2,3,5,7,9,11},   7},
    // Symmetrical / artificial scales
    {"Whole Tone",        2,{0,2,4,6,8,10},      6},
    {"Diminished (W-H)",  2,{0,2,3,5,6,8,9,11},  8},
    {"Diminished (H-W)",  2,{0,1,3,4,6,7,9,10},  8},
    {"Augmented",         2,{0,3,4,7,8,11},      6},
    // Pentatonic
    {"Major Pentatonic",  3,{0,2,4,7,9},   5},
    {"Minor Pentatonic",  3,{0,3,5,7,10},  5},
    {"Blues",             3,{0,3,5,6,7,10},6},
    {"Egyptian (Sus)",    3,{0,2,5,7,10},  5},
    // Japan
    {"Hirajoshi",        4,{0,2,3,7,8},  5},
    {"In Sen",           4,{0,1,5,7,10}, 5},
    {"Iwato",            4,{0,1,5,6,10}, 5},
    {"Kumoi",            4,{0,2,3,7,9},  5},
    {"Yo Scale",         4,{0,2,5,7,9},  5},
    {"Ryukyu (Okinawa)", 4,{0,4,5,7,11}, 5},
    // China — the traditional pentatonic (Gong/Shang/Jue/Zhi/Yu) is
    // intervallically the same 5-note set as Western pentatonic, just
    // started from a different degree (like a mode). Gong mode = Major
    // Pentatonic and Yu mode = Minor Pentatonic above, so only the three
    // remaining, genuinely distinct modes are listed here.
    {"Shang Mode", 5,{0,2,5,7,10}, 5},
    {"Jue Mode",   5,{0,3,5,8,10}, 5},
    {"Zhi Mode",   5,{0,2,5,7,9},  5},
    // India
    {"Bhairav", 6,{0,1,4,5,7,8,11}, 7},
    {"Todi",    6,{0,1,3,6,7,8,11}, 7},
    // Middle East
    {"Hijaz",           7,{0,1,4,5,7,8,10}, 7},
    {"Double Harmonic", 7,{0,1,4,5,7,8,11}, 7},
    // Europe
    {"Hungarian Minor",  8,{0,2,3,6,7,8,11}, 7},
    {"Neapolitan Minor", 8,{0,1,3,5,7,8,11}, 7},
    // Additional scales (appended, not inserted, so existing saved
    // currentScaleIndex values from before this addition stay valid)
    {"Harmonic Major",    1,{0,2,4,5,7,8,11},      7},
    {"Neapolitan Major",  1,{0,1,3,5,7,9,11},      7},
    {"Lydian Augmented",  1,{0,2,4,6,8,9,11},      7},
    {"Lydian Dominant",   1,{0,2,4,6,7,9,10},      7},
    {"Messiaen Mode 3",   2,{0,2,3,4,6,7,8,10,11}, 9},
    {"Messiaen Mode 6",   2,{0,2,4,5,6,8,10,11},   8},
    {"Major b6 Pent.",    3,{0,2,4,7,8},           5},
    {"Pentatonic b5",     3,{0,3,5,6,10},          5},
    {"Ahir Bhairav",      6,{0,1,4,5,7,9,10},      7},
    {"Marva",             6,{0,1,4,6,7,9,11},      7},
    {"Purvi",             6,{0,1,4,6,7,8,11},      7},
    {"Charukeshi",        6,{0,2,4,5,7,8,10},      7},
    {"Nikriz",            7,{0,2,3,6,7,9,10},      7},
    {"Persian",           7,{0,1,4,5,6,8,11},      7},
    {"Romanian Minor",    8,{0,2,3,6,7,9,10},      7},
    {"Hungarian Major",   8,{0,3,4,6,7,9,10},      7},
};
constexpr int NUM_SCALES = sizeof(SCALES)/sizeof(SCALES[0]);
int currentScaleIndex = 0; // index into SCALES[]; default = Chromatic. Only relevant in Pro Mode — EZ always uses MAJOR_SCALE.

enum class PlayMode : uint8_t { EZ, PRO };
PlayMode playMode = PlayMode::EZ;

// How many octaves to shift Row 2 down. Uses floor division so Row 2
// starts as high as possible while still landing a full octave (or more)
// below Row 1 — this favors a wider combined range and allows a bit of
// overlap at the boundary, rather than guaranteeing zero overlap at the
// cost of a gap of unplayable notes in between (which is worse for
// actually playing songs that span more than an octave).
int computeRow2OctaveShift(const ScaleDef &sc){
    int topOffset=sc.intervals[12%sc.length]+(12/sc.length)*12;
    return topOffset/12; // floor division
}

// Recomputes both rows' key frequencies (and the Backspace-key top note)
// from whichever scale is currently active (MAJOR_SCALE for EZ Mode,
// SCALES[currentScaleIndex] for Pro Mode). Cheap (26 powf() calls), so
// safe to call on every scroll step while live-previewing scales, and
// whenever Play Mode is toggled.
void recomputeKeyNotes(){
    const ScaleDef &sc=(playMode==PlayMode::EZ)?MAJOR_SCALE:SCALES[currentScaleIndex];
    for(int i=0;i<13;i++){
        int octave=i/sc.length;
        int degree=i%sc.length;
        float semitones=(float)sc.intervals[degree]+octave*12.f;
        row1Freqs[i]=261.63f*powf(2.f,semitones/12.f);
    }
    int shift=computeRow2OctaveShift(sc);
    for(int i=0;i<13;i++){
        int octave=i/sc.length;
        int degree=i%sc.length;
        float semitones=(float)sc.intervals[degree]+octave*12.f-shift*12.f;
        row2Freqs[i]=261.63f*powf(2.f,semitones/12.f);
    }
}

// Board auto-detection: CardputerADV has an IMU and a TCA8418 keyboard
// controller (10+ key rollover); the original Cardputer has neither (no
// IMU, and only 3-key rollover on its GPIO matrix keyboard). Determined
// once in setup() via M5.getBoard(); everything IMU-related is gated on
// this at runtime, and original-Cardputer builds substitute key-driven
// "PAD" control for the missing IMU (see updatePadVirtualAxes()).
bool isCardputerAdv = true;
uint16_t seqAccentColor = 0xFD20; // placeholder; recomputed properly in setup() via color565(255,140,0)
uint16_t seqAccentNoteColor = 0xF800; // placeholder red; recomputed in setup() — accented steps' velocity bar
// Dimmed fills for the step bars only — see setup() (v0.9925).
uint16_t seqBarNormal = 0xE71C, seqBarAccent = 0xA97F, seqBarPlayhead = 0x47F0;
uint16_t songAccentColor = 0x07FF; // placeholder cyan; recomputed in setup() — SONG mode's own fixed UI color, distinct from PLAY's green and SEQ's orange

// ---- UI theme (v0.9936) ----
//
// A theme sets the accent colour for all THREE home modes at once, not one
// colour. That is deliberate. uiColor is not purely decorative: PLAY, SEQ
// and SONG are told apart by it, and that identification is load-bearing —
// it is how the v0.9922 "SEQ's orange left behind in PLAY" bug was spotted
// at all. Letting someone pick three arbitrary colours would let them pick
// three similar ones and quietly lose it.
//
// Presets rather than free RGB, for the same reason plus one more: every
// combination here has been chosen so the three modes stay distinguishable
// AND stay legible against black. A colour picker on a 240x135 panel would
// happily produce dark grey on black, and that failure only shows up after
// release, in someone else's hands.
//
// What a theme does NOT touch: the sequencer's step colours, the level-bar
// colours, cursor white. Those carry meaning rather than decoration — beat
// position, accent, playhead — and took six versions to get right. They
// stay fixed.
struct UiTheme {
    const char *name;
    uint8_t playR,playG,playB;
    uint8_t seqR ,seqG ,seqB ;
    uint8_t songR,songG,songB;
};
const UiTheme UI_THEMES[]={
    // The original scheme, and still the default.
    {"Classic",   0,255,  0,   255,140,  0,     0,220,220},
    // Requested: red for PLAY. SEQ moves to amber and SONG to violet so
    // all three stay apart.
    // Crimson rather than the vermilion this started as — dropping green
    // to near zero is what makes red read as deep instead of orange-ish.
    {"Ember",   225,  0, 35,   255,180,  0,   190,110,255},
    // Ice moved away from Access's blue: teal-leaning rather than
    // blue-leaning, so the two themes are not mistaken for each other.
    {"Ice",      90,225,235,   190,130,255,   120,150,255},
    // Brightness-separated rather than hue-separated, so it reads for
    // anyone regardless of colour perception — and in bright sunlight.
    {"Mono",    255,255,255,   170,170,170,   110,110,110},
    // Blue / yellow / white avoids the red-green axis entirely, which is
    // what the common forms of colour vision deficiency affect. The three
    // also differ in brightness, so they remain separable even if hue does
    // not read at all. (Deuteranopia and protanopia both leave blue and
    // yellow intact; tritanopia is far rarer but the brightness spread
    // covers it too.)
    {"Access",  120,180,255,   255,215,  0,   245,245,245},
};
constexpr int NUM_UI_THEMES=sizeof(UI_THEMES)/sizeof(UI_THEMES[0]);
int uiThemeIndex=0;
bool uiThemeDirty=false;   // set on change, consumed by loop()'s redraw latch

// ---- Analog drift (v0.9941) ----
//
// Real analog synths do not hold still: oscillators wander a few cents,
// filters breathe, levels creep. This reproduces that, and it fits this
// codebase without any new machinery — base values and EFFECTIVE values
// are already separate everywhere (the *Offset pairs, the fxEff* set), so
// drift is just one more offset. The menus keep showing the base value,
// which is what was asked for and also what a real synth's panel does: the
// knob does not move, the sound does.
//
// Independent random walks rather than one shared wobble. If pitch, cutoff
// and level all moved together it would read as a single tremolo; drifting
// separately is what sounds like circuitry rather than an effect.
//
// Updated once per buffer, so the per-sample cost is zero.
bool  analogDriftOn=false;
float analogDriftAmount=0.35f;   // 0-1
float driftPitch=0.f,driftCutoff=0.f,driftLevel=0.f;   // current, smoothed
float driftPitchT=0.f,driftCutoffT=0.f,driftLevelT=0.f;// walk targets

// Full-scale drift at amount 1.0. Pitch is the one that has to stay
// modest: past a few cents it stops sounding like an old synth and starts
// sounding out of tune. Cutoff can take much more before it reads as
// wrong, and level least of all — amplitude wobble is the most obviously
// artificial of the three.
// Raised in v0.9942 after measuring what the walk actually produced.
// The peak was fine at 9 cents, but the walk only reached it occasionally:
// simulated over 300k buffers the RMS excursion was 0.29 of full scale, so
// the TYPICAL detune was under 3 cents and inaudible, while the audible
// moments were rare enough to look like nothing was happening. Both the
// ceiling and the time spent near it had to go up (see the walk constants
// below for the second half of that).
constexpr float DRIFT_PITCH_CENTS = 22.0f;
constexpr float DRIFT_CUTOFF_FRAC = 0.22f;
constexpr float DRIFT_LEVEL_FRAC  = 0.09f;

void updateAnalogDrift(){
    if(!analogDriftOn){
        // Ease back to neutral rather than snapping, so switching it off
        // mid-note doesn't click.
        driftPitch+=(0.f-driftPitch)*0.02f;
        driftCutoff+=(0.f-driftCutoff)*0.02f;
        driftLevel+=(0.f-driftLevel)*0.02f;
        return;
    }
    // New target occasionally; the smoothing below does the travelling, so
    // what comes out is a slow wander rather than a jitter.
    // Retuned in v0.9942. Targets are picked LESS often and travelled
    // toward FASTER than before — the old pair (3% per buffer, 0.010) let
    // a new target arrive long before the previous one was reached, so the
    // value spent its life crawling around the middle and never got near
    // the range it was allowed. Simulation over 300k buffers: RMS went
    // from 0.29 of full scale to 0.43. The time constant is still about a
    // second, so this is a wander rather than a wobble.
    if(random(0,1000)<15)driftPitchT =(random(0,2001)/1000.f-1.f);
    if(random(0,1000)<18)driftCutoffT=(random(0,2001)/1000.f-1.f);
    if(random(0,1000)<12)driftLevelT =(random(0,2001)/1000.f-1.f);
    driftPitch +=(driftPitchT -driftPitch )*0.020f;
    driftCutoff+=(driftCutoffT-driftCutoff)*0.026f;
    driftLevel +=(driftLevelT -driftLevel )*0.016f;
}
float driftPitchCents(){return analogDriftOn?driftPitch*analogDriftAmount*DRIFT_PITCH_CENTS:driftPitch*DRIFT_PITCH_CENTS;}
float driftCutoffMult(){return 1.f+driftCutoff*analogDriftAmount*DRIFT_CUTOFF_FRAC;}
float driftLevelMult(){ return 1.f+driftLevel *analogDriftAmount*DRIFT_LEVEL_FRAC;}
// Theme-picker state lives up here with the theme itself, not down with
// the menu items that open it: updateThemePicker() runs alongside the
// other picker handlers, which sit far earlier in the file (v0.9939).
bool themePickerOpen=false;
int  themePickerIndex=0;
bool prevThemeUpPressed=false,prevThemeDownPressed=false;
bool prevThemeConfirmPressed=false,prevThemeTabPressed=false;
// Backlight (v0.9937). Stops well above zero on purpose: the display is
// the only way to see what the synth is doing, and a setting that can
// black it out entirely is a setting someone will reach by accident and
// then be unable to see well enough to undo.
constexpr uint8_t UI_BRIGHT_MIN=32, UI_BRIGHT_MAX=255;
uint8_t uiBrightness=UI_BRIGHT_MAX;
// Deferred during boot (v0.99986) — a saved brightness preference (dim
// or otherwise) used to take effect the moment loadSettings() parsed it,
// which lands squarely in the middle of the splash's own fade-in,
// undercutting it with an abrupt jump to whatever level the user last
// saved — jarring in either direction, and specifically defeating the
// point of a deliberately gentle fade if that saved level is dim. While
// this is true, applyUiBrightness() updates uiBrightness's bookkeeping
// as normal but skips the actual hardware call; setup() clears it and
// applies the real value exactly once, right before entering loop() —
// i.e. right as PLAY actually appears, which is the moment being asked
// for.
bool bootBrightnessDeferred=true;
void applyUiBrightness(){
    if(bootBrightnessDeferred)return;
    M5Cardputer.Display.setBrightness(uiBrightness);
}
uint16_t playAccentColor=0x07E0;   // resolved from the theme in applyUiTheme()

void applyUiTheme(){
    const UiTheme &t=UI_THEMES[constrain(uiThemeIndex,0,NUM_UI_THEMES-1)];
    playAccentColor=M5Cardputer.Display.color565(t.playR,t.playG,t.playB);
    seqAccentColor =M5Cardputer.Display.color565(t.seqR ,t.seqG ,t.seqB );
    songAccentColor=M5Cardputer.Display.color565(t.songR,t.songG,t.songB);
}
uint16_t uiColor = GREEN; // the "current" UI accent color — GREEN normally, seqAccentColor whenever SEQ is the active home mode (see loop())

// ---------------------------------------------------------
// IMU mapping
// ---------------------------------------------------------
enum class ImuTarget : uint8_t {
    NONE, TIMBRE, VIBRATO_DEPTH, VIBRATO_RATE, TREMOLO,
    VOLUME, PITCH_BEND, BEND_UP, BEND_DOWN, BITCRUSH, FILTER_CUTOFF,
    SHAPE, DETUNE, NOISE, SUB_LEVEL, RESONANCE, LFO_RATE, LFO_DEPTH,
    ARP_TEMPO, ARP_SWING,
    // FX targets (v0.9876). APPENDED, never inserted — the numeric value
    // is what settings.json stores, so inserting anywhere above would
    // silently re-point every existing saved IMU mapping.
    FX_RING_RATE, FX_RING_MIX, FX_LIMIT_DRIVE,
    FX_CHORUS_DEPTH, FX_CHORUS_MIX, FX_DELAY_FB, FX_DELAY_MIX,
    FX_REVERB_ROOM, FX_REVERB_MIX,   // v0.9879
    // v0.9934. Appended, never inserted — settings.json stores these
    // numerically, so anything above would re-point saved mappings.
    OSC_MIX, OSC2_SHAPE,
    TARGET_COUNT
};

// Forward declarations
const char *imuTargetName(ImuTarget t);
void resetParamToDefault(ImuTarget t);
void drawWaveform(LovyanGFX &gfx,float morph,float shape);
void drawOsc2Waveform(LovyanGFX &gfx,OscWaveform w,float shape);
void drawAdsrGraph();
void arpToggle();
void drawHelpOverlay(LovyanGFX &gfx);

struct ImuAxisConfig {
    ImuTarget target;
    float sensitivity;
    bool bipolar;
    bool invert;        // v0.8: flips tilt direction
    bool exponential;   // v0.8: response curve, false=linear true=exponential
    float deadzone;     // v0.8: 0.0-0.3 (0-30%), center dead zone
    float calOffsetDeg; // v0.8: calibration zero-point (degrees)
};

// Defaults used on a first boot with no settings.json present. Y is
// deliberately NOT Volume (v0.9875): with Volume mapped to tilt, simply
// holding the device at an angle can drop it to silence, which a
// first-time user reads as broken hardware rather than as a control
// working exactly as configured. Shape always leaves the sound audible.
// These must stay in sync with performPatchToneReset()'s IMU section —
// the two had drifted apart before v0.9875 (first boot said Vibrato
// Depth here, Patch Reset said Volume), so the same firmware behaved
// differently depending on which path you arrived through.
ImuAxisConfig imuAxisX = { ImuTarget::TIMBRE, 1.0f, false, false, false, 0.0f, 0.0f };
ImuAxisConfig imuAxisY = { ImuTarget::SHAPE,  1.0f, false, false, false, 0.0f, 0.0f };

// Raw tilt angle (degrees, before calibration offset) from the most recent
// updateImu() call — used by the Calibrate action to capture a new zero point.
float lastAngleXDeg=0.f, lastAngleYDeg=0.f;

constexpr float TILT_MAX_DEGREES = 35.0f;

// ---------------------------------------------------------
// Synth parameters
// ---------------------------------------------------------
struct SynthParams {
    float keyVolume   = 0.5f;
    int   octaveShift = 0;
    // VCO 1
    float oscShape      = 0.5f;
    float detuneCents   = 0.0f;
    float fineTuneCents = 0.0f;
    // VCO 2 (v0.9911). A full second oscillator: its own position in the
    // Morph chain, its own Shape, its own tuning. It shares the chain
    // itself, the filter, the envelope and the VCA — those are one signal
    // path, and duplicating them would be a second voice rather than a
    // second oscillator.
    //
    // osc2Level is the mix between the two, not a gain on osc 2: at 0 you
    // hear only osc 1, at 1 only osc 2, at 0.5 an equal blend. Default 0
    // so every existing patch sounds exactly as it did.
    float osc2Level     = 0.0f;   // 0 = osc1 only, 1 = osc2 only
    float osc2Shape     = 0.5f;
    float osc2DetuneCents = 0.0f;
    float osc2FineCents   = 0.0f;
    int   osc2OctaveShift = 0;    // -2..+2, relative to osc 1
    // Semitone offset (v0.993): the interval control the tuning section
    // was missing. Octave is too coarse for harmony and Detune is measured
    // in cents, so a third or a fifth could only be dialled in as 400 or
    // 700 cents on a +-50 control — which is to say, not at all.
    int   osc2Semitones   = 0;    // -12..+12
    // v0.9934: both reachable from the IMU. Osc Mix is deliberately
    // bipolar — tilting one way brings oscillator 1 forward, the other
    // brings oscillator 2 — so it crossfades rather than just fading
    // something in.
    float osc2LevelOffset=0.0f,      osc2LevelOffsetTarget=0.0f;   // -1..+1
    float osc2ShapeOffset=0.0f,      osc2ShapeOffsetTarget=0.0f;   // -1..+1
    // Oscillator 2 picks a waveform DIRECTLY rather than a position along
    // the Morph chain (v0.9914). The chain exists so IMU and LFO can sweep
    // continuously between waveforms; oscillator 2 is a fixed layer and
    // never needs that, so being restricted to whatever happens to be in
    // the chain only cost it reach — the whole 12-waveform library is
    // available here instead. Simpler too: one table pair, no morph
    // interpolation.
    OscWaveform osc2Waveform = OscWaveform::SAWTOOTH;
    // Sub oscillator
    float subOscLevel  = 0.0f;   // 0.0 - 1.0
    int   subOscOctave = -1;     // -1 or -2
    // Noise blend
    float noiseLevel   = 0.0f;   // 0.0 - 1.0
    // IMU current
    float timbreMorph        = 0.0f;
    float vibratoDepth       = 0.0f;
    float vibratoRateHz      = 5.0f;
    float tremoloDepth       = 0.0f;
    float volumeScale        = 1.0f; // v0.8: relative multiplier (0-1) of keyVolume, was an additive offset
    float pitchBendCents     = 0.0f;
    float bitcrush           = 0.0f;
    float filterCutoffOffset = 0.0f;
    // New in v0.8: offsets for IMU-controlled PWM/Detune/Noise/SubLevel/Resonance.
    // Same pattern as the offsets above: added on top of the VCO/VCF menu's
    // stored value at the point of use, never overwriting the stored value itself.
    float oscShapeOffset       = 0.0f;
    float detuneOffset    = 0.0f;
    float noiseOffset     = 0.0f;
    float subLevelOffset  = 0.0f;
    float resonanceOffset = 0.0f;
    // IMU targets
    float timbreMorphTarget        = 0.0f;
    // v0.989: Vibrato/Tremolo/Bit-crusher moved to the base+offset model
    // every other modulatable parameter here uses. They used to have the
    // IMU write their value DIRECTLY, which is why they could only ever be
    // set by tilt — a menu control would just have been overwritten on the
    // next IMU update. The menu now owns the base value and the IMU adds
    // an offset on top, so both work at once.
    float vibratoDepthOffset=0.0f,  vibratoDepthOffsetTarget=0.0f;  // +-1.0
    float vibratoRateOffset=0.0f,   vibratoRateOffsetTarget=0.0f;   // +-4.5 Hz
    float tremoloDepthOffset=0.0f,  tremoloDepthOffsetTarget=0.0f;  // +-1.0
    float bitcrushOffset=0.0f,      bitcrushOffsetTarget=0.0f;      // +-1.0
    float volumeScaleTarget        = 1.0f;
    float pitchBendCentsTarget     = 0.0f;
    float filterCutoffOffsetTarget = 0.0f;
    float oscShapeOffsetTarget       = 0.0f;
    float detuneOffsetTarget    = 0.0f;
    float noiseOffsetTarget     = 0.0f;
    float subLevelOffsetTarget  = 0.0f;
    float resonanceOffsetTarget = 0.0f;
    // FX (v0.987): Ring Modulator. Mix=0 means fully off (no separate
    // enabled flag needed) — kept simple for this first FX delivery, no
    // IMU offset yet (can follow later using the same offset pattern as
    // everything else here, once wanted).
    float ringModRateHz = 200.0f; // 20-2000 Hz
    float ringModMix    = 0.0f;   // 0-100%, dry/wet blend
    // FX: Soft Limiter (v0.9872). Drive pushes the final mix harder into
    // the limiting curve for more audible saturation/compression
    // character; Mix=0 means fully off, same convention as Ring Mod.
    float limiterDrive = 2.5f; // 1.0-5.0
    float limiterMix   = 0.0f; // 0-100%, dry/wet blend
    // FX: Chorus (v0.9873). Rate = LFO speed modulating delay time,
    // Depth = how far the delay time swings, Mix=0 means off.
    float chorusRateHz  = 1.0f;  // 0.1-5 Hz
    float chorusDepthMs = 15.0f; // 0-20 ms
    float chorusMix     = 0.0f;  // 0-100%, dry/wet blend
    // FX: Delay/Echo (v0.9874). Time = delay length, Feedback = how much
    // of the delayed signal recirculates (repeat echoes), Mix=0 means off.
    float delayTimeMs  = 300.0f; // 50 ms to DELAY_MAX_MS
    float delayFeedback = 0.3f;  // 0-90%
    float delayMix      = 0.0f;  // 0-100%, dry/wet blend
    // FX modulation offsets (v0.9876). Same base+offset+offsetTarget
    // pattern every other IMU/LFO-mappable parameter here uses: the menu
    // value is never written to, the offset is added on top and clamped
    // back into the parameter's own range by updateFxEffective().
    // Only the seven parameters worth performing with are covered — see
    // the note above updateFxEffective() for why Delay Time in particular
    // is deliberately not one of them.
    float ringModRateOffset=0.0f,   ringModRateOffsetTarget=0.0f;   // +-990 Hz
    float ringModMixOffset=0.0f,    ringModMixOffsetTarget=0.0f;    // +-1.0
    float limiterDriveOffset=0.0f,  limiterDriveOffsetTarget=0.0f;  // +-2.0x
    float chorusDepthOffset=0.0f,   chorusDepthOffsetTarget=0.0f;   // +-10 ms
    float chorusMixOffset=0.0f,     chorusMixOffsetTarget=0.0f;     // +-1.0
    float delayFeedbackOffset=0.0f, delayFeedbackOffsetTarget=0.0f; // +-0.45
    float delayMixOffset=0.0f,      delayMixOffsetTarget=0.0f;      // +-1.0
    // Reverb (v0.9879)
    float reverbRoomSize = 0.5f; // 0-100%, maps to comb feedback
    float reverbDamping  = 0.5f; // 0-100%, high-frequency loss per pass
    float reverbMix      = 0.0f; // 0-100%, dry/wet blend
    float reverbRoomOffset=0.0f,    reverbRoomOffsetTarget=0.0f;    // +-0.5
    float reverbMixOffset=0.0f,     reverbMixOffsetTarget=0.0f;     // +-1.0
} params;

// ---- Patch morphing (v0.995) ----
//
// Ten slots, reached with Shift+1..0, that crossfade the current sound
// into a stored patch instead of switching to it. Inspired by Oddity's
// morph, and aimed at the same thing: changing sound WHILE playing, with
// an arpeggio latched or a pattern running, so the change is part of the
// performance rather than a pause in it.
//
// A slot holds a full snapshot, captured once when the slot is assigned
// and kept in RAM. Nothing touches the SD card while morphing — reads
// block just as writes do, and that has interfered with audio twice in
// this project already (v0.9913, v0.9926).
struct PatchSnapshot {
    bool used=false;
    char name[24]={0};
    SynthParams p;            // every continuous tone parameter
    // Discrete state, which cannot be interpolated — see morphApply().
    OscWaveform chain[MAX_MORPH_SLOTS];
    int   chainLen;
    OscWaveform osc2Wave;
    int   osc2Oct,osc2Semi,subOct;
    // IMU mapping (v0.9954). A patch stores which parameter each axis
    // drives, so morphing to a patch has to take that with it — otherwise
    // the sound changes but tilting still does whatever the previous patch
    // said, which is the opposite of loading that patch. Discrete, so it
    // switches at the start along with the waveforms.
    uint8_t imuXTarget,imuYTarget;
    bool    imuXEn,imuYEn;
    // LFO (v0.99874). It was missing entirely, so morphing changed the
    // tone but left the modulation from the previous patch running — the
    // LFO is as much a part of a sound as the filter is. Wave and target
    // are discrete and switch at the start with the waveforms; rate and
    // depth interpolate.
    uint8_t lfoWave,lfoTarget;
    float   lfoRate,lfoDepth;
    int   filterType;
    float cutoffHz,resonanceQ,keyTracking,fEnvDepth;
    float adsrA,adsrD,adsrS,adsrR;
};
constexpr int NUM_MORPH_SLOTS=10;
// Heap-allocated rather than a static array (v0.9953, revised v0.9957).
//
// This was written to use PSRAM, but there is none: both boards run an
// ESP32-S3FN8, and ps_calloc() simply fell through to ordinary calloc.
// Keeping the heap allocation anyway, for the one thing it does buy —
// morphing disables itself cleanly if the memory is not there, instead of
// the array existing unconditionally and squeezing the card mount.
//
// These snapshots are several KB, and internal DRAM here is already very
// heavily committed — the reverb network alone is ~50KB of float and the
// delay line ~70KB. SD.begin() allocates its own buffers from what is
// left, so a static array here does not merely use memory, it can push
// the card mount over the edge; a failed mount then reads as "every
// setting reset to default", which is what v0.995 produced.
//
PatchSnapshot *morphSlots=nullptr;
bool morphSlotsReady(){return morphSlots!=nullptr;}
void allocMorphSlots(){
    if(morphSlots)return;
    // Plain calloc: there is no PSRAM on this hardware, so ps_calloc()
    // only ever fell through to exactly this anyway (v0.9957).
    morphSlots=(PatchSnapshot*)calloc(NUM_MORPH_SLOTS,sizeof(PatchSnapshot));
    if(!morphSlots)Serial.println("[Morph] slot memory unavailable - morphing disabled");
}

// Morph in progress. morphFrom is re-captured from the LIVE sound at the
// start of every morph rather than being the previous slot, so pressing a
// new slot mid-morph continues from wherever the sound currently is
// instead of jumping. That is what makes rapid slot changes usable.
bool  morphActive=false;
PatchSnapshot morphFrom,morphTo;
float morphPos=0.f;          // 0..1
float morphTimeSec=1.5f;     // 0 = instant

// Declared here, defined near the tone-reset code further down: the key
// handler and the slot screen both sit above that point (v0.995).
void morphCapture(PatchSnapshot &d);
void morphStart(int slot);
void morphTick();

// Morph-slot assignment screen (v0.995). Its own screen rather than an
// addition to the patch bank: the bank exists for saving and loading, and
// this is a separate idea — ten sounds you move BETWEEN while playing.
// Keeping it separate also makes "which patches are cached in RAM" an
// explicit list rather than something implied by the bank's contents.
bool morphSlotScreenOpen=false;
int  morphSlotCursor=0;      // 0..NUM_MORPH_SLOTS-1, then the Time row
bool prevMorphUpPressed=false,prevMorphDownPressed=false;
bool prevMorphLeftPressed=false,prevMorphRightPressed=false;
bool prevMorphEnterPressed=false,prevMorphTabPressed=false,prevMorphDelPressed=false;
// Which patch each slot points at, kept as a name so the assignment
// survives a reboot; the snapshot itself is rebuilt from it at startup.
String morphSlotPatch[NUM_MORPH_SLOTS];
// The morph screen keeps its own hold timers rather than borrowing the
// menu's: menuKeyFire() and those timers are defined much further down,
// and this screen's handler sits above them (v0.9951).
unsigned long morphIncHeldMs=0,morphIncLastMs=0;
unsigned long morphDecHeldMs=0,morphDecLastMs=0;
bool menuKeyFire(bool now,bool prev,unsigned long &heldMs,unsigned long &lastMs);
// Declared here, not down with the rest of updateMenuNavigation()'s own
// state, because updateSeqEditing() and updateSongEditor() — both
// defined before that point in the file — now use them too (v0.9996x
// fix, build error: "not declared in this scope"). menuKeyFire() itself
// was already safely callable from anywhere via the forward declaration
// above; these are the actual storage it reads and writes, which has no
// such forward-visibility shortcut.
unsigned long menuIncHeldMs=0,menuIncLastMs=0;
unsigned long menuDecHeldMs=0,menuDecLastMs=0;
// menuUpHeldMs/menuDownHeldMs moved here too (v0.9996x, second fix) —
// same forward-visibility problem as menuIncHeldMs above, just caught a
// build later: SEQ's and SONG's Tempo/Swing use these (the pair that
// actually shares ';'/'.' with vInc/vDec and left/right), and both
// functions are defined before this point in the file.
unsigned long menuUpHeldMs=0,menuUpLastMs=0;
unsigned long menuDownHeldMs=0,menuDownLastMs=0;
void updateMorphSlotScreen();
void openMorphSlotScreen();


// The pitch actually being sounded, in Hz, published from audioTask for
// MIDI bend out (v0.99876). volatile because the audio task writes it and
// loop() reads it; a torn float would at worst be one stale bend message.
volatile float midiSoundingHz=0.f;
float currentFreq = 0.0f;
float seqVelocityMult = 1.0f; // per-step velocity multiplier while the Sequencer is playing a note (1.0 = no effect)
// Slide: a separate, lightweight glide independent of the global
// Portamento toggle, so per-step Slide works regardless of whether the
// user has Portamento on or off elsewhere.
bool  seqSliding=false;
float seqSlideFreq=0.f;
constexpr float SEQ_SLIDE_SPEED=0.35f; // per-buffer smoothing coeff — tuned so the glide clearly completes within roughly one step
// Accent: temporary filter cutoff + resonance boost, smoothed like other offsets.
float seqAccentCutoffBoost=0.f, seqAccentCutoffBoostTarget=0.f;
float seqAccentResoBoost=0.f, seqAccentResoBoostTarget=0.f;
constexpr float SEQ_ACCENT_CUTOFF_BOOST=3500.0f; // Hz, added while an accented step is sounding
constexpr float SEQ_ACCENT_RESO_BOOST=4.0f;      // Q, added while an accented step is sounding — the classic TB-303 "quack"
constexpr float SEQ_ACCENT_VELOCITY_MULT=1.3f;   // velocity multiplier for accented steps

// ---------------------------------------------------------
// Portamento
// ---------------------------------------------------------
bool  portaEnabled = false;
float portaFreq    = 0.0f;   // current smoothed frequency
float portaSpeed   = 0.005f; // smoothing coeff (higher = faster glide)

// ---------------------------------------------------------
// IMU / note hold
// ---------------------------------------------------------
bool imuXHeld = false, imuYHeld = false, noteHeld = false;
// Per-axis enable (v0.9921). Separate from target==NONE so that switching
// an axis off and back on keeps whatever it was assigned to.
bool imuXEnabled = true, imuYEnabled = true;
bool prevImuXEnablePressed=false, prevImuYEnablePressed=false;
// Help overlay latched open by Shift+H, independent of holding H.
bool helpLatched=false, prevHelpLatchPressed=false;
bool imuCalibrated = false; // true once IMU Calibrate has been confirmed at least once
bool prevImuXHoldPressed = false, prevImuYHoldPressed = false;
bool prevNoteHoldPressed = false;
float heldFreq = 0.0f;
bool prevPortaPressed = false;
bool prevArpLatchPressed = false;
bool prevArpToggleKeyPressed = false;
bool prevSeqPlayKeyPressedGlobal = false;

// Help overlay state
bool helpVisible     = false;
bool prevHelpPressed = false;

// Edge detection
bool prevOctaveUpPressed = false, prevOctaveDownPressed = false;
bool prevVolumeUpPressed = false, prevVolumeDownPressed = false;
// Dedicated, not shared with anything else (v0.9996x) — deliberately NOT
// reusing menuUpHeldMs/menuDownHeldMs or menuIncHeldMs/menuDecHeldMs the
// way an earlier attempt at ARP/SEQ/SONG's Tempo/Swing wrongly did: 'k'
// and 'l' are their own physical keys, not aliases for any of ';'/'.'/
// '/'/',', so sharing here would create exactly the same false-reset bug
// that fix had to correct — updateMenuNavigation()'s per-frame "clear if
// not held" logic watches a different key entirely and would zero these
// out from under a genuinely-held 'k'/'l' on the very next frame.
unsigned long volUpHeldMs=0,volUpLastMs=0;
unsigned long volDownHeldMs=0,volDownLastMs=0;
bool prevTransposeUpPressed = false, prevTransposeDownPressed = false;

// Transpose: semitone-level key change, independent of octave shift
int transposeSemitones = 0;
constexpr int TRANSPOSE_MIN = -12, TRANSPOSE_MAX = 12;

// Phase accumulators
float phase        = 0.0f;
float phase2       = 0.0f;   // v0.9911: oscillator 2
float subPhase     = 0.0f; // independent phase accumulator for the sub oscillator
float ringModPhase = 0.0f; // independent phase accumulator for the Ring Modulator carrier (v0.987)
// Chorus (v0.9873): circular delay-line buffer, ~46ms at 44100Hz — well
// over the ~10-30ms range the modulated delay read actually sweeps
// through, so there's no risk of the read/write positions colliding.
constexpr int CHORUS_BUFFER_SIZE = 2048;
int16_t chorusBuffer[CHORUS_BUFFER_SIZE] = {0};
int   chorusWriteIdx  = 0;
float chorusLfoPhase  = 0.0f;
// Smoothed chorus read distance in samples (v0.9877) — see applyChorus().
// Seeded to the middle of the default sweep (10 + 15*0.5 ms) so the first
// buffer after boot starts from a sane distance rather than sliding in
// from zero.
float chorusDelaySmooth = 17.5f*0.001f*SAMPLE_RATE;
// Chorus wet-path re-entry state (v0.9878). When the chorus line is empty
// and a note starts, the read pointer spends the first delay-time worth of
// samples inside the empty region and then crosses into the freshly
// recorded note in a single sample — the wet signal jumps from silence to
// the note's full attack at once, which is a click. chorusFillCount tracks
// how much real audio has been written since the line was last empty, and
// chorusWetGain holds the wet path at zero until the read pointer has
// cleared that boundary, then fades it in. See applyChorus().
int   chorusFillCount = CHORUS_BUFFER_SIZE;
float chorusWetGain   = 1.0f;
// Delay/Echo (v0.9874): circular buffer sized for the max Time setting
// (800ms @44100Hz), holding dry+feedback so repeat echoes decay
// naturally on their own rather than needing an explicit echo count.
// Maximum delay time, and the only number to change if this trade is
// reversed (v0.9965).
//
// This was 800ms, costing 35280 int16 = ~70KB of internal DRAM. Halving it
// frees ~35KB, which is what makes room for the TinyUSB stack (measured at
// ~19KB) alongside SD.begin (~27KB) — the two together did not fit before
// and the board would not boot.
//
// It is a real loss, not a free win: 400ms is short for anything you would
// call a long delay. Being tried deliberately, to see whether USB MIDI is
// worth it. Put DELAY_MAX_MS back to 800 and the buffer follows.
constexpr float DELAY_MAX_MS = 800.0f;
constexpr int DELAY_BUFFER_SIZE = (int)(DELAY_MAX_MS*0.001f*SAMPLE_RATE)+64;
int16_t delayBuffer[DELAY_BUFFER_SIZE] = {0};
int delayWriteIdx = 0;
// Reverb (v0.9879): Schroeder/Freeverb topology — 8 parallel comb filters
// (each with a one-pole lowpass in its feedback path, which is what
// Damping controls) summed, then 4 allpass filters in series to smear the
// result into something that stops sounding like discrete echoes. These
// delay lengths are Freeverb's, mutually prime at 44100Hz so the combs
// don't reinforce each other into a ringing pitch.
//
// These are float, not int16 like the Chorus and Delay lines. Comb
// feedback runs around 0.84, so anything living in the buffer is
// recirculated with a steady-state gain of ~1/(1-0.84) = 6x, and eight of
// them sum on top of that. int16 quantization noise would come back at
// roughly -65dBFS — a faint but real hiss under quiet passages, which is
// exactly where a reverb tail is most exposed. The cost is 12587 floats
// (~50KB) instead of ~25KB; ESP32-S3 has the room.
constexpr int NUM_REVERB_COMBS    = 8;
constexpr int NUM_REVERB_ALLPASS  = 4;
constexpr int REVERB_COMB_LEN[NUM_REVERB_COMBS]   = {1116,1188,1277,1356,1422,1491,1557,1617};
constexpr int REVERB_AP_LEN[NUM_REVERB_ALLPASS]   = {556,441,341,225};
float reverbCombBuf0[1116]={0},reverbCombBuf1[1188]={0},reverbCombBuf2[1277]={0},reverbCombBuf3[1356]={0};
float reverbCombBuf4[1422]={0},reverbCombBuf5[1491]={0},reverbCombBuf6[1557]={0},reverbCombBuf7[1617]={0};
float *reverbCombBuf[NUM_REVERB_COMBS]={reverbCombBuf0,reverbCombBuf1,reverbCombBuf2,reverbCombBuf3,
                                        reverbCombBuf4,reverbCombBuf5,reverbCombBuf6,reverbCombBuf7};
float reverbApBuf0[556]={0},reverbApBuf1[441]={0},reverbApBuf2[341]={0},reverbApBuf3[225]={0};
float *reverbApBuf[NUM_REVERB_ALLPASS]={reverbApBuf0,reverbApBuf1,reverbApBuf2,reverbApBuf3};
int   reverbCombIdx[NUM_REVERB_COMBS]={0,0,0,0,0,0,0,0};
float reverbCombStore[NUM_REVERB_COMBS]={0,0,0,0,0,0,0,0}; // one-pole damping state
int   reverbApIdx[NUM_REVERB_ALLPASS]={0,0,0,0};
// Pitch-ratio (2^(cents/1200)) block-rate optimization: powf() is
// expensive to call every single sample (44100x/sec). Recomputing it
// only every PITCH_RATIO_UPDATE_INTERVAL samples and linearly
// interpolating in between is inaudible — that's still an ~11kHz
// update rate, far above any audible vibrato/LFO modulation rate (max
// 20Hz) — while cutting the powf() call count by that same factor.
float pitchRatioCur  = 1.0f;
float pitchRatioStep = 0.0f;
constexpr int PITCH_RATIO_UPDATE_INTERVAL = 4;
float vibratoPhase = 0.0f;
float tremoloPhase = 0.0f;

constexpr float VIBRATO_MAX_CENTS = 35.0f;

// ---------------------------------------------------------
// Key bend
// ---------------------------------------------------------
float keyBendMaxCents   = 200.0f;
float keyBendGoal       = 0.0f;
float keyBendCurrent    = 0.0f;
constexpr float KEY_BEND_ATTACK_SMOOTH_DEFAULT  = 0.0003f;
constexpr float KEY_BEND_RELEASE_SMOOTH_DEFAULT = 0.003f;
float keyBendAttackSmooth  = KEY_BEND_ATTACK_SMOOTH_DEFAULT;
float keyBendReleaseSmooth = KEY_BEND_RELEASE_SMOOTH_DEFAULT;

// ---------------------------------------------------------
// ADSR
// ---------------------------------------------------------
enum class EnvPhase : uint8_t { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };

struct AdsrParams {
    float attackTime   = 0.05f;
    float decayTime    = 0.15f;
    float sustainLevel = 0.7f;
    float releaseTime  = 0.3f;
} adsr;

constexpr float ADSR_MIN_TIME = 0.0f;
constexpr float ADSR_MAX_TIME = 5.0f;

float    envLevel    = 0.0f;
float    envReleaseStartLevel = 0.0f;   // v0.9993, see advanceEnvelope()
EnvPhase envPhase    = EnvPhase::IDLE;
float    playingFreq = 0.0f;

// ---------------------------------------------------------
// Filter
// ---------------------------------------------------------
enum class FilterType : uint8_t { LPF, HPF, BPF, NOTCH, NONE };

const char *filterTypeName(FilterType t);  // forward declaration

struct FilterParams {
    FilterType type      = FilterType::LPF;
    float cutoffHz       = 2000.0f;
    float resonanceQ     = 0.707f;
    float keyTracking    = 0.0f;  // 0=off, 1=full (cutoff tracks note pitch)
} filterParams;

constexpr float FILTER_CUTOFF_MIN = 100.0f;
constexpr float FILTER_CUTOFF_MAX = 8000.0f;
constexpr float FILTER_Q_MIN      = 0.5f;
constexpr float FILTER_Q_MAX      = 10.0f;

// Filter envelope: modulates cutoff with its own ADSR
struct FilterEnvParams {
    float depth      = 0.0f;    // Hz offset at peak (+/- up to 3900Hz)
    float attackTime = 0.1f;
    float decayTime  = 0.3f;
    float sustainLvl = 0.0f;
    float releaseTime= 0.3f;
} filterEnv;

float    filterEnvLevel = 0.0f;
float    filterEnvReleaseStartLevel = 0.0f;   // v0.9993
EnvPhase filterEnvPhase = EnvPhase::IDLE;

float filterB0=1.f,filterB1=0.f,filterB2=0.f;
float filterA1=0.f,filterA2=0.f;
float filterX1=0.f,filterX2=0.f;
float filterY1=0.f,filterY2=0.f;

// ---------------------------------------------------------
// General-purpose LFO
// ---------------------------------------------------------
// This LFO is independent from the existing Vibrato and Tremolo LFOs
// (those remain hard-wired to pitch / volume via the IMU mapping system).
// It can be routed to one destination at a time.
enum class LfoWave : uint8_t { SINE, TRIANGLE, SAWTOOTH, SQUARE, SAMPLE_HOLD };
// FX entries appended (v0.9876), never inserted — lfo_target is stored
// numerically in settings.json, same reasoning as ImuTarget above.
enum class LfoTarget : uint8_t { NONE, PITCH, VOLUME, TIMBRE, FILTER, SHAPE,
    FX_RING_RATE, FX_RING_MIX, FX_LIMIT_DRIVE,
    FX_CHORUS_DEPTH, FX_CHORUS_MIX, FX_DELAY_FB, FX_DELAY_MIX,
    FX_REVERB_ROOM, FX_REVERB_MIX,
    TARGET_COUNT };

struct LfoParams {
    LfoWave   wave   = LfoWave::SINE;
    float     rateHz = 2.0f;   // 0.1 - 20 Hz
    float     depth  = 0.0f;   // 0.0 - 1.0
    LfoTarget target = LfoTarget::NONE;
} lfo;

float lfoPhase = 0.0f;

// IMU-controlled offsets for the general LFO's own Rate/Depth (v0.8).
// Same additive-offset pattern as the SynthParams offsets above: never
// overwrites the LFO menu's stored rate/depth, just nudges it live.
float lfoRateOffset  = 0.0f, lfoRateOffsetTarget  = 0.0f;
float lfoDepthOffset = 0.0f, lfoDepthOffsetTarget = 0.0f;
float arpTempoOffset  = 0.0f, arpTempoOffsetTarget  = 0.0f; // +/- BPM, applied to arpTempoBpm
float arpSwingOffset= 0.0f, arpSwingOffsetTarget= 0.0f; // +/- %, applied to arpSwing
float seqTempoOffset  = 0.0f, seqTempoOffsetTarget  = 0.0f; // same IMU target (ARP_TEMPO), rerouted here when lastMainMode==SEQ
float seqSwingOffset= 0.0f, seqSwingOffsetTarget= 0.0f;     // same IMU target (ARP_SWING), rerouted here when lastMainMode==SEQ

constexpr float LFO_RATE_MIN = 0.1f;
constexpr float LFO_RATE_MAX = 20.0f;
constexpr float LFO_PITCH_MAX_CENTS = 1200.0f; // +/- 1 octave at full depth
constexpr float LFO_FILTER_MAX_HZ   = 3900.0f; // matches filter envelope range
constexpr float LFO_TIMBRE_MAX      = 3.0f;    // full morph range at full depth
constexpr float LFO_SHAPE_MAX         = 0.4f;    // +/- offset from current PWM width

const char *lfoWaveName(LfoWave w){
    switch(w){
        case LfoWave::SINE:        return "Sine";
        case LfoWave::TRIANGLE:    return "Tri";
        case LfoWave::SAWTOOTH:    return "Saw";
        case LfoWave::SQUARE:      return "Square";
        case LfoWave::SAMPLE_HOLD: return "S&H";
        default:                   return "?";
    }
}

const char *lfoTargetName(LfoTarget t){
    switch(t){
        case LfoTarget::NONE:   return "None";
        case LfoTarget::PITCH:  return "Pitch";
        case LfoTarget::VOLUME: return "Volume";
        case LfoTarget::TIMBRE: return "Timbre";
        case LfoTarget::FILTER: return "Filter";
        case LfoTarget::SHAPE:    return "Shape";
        case LfoTarget::FX_RING_RATE:    return "RingRate";
        case LfoTarget::FX_RING_MIX:     return "RingMix";
        case LfoTarget::FX_LIMIT_DRIVE:  return "LimDrive";
        case LfoTarget::FX_CHORUS_DEPTH: return "ChoDepth";
        case LfoTarget::FX_CHORUS_MIX:   return "ChoMix";
        case LfoTarget::FX_DELAY_FB:     return "DlyFdbk";
        case LfoTarget::FX_DELAY_MIX:    return "DlyMix";
        case LfoTarget::FX_REVERB_ROOM:  return "RvbRoom";
        case LfoTarget::FX_REVERB_MIX:   return "RvbMix";
        default:                return "?";
    }
}

// Samples one of the existing wavetables, normalized to roughly -1..+1.
// Reusing the oscillator tables keeps the LFO shapes consistent with
// the VCO waveform morph and avoids allocating separate tables.
// Sample & Hold LFO's currently-held value. Updated in exactly one
// place — the main per-sample audio loop, right when the LFO's phase
// wraps to a new cycle (see audioTask) — so every other reader
// (the once-per-buffer filter-cutoff calculation, the LFO screen's
// waveform preview) can safely sample it via lfoTableSample() without
// ever disturbing the real audio-rate sequence.
float lfoSampleHoldValue = 0.f;

float lfoTableSample(LfoWave w,int idx){
    switch(w){
        case LfoWave::SINE:        return sineTable[idx]/SINE_AMP;
        case LfoWave::TRIANGLE:    return triangleTable[idx]/TRIANGLE_AMP;
        case LfoWave::SAWTOOTH: {
            // Clean/naive formula, not the oscillator's band-limited
            // sawtoothTable — LFO rates (0.1-20Hz) are nowhere near audio
            // rates, so anti-aliasing buys nothing here, while the
            // truncated-harmonic-series ripple (Gibbs phenomenon) near
            // the wrap would otherwise show up as a visibly wavy curve
            // and a subtly unsteady modulation.
            float t=(float)idx/WAVE_TABLE_SIZE;
            return 2.f*t-1.f;
        }
        case LfoWave::SQUARE: {
            float t=(float)idx/WAVE_TABLE_SIZE;
            return (t<0.5f)?1.f:-1.f;
        }
        case LfoWave::SAMPLE_HOLD: return lfoSampleHoldValue;
        default:                   return 0.f;
    }
}

// ---------------------------------------------------------
// App mode
// ---------------------------------------------------------
enum class AppMode : uint8_t { PLAY, VCO, VCF, VCA, LFO, FX, SETTINGS, PATCH, CATEGORY, SEQ, PATTERN, SONG, TIMBRE };
AppMode appMode = AppMode::PLAY;
AppMode lastMainMode = AppMode::PLAY; // remembers whether PLAY or SEQ is "home" (G0 toggles between them; Tab cycling returns to whichever was last active)
bool prevTabPressed = false;
bool prevMenuUpPressed=false,prevMenuDownPressed=false;
bool prevMenuIncPressed=false,prevMenuDecPressed=false;

// ---------------------------------------------------------
// Setting item
// ---------------------------------------------------------
struct SettingItem {
    const char *name;
    void (*onIncrement)();
    void (*onDecrement)();
    const char *(*valueLabel)();
    // Which modulation targets, if any, drive this parameter (v0.9903).
    // Lets drawItemList() mark a row when an IMU axis or the LFO is
    // actually pointing at it — until now a menu showed its base value and
    // nothing else, so a parameter being modulated was invisible and the
    // sound appeared to change on its own.
    //
    // NOTE: declared with NO in-class default initializer on purpose. This
    // is a C++11 build, where adding one would make the struct non-aggregate
    // and break every existing {"Name",inc,dec,label} in this file (a trap
    // this project has hit before). Without one, aggregate initialization
    // value-initializes the extras to 0 == NONE, so untouched item arrays
    // keep working exactly as they are.
    ImuTarget imuT;
    LfoTarget lfoT;
};

// True when an IMU axis is assigned to this target. NONE never counts, or
// every unmapped row would light up.
inline bool imuTargetActive(ImuTarget t){
    return t!=ImuTarget::NONE&&(imuAxisX.target==t||imuAxisY.target==t);
}
inline bool lfoTargetActive(LfoTarget t){
    return t!=LfoTarget::NONE&&lfo.target==t;
}

bool saveSettings(); // forward declaration

// ==========================================================
// Biquad filter
// ==========================================================
// cutoffOverride/qOverride let the per-buffer envelope/LFO/drift-modulated
// values feed the filter WITHOUT ever writing them into the shared
// filterParams.cutoffHz/resonanceQ fields (v0.99946). Every other caller
// passes neither and gets the previous behaviour exactly, reading the
// live patch's own static values.
//
// This replaces a save/overwrite/restore dance that used to write the
// dynamic value directly into filterParams.cutoffHz/resonanceQ, call this
// function, then write the saved value back — a real window, however
// brief, during which those shared fields held an ENVELOPE-INFLATED value
// rather than the patch's true one. morphCapture() (loop(), the other
// core) reads those same fields with no synchronisation, and landing in
// that window meant a same-patch re-morph would sweep from whatever the
// envelope happened to be doing at that exact instant back down to the
// patch's real value over the whole morph duration — heard as a phaser,
// worst on patches with a large filter envelope depth (Lead, Bass, Brass,
// Pluck) since that is exactly what made the captured value most
// different from the true one. Not writing the shared fields at all
// removes the window rather than narrowing it with a lock.
void updateFilterCoefficients(float cutoffOverride=-1.f,float qOverride=-1.f) {
    float cut=constrain(cutoffOverride>=0.f?cutoffOverride:filterParams.cutoffHz,
                         FILTER_CUTOFF_MIN,SAMPLE_RATE*0.45f);
    float Q=constrain(qOverride>=0.f?qOverride:filterParams.resonanceQ,
                       FILTER_Q_MIN,FILTER_Q_MAX);
    float omega=2.0f*PI*cut/SAMPLE_RATE;
    float sinW=sinf(omega),cosW=cosf(omega),alpha=sinW/(2.0f*Q);
    float b0,b1,b2,a0,a1,a2;
    switch(filterParams.type){
        case FilterType::LPF:  b0=(1-cosW)/2;b1=1-cosW;b2=(1-cosW)/2;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::HPF:  b0=(1+cosW)/2;b1=-(1+cosW);b2=(1+cosW)/2;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::BPF:  b0=alpha;b1=0;b2=-alpha;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::NOTCH:b0=1;b1=-2*cosW;b2=1;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::NONE: b0=1;b1=b2=0;a0=1;a1=a2=0;break; // bypass
        default:               b0=1;b1=b2=0;a0=1;a1=a2=0;break;
    }
    filterB0=b0/a0;filterB1=b1/a0;filterB2=b2/a0;
    filterA1=a1/a0;filterA2=a2/a0;
}

int16_t applyFilter(int16_t in){
    float x0=(float)in;
    float y0=filterB0*x0+filterB1*filterX1+filterB2*filterX2-filterA1*filterY1-filterA2*filterY2;
    filterX2=filterX1;filterX1=x0;filterY2=filterY1;filterY1=y0;
    return (int16_t)constrain(y0,-32768.f,32767.f);
}

// ==========================================================
// Splash screen (v0.9998)
// ==========================================================
// The logo mark is a ring with a crosshair reaching to its edge, evoking
// the IMU X/Y axis control this synth is built around — the same visual
// language as the project's actual logo artwork. Built once into a small
// square sprite (splashLogo, declared as a global above) so the boot
// animation can rotate the WHOLE mark as one unit via pushRotateZoom(),
// rather than recomputing rotated line/circle geometry by hand every
// frame.
//
// Sequence: the mark starts tilted, as if the device had just been
// picked up, and settles level over ~900ms with an ease-out curve (fast
// start, gentle stop) — the one-shot version of the exponential-approach
// shape used for smoothing elsewhere in this file (portamento, IMU
// offsets, etc.), just driven by elapsed wall-clock time instead of a
// per-buffer smoothing constant, since this runs once, not continuously.
// Once level, the subtitle/version/credit lines appear and buildWaveTables()
// + the SD mount (both already slow enough to want covering) run while
// all of it stays on screen — this call sits exactly where the old
// single-line boot text used to, right before those two calls, so it
// still serves the same "please wait" purpose it always did, just with
// more to look at.
//
// vTaskDelay(1) between animation frames, not delay() — this project's
// own hard-won rule (see the delay()-removal history around v0.99956-57)
// is specifically about avoiding delay() inside loop()'s input-handling
// path, where it let another task's state changes go unnoticed mid-call.
// Neither risk applies here: this runs once in setup(), before loop()
// exists and before any input handling is possible, so a short blocking
// wait is exactly as safe as it looks — vTaskDelay(1) is used anyway,
// simply because yielding to the scheduler for free is never worse than
// a bare spin-wait.
// Split in two (v0.9998, hardware-tested fix — see below).
//
// drawSplashBootText() is the cheap, immediate part: plain text, no
// sprite allocation, no repeated drawing. Called early, in the exact
// position the original single-line boot text always occupied, before
// canvas/canvasTop/etc. are created, before buildWaveTables(), and
// before the SD mount.
//
// drawSplashLogoAnimation() is the full rotating-logo experience, and is
// now called AFTER initSDCard() instead of before it. A real-hardware
// test found the SD card failing to mount with the original ordering —
// this project has hit exactly this class of bug before (see the
// v0.99953-54 history: a diagnostic struct gaining a few float fields
// shifted memory layout enough to break the mount, with no full
// explanation ever found, just a correlation strong enough to make
// reverting the safe fix). canvas/canvasTop/canvasName/canvasImu/
// canvasNav — five more M5Canvas sprites — already get allocated before
// the SD mount without issue, so simply creating a sprite here isn't
// itself the likely trigger; what's genuinely new is the sustained,
// repeated pushRotateZoom() activity across dozens of frames over
// ~900ms, a pattern nothing else in setup() does. Rather than trying to
// prove which part is responsible, the whole animation — sprite
// creation included — moves to run after the SD mount has already
// succeeded or failed, matching the same conservative fix this project
// already has a precedent for.
void drawSplashBootText(){
    // Text removed (v0.99984), on request — the old single-line message
    // was only ever meant as a "something is happening" placeholder for
    // the SD-mount window before the real splash existed; now that the
    // real splash follows shortly after, having this flash briefly first
    // reads as a stray, unintended screen rather than a deliberate one.
    // A plain black screen for that window is the plainer, calmer choice.
    M5Cardputer.Display.fillScreen(BLACK);
}

void drawSplashLogoAnimation(){
    splashLogo.setColorDepth(16);
    splashLogo.setPsram(true);
    // 80x80 -> 75x75 (v0.99988) — this time sized against a real number
    // instead of another guess: the previous failure's diagnostic log
    // reported free heap 24,528 with the LARGEST CONTIGUOUS BLOCK only
    // 11,764 of that — the heap is fragmented after SD.begin(), not
    // simply short on total free memory, so the ceiling that actually
    // matters is the block size, not the free-heap figure. 80x80x16bpp
    // (12,800 bytes) exceeded that block by just over 1,000 bytes, which
    // is exactly why it failed while smaller sizes hadn't. 75x75x16bpp is
    // 11,250 bytes — about 500 bytes of margin under the measured
    // ceiling, since fragmentation can vary slightly boot to boot and
    // 76x76 (11,552) would leave almost none. Bigger than this would
    // need addressing the fragmentation itself, a different problem than
    // picking a larger number.
    bool logoOk=splashLogo.createSprite(75,75);
    if(!logoOk){
        // Diagnostic, not a silent fallback (v0.99981) — the previous
        // version had no visibility into WHY this failed on real
        // hardware beyond "it did"; this gives the next boot log an
        // actual number to look at instead of another guess.
        Serial.printf("[Splash] logo sprite alloc FAILED — free heap %u, largest block %u\n",
            (unsigned)ESP.getFreeHeap(),(unsigned)ESP.getMaxAllocHeap());
    }
    if(logoOk){
        splashLogo.fillSprite(BLACK);
        const int cx=37,cy=37,ringR=29,innerR=22;
        uint16_t ring=splashLogo.color565(95,168,224); // #5FA8E0
        splashLogo.drawCircle(cx,cy,ringR,ring);
        splashLogo.drawCircle(cx,cy,ringR-1,ring); // ~2px stroke
        splashLogo.drawCircle(cx,cy,innerR,ring);  // thin inner ring
        splashLogo.drawLine(cx,cy-ringR,cx,cy+ringR,ring); // crosshair,
        splashLogo.drawLine(cx-ringR,cy,cx+ringR,cy,ring); // edge to edge
        splashLogo.setTextColor(WHITE,BLACK);
        splashLogo.setTextSize(1);
        splashLogo.drawString("CPS",cx-9,cy-4); // manually centered —
        // this file never uses a text-datum API (setTextDatum etc. don't
        // appear anywhere else in it), so this keeps the same
        // hand-computed-offset convention as every other text draw here.
    }

    // Fade-in (v0.99984), replacing the rotation animation removed last
    // round and the staggered per-line reveal before that, on request:
    // the whole mark and all three text lines are drawn together as one
    // finished frame, dim, then the SCREEN BACKLIGHT itself ramps from
    // dim to full via setBrightness() — the same call applyUiBrightness()
    // already uses elsewhere in this file, so nothing new or unproven is
    // being asked of the hardware here. This is cheaper than the
    // rotation it replaces (one brightness register write per step, no
    // per-pixel transform or resampling at all) and sidesteps
    // pushRotateZoom entirely, which is the API every hardware surprise
    // in this feature so far has come from.
    M5Cardputer.Display.fillScreen(BLACK);
    const int logoCx=120, logoCy=55;
    // UI_BRIGHT_MIN (32), not 0 — this project already treats 32 as the
    // established minimum safe brightness (see its own definition
    // earlier in this file); starting the fade from an untested 0 isn't
    // worth it when a proven floor is right there.
    M5Cardputer.Display.setBrightness(UI_BRIGHT_MIN);
    M5Cardputer.Display.startWrite();
    if(logoOk){
        splashLogo.pushSprite(logoCx-37,logoCy-37); // matches the plain
        // pushSprite(x,y) form canvas.pushSprite(0,0) already uses
        // successfully throughout this file — both splashLogo and
        // canvas were constructed with &M5Cardputer.Display as their
        // parent, so neither needs (or should need) an explicit
        // destination argument.
    } else {
        // Fallback if the sprite failed to allocate (out of memory etc.)
        // — at minimum still show readable text, matching the plain
        // single-line boot text this replaces.
        M5Cardputer.Display.setTextColor(uiColor,BLACK);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.drawString("C.P.S.",10,10);
    }
    M5Cardputer.Display.setTextColor(uiColor,BLACK);
    M5Cardputer.Display.setTextSize(1);
    // Manually centered, same convention as the logo text above. Each
    // char is ~6px at textSize(1); width = 6*charCount.
    auto centerX=[&](const char*s){ return 120-(int)(strlen(s)*3); };
    M5Cardputer.Display.drawString("CARDPUTER SYNTH",centerX("CARDPUTER SYNTH"),100);
    M5Cardputer.Display.drawString("v1.0",centerX("v1.0"),114);
    // Credit line removed (v0.99989), on request — read as too prominent
    // sitting right below the version. Just the mark, subtitle, and
    // version now.
    M5Cardputer.Display.endWrite();

    if(logoOk){
        // Freed here (v0.99982) — the actual cause of a much more
        // serious symptom than anything from the previous round: no
        // sound at all, with the boot log showing I2S DMA-buffer
        // allocation failing on a tight repeating loop the moment a key
        // was pressed. This board has no PSRAM at all, so the five
        // existing UI canvases (canvas/canvasTop/canvasName/canvasImu/
        // canvasNav, ~130KB combined) already draw from the same small
        // internal-DRAM pool as everything else, including the I2S
        // driver's own DMA buffers. splashLogo was a GLOBAL M5Canvas
        // that createSprite() had allocated but nothing ever freed — it
        // sat there permanently for the rest of the program's life,
        // taking its ~10KB out of circulation right when the speaker's
        // own driver needed to claim DMA-capable memory later in
        // setup(). Sizing it down (the previous round's fix) helped but
        // was solving the wrong half of the problem: the fix that
        // actually matters is not holding onto it at all past the
        // moment this function is done with it.
        splashLogo.deleteSprite();
    }

    // Ramp brightness up over ~900ms, in steps small enough (roughly
    // every 25ms) to read as a smooth fade rather than a few visible
    // jumps, then hold at full brightness for another ~900ms so the
    // finished screen is clearly readable for a total of close to 2
    // seconds, not just the fade itself.
    const uint8_t fadeTarget=uiBrightness; // the user's own configured
    // level, not a hardcoded max — loadSettings() has not necessarily
    // run yet at this point in setup(), so this may still be the
    // UI_BRIGHT_MAX default rather than a saved preference, but that is
    // no different from how the rest of the UI behaves before settings
    // load, and applyUiBrightness() corrects it again shortly after
    // regardless.
    // Slowed 900ms->1800ms and switched linear->smoothstep (v0.99985),
    // on request — a linear ramp read as fast/abrupt on real hardware.
    // Smoothstep (3p²-2p³) eases in and out at both ends of the fade
    // rather than changing brightness at a constant rate throughout,
    // which is the gentler curve being asked for.
    const unsigned long fadeMs=1800;
    unsigned long fadeStart=millis();
    while(true){
        unsigned long t=millis()-fadeStart;
        if(t>=fadeMs)break;
        float p=(float)t/(float)fadeMs;
        float eased=p*p*(3.f-2.f*p); // smoothstep
        uint8_t level=(uint8_t)(UI_BRIGHT_MIN+eased*(float)(fadeTarget-UI_BRIGHT_MIN));
        M5Cardputer.Display.setBrightness(level);
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    M5Cardputer.Display.setBrightness(fadeTarget);
    vTaskDelay(pdMS_TO_TICKS(900)); // hold the finished, fully-lit screen
}

// ==========================================================
// Wavetable
// ==========================================================
void buildWaveTables(){
    for(int i=0;i<WAVE_TABLE_SIZE;i++){
        float t=float(i)/WAVE_TABLE_SIZE,rad=2*PI*t;
        sineTable[i]    =(int16_t)(sinf(rad)*SINE_AMP);
        float tri=(t<0.5f)?(4*t-1):(3-4*t);
        triangleTable[i]=(int16_t)(tri*TRIANGLE_AMP);
    }
    // Band-limited additive synthesis for Sawtooth/Square: sum a limited
    // number of harmonics instead of the naive linear-ramp/step formula.
    // A naive saw/square has (in principle) infinite harmonic content, so
    // on higher notes those upper harmonics exceed Nyquist and fold back
    // as harsh, inharmonic aliasing noise — audibly worse the higher the
    // note. Building the table this way instead keeps every harmonic
    // that's actually in the table safely below the table's own Nyquist
    // (256 samples -> up to the 128th harmonic is representable; 20 is
    // comfortably inside that), trading a little brightness at extreme
    // octave-shifted-up notes for a noticeably cleaner sound everywhere
    // else. This only runs once at boot — zero effect on audioTask's
    // per-sample budget.
    constexpr int SAW_HARMONICS=20;
    constexpr int SQR_HARMONICS=19; // odd only below this, so ~10 actual partials
    float sawBuf[WAVE_TABLE_SIZE], sqrBuf[WAVE_TABLE_SIZE];
    float sawPeak=0.f, sqrPeak=0.f;
    for(int i=0;i<WAVE_TABLE_SIZE;i++){
        float ph=2.f*PI*i/WAVE_TABLE_SIZE;
        float sawSum=0.f;
        for(int h=1;h<=SAW_HARMONICS;h++)sawSum+=sinf(h*ph)/h;
        sawBuf[i]=sawSum;
        if(fabsf(sawSum)>sawPeak)sawPeak=fabsf(sawSum);
        float sqrSum=0.f;
        for(int h=1;h<=SQR_HARMONICS;h+=2)sqrSum+=sinf(h*ph)/h;
        sqrBuf[i]=sqrSum;
        if(fabsf(sqrSum)>sqrPeak)sqrPeak=fabsf(sqrSum);
    }
    // Normalize each to the existing amplitude constants, same as before —
    // downstream code (getMorphedSample, PWM blend, etc.) is unaffected.
    for(int i=0;i<WAVE_TABLE_SIZE;i++){
        sawtoothTable[i]=(int16_t)(sawBuf[i]/sawPeak*SAWTOOTH_AMP);
        squareTable[i]  =(int16_t)(sqrBuf[i]/sqrPeak*SQUARE_AMP);
    }

    // Wavefolder (v0.983): drive a sine hard enough that it folds back on
    // itself (reflects rather than clips) several times per cycle,
    // producing a rich, complex harmonic texture — classic "West Coast"
    // synthesis character, distinct from the other four waveforms. Boot-
    // time only, same as everything else in this function.
    constexpr float WAVEFOLD_DRIVE=3.5f; // higher = more folds/richer harmonics
    for(int i=0;i<WAVE_TABLE_SIZE;i++){
        float t=float(i)/WAVE_TABLE_SIZE,rad=2*PI*t;
        float driven=sinf(rad)*WAVEFOLD_DRIVE;
        while(driven>1.f||driven<-1.f){
            if(driven>1.f)driven=2.f-driven;
            if(driven<-1.f)driven=-2.f-driven;
        }
        wavefolderTable[i]=(int16_t)(driven*WAVEFOLDER_AMP);
    }

    // Half-Sine / rectified (v0.9831): only the positive half of a sine
    // plays, the negative half is clamped to silence — a buzzy, reedy
    // character distinct from the other five. A true half-wave rectified
    // signal has a strong DC bias (its average isn't zero), which could
    // push a nonzero offset through the filter/mix downstream, so the
    // table is re-centered (mean subtracted) before normalizing to the
    // full amplitude range, consistent with every other table here.
    float hsBuf[WAVE_TABLE_SIZE];
    float hsMean=0.f;
    for(int i=0;i<WAVE_TABLE_SIZE;i++){
        float t=float(i)/WAVE_TABLE_SIZE,rad=2*PI*t;
        float s=sinf(rad);
        hsBuf[i]=(s>0.f)?s:0.f;
        hsMean+=hsBuf[i];
    }
    hsMean/=WAVE_TABLE_SIZE;
    float hsPeak=0.f;
    for(int i=0;i<WAVE_TABLE_SIZE;i++){
        hsBuf[i]-=hsMean;
        if(fabsf(hsBuf[i])>hsPeak)hsPeak=fabsf(hsBuf[i]);
    }
    for(int i=0;i<WAVE_TABLE_SIZE;i++){
        halfSineTable[i]=(int16_t)(hsBuf[i]/hsPeak*HALFSINE_AMP);
    }

    // ---- Shape=1 extreme tables (v0.985) ----
    // Sine -> "bent sine": a touch of 2nd harmonic added for asymmetry,
    // a gentle, subtle shape change appropriate for the "purest" waveform.
    {
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float rad=2.f*PI*i/WAVE_TABLE_SIZE;
            buf[i]=sinf(rad)+0.4f*sinf(2.f*rad);
            if(fabsf(buf[i])>peak)peak=fabsf(buf[i]);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)sineTableB[i]=(int16_t)(buf[i]/peak*SINE_AMP);
    }
    // Triangle -> skewed toward Sawtooth (asymmetric rise/fall, ~90/10).
    {
        constexpr float SKEW=0.9f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float t=float(i)/WAVE_TABLE_SIZE;
            float tri=(t<SKEW)?(2.f*t/SKEW-1.f):(1.f-2.f*(t-SKEW)/(1.f-SKEW));
            triangleTableB[i]=(int16_t)(tri*TRIANGLE_AMP);
        }
    }
    // Sawtooth -> "double saw": two ramps per cycle, giving the notched/
    // stepped look-and-sound the Nord Wave 2 chart showed for Saw's
    // extreme Osc Ctrl setting. Same band-limiting approach as the base
    // Sawtooth table, just at double the fundamental repetition rate.
    {
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float ph=2.f*2.f*PI*i/WAVE_TABLE_SIZE; // double rate
            float s=0.f;
            for(int h=1;h<=SAW_HARMONICS;h++)s+=sinf(h*ph)/h;
            buf[i]=s;
            if(fabsf(s)>peak)peak=fabsf(s);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)sawtoothTableB[i]=(int16_t)(buf[i]/peak*SAWTOOTH_AMP);
    }
    // Square -> narrow pulse (~10% duty). This absorbs what PWM used to
    // do — Square's own Shape=1 IS the narrow-pulse extreme, interpolated
    // continuously from the base 50%-duty Square at Shape=0.
    // Wavefolder -> more folds (higher drive than the Shape=0 table).
    {
        constexpr float WAVEFOLD_DRIVE_B=6.0f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float rad=2.f*PI*i/WAVE_TABLE_SIZE;
            float driven=sinf(rad)*WAVEFOLD_DRIVE_B;
            while(driven>1.f||driven<-1.f){
                if(driven>1.f)driven=2.f-driven;
                if(driven<-1.f)driven=-2.f-driven;
            }
            wavefolderTableB[i]=(int16_t)(driven*WAVEFOLDER_AMP);
        }
    }
    // Half-Sine -> narrower positive lobe (steeper spike), same
    // re-centering treatment as the Shape=0 table so it doesn't push a
    // DC bias downstream either.
    {
        constexpr float HS_NARROW=0.25f; // fraction of the cycle the positive lobe occupies
        float buf[WAVE_TABLE_SIZE]; float mean=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float t=float(i)/WAVE_TABLE_SIZE;
            float s=(t<HS_NARROW)?sinf(PI*t/HS_NARROW):0.f;
            buf[i]=s;
            mean+=s;
        }
        mean/=WAVE_TABLE_SIZE;
        float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){buf[i]-=mean;if(fabsf(buf[i])>peak)peak=fabsf(buf[i]);}
        for(int i=0;i<WAVE_TABLE_SIZE;i++)halfSineTableB[i]=(int16_t)(buf[i]/peak*HALFSINE_AMP);
    }

    // Parabolic (v0.986): a smooth, rounded "arch" from -1 up to +1 and
    // back down — like a Triangle with its sharp corners rounded off,
    // giving fewer high harmonics and a softer, mellower character.
    for(int i=0;i<WAVE_TABLE_SIZE;i++){
        float t=float(i)/WAVE_TABLE_SIZE;
        float y=4.f*t*(1.f-t)*2.f-1.f;
        parabolicTable[i]=(int16_t)(y*PARABOLIC_AMP);
    }

    // ESaw (v0.986): an exponential-curved Sawtooth — starts slow and
    // accelerates through the cycle, instead of Sawtooth's constant-rate
    // linear ramp, giving it a distinctly different character despite
    // the shared "ramp then jump back" overall shape.
    {
        constexpr float ESAW_CURVE=3.f;
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float t=float(i)/WAVE_TABLE_SIZE;
            float y=2.f*(expf(ESAW_CURVE*t)-1.f)/(expf(ESAW_CURVE)-1.f)-1.f;
            buf[i]=y;
            if(fabsf(y)>peak)peak=fabsf(y);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)esawTable[i]=(int16_t)(buf[i]/peak*ESAW_AMP);
    }

    // Squeeze (v0.9862): a sine phase-distorted by itself — compresses
    // one side of the wave into a narrower span, expanding the other,
    // giving an asymmetric "squeezed" sine character.
    {
        constexpr float SQUEEZE_AMOUNT=0.6f;
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float rad=2.f*PI*i/WAVE_TABLE_SIZE;
            float y=sinf(rad+SQUEEZE_AMOUNT*sinf(rad));
            buf[i]=y;
            if(fabsf(y)>peak)peak=fabsf(y);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)squeezeTable[i]=(int16_t)(buf[i]/peak*SQUEEZE_AMP);
    }

    // ESquare (v0.9862): a sine pushed through tanh — a smoothly-rounded
    // square-like shape (no hard edges, so no additive-harmonic aliasing
    // concern), distinct in character from the additive-synthesis Square.
    {
        constexpr float ESQUARE_DRIVE=2.5f;
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float rad=2.f*PI*i/WAVE_TABLE_SIZE;
            float y=tanhf(ESQUARE_DRIVE*sinf(rad));
            buf[i]=y;
            if(fabsf(y)>peak)peak=fabsf(y);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)esquareTable[i]=(int16_t)(buf[i]/peak*ESQUARE_AMP);
    }

    // Saw2 (v0.9862): an alternate Sawtooth character — sine phase-
    // distorted by its own 2nd harmonic, giving a different asymmetric
    // ramp-like shape than either the additive-synthesis Sawtooth or ESaw.
    {
        constexpr float SAW2_AMOUNT=0.8f;
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float rad=2.f*PI*i/WAVE_TABLE_SIZE;
            float y=sinf(rad-SAW2_AMOUNT*sinf(2.f*rad));
            buf[i]=y;
            if(fabsf(y)>peak)peak=fabsf(y);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)saw2Table[i]=(int16_t)(buf[i]/peak*SAW2_AMP);
    }

    // Square2 (v0.9862): an alternate Square character — combines the
    // same phase-distortion idea with the tanh waveshaper, giving a
    // squared-off shape with a subtly different asymmetry than ESquare.
    {
        constexpr float SQUARE2_DRIVE=4.f, SQUARE2_PD=0.3f;
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float rad=2.f*PI*i/WAVE_TABLE_SIZE;
            float y=tanhf(SQUARE2_DRIVE*sinf(rad+SQUARE2_PD*sinf(2.f*rad)));
            buf[i]=y;
            if(fabsf(y)>peak)peak=fabsf(y);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)square2Table[i]=(int16_t)(buf[i]/peak*SQUARE2_AMP);
    }

    // ---- Shape=1 tables for the 6 waveforms added in v0.986/v0.9862 ----
    // Parabolic -> flatter-topped arch (a fractional power of sine
    // flattens the peak while keeping the same zero-crossings).
    {
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float t=float(i)/WAVE_TABLE_SIZE;
            float y=2.f*powf(sinf(PI*t),0.3f)-1.f;
            buf[i]=y;
            if(fabsf(y)>peak)peak=fabsf(y);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)parabolicTableB[i]=(int16_t)(buf[i]/peak*PARABOLIC_AMP);
    }
    // ESaw -> steeper exponential curve (more extreme acceleration).
    {
        constexpr float ESAW_CURVE_B=6.f;
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float t=float(i)/WAVE_TABLE_SIZE;
            float y=2.f*(expf(ESAW_CURVE_B*t)-1.f)/(expf(ESAW_CURVE_B)-1.f)-1.f;
            buf[i]=y;
            if(fabsf(y)>peak)peak=fabsf(y);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)esawTableB[i]=(int16_t)(buf[i]/peak*ESAW_AMP);
    }
    // Squeeze -> stronger self-phase-distortion (more pronounced squeeze).
    {
        constexpr float SQUEEZE_AMOUNT_B=1.2f;
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float rad=2.f*PI*i/WAVE_TABLE_SIZE;
            float y=sinf(rad+SQUEEZE_AMOUNT_B*sinf(rad));
            buf[i]=y;
            if(fabsf(y)>peak)peak=fabsf(y);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)squeezeTableB[i]=(int16_t)(buf[i]/peak*SQUEEZE_AMP);
    }
    // ESquare -> harder tanh drive (closer to a true square, still smooth).
    {
        constexpr float ESQUARE_DRIVE_B=6.f;
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float rad=2.f*PI*i/WAVE_TABLE_SIZE;
            float y=tanhf(ESQUARE_DRIVE_B*sinf(rad));
            buf[i]=y;
            if(fabsf(y)>peak)peak=fabsf(y);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)esquareTableB[i]=(int16_t)(buf[i]/peak*ESQUARE_AMP);
    }
    // Saw2 -> stronger 2nd-harmonic phase-distortion (more extreme ramp).
    {
        constexpr float SAW2_AMOUNT_B=1.4f;
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float rad=2.f*PI*i/WAVE_TABLE_SIZE;
            float y=sinf(rad-SAW2_AMOUNT_B*sinf(2.f*rad));
            buf[i]=y;
            if(fabsf(y)>peak)peak=fabsf(y);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)saw2TableB[i]=(int16_t)(buf[i]/peak*SAW2_AMP);
    }
    // Square2 -> harder drive + more phase-distortion (more pronounced
    // asymmetric squaring).
    {
        constexpr float SQUARE2_DRIVE_B=7.f, SQUARE2_PD_B=0.6f;
        float buf[WAVE_TABLE_SIZE]; float peak=0.f;
        for(int i=0;i<WAVE_TABLE_SIZE;i++){
            float rad=2.f*PI*i/WAVE_TABLE_SIZE;
            float y=tanhf(SQUARE2_DRIVE_B*sinf(rad+SQUARE2_PD_B*sinf(2.f*rad)));
            buf[i]=y;
            if(fabsf(y)>peak)peak=fabsf(y);
        }
        for(int i=0;i<WAVE_TABLE_SIZE;i++)square2TableB[i]=(int16_t)(buf[i]/peak*SQUARE2_AMP);
    }
}

// Square's Shape = pulse width (v0.9891).
//
// It used to be a crossfade between a 50%-duty band-limited square and a
// hard-edged 10% pulse, which is not what a duty sweep is: mixing those two
// gives their sum — a stepped, three-level waveform — rather than a square
// of some intermediate width. On hardware that showed up as an obviously
// wrong shape anywhere between the extremes.
//
// The table is rebuilt instead, using the identity that a pulse is the
// difference of two sawtooths offset in phase by the pulse width:
//   pulse(x, w) = saw(x) - saw(x - w)
// Since sawtoothTable is already band-limited, so is the result — no extra
// aliasing, and no per-sample cost at all. Rebuilding 256 entries when the
// value actually changes is also cheaper than 1024 extra lookups per buffer
// would have been.
//
// Normalization keeps the peak at SQUARE_AMP regardless of width: the two
// levels are asymmetric (a narrow pulse sits high and brief over a shallow
// baseline), so without it a thin pulse would clip.
float squareDutyBuilt=-1.f;   // width currently in the table; -1 = never built
// Oscillator 2 needs its OWN square table (v0.9935). Square's Shape is a
// duty sweep baked into the table rather than a crossfade between two
// tables, and there was only one table — rebuilt from oscillator 1's
// Shape. So oscillator 2's Square ignored its own Shape entirely and
// followed oscillator 1's instead, which is also why Square looked like
// the one waveform that responded on the VCO 2 page: what was moving was
// oscillator 1's duty, not oscillator 2's.
int16_t squareTable2[WAVE_TABLE_SIZE];
float squareDutyBuilt2=-1.f;
// Shared builder so both oscillators get identical maths.
void buildSquareDuty(int16_t *dst,float &built,float shape){
    // Shape 0 -> 10% duty, 0.5 -> 50% (symmetrical), 1 -> 90% (v0.9892).
    // This is the mapping the original PWM control had, restored after
    // hardware photos of the pre-Shape firmware showed it swept BOTH ways
    // around a symmetrical square at the mid point. v0.9891 only went one
    // way — symmetrical at Shape 0, narrowing from there — so the wide
    // side was unreachable and the default Shape of 0.5 landed on a 30%
    // pulse instead of a plain square. Linear across the whole knob rather
    // than clamping at 10/90, so there are no dead zones at either end.
    float duty=0.1f+constrain(shape,0.f,1.f)*0.8f;
    if(fabsf(duty-built)<0.0005f)return;
    built=duty;
    int w=(int)(duty*WAVE_TABLE_SIZE+0.5f);
    if(w<1)w=1; if(w>=WAVE_TABLE_SIZE)w=WAVE_TABLE_SIZE-1;
    // Peak-to-peak is what is held constant (v0.9894), not peak. Earlier
    // versions normalized the peak, which meant the waveform visibly and
    // audibly shrank as the width moved away from 50% — a pulse is
    // asymmetric about zero once it is DC-free, so holding the larger side
    // fixed lets the total span collapse. A real pulse wave keeps its span
    // and moves where zero sits inside it, which is what the pre-Shape
    // firmware showed and what this now does.
    //
    // The cost is headroom: at the 10%/90% extremes 90% of the span sits
    // on one side of zero, so the span has to be sized against that worst
    // case rather than against the symmetrical one. Sizing it so the
    // extremes peak at 26000 — in line with ESQUARE_AMP, the loudest
    // waveform here — puts the symmetrical square at about +/-14400 rather
    // than the +/-18000 it had, roughly 1.9dB quieter. That is a fair
    // trade: it also makes the loudness change across a PWM sweep much
    // gentler (RMS varies 1.67x rather than 2.6x), which is what a sweep
    // should sound like.
    //
    // Measured rather than derived because band-limiting adds Gibbs
    // overshoot an analytical figure would miss. Static rather than stack:
    // 1KB of locals inside audioTask is not worth the risk, and this is
    // only ever reached from that one task.
    constexpr float SQUARE_PEAK_LIMIT=26000.f;      // worst-case excursion
    constexpr float SQUARE_PULSE_PP  =SQUARE_PEAK_LIMIT/0.9f;
    static float tmp[WAVE_TABLE_SIZE];
    float hi=-1e9f,lo=1e9f;
    for(int i=0;i<WAVE_TABLE_SIZE;i++){
        int j=i-w; if(j<0)j+=WAVE_TABLE_SIZE;
        tmp[i]=(float)sawtoothTable[i]-(float)sawtoothTable[j];
        if(tmp[i]>hi)hi=tmp[i];
        if(tmp[i]<lo)lo=tmp[i];
    }
    float pp=hi-lo; if(pp<1e-6f)pp=1e-6f;
    float sc=SQUARE_PULSE_PP/pp;
    for(int i=0;i<WAVE_TABLE_SIZE;i++)dst[i]=(int16_t)(tmp[i]*sc);
}

void updateSquareDuty(float shape){buildSquareDuty(squareTable,squareDutyBuilt,shape);}
void updateSquareDuty2(float shape){buildSquareDuty(squareTable2,squareDutyBuilt2,shape);}

int16_t getMorphedSample(int idx,float morph,float shape){
    int maxMorph=max(1,morphChainLen-1);
    morph=constrain(morph,0.f,(float)maxMorph);
    int lo=constrain((int)morph,0,max(0,morphChainLen-2));
    float frac=(morphChainLen<2)?0.f:morph-(float)lo;
    float sh=constrain(shape,0.f,1.f);

    // Shape interpolates each of the two morph-blended waveforms between
    // its own Shape=0/Shape=1 tables FIRST, then the morph blend runs on
    // top of the two already-shaped results. This is a uniform mechanism
    // for every waveform in the library — it's what replaced the old
    // Square-only PWM special case (Square's own Shape=1 table IS the
    // narrow-pulse extreme PWM used to produce).
    int hi=min(lo+1,morphChainLen-1);
    // Defensive: a null would mean refreshMorphTablePtrs() never ran for
    // this slot. Cheaper to check than to risk a crash inside audioTask.
    // refreshMorphTablePtrs() locks internally now (v0.99943), so the hot
    // path above this — the overwhelming majority of calls — never pays
    // for it; only this rare fallback does.
    if(!morphTblA[lo]||!morphTblA[hi])refreshMorphTablePtrs();
    int16_t sampleLo=(int16_t)(morphTblA[lo][idx]*(1-sh)+morphTblB[lo][idx]*sh);
    int16_t sampleHi=(int16_t)(morphTblA[hi][idx]*(1-sh)+morphTblB[hi][idx]*sh);
    return (int16_t)(sampleLo*(1-frac)+sampleHi*frac);
}

// Simple white noise via LCG (Linear Congruential Generator)
uint32_t noiseSeed = 12345;
int16_t nextNoise(){
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    return (int16_t)(noiseSeed >> 16);
}

// v0.989: takes the precomputed quantization level count rather than the
// raw amount. This used to call powf() on every single sample, which is
// exactly what this file's own rule about expensive math in the per-sample
// loop forbids — the amount can only change once per buffer, so the powf()
// now happens there (see updateFxEffective) and this is left with a round
// and two multiplies.
int16_t applyBitcrush(int16_t s,float levels){
    if(levels<=0.f) return s;
    // Clamped before the cast (v0.9902). Rounding pushes values near full
    // scale UP past it — at 4 levels an input of 30000 rounds to 32768,
    // one above int16's maximum, and the cast wraps it to -32768. A loud
    // note hit that on most peaks, turning the effect into a burst of
    // wrapped noise rather than the quantization it should be.
    float q=roundf(s/32768.f*levels)/levels*32768.f;
    if(q> 32767.f)q= 32767.f;
    if(q<-32768.f)q=-32768.f;
    return (int16_t)q;
}

// Effective FX parameter values for the current buffer (v0.9876) — the
// menu value plus any IMU offset and LFO modulation, already clamped
// back into each parameter's own range. The apply* functions below read
// these rather than params.* directly, so modulation costs nothing in
// the per-sample loop: it is all resolved once per buffer, matching how
// filter cutoff and every other modulated parameter is handled here.
// Initialized to the same defaults as their base parameters so they are
// sane before the first buffer is ever generated.
float fxEffRingRateHz    = 200.0f;
float fxEffRingMix       = 0.0f;
float fxEffLimiterDrive  = 2.5f;
float fxEffChorusDepthMs = 15.0f;
float fxEffChorusMix     = 0.0f;
float fxEffDelayFeedback = 0.3f;
float fxEffDelayMix      = 0.0f;
float fxEffReverbFeedback= 0.84f; // comb feedback, derived from Room Size
float fxEffReverbDamp    = 0.2f;  // one-pole coefficient in the comb feedback
float fxEffReverbMix     = 0.0f;
// v0.989: Vibrato/Tremolo/Bit-crusher joined the same once-per-buffer
// resolution. fxEffBitcrushLevels is the quantization step count, not the
// amount — the powf() that derives it used to run per sample.
float fxEffVibratoDepth  = 0.0f;
float fxEffVibratoRateHz = 5.0f;
float fxEffTremoloDepth  = 0.0f;
float fxEffBitcrushLevels= 0.0f;

// Resolves the seven modulatable FX parameters for this buffer.
// lfoVal is the general LFO's current value scaled by its Depth, already
// sampled once for this buffer by the caller; it only lands on whichever
// single parameter lfo.target names, since the LFO has one destination.
//
// Delay Time is deliberately NOT modulatable. applyDelay() reads its
// delay line at a plain integer offset, so sweeping the time would jump
// the read position a whole sample at a step and produce zipper noise,
// and would also re-pitch the echoes already recirculating in the
// buffer. Doing it properly needs fractional/interpolated reads like
// Chorus already has — a worthwhile change on its own terms, but one
// that alters how the existing Delay sounds, so it is kept separate.
// Chorus Rate is left out for a different reason: modulating the rate of
// one LFO with another is hard to hear as anything but drift.
void updateFxEffective(float lfoVal){
    float lRingRate = (lfo.target==LfoTarget::FX_RING_RATE)   ? lfoVal*990.f : 0.f;
    float lRingMix  = (lfo.target==LfoTarget::FX_RING_MIX)    ? lfoVal       : 0.f;
    float lLimDrive = (lfo.target==LfoTarget::FX_LIMIT_DRIVE) ? lfoVal*2.f   : 0.f;
    float lChoDepth = (lfo.target==LfoTarget::FX_CHORUS_DEPTH)? lfoVal*10.f  : 0.f;
    float lChoMix   = (lfo.target==LfoTarget::FX_CHORUS_MIX)  ? lfoVal       : 0.f;
    float lDlyFb    = (lfo.target==LfoTarget::FX_DELAY_FB)    ? lfoVal*0.45f : 0.f;
    float lDlyMix   = (lfo.target==LfoTarget::FX_DELAY_MIX)   ? lfoVal       : 0.f;
    float lRvbRoom  = (lfo.target==LfoTarget::FX_REVERB_ROOM) ? lfoVal*0.5f  : 0.f;
    float lRvbMix   = (lfo.target==LfoTarget::FX_REVERB_MIX)  ? lfoVal       : 0.f;

    fxEffRingRateHz    = constrain(params.ringModRateHz +params.ringModRateOffset  +lRingRate, 20.f, 2000.f);
    fxEffRingMix       = constrain(params.ringModMix    +params.ringModMixOffset   +lRingMix,   0.f,    1.f);
    fxEffLimiterDrive  = constrain(params.limiterDrive  +params.limiterDriveOffset +lLimDrive,  1.f,    5.f);
    fxEffChorusDepthMs = constrain(params.chorusDepthMs +params.chorusDepthOffset  +lChoDepth,  0.f,   20.f);
    fxEffChorusMix     = constrain(params.chorusMix     +params.chorusMixOffset    +lChoMix,    0.f,    1.f);
    fxEffDelayFeedback = constrain(params.delayFeedback +params.delayFeedbackOffset+lDlyFb,     0.f,   0.9f);
    fxEffDelayMix      = constrain(params.delayMix      +params.delayMixOffset     +lDlyMix,    0.f,    1.f);

    // A pad that reads "off" must actually BE off. Mix=0 is this
    // project's off switch, and fxIsOn() (which decides how the pad is
    // drawn) tests the base value — so without this, tilting into a Mix
    // offset would make an effect audible while its pad still showed as
    // off. Fading an effect up from near-silence with tilt still works:
    // turn the pad on and set a low base Mix. The other parameters need
    // no such guard, since an effect whose Mix is zero never runs them.
    // Reverb. Room Size and Damping are mapped rather than used directly:
    // comb feedback below ~0.7 decays too fast to read as a room at all,
    // and full damping would kill the tail outright, so both are scaled
    // into the useful part of their range (Freeverb's own constants).
    float room=constrain(params.reverbRoomSize+params.reverbRoomOffset+lRvbRoom,0.f,1.f);
    fxEffReverbFeedback=0.70f+room*0.28f;                       // 0.70 - 0.98
    fxEffReverbDamp    =constrain(params.reverbDamping,0.f,1.f)*0.4f;
    fxEffReverbMix     =constrain(params.reverbMix+params.reverbMixOffset+lRvbMix,0.f,1.f);

    if(params.ringModMix<0.001f)fxEffRingMix  =0.f;
    if(params.chorusMix <0.001f)fxEffChorusMix=0.f;
    if(params.delayMix  <0.001f)fxEffDelayMix =0.f;
    if(params.reverbMix <0.001f)fxEffReverbMix=0.f;

    fxEffVibratoDepth =constrain(params.vibratoDepth +params.vibratoDepthOffset,0.f,1.f);
    fxEffVibratoRateHz=constrain(params.vibratoRateHz+params.vibratoRateOffset,1.f,10.f);
    fxEffTremoloDepth =constrain(params.tremoloDepth +params.tremoloDepthOffset,0.f,1.f);
    float crush=constrain(params.bitcrush+params.bitcrushOffset,0.f,1.f);
    // 0 means "off"; applyBitcrush() treats a level count of 0 as bypass.
    // Range reworked in v0.9894. The mapping used to run 16 bits down to
    // 3, which sounds like a sensible full sweep but wastes most of the
    // control: the source is already 16-bit, so nothing at all is audible
    // until the depth drops below roughly 10 bits, and the old curve did
    // not reach that until about 45%. Half the travel did nothing and the
    // effect only announced itself near the top. 10 bits down to 2 puts
    // the audible range across the whole control — 30% now lands on 7.6
    // bits where the old curve gave 12.1. The maths is otherwise
    // untouched; this is purely how the setting maps onto bit depth.
    fxEffBitcrushLevels=(crush<=0.001f)?0.f:powf(2.f,10.f-crush*8.f);
    // The "Bit-crusher does nothing" diagnostic that used to sit here
    // (v0.9902) is retired (v0.9997x, UI/UX diagnostic pass) — its own
    // comment said to remove it once the cause was known, and bitcrush
    // has since been exercised extensively (the Randomize audit, morph
    // investigations, ordinary use) with no further reports of it doing
    // nothing.
}

// FX: Soft Limiter (v0.9872). Unlike control-rate parameters (pitch,
// filter cutoff, etc.), a limiter/saturator has to respond to the
// INSTANTANEOUS sample value — it can't be computed once per buffer the
// way expensive-but-slowly-changing parameters are elsewhere in this
// file. So instead of a transcendental function (tanhf, which the
// project's own established rule keeps out of the per-sample loop),
// this uses the classic x/(1+|x|) rational soft-clip curve — a smooth,
// tanh-like saturation shape from just one fabsf() and one division,
// cheap enough to run every sample safely.
int16_t applySoftLimit(int16_t s,float drive,float mix){
    if(mix<0.001f)return s;
    float x=(s/32767.f)*drive;
    float limited=x/(1.f+fabsf(x));
    int16_t out=(int16_t)constrain(limited*32767.f,-32767.f,32767.f);
    return (int16_t)(s*(1.f-mix)+out*mix);
}

// FX: Chorus (v0.9873). Writes the dry signal into a circular delay
// buffer every sample, then reads it back from a position that swings
// around a ~10ms base delay by +-Depth/2, driven by an LFO (sineTable
// lookup, same cheap approach as Ring Mod's carrier — no fresh sinf()
// call per sample). The read position is fractional, so the two nearest
// buffer samples are linearly interpolated for a smooth result instead
// of a stair-stepped one.
int16_t applyChorus(int16_t dry){
    chorusBuffer[chorusWriteIdx]=dry;
    if(fxEffChorusMix<0.001f){
        chorusWriteIdx=(chorusWriteIdx+1)%CHORUS_BUFFER_SIZE;
        return dry;
    }
    int lfoIdx=((int)chorusLfoPhase)%WAVE_TABLE_SIZE;if(lfoIdx<0)lfoIdx+=WAVE_TABLE_SIZE;
    float lfoVal=sineTable[lfoIdx]/SINE_AMP; // -1..1
    float delayMs=10.f+fxEffChorusDepthMs*0.5f*(1.f+lfoVal); // ~10-30ms range

    // Smooth the read distance per sample (v0.9877). Anything that moves
    // this value in a step moves the read position in a step, and that is
    // a discontinuity in the output — a click. fxEffChorusDepthMs is
    // resolved once per buffer, so as soon as Chorus Depth became an
    // IMU/LFO target in v0.9876 it could jump every ~23ms; a one-pole
    // filter here removes that whole class of problem for a single
    // multiply-add per sample. The ~11ms time constant is far shorter
    // than a chorus LFO cycle (200ms at the fastest 5Hz setting), so the
    // sweep itself is untouched — only steps get rounded off.
    float delaySamplesTarget=delayMs*0.001f*SAMPLE_RATE;
    chorusDelaySmooth+=(delaySamplesTarget-chorusDelaySmooth)*0.002f;
    float delaySamples=chorusDelaySmooth;

    float readPos=(float)chorusWriteIdx-delaySamples;
    while(readPos<0.f)readPos+=CHORUS_BUFFER_SIZE;
    int idx0=(int)readPos;
    int idx1=(idx0+1)%CHORUS_BUFFER_SIZE;
    float frac=readPos-(float)idx0;
    float wet=chorusBuffer[idx0]*(1.f-frac)+chorusBuffer[idx1]*frac;

    // Hold the wet path silent until the read pointer has left the empty
    // region, then fade it in over ~28ms (v0.9878). Without this the wet
    // signal reappears as a step at exactly delaySamples after the first
    // note following a silence — audible as a click on key press, scaled
    // by Mix. It never fired during an arpeggio because the line never
    // empties between fast notes, which is precisely the difference that
    // pointed here.
    if(chorusFillCount<CHORUS_BUFFER_SIZE)chorusFillCount++;
    if((float)chorusFillCount<delaySamples)chorusWetGain=0.f;
    else chorusWetGain+=(1.f-chorusWetGain)*0.0008f;
    wet*=chorusWetGain;

    chorusLfoPhase+=(float)WAVE_TABLE_SIZE*params.chorusRateHz/SAMPLE_RATE;
    if(chorusLfoPhase>=WAVE_TABLE_SIZE)chorusLfoPhase-=WAVE_TABLE_SIZE;
    chorusWriteIdx=(chorusWriteIdx+1)%CHORUS_BUFFER_SIZE;

    return (int16_t)(dry*(1.f-fxEffChorusMix)+wet*fxEffChorusMix);
}

// FX: Delay/Echo (v0.9874). Reads back a single delayed copy at a fixed
// (integer, not fractional — delay time doesn't need to sweep smoothly
// the way Chorus's does) offset, then writes dry+feedback*delayed back
// into the buffer — repeat echoes emerge and decay naturally on their
// own from that recirculation, without needing to explicitly track or
// sum multiple echo taps.
int16_t applyDelay(int16_t dry){
    if(fxEffDelayMix<0.001f){
        delayBuffer[delayWriteIdx]=dry;
        delayWriteIdx=(delayWriteIdx+1)%DELAY_BUFFER_SIZE;
        return dry;
    }
    int delaySamples=(int)(params.delayTimeMs*0.001f*SAMPLE_RATE);
    int readIdx=delayWriteIdx-delaySamples;
    while(readIdx<0)readIdx+=DELAY_BUFFER_SIZE;
    int16_t delayed=delayBuffer[readIdx];

    float toWrite=dry+delayed*fxEffDelayFeedback;
    delayBuffer[delayWriteIdx]=(int16_t)constrain(toWrite,-32767.f,32767.f);
    delayWriteIdx=(delayWriteIdx+1)%DELAY_BUFFER_SIZE;

    return (int16_t)(dry*(1.f-fxEffDelayMix)+delayed*fxEffDelayMix);
}

// FX: Reverb (v0.9879). Freeverb topology — see the buffer declarations
// above for why these particular lengths and why float.
//
// Each comb is a delay line fed back on itself through a one-pole lowpass,
// so every pass round the loop loses a little more high end; that is what
// makes a tail sound like it is decaying into a room rather than just
// getting quieter. The four allpass stages afterwards leave the magnitude
// response flat but scramble the phase, which is what turns eight discrete
// repeating echoes into something that reads as diffuse.
//
// Cost is deliberately kept to multiply-adds and index increments: the
// per-sample budget rule in this file rules out anything transcendental
// here, and the wrap is an if-compare rather than a modulo since a modulo
// is an integer division and there would be twelve of them per sample.
// Reverb over a whole buffer at once (v0.9915).
//
// Same algorithm, same coefficients, same order — only the bookkeeping
// moved. Called per sample, this function had to reload twelve buffer
// indices and eight damping states from memory, use them once, and write
// them all back, 1024 times per buffer. None of that is the reverb; it is
// the cost of the call boundary. Hoisting the state into locals for the
// duration of the buffer leaves the arithmetic untouched and does the
// load/store once instead of 1024 times.
//
// Operates in place on the output buffer, which is why it runs after the
// per-sample loop has finished writing it rather than inside it.
void applyReverbBlock(int16_t *b,int n){
    if(fxEffReverbMix<0.001f)return;
    const float damp =fxEffReverbDamp;
    const float damp1=1.f-damp;
    const float fb   =fxEffReverbFeedback;
    const float mix  =fxEffReverbMix;
    const float dry1 =1.f-mix;

    // State into locals for the duration of the buffer.
    int   i0=reverbCombIdx[0],i1=reverbCombIdx[1],i2=reverbCombIdx[2],i3=reverbCombIdx[3];
    int   i4=reverbCombIdx[4],i5=reverbCombIdx[5],i6=reverbCombIdx[6],i7=reverbCombIdx[7];
    float c0=reverbCombStore[0],c1=reverbCombStore[1],c2=reverbCombStore[2],c3=reverbCombStore[3];
    float c4=reverbCombStore[4],c5=reverbCombStore[5],c6=reverbCombStore[6],c7=reverbCombStore[7];
    int   p0=reverbApIdx[0],p1=reverbApIdx[1],p2=reverbApIdx[2],p3=reverbApIdx[3];

    for(int k=0;k<n;k++){
        float dry=(float)b[k];
        float in=dry*0.015f;
        float out=0.f;
        #define RVB_COMB(N,LEN) { \
            float y=reverbCombBuf##N[i##N]; \
            c##N=c##N*damp+y*damp1; \
            reverbCombBuf##N[i##N]=in+c##N*fb; \
            if(++i##N>=LEN)i##N=0; \
            out+=y; }
        RVB_COMB(0,1116) RVB_COMB(1,1188) RVB_COMB(2,1277) RVB_COMB(3,1356)
        RVB_COMB(4,1422) RVB_COMB(5,1491) RVB_COMB(6,1557) RVB_COMB(7,1617)
        #undef RVB_COMB
        #define RVB_AP(N,LEN) { \
            float y=reverbApBuf##N[p##N]; \
            reverbApBuf##N[p##N]=out+y*0.5f;   /* Freeverb's fixed allpass gain */ \
            out=y-out; \
            if(++p##N>=LEN)p##N=0; }
        RVB_AP(0,556) RVB_AP(1,441) RVB_AP(2,341) RVB_AP(3,225)
        #undef RVB_AP
        float wet=constrain(out,-32767.f,32767.f);
        b[k]=(int16_t)(dry*dry1+wet*mix);
    }

    reverbCombIdx[0]=i0;reverbCombIdx[1]=i1;reverbCombIdx[2]=i2;reverbCombIdx[3]=i3;
    reverbCombIdx[4]=i4;reverbCombIdx[5]=i5;reverbCombIdx[6]=i6;reverbCombIdx[7]=i7;
    reverbCombStore[0]=c0;reverbCombStore[1]=c1;reverbCombStore[2]=c2;reverbCombStore[3]=c3;
    reverbCombStore[4]=c4;reverbCombStore[5]=c5;reverbCombStore[6]=c6;reverbCombStore[7]=c7;
    reverbApIdx[0]=p0;reverbApIdx[1]=p1;reverbApIdx[2]=p2;reverbApIdx[3]=p3;
}

// Soft limiter over a buffer (v0.9915). Moved out of the per-sample loop
// alongside the reverb so the two share one pass instead of the limiter
// needing its own; skipped entirely when the effect is off.
void applySoftLimitBlock(int16_t *b,int n,float drive,float mix){
    if(mix<0.001f)return;
    for(int k=0;k<n;k++)b[k]=applySoftLimit(b[k],drive,mix);
}

// Empties the reverb network (v0.9879). Needed in two places, for the
// same underlying reason the Delay needed clearing in v0.9875: applyReverb
// returns early when Mix is zero, so an effect that is switched off stops
// advancing its buffers and freezes mid-tail. Switching it back on would
// then read that frozen content straight out as a burst. Clearing on the
// way OFF means it always restarts from silence.
void clearReverbState(){
    for(int i=0;i<NUM_REVERB_COMBS;i++){
        memset(reverbCombBuf[i],0,sizeof(float)*REVERB_COMB_LEN[i]);
        reverbCombIdx[i]=0; reverbCombStore[i]=0.f;
    }
    for(int i=0;i<NUM_REVERB_ALLPASS;i++){
        memset(reverbApBuf[i],0,sizeof(float)*REVERB_AP_LEN[i]);
        reverbApIdx[i]=0;
    }
}

// Wipes both time-based FX delay lines back to silence (v0.9875).
// Needed because a frozen, non-empty delay line is exactly what caused
// the note-on click this version fixes: whatever is left sitting in the
// buffer gets read straight back out the moment processing resumes, as
// a hard step away from silence. Called when the FX tail has finished
// decaying, and from performPatchToneReset() so a reset really does
// start from silence rather than from the previous patch's echoes.
// Resetting the write indices to 0 alongside is safe precisely because
// the whole buffer is zero — there is no "old" position left to respect.
void clearFxBuffers(){
    memset(chorusBuffer,0,sizeof(chorusBuffer));
    memset(delayBuffer, 0,sizeof(delayBuffer));
    chorusWriteIdx=0; delayWriteIdx=0;
    chorusFillCount=0; chorusWetGain=0.f;   // v0.9878: wet path re-enters faded
    // Reverb (v0.9879): its tail is by far the longest-lived FX state, so
    // leaving it behind would mean a patch reset still rings out the old
    // room. Indices go back to 0 alongside, which is safe because the
    // buffers are zero — there is no older position left to respect.
    clearReverbState();
    // chorusLfoPhase is deliberately NOT reset here (v0.9877). It was,
    // and that was the source of the chorus crackle: the phase sets the
    // read distance, so snapping it to 0 teleports the read position by
    // up to ~900 samples in a single step, and a step in read position is
    // a step in the output waveform — a click, scaled by Mix, which is
    // exactly how it presented (worse the higher Mix went). This runs at
    // the end of every FX tail, so with an arpeggio it fired in the gap
    // between notes and clicked on every one. The LFO is a free-running
    // modulator; nothing about wiping the delay line requires restarting
    // it. (Delay then captured those clicks and echoed them back for the
    // full length of its tail, which is why the noise carried on for
    // seconds after playing stopped.)
}

// FX tail state (v0.9875). Set while a note is sounding; consumed by
// audioTask's idle branch, which keeps producing audio until the tail
// has decayed rather than cutting it off the instant the note ends.
bool fxTailPending  = false;
int  fxTailQuietBufs= 0;
int  fxTailBufCount = 0;
// Peak (absolute, int16 scale) below which a whole buffer counts as
// silent — roughly -60dBFS, well under anything audible through the
// Cardputer's speaker.
constexpr int FX_TAIL_SILENCE_PEAK = 32;
// Two consecutive quiet buffers (~46ms) before declaring the tail over,
// so a slow Chorus sweep passing through a momentary null can't end it
// early.
constexpr int FX_TAIL_QUIET_BUFS   = 2;
// Hard upper bound on tail length (~8s), with a short linear fade over
// the last few buffers so hitting the cap can never itself produce a
// click. Delay at 90% feedback and 800ms decays slowly enough to run
// close to a minute unbounded, which is longer than anyone wants the
// synth to keep making noise after they stop playing.
constexpr int FX_TAIL_MAX_BUFS     = 344;
constexpr int FX_TAIL_FADE_BUFS    = 9;

// ==========================================================
// ADSR
// ==========================================================
void advanceEnvelope(bool keyHeld){
    const float dt=(float)1024/SAMPLE_RATE;
    switch(envPhase){
        case EnvPhase::IDLE:    envLevel=0;playingFreq=0;break;
        case EnvPhase::ATTACK:
            // Only use the held frequency when no key is actively pressed —
            // otherwise a genuinely new keypress should always take priority
            // (previously heldFreq permanently overrode any new note).
            playingFreq=(noteHeld&&heldFreq>0&&currentFreq==0)?heldFreq:currentFreq;
            if(adsr.attackTime<=0){envLevel=1;envPhase=EnvPhase::DECAY;}
            else{envLevel+=dt/adsr.attackTime;if(envLevel>=1){envLevel=1;envPhase=EnvPhase::DECAY;}}
            // Captured at the moment of release, not read back from
            // sustainLevel (v0.9993) — see the note above RELEASE.
            if(!keyHeld){envReleaseStartLevel=envLevel;envPhase=EnvPhase::RELEASE;}
            break;
        case EnvPhase::DECAY:
            if(adsr.decayTime<=0){envLevel=adsr.sustainLevel;envPhase=EnvPhase::SUSTAIN;}
            else{envLevel-=dt/adsr.decayTime*(1-adsr.sustainLevel);if(envLevel<=adsr.sustainLevel){envLevel=adsr.sustainLevel;envPhase=EnvPhase::SUSTAIN;}}
            if(!keyHeld){envReleaseStartLevel=envLevel;envPhase=EnvPhase::RELEASE;}
            break;
        case EnvPhase::SUSTAIN:
            envLevel=adsr.sustainLevel;
            if(!keyHeld){envReleaseStartLevel=envLevel;envPhase=EnvPhase::RELEASE;}
            break;
        case EnvPhase::RELEASE:
            // Releasing during DECAY, before it reaches sustainLevel,
            // means envLevel at this point can be well above sustainLevel
            // — and for a percussive patch with sustainLevel=0 (decay-only,
            // no held tone), the old formula multiplied the decrement by
            // sustainLevel itself, so a release starting above 0 decremented
            // by exactly zero every sample and never reached 0 at all. The
            // note rang forever at whatever level DECAY had reached the
            // instant the key lifted (v0.9993).
            //
            // envReleaseStartLevel is the level RELEASE actually began at,
            // captured once on entry from whichever phase preceded it. From
            // SUSTAIN this equals sustainLevel exactly, so existing patches
            // that release from a held sustain are timed identically to
            // before; the fix only changes what happens when release
            // interrupts ATTACK or DECAY.
            if(adsr.releaseTime<=0){envLevel=0;envPhase=EnvPhase::IDLE;playingFreq=0;}
            else{envLevel-=dt/adsr.releaseTime*envReleaseStartLevel;if(envLevel<=0){envLevel=0;envPhase=EnvPhase::IDLE;playingFreq=0;}}
            if(keyHeld&&currentFreq>0){envPhase=EnvPhase::ATTACK;playingFreq=currentFreq;}
            break;
    }

    // Filter envelope: follows the same gate as the main envelope
    switch(filterEnvPhase){
        case EnvPhase::IDLE:    filterEnvLevel=0;break;
        case EnvPhase::ATTACK:
            if(filterEnv.attackTime<=0){filterEnvLevel=1;filterEnvPhase=EnvPhase::DECAY;}
            else{filterEnvLevel+=dt/filterEnv.attackTime;if(filterEnvLevel>=1){filterEnvLevel=1;filterEnvPhase=EnvPhase::DECAY;}}
            if(!keyHeld){filterEnvReleaseStartLevel=filterEnvLevel;filterEnvPhase=EnvPhase::RELEASE;}
            break;
        case EnvPhase::DECAY:
            if(filterEnv.decayTime<=0){filterEnvLevel=filterEnv.sustainLvl;filterEnvPhase=EnvPhase::SUSTAIN;}
            else{filterEnvLevel-=dt/filterEnv.decayTime*(1-filterEnv.sustainLvl);if(filterEnvLevel<=filterEnv.sustainLvl){filterEnvLevel=filterEnv.sustainLvl;filterEnvPhase=EnvPhase::SUSTAIN;}}
            if(!keyHeld){filterEnvReleaseStartLevel=filterEnvLevel;filterEnvPhase=EnvPhase::RELEASE;}
            break;
        case EnvPhase::SUSTAIN:
            filterEnvLevel=filterEnv.sustainLvl;
            if(!keyHeld){filterEnvReleaseStartLevel=filterEnvLevel;filterEnvPhase=EnvPhase::RELEASE;}
            break;
        case EnvPhase::RELEASE:
            // Same fix as the amplitude envelope above, and the same bug:
            // multiplying by sustainLvl meant a filter-envelope release
            // interrupting DECAY on a sustainLvl=0 patch never closed the
            // filter back down, leaving it stuck open (v0.9993).
            if(filterEnv.releaseTime<=0){filterEnvLevel=0;filterEnvPhase=EnvPhase::IDLE;}
            else{filterEnvLevel-=dt/filterEnv.releaseTime*filterEnvReleaseStartLevel;if(filterEnvLevel<=0){filterEnvLevel=0;filterEnvPhase=EnvPhase::IDLE;}}
            if(keyHeld&&currentFreq>0)filterEnvPhase=EnvPhase::ATTACK;
            break;
    }
}

// ==========================================================
// Audio task (Core 1)
// ==========================================================
// Instantaneous morph/PWM actually used for audio this sample (includes IMU
// offsets and General LFO modulation) — read by MAIN screen's waveform
// preview so what's displayed matches what's actually playing.
float lastModMorph=0.f, lastModShape=0.5f;

// Diagnostic buffer-timing counters — audioTask (Core 0) only updates these
// (fast, no I/O); loop() (Core 1) does the actual Serial.printf() once a
// second, so USB/Serial I/O timing can never affect audio-critical Core 0.
unsigned long diagWindowStartMs=0, diagCount=0, diagOverCount=0, diagSumUs=0, diagMaxUs=0;
volatile bool diagPrintPending=false;
unsigned long diagPrintCount=0, diagPrintSumUs=0, diagPrintMaxUs=0, diagPrintOverCount=0;

// The mirror direction (v0.9997x): set every loop() pass, checked from
// audioTask in diagRecordBuffer() below. Added after a report that no
// earlier diagnostic covers — total input lockup (ARP wouldn't stop,
// Latch wouldn't release, no key did anything) while audioTask kept
// logging normally throughout and after, meaning audioTask itself was
// never the one stuck. loop() has no way to report its own hang from
// inside it — if it is truly stuck, nothing there can run, including a
// print statement — so the only place that can notice is the one side
// genuinely guaranteed to keep running independently: audioTask, on the
// other core, exactly the same reasoning as the original heartbeat, just
// checking in the opposite direction. Declared here, ahead of
// diagRecordBuffer(), rather than next to audioTaskHeartbeatUs further
// down — that placement was tried first and built after
// diagRecordBuffer() already needed it.
volatile unsigned long loopHeartbeatMs=0;

// Also moved here (v0.9997x, same fix) — morphStart() resets this on
// every call now, including redirects, and morphStart() is defined
// before morphDiagLog[]/morphDiagCount's original spot near morphTick().
// The array itself (morphDiagLog[]) stays where it was; only the count
// needed to move, since morphStart() never touches the array directly.
int morphDiagCount=0;

// Accumulates one buffer's timing (v0.9875: factored out of audioTask so
// the normal path and the FX-tail path both report through it).
// dspUs must cover DSP work ONLY, stopping before playRaw(). Measuring
// past playRaw() made this figure useless: playRaw() blocks once the
// speaker queue is full, so the reported value stopped tracking CPU cost
// and simply converged on the buffer period (~23220us), with the "over
// budget" counter then firing on buffers that were never actually late.
// Checked directly here, not deferred through diagPrintPending the way
// [audioTask]'s own timing summary is (v0.9997x) — if loop() is truly
// hung, it would never get around to consuming a deferred flag, so this
// prints immediately from audioTask's own side instead, the one place
// still guaranteed to be running independently.
unsigned long lastLoopHeartbeatCheckMs=0;
inline void diagRecordBuffer(unsigned long dspUs){
    diagCount++;
    diagSumUs+=dspUs;
    if(dspUs>diagMaxUs)diagMaxUs=dspUs;
    if(dspUs>23220)diagOverCount++;
    unsigned long nowMs=millis();
    if(diagWindowStartMs==0)diagWindowStartMs=nowMs;
    if(nowMs-diagWindowStartMs>=1000&&!diagPrintPending){
        diagPrintCount=diagCount;diagPrintSumUs=diagSumUs;
        diagPrintMaxUs=diagMaxUs;diagPrintOverCount=diagOverCount;
        diagPrintPending=true;
        diagWindowStartMs=nowMs;diagCount=0;diagOverCount=0;diagSumUs=0;diagMaxUs=0;
    }
    if(nowMs-lastLoopHeartbeatCheckMs>=1000){
        lastLoopHeartbeatCheckMs=nowMs;
        unsigned long loopSilentMs=nowMs-loopHeartbeatMs;
        // Generous margin (2s): loop() is not real-time critical the way
        // audioTask is, and can legitimately take a bit longer during a
        // full-screen redraw or SD access — this is for a genuine hang,
        // not routine variance.
        if(loopSilentMs>2000){
            Serial.printf("[loop] no heartbeat for %lums - main loop may be stuck\n",
                loopSilentMs);
        }
    }
}

// Set every buffer by audioTask, read only by loop() (v0.99944) — plain
// volatile is enough for a single word that only needs "is this moving",
// not multi-field consistency the way the morph-chain state did.
volatile unsigned long audioTaskHeartbeatUs=0;

void audioTask(void *pvParameters){
    const int BUF=1024;
    static int16_t bufs[5][1024];
    int bi=0;
    constexpr int CH=0;
    constexpr float SM=0.0008f;

    while(true){
        unsigned long bufStartUs=micros();
        // Proof of life, checked from loop() (v0.99944). Added to actually
        // find out what a "silent until reset" morph incident is —
        // whether audioTask has genuinely stopped running (a deadlock,
        // most likely on morphChainMux given v0.99943's timing) versus
        // still running but producing zero/near-zero samples for some
        // other reason (a stuck envelope, a bad parameter). Those need
        // completely different fixes, and guessing which one without
        // this would be a sixth guess in a row.
        audioTaskHeartbeatUs=bufStartUs;
        int16_t *buf=bufs[bi];bi=(bi+1)%5;
        bool keyHeld=(currentFreq>0)||noteHeld;
        advanceEnvelope(keyHeld);

        if(envPhase==EnvPhase::IDLE){
            // params.timbreMorph itself was frozen while idle (not just
            // the two below) — its smoothing toward timbreMorphTarget
            // only happens in the per-sample loop this branch skips, so
            // IMU tilts updating the target (via updateImu(), which runs
            // regardless of note state) never actually moved the real
            // value while nothing was playing — meaning the chain's last
            // waveform could be aimed at by tilt but never actually
            // reached/heard/shown until a note resumed the smoothing.
            // Snap directly (no audible smoothing needed with no note
            // sounding) rather than leaving it to catch up gradually.
            //
            // Gated on !morphActive now (v0.99945): a patch morph also
            // drives this same pair on its own independent timeline, via
            // morphApply()'s mlerp — which runs regardless of note state
            // deliberately, so the sweep is visible/audible with nothing
            // held. Snapping here unconditionally fought that every
            // buffer while idle, so the display sat at the FINAL target
            // throughout an idle morph and only revealed the true
            // in-progress value the instant a key left IDLE and this
            // branch stopped running. The IMU case this was written for
            // is unaffected — it only ever mattered while NOT mid-morph.
            if(!morphActive)params.timbreMorph=params.timbreMorphTarget;
            lastModMorph=params.timbreMorph;
            // Shape's IMU offset is smoothed in the buffer-rate block, which
            // this branch skips — so with no note sounding it froze, and the
            // IMU screen's Shape readout and level bar sat still while
            // Timbre's moved. Snap it here for the same reason timbreMorph
            // is snapped just above: no note is sounding, so there is
            // nothing to smooth for. v0.9841 fixed exactly this for
            // timbreMorph and missed the neighbouring parameter (v0.9902).
            params.oscShapeOffset=params.oscShapeOffsetTarget;
            lastModShape=params.oscShape+params.oscShapeOffset;
            // Rebuild here too (v0.9891) — the buffer-rate block above is
            // skipped while idle, and without this the VCO waveform preview
            // would freeze at the last duty until a note was played. Same
            // idle-branch trap that caught the preview in v0.9831 and
            // timbreMorph in v0.9841.
            updateSquareDuty(constrain(params.oscShape+params.oscShapeOffset,0.f,1.f));
            // The Morph chain is edited from its own screen, which means
            // with no note sounding — so this branch has to refresh too,
            // or the VCO preview would keep drawing the old chain.
            refreshMorphTablePtrs();
            phase=0;subPhase=0;

            // FX tail (v0.9875). This branch skipping the per-sample loop
            // is exactly what broke the time-based effects: applyChorus()
            // and applyDelay() simply stopped being called, so the delay
            // line froze mid-stride with the last note still in it. Two
            // symptoms, one cause — the echoes were cut off dead the
            // instant the note ended, and the NEXT note-on read that stale
            // audio straight back out at full level, a hard step away from
            // silence heard as a click. So instead of going straight to
            // sleep, keep producing real buffers, feeding silence in, until
            // the tail has actually decayed. Cheap: no oscillator, filter,
            // morph or envelope work happens here, just the FX chain.
            if(fxTailPending){
                if(fxEffChorusMix>0.001f||fxEffDelayMix>0.001f||fxEffReverbMix>0.001f){
                    // Linear fade over the final buffers so hitting the
                    // duration cap can't itself click.
                    float fade=1.0f;
                    int remaining=FX_TAIL_MAX_BUFS-fxTailBufCount;
                    if(remaining<FX_TAIL_FADE_BUFS)
                        fade=(remaining>0)?((float)remaining/FX_TAIL_FADE_BUFS):0.f;

                    int32_t peak=0;
                    for(int i=0;i<BUF;i++){
                        int16_t s=applyChorus(0);
                        s=applyDelay(s);
                        buf[i]=s;
                    }
                    applyReverbBlock(buf,BUF);
                    applySoftLimitBlock(buf,BUF,fxEffLimiterDrive,params.limiterMix);
                    // Fade and peak detection have to come after the block
                    // passes now, since the reverb tail is most of what is
                    // left here and the peak test decides when to stop.
                    for(int i=0;i<BUF;i++){
                        int16_t s=buf[i];
                        if(fade<1.0f){s=(int16_t)(s*fade);buf[i]=s;}
                        int32_t a=(s<0)?-(int32_t)s:(int32_t)s;
                        if(a>peak)peak=a;
                    }
                    diagRecordBuffer(micros()-bufStartUs);
                    M5Cardputer.Speaker.playRaw(buf,BUF,SAMPLE_RATE,false,1,CH,false);

                    fxTailBufCount++;
                    // Silence alone is not enough to end the tail while
                    // Delay is on (v0.9964). Right after a note stops, the
                    // delay line's READ position is still inside the
                    // silent stretch before the first echo — so the output
                    // is genuinely quiet, the two-quiet-buffer test fired
                    // after ~46ms, and clearFxBuffers() wiped the line
                    // before the echo ever arrived. With Delay alone that
                    // meant no audible delay at all; adding Reverb hid it,
                    // because the reverb tail kept the level above the
                    // threshold long enough for the echo to come round.
                    // Which is exactly how the fault presented.
                    //
                    // So require enough quiet buffers to cover a full
                    // delay time before believing the tail is over.
                    int quietNeeded=FX_TAIL_QUIET_BUFS;
                    if(fxEffDelayMix>0.001f){
                        int delayBufs=(int)(params.delayTimeMs/1000.f*SAMPLE_RATE/BUF)+2;
                        if(delayBufs>quietNeeded)quietNeeded=delayBufs;
                    }
                    if(peak<FX_TAIL_SILENCE_PEAK)fxTailQuietBufs++;
                    else                         fxTailQuietBufs=0;
                    if(fxTailQuietBufs>=quietNeeded||fxTailBufCount>=FX_TAIL_MAX_BUFS){
                        fxTailPending=false;fxTailQuietBufs=0;fxTailBufCount=0;
                        clearFxBuffers();
                    }
                    // No 5ms sleep here: this path is producing real-time
                    // audio exactly like the normal path, and relies on
                    // playRaw() blocking on a full speaker queue to pace
                    // itself the same way. It does still need the same
                    // occasional yield the normal path takes at the bottom
                    // of the loop, since a long tail can run for seconds
                    // and this path never reaches that code.
                    static uint8_t tailYieldCounter=0;
                    if(++tailYieldCounter>=8){tailYieldCounter=0;vTaskDelay(1);}
                    continue;
                }
                // Neither time-based effect is on, so there is no tail to
                // ring out — but the buffers still hold the last note, and
                // switching Delay on later would read it back out as that
                // same click. Clear now, while nothing is sounding.
                fxTailPending=false;fxTailQuietBufs=0;fxTailBufCount=0;
                clearFxBuffers();
            }

            // Genuinely asleep: nothing is feeding the chorus line, so it
            // is empty by definition. Marking it so means the wet path
            // fades in on the next note however we arrived here, including
            // the case where Chorus was switched on while idle and no FX
            // tail ever ran to call clearFxBuffers().
            chorusFillCount=0; chorusWetGain=0.f;
            vTaskDelay(5/portTICK_PERIOD_MS);
            continue;
        }
        // A note is sounding, so a tail may need to ring out once it ends.
        fxTailPending=true;fxTailQuietBufs=0;fxTailBufCount=0;

        // Portamento: smoothly move portaFreq toward target
        float targetFreq=(noteHeld&&heldFreq>0&&currentFreq==0)?heldFreq:currentFreq;
        if(portaEnabled&&portaFreq>0&&targetFreq>0){
            portaFreq+=(targetFreq-portaFreq)*portaSpeed;
            if(fabsf(portaFreq-targetFreq)<0.1f)portaFreq=targetFreq;
        } else {
            portaFreq=targetFreq;
        }

        // Sequencer Slide: a separate glide, independent of the Portamento
        // toggle above, so per-step Slide works regardless of it.
        if(seqSliding&&currentFreq>0){
            seqSlideFreq+=(currentFreq-seqSlideFreq)*SEQ_SLIDE_SPEED;
            if(fabsf(seqSlideFreq-currentFreq)<0.1f){seqSlideFreq=currentFreq;seqSliding=false;}
        }

        // v0.8's 7 new IMU-controlled offsets only need to be this smooth,
        // not audio-rate: IMU tilt changes slowly compared to a ~23ms buffer,
        // so smoothing these once per buffer (instead of once per sample,
        // 1024x fewer times) removes a meaningful chunk of per-sample CPU
        // cost with no audible difference — this was found to be a likely
        // contributor to periodic buffer-underrun stutter under heavy load.
        constexpr float SM_BUF=0.5f;
        params.oscShapeOffset       +=(params.oscShapeOffsetTarget      -params.oscShapeOffset)      *SM_BUF;
        params.detuneOffset    +=(params.detuneOffsetTarget   -params.detuneOffset)   *SM_BUF;
        params.noiseOffset     +=(params.noiseOffsetTarget    -params.noiseOffset)    *SM_BUF;
        params.subLevelOffset  +=(params.subLevelOffsetTarget -params.subLevelOffset) *SM_BUF;
        params.resonanceOffset +=(params.resonanceOffsetTarget-params.resonanceOffset)*SM_BUF;
        lfoRateOffset          +=(lfoRateOffsetTarget         -lfoRateOffset)         *SM_BUF;
        lfoDepthOffset         +=(lfoDepthOffsetTarget        -lfoDepthOffset)        *SM_BUF;
        arpTempoOffset         +=(arpTempoOffsetTarget         -arpTempoOffset)        *SM_BUF;
        arpSwingOffset       +=(arpSwingOffsetTarget       -arpSwingOffset)      *SM_BUF;
        seqTempoOffset         +=(seqTempoOffsetTarget         -seqTempoOffset)        *SM_BUF;
        seqSwingOffset       +=(seqSwingOffsetTarget       -seqSwingOffset)      *SM_BUF;
        seqAccentCutoffBoost +=(seqAccentCutoffBoostTarget -seqAccentCutoffBoost)*SM_BUF;
        seqAccentResoBoost   +=(seqAccentResoBoostTarget   -seqAccentResoBoost)  *SM_BUF;
        // FX offsets (v0.9876) — smoothed at buffer rate for the same
        // reason as everything above: tilt moves far more slowly than a
        // ~23ms buffer, so per-sample smoothing would be wasted work.
        params.ringModRateOffset  +=(params.ringModRateOffsetTarget  -params.ringModRateOffset)  *SM_BUF;
        params.ringModMixOffset   +=(params.ringModMixOffsetTarget   -params.ringModMixOffset)   *SM_BUF;
        params.limiterDriveOffset +=(params.limiterDriveOffsetTarget -params.limiterDriveOffset) *SM_BUF;
        params.chorusDepthOffset  +=(params.chorusDepthOffsetTarget  -params.chorusDepthOffset)  *SM_BUF;
        params.chorusMixOffset    +=(params.chorusMixOffsetTarget    -params.chorusMixOffset)    *SM_BUF;
        params.delayFeedbackOffset+=(params.delayFeedbackOffsetTarget-params.delayFeedbackOffset)*SM_BUF;
        params.delayMixOffset     +=(params.delayMixOffsetTarget     -params.delayMixOffset)     *SM_BUF;
        params.reverbRoomOffset   +=(params.reverbRoomOffsetTarget   -params.reverbRoomOffset)   *SM_BUF;
        params.reverbMixOffset    +=(params.reverbMixOffsetTarget    -params.reverbMixOffset)    *SM_BUF;
        params.osc2LevelOffset    +=(params.osc2LevelOffsetTarget    -params.osc2LevelOffset)    *SM_BUF;
        params.osc2ShapeOffset    +=(params.osc2ShapeOffsetTarget    -params.osc2ShapeOffset)    *SM_BUF;
        // v0.989: these three moved here from the per-sample loop when they
        // gained menu controls. They are IMU offsets like every other entry
        // in this block now, and tilt moves far slower than one buffer.
        params.vibratoDepthOffset +=(params.vibratoDepthOffsetTarget -params.vibratoDepthOffset) *SM_BUF;
        params.vibratoRateOffset  +=(params.vibratoRateOffsetTarget  -params.vibratoRateOffset)  *SM_BUF;
        params.tremoloDepthOffset +=(params.tremoloDepthOffsetTarget -params.tremoloDepthOffset) *SM_BUF;
        params.bitcrushOffset     +=(params.bitcrushOffsetTarget     -params.bitcrushOffset)     *SM_BUF;

        // Hoisted out of the per-sample loop: these only depend on values that
        // are now buffer-constant (the offsets above, or menu-set values that
        // never change mid-buffer), so recomputing them 1024x/buffer was pure
        // waste. The sub-oscillator's powf() call in particular was expensive
        // (transcendental function) and was being called on every single
        // sample whenever the sub-oscillator was active — this was a major
        // contributor to audioTask missing its real-time budget every buffer.
        float effSubLevel  =constrain(params.subOscLevel+params.subLevelOffset,0.f,1.f);
        float effNoiseLevel=constrain(params.noiseLevel+params.noiseOffset,0.f,1.f);
        float shapeBase    =constrain(params.oscShape+params.oscShapeOffset,0.f,1.f);
        // Square's pulse width is baked into its table, so it resolves once
        // per buffer rather than per sample. The LFO is sampled here the
        // same way the FX block below does it — ample for the sub-5Hz rates
        // PWM is actually used at, and a table cannot be rebuilt per sample
        // in any case.
        {
            float shapeForDuty=shapeBase;
            if(lfo.target==LfoTarget::SHAPE){
                int li=((int)lfoPhase)%WAVE_TABLE_SIZE;if(li<0)li+=WAVE_TABLE_SIZE;
                shapeForDuty=constrain(shapeBase+lfoTableSample(lfo.wave,li)
                                       *constrain(lfo.depth+lfoDepthOffset,0.f,1.f)*LFO_SHAPE_MAX,0.f,1.f);
            }
            updateSquareDuty(shapeForDuty);
        }
        // Morph table pointers, same buffer-rate treatment (v0.991).
        refreshMorphTablePtrs();
        float subOctaveRatio=powf(2.f,(float)params.subOscOctave);
        // Oscillator 2, resolved once per buffer (v0.9911). The pitch ratio
        // needs a powf(), which must never run per sample — same reason
        // subOctaveRatio is computed here.
        float effOsc2Level=constrain(params.osc2Level+params.osc2LevelOffset,0.f,1.f);
        // Same buffer-rate pointer resolution the Morph chain gets.
        int16_t *osc2TblA=oscWaveformTable(params.osc2Waveform);
        int16_t *osc2TblB=oscWaveformTableB(params.osc2Waveform);
        // Square's Shape lives in the table, not in the A/B blend, so
        // oscillator 2 rebuilds its own copy from its own Shape and then
        // points both sides at it — making the blend below a no-op, which
        // is what it already was for Square (v0.9935).
        bool osc2IsSquare=(params.osc2Waveform==OscWaveform::SQUARE);
        if(osc2IsSquare){
            updateSquareDuty2(constrain(params.osc2Shape+params.osc2ShapeOffset,0.f,1.f));
            osc2TblA=osc2TblB=squareTable2;
        }
        float osc2Ratio=powf(2.f,(float)params.osc2OctaveShift
                                 +(float)params.osc2Semitones/12.f
                                 +(params.osc2DetuneCents+params.osc2FineCents)/1200.f);
        float effLfoRateHz =constrain(lfo.rateHz+lfoRateOffset,LFO_RATE_MIN,LFO_RATE_MAX);
        float effLfoDepth  =constrain(lfo.depth+lfoDepthOffset,0.f,1.f);

        // FX modulation resolved once per buffer (v0.9876), sampling the
        // general LFO the same way the filter-cutoff block just below
        // does — these are control-rate parameters, so there is nothing
        // to gain from re-deriving them 1024x per buffer.
        {
            float lfoFx=0.f;
            if(lfo.target>=LfoTarget::FX_RING_RATE&&lfo.target<LfoTarget::TARGET_COUNT){
                int li=((int)lfoPhase)%WAVE_TABLE_SIZE;if(li<0)li+=WAVE_TABLE_SIZE;
                lfoFx=lfoTableSample(lfo.wave,li)*effLfoDepth;
            }
            updateFxEffective(lfoFx);
        }
        // Analog drift resolved once per buffer alongside everything else
        // (v0.9941); driftCents is read inside the per-sample loop below.
        updateAnalogDrift();
        float driftCents=driftPitchCents();

        // Dynamic filter cutoff: base + key tracking + filter env + IMU offset + LFO
        {
            float playF=(seqSliding&&seqSlideFreq>0)?seqSlideFreq:((portaEnabled&&portaFreq>0)?portaFreq:playingFreq);
            // Key tracking: higher notes raise cutoff
            float trackHz = (playF>0)
                ? filterParams.keyTracking * (12.0f*log2f(playF/261.63f)) * 100.0f
                : 0.0f;
            // Filter envelope modulation
            float envHz = filterEnvLevel * filterEnv.depth;
            // IMU offset (multiplicative, lowers cutoff)
            // Drift multiplies the resolved cutoff rather than joining the
            // offset chain, so it scales with wherever the filter is set
            // instead of being a fixed number of Hz (v0.9941).
            float imuScale = (params.filterCutoffOffset>0.0001f)
                ? (1.0f - params.filterCutoffOffset*0.9f)
                : 1.0f;
            // General LFO -> filter cutoff (sampled once per buffer; the
            // biquad coefficients are only recalculated at this rate anyway)
            float lfoHz = 0.0f;
            if(lfo.target==LfoTarget::FILTER){
                int li=((int)lfoPhase)%WAVE_TABLE_SIZE;if(li<0)li+=WAVE_TABLE_SIZE;
                lfoHz = lfoTableSample(lfo.wave,li)*effLfoDepth*LFO_FILTER_MAX_HZ;
            }
            float dynCutoff = constrain(
                (filterParams.cutoffHz + trackHz + envHz + lfoHz + seqAccentCutoffBoost)
                    * imuScale * driftCutoffMult(),
                FILTER_CUTOFF_MIN, FILTER_CUTOFF_MAX);
            float dynQ = constrain(filterParams.resonanceQ + params.resonanceOffset + seqAccentResoBoost, FILTER_Q_MIN, FILTER_Q_MAX);
            // Passed directly now (v0.99946) — filterParams.cutoffHz/
            // resonanceQ are never touched here at all any more. See the
            // note above updateFilterCoefficients() for what this
            // replaced and why.
            updateFilterCoefficients(dynCutoff,dynQ);
        }

        for(int i=0;i<BUF;i++){
            // Exponential smoothing asymptotically approaches its target
            // but mathematically never exactly reaches it — invisible for
            // most of the parameters below, but timbreMorph is used for
            // discrete waveform-index lookup (both the audio blend's
            // (int) truncation and the VCO/IMU display labels), so
            // without an explicit snap it can spend a long time — or,
            // once float precision is exhausted, forever — just short of
            // the true target, meaning the chain's LAST waveform could
            // never actually be selected/shown even at full IMU deflection.
            if(fabsf(params.timbreMorphTarget-params.timbreMorph)<0.01f)params.timbreMorph=params.timbreMorphTarget;
            else params.timbreMorph+=(params.timbreMorphTarget-params.timbreMorph)*SM;
            params.volumeScale        +=(params.volumeScaleTarget       -params.volumeScale)       *SM;
            params.pitchBendCents     +=(params.pitchBendCentsTarget    -params.pitchBendCents)    *SM;
            params.filterCutoffOffset +=(params.filterCutoffOffsetTarget-params.filterCutoffOffset)*SM;

            // Vibrato LFO
            int vi=((int)vibratoPhase)%WAVE_TABLE_SIZE;if(vi<0)vi+=WAVE_TABLE_SIZE;
            float vlfo=sineTable[vi]/32000.f;
            vibratoPhase+=(float)WAVE_TABLE_SIZE*fxEffVibratoRateHz/SAMPLE_RATE;
            if(vibratoPhase>=WAVE_TABLE_SIZE)vibratoPhase-=WAVE_TABLE_SIZE;

            // Tremolo LFO
            int ti=((int)tremoloPhase)%WAVE_TABLE_SIZE;if(ti<0)ti+=WAVE_TABLE_SIZE;
            float tlfo=(sineTable[ti]/32000.f+1.f)*0.5f;
            tremoloPhase+=(float)WAVE_TABLE_SIZE*5.f/SAMPLE_RATE;
            if(tremoloPhase>=WAVE_TABLE_SIZE)tremoloPhase-=WAVE_TABLE_SIZE;

            // General-purpose LFO (independent from vibrato/tremolo above).
            // Always runs so the LFO tab's live phase marker stays accurate,
            // but only affects audio when routed to a target below.
            // IMU can nudge the LFO's own Rate/Depth without touching the
            // stored LFO menu values (same offset pattern as everything else).
            // effLfoRateHz/effLfoDepth are computed once per buffer above.
            int li=((int)lfoPhase)%WAVE_TABLE_SIZE;if(li<0)li+=WAVE_TABLE_SIZE;
            float lfoRaw=lfoTableSample(lfo.wave,li);
            lfoPhase+=(float)WAVE_TABLE_SIZE*effLfoRateHz/SAMPLE_RATE;
            if(lfoPhase>=WAVE_TABLE_SIZE){
                lfoPhase-=WAVE_TABLE_SIZE;
                // New Sample & Hold cycle starting — draw a fresh value.
                // Done here (the one place with true per-sample phase
                // tracking) rather than inside lfoTableSample(), so the
                // once-per-buffer filter-cutoff read and the LFO screen's
                // waveform preview can sample the LFO freely without ever
                // disturbing this sequence.
                if(lfo.wave==LfoWave::SAMPLE_HOLD)
                    lfoSampleHoldValue=((float)random(0,2001)-1000.f)/1000.f;
            }
            float lfoVal=lfoRaw*effLfoDepth;

            // Key bend smoothing
            float bd=keyBendGoal-keyBendCurrent;
            float bs=(fabsf(keyBendGoal)<fabsf(keyBendCurrent)||keyBendGoal==0)?keyBendReleaseSmooth:keyBendAttackSmooth;
            keyBendCurrent+=bd*bs;

            // Total pitch
            // v0.989: the "only if an IMU axis points at it" gate is gone.
            // It existed because the IMU was the only thing that could set
            // Vibrato at all, so a leftover depth with no axis assigned
            // would have been unreachable dead modulation. Now that Depth
            // is a VCO menu control the gate would do the opposite of what
            // a user expects — set Depth, hear nothing, no way to tell why.
            float effVibratoDepth=fxEffVibratoDepth;
            float lfoPitchCents=(lfo.target==LfoTarget::PITCH)?lfoVal*LFO_PITCH_MAX_CENTS:0.f;
            float totalCents=vlfo*effVibratoDepth*VIBRATO_MAX_CENTS
                            +params.pitchBendCents+keyBendCurrent
                            +params.detuneCents+params.fineTuneCents+params.detuneOffset
                            +lfoPitchCents
                            +driftCents;   // v0.9941, 0 unless Analog Drift is on
            // Only recompute the expensive powf() every few samples;
            // interpolate linearly in between (see pitchRatioCur/Step
            // declaration above for why this is inaudible).
            if(i%PITCH_RATIO_UPDATE_INTERVAL==0){
                float pitchRatioTarget=powf(2.f,totalCents/1200.f);
                pitchRatioStep=(pitchRatioTarget-pitchRatioCur)/PITCH_RATIO_UPDATE_INTERVAL;
            }
            float pr=pitchRatioCur;
            pitchRatioCur+=pitchRatioStep;
            float playF=(seqSliding&&seqSlideFreq>0)?seqSlideFreq:((portaEnabled&&portaFreq>0)?portaFreq:playingFreq);
            float phInc=(float)WAVE_TABLE_SIZE*(playF*pr)/SAMPLE_RATE;
            // Published for MIDI bend out (v0.99876). currentFreq is only
            // the TARGET note; everything that makes the pitch expressive
            // — portamento's glide, key bend, vibrato, detune, Analog
            // Drift — is applied here and nowhere else, which is why the
            // bend calculation had nothing to work from and sent nothing.
            // Sampled once per buffer: the MIDI side is throttled to 15ms
            // anyway, so per-sample accuracy would be thrown away.
            if(i==0)midiSoundingHz=playF*pr;
            // Osc 2 rides osc 1's pitch (portamento, bend, vibrato all
            // included via pr) and applies its own offset on top.
            float phInc2=phInc*osc2Ratio;

            int idx=((int)phase)%WAVE_TABLE_SIZE;if(idx<0)idx+=WAVE_TABLE_SIZE;

            // General LFO -> Timbre / Shape (applied locally, doesn't
            // touch the stored params so the VCO menu values stay untouched)
            float modMorph=params.timbreMorph;
            if(lfo.target==LfoTarget::TIMBRE)
                modMorph=constrain(modMorph+lfoVal*LFO_TIMBRE_MAX,0.f,(float)max(1,morphChainLen-1));
            float modShape=shapeBase;
            if(lfo.target==LfoTarget::SHAPE)
                modShape=constrain(modShape+lfoVal*LFO_SHAPE_MAX,0.f,1.f);
            lastModMorph=modMorph;lastModShape=modShape;

            // Oscillator 1
            int16_t sample=getMorphedSample(idx,modMorph,modShape);

            // Oscillator 2 (v0.9911). Skipped entirely at level 0, so a
            // patch that does not use it costs nothing — same convention
            // as the FX mixes. LFO->Timbre/Shape modulates both
            // oscillators: it is one modulation source, and having it
            // reach only osc 1 would be surprising.
            if(effOsc2Level>0.001f){
                int idx2=((int)phase2)%WAVE_TABLE_SIZE;if(idx2<0)idx2+=WAVE_TABLE_SIZE;
                // LFO->Shape reaches oscillator 2, LFO->Timbre does not
                // (v0.9914). Its waveform is fixed by choice so there is no
                // Timbre to sweep, but Shape still gives the layer
                // movement, which is the part worth keeping.
                float s2=constrain(params.osc2Shape+params.osc2ShapeOffset,0.f,1.f);
                if(lfo.target==LfoTarget::SHAPE)
                    s2=constrain(s2+lfoVal*LFO_SHAPE_MAX,0.f,1.f);
                int16_t sample2=(int16_t)(osc2TblA[idx2]*(1.f-s2)+osc2TblB[idx2]*s2);
                sample=(int16_t)(sample*(1.f-effOsc2Level)+sample2*effOsc2Level);
                phase2+=phInc2;
                if(phase2>=WAVE_TABLE_SIZE)phase2-=WAVE_TABLE_SIZE;
            }

            // Sub oscillator (sine wave, 1 or 2 octaves below).
            // Uses its own independent phase accumulator (subPhase) rather
            // than being derived from the main oscillator's idx — deriving
            // it from idx caused a discontinuous phase reset every time the
            // main oscillator wrapped (i.e. every main cycle), which got
            // audibly worse the lower the octave (more main-cycles pass per
            // sub-cycle), producing periodic clicks/warble that got more
            // prominent the higher the sub level.
            // effSubLevel/subOctaveRatio are computed once per buffer above —
            // the powf() call in particular is too expensive to repeat every
            // sample (44100x/sec).
            if(effSubLevel>0.001f && playF>0){
                float subPhInc=phInc*subOctaveRatio;
                int subIdx=((int)subPhase)%WAVE_TABLE_SIZE;if(subIdx<0)subIdx+=WAVE_TABLE_SIZE;
                int16_t sub=sineTable[subIdx];
                sample=(int16_t)(sample*(1.f-effSubLevel)+sub*effSubLevel);
                subPhase+=subPhInc;
                if(subPhase>=WAVE_TABLE_SIZE)subPhase-=WAVE_TABLE_SIZE;
            }

            // Noise blend (effNoiseLevel computed once per buffer above)
            if(effNoiseLevel>0.001f){
                int16_t noise=nextNoise();
                sample=(int16_t)(sample*(1.f-effNoiseLevel)+noise*effNoiseLevel);
            }

            // Ring Modulator (v0.987): multiplies the signal by an
            // audio-rate carrier (its own independent phase accumulator,
            // reusing sineTable like the sub-oscillator does — a table
            // lookup, not a fresh sinf() call, so this stays cheap).
            // Mix=0 means fully off; skip the phase advance too so an
            // unused Ring Mod doesn't quietly drift out of sync while off.
            if(fxEffRingMix>0.001f){
                float ringPhInc=(float)WAVE_TABLE_SIZE*fxEffRingRateHz/SAMPLE_RATE;
                int ringIdx=((int)ringModPhase)%WAVE_TABLE_SIZE;if(ringIdx<0)ringIdx+=WAVE_TABLE_SIZE;
                float carrier=sineTable[ringIdx]/SINE_AMP;
                int16_t modulated=(int16_t)(sample*carrier);
                sample=(int16_t)(sample*(1.f-fxEffRingMix)+modulated*fxEffRingMix);
                ringModPhase+=ringPhInc;
                if(ringModPhase>=WAVE_TABLE_SIZE)ringModPhase-=WAVE_TABLE_SIZE;
            }

            phase+=phInc;
            if(phase>=WAVE_TABLE_SIZE)phase-=WAVE_TABLE_SIZE;

            sample=applyBitcrush(sample,fxEffBitcrushLevels);
            sample=applyFilter(sample);

            // driftLevelMult() is 1.0 unless Analog Drift is on (v0.9941).
            float vol=constrain(params.keyVolume*params.volumeScale*seqVelocityMult*driftLevelMult(),0.f,1.f);
            float effTremoloDepth=fxEffTremoloDepth;   // v0.989: see the Vibrato note above
            float tg=constrain(1-effTremoloDepth+effTremoloDepth*tlfo*2,0.f,2.f);
            float lfoVolMult=(lfo.target==LfoTarget::VOLUME)?constrain(1.0f+lfoVal,0.f,2.f):1.0f;
            int16_t finalSample=(int16_t)(sample*vol*tg*envLevel*lfoVolMult);

            // Time-based FX sit AFTER the VCA (v0.9875), giving the
            // conventional VCO -> VCF -> VCA -> FX order. They used to run
            // before the envelope multiply, which meant every echo was
            // gated a second time by the envelope that produced it — so a
            // Delay could never actually outlast its own note. Ring Mod and
            // Bit-crusher stay upstream on purpose: those shape the tone
            // itself and belong inside the voice, not after it.
            finalSample=applyChorus(finalSample);
            finalSample=applyDelay(finalSample);
            // Reverb and the limiter run as whole-buffer passes below
            // (v0.9915) rather than per sample.
            buf[i]=finalSample;
        }
        applyReverbBlock(buf,BUF);
        applySoftLimitBlock(buf,BUF,fxEffLimiterDrive,params.limiterMix);
        // Diagnostic: one buffer's real-time budget is BUF/SAMPLE_RATE ≈ 23220us.
        // Measured here, BEFORE playRaw(), so it reflects DSP cost alone
        // (see diagRecordBuffer above for why measuring past playRaw()
        // reported the playback rate instead). Only simple counters are
        // touched on this core — the actual Serial.printf() summary line
        // happens once a second from loop() on Core 1 instead (see
        // diagPrintPending). Even a single, infrequent Serial write can
        // occasionally block for some time on USB CDC (particularly with a
        // host actively reading, e.g. a serial monitor open) — enough to
        // delay the next buffer's start. Moving it off Core 0 entirely
        // removes that as a possible contributor.
        diagRecordBuffer(micros()-bufStartUs);
        M5Cardputer.Speaker.playRaw(buf,BUF,SAMPLE_RATE,false,1,CH,false);
        // Yield briefly, but not every single buffer — vTaskDelay(1) can
        // actually take longer than 1ms depending on the system tick rate,
        // and doing it every ~23ms buffer adds up. Since we only need to
        // give Core 0's idle task/watchdog a chance far more often than its
        // ~5s timeout, yielding every 8th buffer (~every 185ms) is still a
        // huge safety margin while cutting this overhead 8x.
        static uint8_t yieldCounter=0;
        if(++yieldCounter>=8){yieldCounter=0;vTaskDelay(1);}
    }
}

// ==========================================================
// IMU
// ==========================================================
float lastAccelX=0,lastAccelY=0;
float imuXLastNorm=0.f, imuYLastNorm=0.f; // last normalized value applied per axis (for save/restore while held)

void applyImuValue(ImuTarget target,float value){
    switch(target){
        case ImuTarget::TIMBRE:        params.timbreMorphTarget=value*(float)max(1,morphChainLen-1);break;
        case ImuTarget::VIBRATO_DEPTH: params.vibratoDepthOffsetTarget=value;break;
        case ImuTarget::VIBRATO_RATE:  params.vibratoRateOffsetTarget=value*4.5f;break;
        case ImuTarget::TREMOLO:       params.tremoloDepthOffsetTarget=value;break;
        case ImuTarget::VOLUME:        params.volumeScaleTarget=constrain(1.0f-fabsf(value),0.f,1.f);break;
        case ImuTarget::PITCH_BEND:    params.pitchBendCentsTarget=value*keyBendMaxCents;break;
        case ImuTarget::BEND_UP:       params.pitchBendCentsTarget=fabsf(value)*keyBendMaxCents;break;
        case ImuTarget::BEND_DOWN:     params.pitchBendCentsTarget=-fabsf(value)*keyBendMaxCents;break;
        case ImuTarget::BITCRUSH:      params.bitcrushOffsetTarget=value;break;
        case ImuTarget::FILTER_CUTOFF: params.filterCutoffOffsetTarget=value;break;
        // +-0.5 rather than +-0.4 (v0.9944), so that from the natural
        // centre of 50% a full tilt reaches 0% and 100% exactly. Note the
        // offset is relative to the VCO's Shape setting, as every other
        // IMU target is relative to its own base: with Shape parked at 0%
        // there is simply no room below it, and only one direction does
        // anything. Setting Shape to 50% in VCO is what makes both
        // directions live.
        case ImuTarget::SHAPE:           params.oscShapeOffsetTarget=value*0.5f;break;
        case ImuTarget::DETUNE:        params.detuneOffsetTarget=value*50.f;break;
        case ImuTarget::NOISE:         params.noiseOffsetTarget=value;break;
        case ImuTarget::SUB_LEVEL:     params.subLevelOffsetTarget=value;break;
        case ImuTarget::RESONANCE:     params.resonanceOffsetTarget=value*3.f;break;
        case ImuTarget::LFO_RATE:      lfoRateOffsetTarget=value*LFO_RATE_MAX;break;
        case ImuTarget::LFO_DEPTH:     lfoDepthOffsetTarget=value;break;
        case ImuTarget::ARP_TEMPO:
            if(lastMainMode==AppMode::SEQ)seqTempoOffsetTarget=value*100.f;
            else                          arpTempoOffsetTarget=value*100.f;
            break; // +/-100 BPM
        case ImuTarget::ARP_SWING:
            if(lastMainMode==AppMode::SEQ)seqSwingOffsetTarget=value*50.f;
            else                          arpSwingOffsetTarget=value*50.f;
            break; // +/-50%
        // FX (v0.9876). Each scales the normalized tilt to roughly half
        // that parameter's own span, so a full tilt sweeps a musically
        // useful amount without pinning the value at an extreme; the
        // final clamp into range happens in updateFxEffective().
        case ImuTarget::FX_RING_RATE:    params.ringModRateOffsetTarget  =value*990.f; break;
        case ImuTarget::FX_RING_MIX:     params.ringModMixOffsetTarget   =value;       break;
        case ImuTarget::FX_LIMIT_DRIVE:  params.limiterDriveOffsetTarget =value*2.f;   break;
        case ImuTarget::FX_CHORUS_DEPTH: params.chorusDepthOffsetTarget  =value*10.f;  break;
        case ImuTarget::FX_CHORUS_MIX:   params.chorusMixOffsetTarget    =value;       break;
        case ImuTarget::FX_DELAY_FB:     params.delayFeedbackOffsetTarget=value*0.45f; break;
        case ImuTarget::FX_DELAY_MIX:    params.delayMixOffsetTarget     =value;       break;
        case ImuTarget::FX_REVERB_ROOM:  params.reverbRoomOffsetTarget   =value*0.5f;  break;
        case ImuTarget::FX_REVERB_MIX:   params.reverbMixOffsetTarget    =value;       break;
        // Negated so tilting toward the positive end favours oscillator 1,
        // which is the one you were already hearing.
        case ImuTarget::OSC_MIX:         params.osc2LevelOffsetTarget    =-value;      break;
        case ImuTarget::OSC2_SHAPE:      params.osc2ShapeOffsetTarget    =value*0.5f;  break;
        default:break;
    }
}

void resetParamToDefault(ImuTarget t){
    switch(t){
        case ImuTarget::TIMBRE:        params.timbreMorph=params.timbreMorphTarget=0;break;
        case ImuTarget::VIBRATO_DEPTH: params.vibratoDepthOffset=params.vibratoDepthOffsetTarget=0;break;
        case ImuTarget::VIBRATO_RATE:  params.vibratoRateOffset =params.vibratoRateOffsetTarget =0;break;
        case ImuTarget::TREMOLO:       params.tremoloDepthOffset=params.tremoloDepthOffsetTarget=0;break;
        case ImuTarget::VOLUME:        params.volumeScale=params.volumeScaleTarget=1.0f;break;
        case ImuTarget::PITCH_BEND:
        case ImuTarget::BEND_UP:
        case ImuTarget::BEND_DOWN:     params.pitchBendCents=params.pitchBendCentsTarget=0;break;
        case ImuTarget::BITCRUSH:      params.bitcrushOffset=params.bitcrushOffsetTarget=0;break;
        case ImuTarget::FILTER_CUTOFF: params.filterCutoffOffset=params.filterCutoffOffsetTarget=0;break;
        case ImuTarget::SHAPE:           params.oscShapeOffset=params.oscShapeOffsetTarget=0;break;
        case ImuTarget::DETUNE:        params.detuneOffset=params.detuneOffsetTarget=0;break;
        case ImuTarget::NOISE:         params.noiseOffset=params.noiseOffsetTarget=0;break;
        case ImuTarget::SUB_LEVEL:     params.subLevelOffset=params.subLevelOffsetTarget=0;break;
        case ImuTarget::RESONANCE:     params.resonanceOffset=params.resonanceOffsetTarget=0;break;
        case ImuTarget::LFO_RATE:      lfoRateOffset=lfoRateOffsetTarget=0;break;
        case ImuTarget::LFO_DEPTH:     lfoDepthOffset=lfoDepthOffsetTarget=0;break;
        case ImuTarget::ARP_TEMPO:
            arpTempoOffset=arpTempoOffsetTarget=0;
            seqTempoOffset=seqTempoOffsetTarget=0;
            break;
        case ImuTarget::ARP_SWING:
            arpSwingOffset=arpSwingOffsetTarget=0;
            seqSwingOffset=seqSwingOffsetTarget=0;
            break;
        // FX (v0.9876) — clearing the offset (not the menu value) is the
        // right "default" here, exactly as for every other offset above.
        case ImuTarget::FX_RING_RATE:    params.ringModRateOffset  =params.ringModRateOffsetTarget  =0;break;
        case ImuTarget::FX_RING_MIX:     params.ringModMixOffset   =params.ringModMixOffsetTarget   =0;break;
        case ImuTarget::FX_LIMIT_DRIVE:  params.limiterDriveOffset =params.limiterDriveOffsetTarget =0;break;
        case ImuTarget::FX_CHORUS_DEPTH: params.chorusDepthOffset  =params.chorusDepthOffsetTarget  =0;break;
        case ImuTarget::FX_CHORUS_MIX:   params.chorusMixOffset    =params.chorusMixOffsetTarget    =0;break;
        case ImuTarget::FX_DELAY_FB:     params.delayFeedbackOffset=params.delayFeedbackOffsetTarget=0;break;
        case ImuTarget::FX_DELAY_MIX:    params.delayMixOffset     =params.delayMixOffsetTarget     =0;break;
        case ImuTarget::FX_REVERB_ROOM:  params.reverbRoomOffset   =params.reverbRoomOffsetTarget   =0;break;
        case ImuTarget::FX_REVERB_MIX:   params.reverbMixOffset    =params.reverbMixOffsetTarget    =0;break;
        case ImuTarget::OSC_MIX:         params.osc2LevelOffset    =params.osc2LevelOffsetTarget    =0;break;
        case ImuTarget::OSC2_SHAPE:      params.osc2ShapeOffset    =params.osc2ShapeOffsetTarget    =0;break;
        default:break;
    }
}

// Applies deadzone remapping (rescales the remaining range so there's no
// jump at the boundary) then an optional exponential response curve.
float applyDeadzoneAndCurve(float n,float deadzone,bool exponential){
    float s=(n<0.f)?-1.f:1.f;
    float a=fabsf(n);
    if(deadzone>0.001f){
        a=(a<deadzone)?0.f:(a-deadzone)/(1.f-deadzone);
    }
    if(exponential)a=a*a;
    return s*constrain(a,0.f,1.f);
}

// Full per-axis processing: calibration offset -> sensitivity -> invert -> deadzone/curve.
float computeAxisNorm(float angleDeg,const ImuAxisConfig &cfg){
    float adj=angleDeg-cfg.calOffsetDeg;
    float n=constrain((adj/TILT_MAX_DEGREES)*cfg.sensitivity,-1.f,1.f);
    if(cfg.invert)n=-n;
    return applyDeadzoneAndCurve(n,cfg.deadzone,cfg.exponential);
}

bool imuBipolarAuto(ImuTarget t,bool cfg){
    if(t==ImuTarget::PITCH_BEND)return true;
    if(t==ImuTarget::BEND_UP||t==ImuTarget::BEND_DOWN)return false;
    if(t==ImuTarget::ARP_TEMPO||t==ImuTarget::ARP_SWING)return true; // always +/-, like Pitch Bend
    // Osc Mix is a crossfade between the two oscillators, so which
    // DIRECTION you tilt is the whole control (v0.9935). Without this it
    // inherited the axis's own bipolar setting, and with that off the
    // caller takes fabsf() of the tilt — both directions then produced the
    // same positive value and the mix only ever moved toward oscillator 1.
    if(t==ImuTarget::OSC_MIX)return true;
    // Shape is bipolar by nature (v0.9943). Its neutral is the MIDDLE of
    // its range, not an end: on Square, 0.5 is the symmetrical square and
    // the two directions widen or narrow the pulse. With the axis's own
    // bipolar setting off the caller took fabsf(), so both tilts produced
    // the same positive offset and Shape only ever travelled one way —
    // exactly the OSC_MIX fault above, in a parameter where the
    // asymmetry was harder to spot because the result still changed.
    // Same for oscillator 2's Shape, for the same reason.
    if(t==ImuTarget::SHAPE||t==ImuTarget::OSC2_SHAPE)return true;
    return cfg;
}

// Original Cardputer has no IMU, so tilt is substituted with key input:
// ';'/'.' move a virtual Y axis up/down, ','/'/' move a virtual X axis
// left/right — moves toward the extreme while held, springs back to
// center on release (unless that axis's Hold is toggled on via A/S,
// matching the IMU hold behavior). Deadzone and Calibration don't apply
// to a key-driven virtual axis, so those items are hidden from the PAD
// sub-menu entirely (see imuMenuItemsOriginal).
float padVirtualX=0.f, padVirtualY=0.f; // -1..1, key-driven substitute for tilt
constexpr float PAD_MOVE_RATE=0.06f;    // fraction of full range moved per poll while held
constexpr float PAD_SPRING_RATE=0.15f;  // fraction of the way back to center per poll

void updatePadVirtualAxes(){
    // ';' '.' ',' '/' double as menu navigation on every other screen, so
    // only let them move the PAD while actually on MAIN — otherwise
    // scrolling through VCO/VCF/etc. would silently drag the PAD position
    // around in the background.
    if(appMode!=AppMode::PLAY)return;
    auto s=M5Cardputer.Keyboard.keysState();
    bool yUp=false,yDown=false,xRight=false,xLeft=false;
    for(char c:s.word){
        if(c==';')yUp=true;   if(c=='.')yDown=true;
        if(c=='/')xRight=true;if(c==',')xLeft=true;
    }
    if(yUp)             padVirtualY=min(padVirtualY+PAD_MOVE_RATE,1.f);
    else if(yDown)      padVirtualY=max(padVirtualY-PAD_MOVE_RATE,-1.f);
    else if(!imuYHeld)  padVirtualY+=(0.f-padVirtualY)*PAD_SPRING_RATE;

    if(xRight)           padVirtualX=min(padVirtualX+PAD_MOVE_RATE,1.f);
    else if(xLeft)       padVirtualX=max(padVirtualX-PAD_MOVE_RATE,-1.f);
    else if(!imuXHeld)   padVirtualX+=(0.f-padVirtualX)*PAD_SPRING_RATE;
}

void updateImu(){
    if(isCardputerAdv){
        if(!M5.Imu.update())return;
        auto data=M5.Imu.getImuData();
        lastAccelX=data.accel.x;lastAccelY=data.accel.y;
        auto clamp1=[](float v){return constrain(v,-1.f,1.f);};
        float aX=asinf(clamp1(lastAccelX))*180/PI;
        float aY=asinf(clamp1(lastAccelY))*180/PI;
        lastAngleXDeg=aX;lastAngleYDeg=aY;
        if(imuXEnabled&&imuAxisX.target!=ImuTarget::NONE&&!imuXHeld){
            float n=computeAxisNorm(aX,imuAxisX);
            float applied=imuBipolarAuto(imuAxisX.target,imuAxisX.bipolar)?n:fabsf(n);
            imuXLastNorm=applied;
            applyImuValue(imuAxisX.target,applied);
        }
        if(imuYEnabled&&imuAxisY.target!=ImuTarget::NONE&&!imuYHeld){
            float n=computeAxisNorm(aY,imuAxisY);
            float applied=imuBipolarAuto(imuAxisY.target,imuAxisY.bipolar)?n:fabsf(n);
            imuYLastNorm=applied;
            applyImuValue(imuAxisY.target,applied);
        }
    } else {
        updatePadVirtualAxes();
        if(imuXEnabled&&imuAxisX.target!=ImuTarget::NONE){
            float n=constrain(padVirtualX*imuAxisX.sensitivity,-1.f,1.f);
            if(imuAxisX.invert)n=-n;
            if(imuAxisX.exponential){float sgn=(n<0.f)?-1.f:1.f;n=sgn*n*n;}
            float applied=imuBipolarAuto(imuAxisX.target,imuAxisX.bipolar)?n:fabsf(n);
            imuXLastNorm=applied;
            applyImuValue(imuAxisX.target,applied);
        }
        if(imuYEnabled&&imuAxisY.target!=ImuTarget::NONE){
            float n=constrain(padVirtualY*imuAxisY.sensitivity,-1.f,1.f);
            if(imuAxisY.invert)n=-n;
            if(imuAxisY.exponential){float sgn=(n<0.f)?-1.f:1.f;n=sgn*n*n;}
            float applied=imuBipolarAuto(imuAxisY.target,imuAxisY.bipolar)?n:fabsf(n);
            imuYLastNorm=applied;
            applyImuValue(imuAxisY.target,applied);
        }
    }
}

const char *imuTargetName(ImuTarget t){
    switch(t){
        case ImuTarget::NONE:          return "None";
        case ImuTarget::TIMBRE:        return "Timbre";
        case ImuTarget::VIBRATO_DEPTH: return "Vib.Depth";
        case ImuTarget::VIBRATO_RATE:  return "Vib.Rate";
        case ImuTarget::TREMOLO:       return "Tremolo";
        case ImuTarget::VOLUME:        return "Volume";
        case ImuTarget::PITCH_BEND:    return "PitchBend";
        case ImuTarget::BEND_UP:       return "BendUp";
        case ImuTarget::BEND_DOWN:     return "BendDown";
        case ImuTarget::BITCRUSH:      return "Bitcrush";
        case ImuTarget::FILTER_CUTOFF:{
            static char buf[16];
            snprintf(buf,sizeof(buf),"Flt(%s)",filterTypeName(filterParams.type));
            return buf;
        }
        case ImuTarget::SHAPE:       return "Shape";
        case ImuTarget::DETUNE:    return "Detune";
        case ImuTarget::NOISE:     return "Noise";
        case ImuTarget::SUB_LEVEL: return "SubLevel";
        case ImuTarget::RESONANCE: return "Resonance";
        case ImuTarget::LFO_RATE:  return "LFO Rate";
        case ImuTarget::LFO_DEPTH: return "LFO Depth";
        case ImuTarget::ARP_TEMPO:   return (lastMainMode==AppMode::SEQ)?"SeqTempo":"ArpTempo";
        case ImuTarget::ARP_SWING: return (lastMainMode==AppMode::SEQ)?"SeqSwing":"ArpSwing";
        case ImuTarget::FX_RING_RATE:    return "Ring Rate";
        case ImuTarget::FX_RING_MIX:     return "Ring Mix";
        case ImuTarget::FX_LIMIT_DRIVE:  return "Lim.Drive";
        case ImuTarget::FX_CHORUS_DEPTH: return "Cho.Depth";
        case ImuTarget::FX_CHORUS_MIX:   return "Cho.Mix";
        case ImuTarget::FX_DELAY_FB:     return "Dly Fdbk";
        case ImuTarget::FX_DELAY_MIX:    return "Dly Mix";
        case ImuTarget::FX_REVERB_ROOM:  return "Rvb Room";
        case ImuTarget::FX_REVERB_MIX:   return "Rvb Mix";
        case ImuTarget::OSC_MIX:         return "Osc Mix";
        case ImuTarget::OSC2_SHAPE:      return "Osc2 Shp";
        default:return "?";
    }
}

// ==========================================================
// Key helpers
// ==========================================================
float resolveFreqFromKeys(){
    auto s=M5Cardputer.Keyboard.keysState();
    for(auto it=s.word.rbegin();it!=s.word.rend();++it){
        for(int i=0;i<12;i++)
            if(ROW1_KEYS[i]==*it)
                return row1Freqs[i]*powf(2.f,(float)params.octaveShift)*powf(2.f,(float)transposeSemitones/12.f);
        for(int i=0;i<13;i++)
            if(ROW2_KEYS[i]==*it)
                return row2Freqs[i]*powf(2.f,(float)params.octaveShift)*powf(2.f,(float)transposeSemitones/12.f);
    }
    // Row 1's 13th note is the Backspace/Delete key — not a printable
    // character, so KeysState reports it via .del instead of .word.
    // Checked last (lowest priority vs. any note key).
    if(s.del)
        return row1Freqs[12]*powf(2.f,(float)params.octaveShift)*powf(2.f,(float)transposeSemitones/12.f);
    return 0.f;
}

// ---------------------------------------------------------
// Arpeggiator (CardputerADV only — original Cardputer's 3-key rollover
// limit can't reliably support the multi-key chord holding this needs)
// ---------------------------------------------------------
enum class ArpType : uint8_t { UP, DOWN, UP_DOWN, AS_PLAYED, RANDOM };
constexpr int ARP_MAX_NOTES=6;

bool arpEnabled=false;
ArpType arpType=ArpType::UP;
float arpTempoBpm=120.0f; // 40-240
float arpSwing=0.0f;    // -100 to +100%: positive delays the off-beat step (laid-back), negative pushes it earlier (anticipated/"pushed" feel)

float arpHeldFreqs[ARP_MAX_NOTES];   // press order
float arpSortedFreqs[ARP_MAX_NOTES]; // pitch order, for Up/Down/Up-Down
int   arpHeldCount=0;
int   arpStepIndex=0;
unsigned long arpLastStepMs=0;

// Latch mode ('V' key): on a cramped keyboard, reliably holding several
// keys down at once is hard — a finger lifting even briefly drops that
// note from the chord. With Latch on, each note-key press TOGGLES that
// note's membership in the held chord instead, so a chord can be built
// up one tap at a time without needing continuous physical holds.
// The MIDI note lists and the note-to-Hz helper live further down with the
// rest of the MIDI code, but the arpeggiator below needs them (v0.9985).
// Declared here rather than moved, because they belong with MIDI and the
// arp is the borrower — this file has repeatedly gone wrong by relocating
// code to satisfy the compiler instead of declaring it.
constexpr int MIDI_NOTE_STACK_FWD=8;
extern uint8_t midiHeldNotes[MIDI_NOTE_STACK_FWD];
extern int     midiHeldCount;
extern uint8_t midiLatchedNotes[MIDI_NOTE_STACK_FWD];
extern int     midiLatchedCount;
float midiNoteToHz(uint8_t note);

bool arpLatchEnabled=false;
bool arpLatchEnabledFwd(){return arpLatchEnabled;}
constexpr char ARP_DEL_SENTINEL='\x7F'; // stands in for the Backspace/del "key" in the latched-keys list
char arpLatchedKeys[ARP_MAX_NOTES];
int  arpLatchedCount=0;
char arpPrevWord[16]; int arpPrevWordLen=0;
bool arpPrevDel=false;

extern bool seqPlaying;   // declared with the sequencer, far below
                          // (v0.99931 build fix)

void rebuildArpChord();   // defined just below; declared here so the
                          // switch below can call it directly (v0.99931)

void arpLatchToggle(){
    arpLatchEnabled=!arpLatchEnabled;
    arpLatchedCount=0; // always start fresh, whichever direction we're toggling
    midiLatchedCount=0;   // v0.9984: the MIDI half of the same latch
    // Reflects the clear immediately rather than waiting for the next key
    // or MIDI event to happen to trigger a rebuild (v0.99931) — toggling
    // Latch off should silence what it was holding right away.
    rebuildArpChord();
}

bool noteKeyBaseFreq(char c,float &freqOut){
    for(int i=0;i<12;i++)if(ROW1_KEYS[i]==c){freqOut=row1Freqs[i];return true;}
    for(int i=0;i<13;i++)if(ROW2_KEYS[i]==c){freqOut=row2Freqs[i];return true;}
    return false;
}

// Latch's toggle-on-new-keypress bookkeeping (v0.99931, split out of what
// used to be updateArpHeldNotes()). This is genuinely a keyChanged-only
// concern — it detects a NEW physical keypress by diffing against the
// previous frame's key set, which only means anything at the instant a
// key transitions. Rebuilding arpHeldFreqs[] is a different question
// (see rebuildArpChord() below) and used to be welded to this in one
// function with two unrelated call sites, which is the root of three
// separate bugs (v0.9986, v0.99891, v0.99894): whichever call site
// existed at the time did edge-detection AND rebuild together, so a path
// that only needed a rebuild (MIDI) either skipped both or redundantly
// re-ran edge-detection every time.
void updateArpLatchEdges(){
    if(!arpLatchEnabled)return;
    auto s=M5Cardputer.Keyboard.keysState();
    {
        for(char c:s.word){
            bool wasPressed=false;
            for(int i=0;i<arpPrevWordLen;i++)if(arpPrevWord[i]==c){wasPressed=true;break;}
            if(wasPressed)continue;
            float dummy;
            if(!noteKeyBaseFreq(c,dummy))continue;
            int foundIdx=-1;
            for(int i=0;i<arpLatchedCount;i++)if(arpLatchedKeys[i]==c){foundIdx=i;break;}
            if(foundIdx>=0){
                for(int j=foundIdx;j<arpLatchedCount-1;j++)arpLatchedKeys[j]=arpLatchedKeys[j+1];
                arpLatchedCount--;
            } else if(arpLatchedCount<ARP_MAX_NOTES){
                arpLatchedKeys[arpLatchedCount++]=c;
            }
        }
        if(s.del&&!arpPrevDel){
            int foundIdx=-1;
            for(int i=0;i<arpLatchedCount;i++)if(arpLatchedKeys[i]==ARP_DEL_SENTINEL){foundIdx=i;break;}
            if(foundIdx>=0){
                for(int j=foundIdx;j<arpLatchedCount-1;j++)arpLatchedKeys[j]=arpLatchedKeys[j+1];
                arpLatchedCount--;
            } else if(arpLatchedCount<ARP_MAX_NOTES){
                arpLatchedKeys[arpLatchedCount++]=ARP_DEL_SENTINEL;
            }
        }
        arpPrevWordLen=0;
        for(char c:s.word){if(arpPrevWordLen<16)arpPrevWord[arpPrevWordLen++]=c;}
        arpPrevDel=s.del;
    }
}

// The one place arpHeldFreqs[]/arpSortedFreqs[] are built, called by every
// path that can change what should be sounding — a local key event, a
// MIDI note on/off, a switch toggling Latch, panic — rather than each
// path deciding for itself whether a rebuild is owed (v0.99931). Reads
// the CURRENT state directly (arpLatchedKeys[]/midiLatchedNotes[] in
// Latch mode, live keysState()/midiHeldNotes[] otherwise); it does not
// care what triggered the call, only what the chord should be right now.
void rebuildArpChord(){
    // The same eligibility rule the local-key path already used
    // (notesAllowed, further down) applies here too (v0.99931), folded in
    // once so every caller — local keys, MIDI, Latch toggling, panic —
    // gets it automatically rather than each repeating its own version of
    // the check. This is what previously let MIDI rebuild the chord on
    // screens where local keys could not.
    if(!arpEnabled||seqPlaying)return;
    bool notesAllowed=(appMode!=AppMode::PATCH&&appMode!=AppMode::SEQ
                       &&appMode!=AppMode::PATTERN&&appMode!=AppMode::SONG
                       &&appMode!=AppMode::TIMBRE);
    if(!notesAllowed)return;
    auto s=M5Cardputer.Keyboard.keysState();
    float mult=powf(2.f,(float)params.octaveShift)*powf(2.f,(float)transposeSemitones/12.f);

    if(arpLatchEnabled){
        arpHeldCount=0;
        for(int k=0;k<arpLatchedCount&&arpHeldCount<ARP_MAX_NOTES;k++){
            char c=arpLatchedKeys[k];
            float freq=0.f;
            if(c==ARP_DEL_SENTINEL)freq=row1Freqs[12];
            else noteKeyBaseFreq(c,freq);
            if(freq>0.f)arpHeldFreqs[arpHeldCount++]=freq*mult;
        }
        // Latched MIDI notes join the chord too (v0.9984). Note the
        // absent `*mult`: a MIDI note number already IS an absolute
        // pitch, so applying the local octave shift and transpose to it
        // would move the keyboard out from under the player.
        for(int k=0;k<midiLatchedCount&&arpHeldCount<ARP_MAX_NOTES;k++)
            arpHeldFreqs[arpHeldCount++]=midiNoteToHz(midiLatchedNotes[k]);
    } else {
        arpHeldCount=0;
        for(char c:s.word){
            if(arpHeldCount>=ARP_MAX_NOTES)break;
            float freq=0.f;
            if(noteKeyBaseFreq(c,freq))arpHeldFreqs[arpHeldCount++]=freq*mult;
        }
        if(s.del&&arpHeldCount<ARP_MAX_NOTES)arpHeldFreqs[arpHeldCount++]=row1Freqs[12]*mult;
        // MIDI notes currently held feed the arpeggiator exactly as local
        // keys do — this is the whole point of MIDI IN here, since the
        // built-in keyboard can only manage three keys at once and an
        // arpeggio wants more (v0.9984).
        for(int k=0;k<midiHeldCount&&arpHeldCount<ARP_MAX_NOTES;k++)
            arpHeldFreqs[arpHeldCount++]=midiNoteToHz(midiHeldNotes[k]);
    }

    for(int i=0;i<arpHeldCount;i++)arpSortedFreqs[i]=arpHeldFreqs[i];
    for(int i=1;i<arpHeldCount;i++){ // insertion sort, n<=6
        float key=arpSortedFreqs[i];int j=i-1;
        while(j>=0&&arpSortedFreqs[j]>key){arpSortedFreqs[j+1]=arpSortedFreqs[j];j--;}
        arpSortedFreqs[j+1]=key;
    }
}

int nextArpIndex(){
    int n=arpHeldCount;
    if(n<=1)return 0;
    switch(arpType){
        case ArpType::UP:        return arpStepIndex%n;
        case ArpType::DOWN:      return n-1-(arpStepIndex%n);
        case ArpType::UP_DOWN: {
            int cycle=2*(n-1);
            int pos=arpStepIndex%cycle;
            return (pos<n)?pos:(cycle-pos);
        }
        case ArpType::AS_PLAYED: return arpStepIndex%n;
        case ArpType::RANDOM:    return random(0,n);
    }
    return 0;
}

// ---- MIDI (v0.996) ----
//
// Deliberately split from any transport. This layer only turns a stream of
// BYTES into note events; where those bytes came from — USB, a DIN socket
// on the MIDI unit, or a test harness — is not its concern. That matters
// because USB MIDI needs a build-time change to the USB mode (and with it
// the serial log), so getting the message handling right BEFORE touching
// that keeps the risky part small and keeps this code useful whichever
// transport ends up carrying it.
//
// Channel handling is Omni for now: every channel is accepted. Anything
// else is a setting to add later, and Omni is what makes "plug it in and
// it plays" true, which is the whole point of the first version.
constexpr uint8_t MIDI_NOTE_OFF=0x80,MIDI_NOTE_ON=0x90,MIDI_PITCH_BEND=0xE0;
// Control Change and Program Change (v0.99861). Both are two- and
// one-data-byte messages respectively, which is why midiDataNeeded exists.
constexpr uint8_t MIDI_CC=0xB0,MIDI_PROGRAM=0xC0;
constexpr uint8_t CC_MODULATION=1,CC_SUSTAIN=64,CC_ALL_NOTES_OFF=123;
constexpr float MIDI_A4_HZ=440.f;   // note 69

// Modulation wheel drives vibrato depth. It is the near-universal default
// for CC1, so a keyboard's wheel does something sensible with no setup —
// and it reuses the existing vibrato offset rather than adding a parallel
// control, the same approach velocity and pitch bend already take.
constexpr float MIDI_MOD_MAX_VIBRATO=0.6f;

// ---- IMU -> MIDI CC out (v0.99871) ----
//
// Tilt the synth, and an external instrument responds. This is the feature
// people actually noticed at release — IMU-driven timbre — pointed
// outward, and it is the one thing here no other small synth does.
//
// Two things make or break it, and both are about not flooding the wire.
// A MIDI cable carries about 3125 bytes a second and a CC message is
// three of them, so a value sent every IMU update would be roughly a
// thousand messages a second per axis: enough to swamp the link and delay
// the notes travelling on it. So values are only sent when the 7-bit
// value CHANGES, and never closer together than MIDI_CC_MIN_INTERVAL_MS.
// Sending on change alone is not enough — a hand shaking gently around a
// boundary would still send continuously.
constexpr unsigned long MIDI_CC_MIN_INTERVAL_MS=15;   // ~66/s per axis, worst case
bool    midiCcOutEnabled=false;
uint8_t midiCcOutNumX=CC_MODULATION;   // CC1  — the default a receiver most likely maps
uint8_t midiCcOutNumY=74;              // CC74 — filter cutoff by convention
uint8_t midiCcOutChannel=0;            // 0-15, shown as 1-16
int     midiCcLastSentX=-1,midiCcLastSentY=-1;
unsigned long midiCcLastMsX=0,midiCcLastMsY=0;

// The UART itself is declared further down with the rest of the transport,
// where it belongs; this is the borrower. (Declaring rather than moving —
// see v0.9985 and v0.99862.)
extern HardwareSerial midiSerial;
extern bool midiSerialReady;

void midiSendCC(uint8_t ch,uint8_t cc,uint8_t val){
    if(!midiSerialReady)return;
    uint8_t msg[3]={(uint8_t)(MIDI_CC|(ch&0x0F)),(uint8_t)(cc&0x7F),(uint8_t)(val&0x7F)};
    midiSerial.write(msg,3);
}

// norm is the axis's own -1..+1 (bipolar) or 0..1 value, so this maps the
// full travel onto 0-127 either way — the receiving end gets the whole
// range whichever way the axis is configured.
// ---- Note out (v0.99872) ----
//
// Sends whatever this synth is playing as MIDI notes. In Bypass mode the
// controller's TX reaches the Unit's SAM2695 chip, so this plays the
// unit's built-in GM synth — WHILE MIDI in still works, since Bypass is
// also the mode where the RX pin receives the INPUT socket. That
// combination is the useful one: play the local keys or an external
// keyboard, hear C.P.S. and a GM instrument together, with no second
// piece of hardware involved. (In Separate mode the same bytes go out the
// OUTPUT socket to external gear instead, but MIDI in stops working —
// the unit cannot do both.)
//
// Deliberately driven from what the synth is ACTUALLY sounding rather
// than from the key handlers: local keys, MIDI in, the arpeggiator and
// the sequencer all end up setting currentFreq, so watching that one
// value covers every source at once instead of needing a hook in each.
bool    midiNoteOutEnabled=false;
uint8_t midiGmProgram=0;        // 0 = Acoustic Grand Piano
int     midiLastSentNote=-1;
uint8_t midiLastSentVel=100;

void midiSendNoteOn(uint8_t ch,uint8_t note,uint8_t vel){
    if(!midiSerialReady)return;
    uint8_t msg[3]={(uint8_t)(MIDI_NOTE_ON|(ch&0x0F)),(uint8_t)(note&0x7F),(uint8_t)(vel&0x7F)};
    midiSerial.write(msg,3);
}
void midiSendNoteOff(uint8_t ch,uint8_t note){
    if(!midiSerialReady)return;
    uint8_t msg[3]={(uint8_t)(MIDI_NOTE_OFF|(ch&0x0F)),(uint8_t)(note&0x7F),0};
    midiSerial.write(msg,3);
}
void midiSendProgram(uint8_t ch,uint8_t prog){
    if(!midiSerialReady)return;
    uint8_t msg[2]={(uint8_t)(MIDI_PROGRAM|(ch&0x0F)),(uint8_t)(prog&0x7F)};
    midiSerial.write(msg,2);
}

// Inverse of midiNoteToHz(). Rounded rather than truncated: a note bent or
// detuned slightly flat would otherwise be reported a semitone low.
int midiHzToNote(float hz){
    if(hz<=0.f)return -1;
    int n=(int)(69.f+12.f*log2f(hz/MIDI_A4_HZ)+0.5f);
    return constrain(n,0,127);
}

// Pitch bend out (v0.99875).
//
// midiHzToNote() rounds to the nearest semitone, so without this every
// bend, portamento glide, vibrato, detune and drift is thrown away on the
// MIDI side. Worse than merely lost: Analog Drift reaches +-22 cents, so
// near a semitone boundary the note number flips back and forth and the
// receiver retriggers repeatedly — a chattering that has nothing to do
// with what is being played.
//
// So the note number is fixed at note-on and everything after it is sent
// as bend. +-2 semitones is the General MIDI default range, which the
// SAM2695 and practically every receiver assume without being told.
// Beyond that the deviation cannot be expressed, so the note is
// retriggered at the new pitch — which is right anyway, since a glide of
// more than a whole tone IS a new note musically.
constexpr float MIDI_BEND_RANGE_CENTS=200.f;
int  midiLastBendSent=8192;
unsigned long midiLastBendMs=0;
float midiSentNoteHz=0.f;

void midiSendPitchBend(uint8_t ch,int value14){
    if(!midiSerialReady)return;
    value14=constrain(value14,0,16383);
    uint8_t msg[3]={(uint8_t)(MIDI_PITCH_BEND|(ch&0x0F)),
                    (uint8_t)(value14&0x7F),(uint8_t)((value14>>7)&0x7F)};
    midiSerial.write(msg,3);
}

// Throttled like the CC path, and for the same reason: a vibrato at 5Hz
// would otherwise emit hundreds of messages a second.
void midiBendOutUpdate(){
    if(!midiNoteOutEnabled||midiLastSentNote<0||midiSentNoteHz<=0.f)return;
    float hz=midiSoundingHz;
    if(hz<=0.f)return;
    float cents=1200.f*log2f(hz/midiSentNoteHz);
    int v=8192+(int)(constrain(cents/MIDI_BEND_RANGE_CENTS,-1.f,1.f)*8191.f);
    unsigned long now=millis();
    if(v!=midiLastBendSent&&now-midiLastBendMs>=MIDI_CC_MIN_INTERVAL_MS){
        midiSendPitchBend(midiCcOutChannel,v);
        midiLastBendSent=v; midiLastBendMs=now;
    }
}

void midiNoteOutUpdate(){
    if(!midiNoteOutEnabled){
        // Switched off mid-note: release what is still held, or the
        // receiver keeps sounding it forever.
        if(midiLastSentNote>=0){
            midiSendNoteOff(midiCcOutChannel,(uint8_t)midiLastSentNote);
            midiLastSentNote=-1;
        }
        return;
    }
    bool sounding=(envPhase!=EnvPhase::IDLE&&currentFreq>0.f);
    int want=sounding?midiHzToNote(currentFreq):-1;
    // While a note is held, stay on it as long as the deviation fits in
    // the bend range — midiBendOutUpdate() expresses the difference
    // (v0.99875). Only a move beyond that warrants a new note.
    if(sounding&&midiLastSentNote>=0&&midiSentNoteHz>0.f&&midiSoundingHz>0.f){
        // Measured against the SOUNDING pitch, the same value the bend is
        // derived from — testing one and bending the other would let the
        // two disagree about when a new note is needed (v0.99876).
        float cents=fabsf(1200.f*log2f(midiSoundingHz/midiSentNoteHz));
        if(cents<=MIDI_BEND_RANGE_CENTS)want=midiLastSentNote;
    }
    if(want==midiLastSentNote)return;
    if(midiLastSentNote>=0)midiSendNoteOff(midiCcOutChannel,(uint8_t)midiLastSentNote);
    if(want>=0){
        uint8_t vel=(uint8_t)constrain((int)(seqVelocityMult*127.f),1,127);
        // Centre the bend before the note starts, or it inherits whatever
        // the previous note was bent to.
        midiSendPitchBend(midiCcOutChannel,8192);
        midiLastBendSent=8192;
        midiSendNoteOn(midiCcOutChannel,(uint8_t)want,vel);
        midiLastSentVel=vel;
        midiSentNoteHz=midiNoteToHz((uint8_t)want);
    } else {
        midiSentNoteHz=0.f;
    }
    midiLastSentNote=want;
}

void midiCcOutUpdate(float normX,float normY,bool xLive,bool yLive){
    if(!midiCcOutEnabled)return;
    unsigned long now=millis();
    if(xLive){
        int v=(int)(constrain((normX+1.f)*0.5f,0.f,1.f)*127.f+0.5f);
        if(v!=midiCcLastSentX&&now-midiCcLastMsX>=MIDI_CC_MIN_INTERVAL_MS){
            midiSendCC(midiCcOutChannel,midiCcOutNumX,(uint8_t)v);
            midiCcLastSentX=v; midiCcLastMsX=now;
        }
    }
    if(yLive){
        int v=(int)(constrain((normY+1.f)*0.5f,0.f,1.f)*127.f+0.5f);
        if(v!=midiCcLastSentY&&now-midiCcLastMsY>=MIDI_CC_MIN_INTERVAL_MS){
            midiSendCC(midiCcOutChannel,midiCcOutNumY,(uint8_t)v);
            midiCcLastSentY=v; midiCcLastMsY=now;
        }
    }
}

// Last-note priority, matching how the built-in keyboard already behaves.
// A small stack rather than a single note: releasing a key while another
// is still held should fall back to that one rather than cutting off, and
// on a real keyboard overlapping notes are constant.
constexpr int MIDI_NOTE_STACK=8;
// The arpeggiator forward-declares these arrays above, and an extern
// declaration's bounds must match the definition exactly. Catch a future
// change to one that forgets the other at compile time rather than as a
// confusing linker error (v0.9985).
static_assert(MIDI_NOTE_STACK==MIDI_NOTE_STACK_FWD,
    "MIDI_NOTE_STACK_FWD (declared with the arpeggiator) is out of step");
uint8_t midiHeldNotes[MIDI_NOTE_STACK];
int     midiHeldCount=0;
bool    midiNoteActive=false;

// Sustain pedal (v0.99861). Held notes are not released while it is down;
// they go into a pending list and are released together when it lifts,
// which is what a piano pedal does and what every keyboard player expects.
// Declared HERE rather than up with the CC constants, because it is sized
// by MIDI_NOTE_STACK just above — the same use-before-declare trap this
// file keeps setting for itself (fixed in v0.99862).
bool    midiSustain=false;
uint8_t midiSustainedNotes[MIDI_NOTE_STACK];
int     midiSustainedCount=0;
// True when the pedal is what turned Hold on, so releasing it does not
// cancel a Hold the player set with the H key (v0.99863).
bool    midiPedalTookHold=false;

// Latched MIDI notes, for the arpeggiator's Latch mode (v0.9984). Latch
// means "keep playing what I pressed after I let go", so it needs its own
// list — midiHeldNotes empties as fingers lift, which is exactly what
// Latch is there to survive. Pressing a latched note again removes it, the
// same toggle the local keys use.
uint8_t midiLatchedNotes[MIDI_NOTE_STACK];
int     midiLatchedCount=0;
bool    arpLatchEnabledFwd();   // defined with the arp, far below

void midiLatchToggle(uint8_t n){
    for(int i=0;i<midiLatchedCount;i++){
        if(midiLatchedNotes[i]==n){
            for(int k=i+1;k<midiLatchedCount;k++)midiLatchedNotes[k-1]=midiLatchedNotes[k];
            midiLatchedCount--; return;
        }
    }
    if(midiLatchedCount<MIDI_NOTE_STACK)midiLatchedNotes[midiLatchedCount++]=n;
}

float midiNoteToHz(uint8_t note){
    // 69 = A4. powf is fine here: this runs on note events, not per sample.
    return MIDI_A4_HZ*powf(2.f,((float)note-69.f)/12.f);
}

// Set whenever the held MIDI notes change, so the arpeggiator can rebuild
// its chord (v0.9986). The arp's chord is rebuilt inside the keyChanged
// branch of the main loop — i.e. only when a LOCAL key event happens —
// which is why MIDI alone never started an arpeggio, why adding a local
// keypress suddenly swept the MIDI notes in, and why letting go left the
// MIDI notes playing as if latched: the list was simply never rebuilt
// again. This flag gives MIDI its own way to ask for that rebuild.

void midiStackPush(uint8_t n){
    for(int i=0;i<midiHeldCount;i++)if(midiHeldNotes[i]==n)return;
    if(midiHeldCount<MIDI_NOTE_STACK)midiHeldNotes[midiHeldCount++]=n;
    else{  // full: drop the oldest, keep the newest
        for(int i=1;i<MIDI_NOTE_STACK;i++)midiHeldNotes[i-1]=midiHeldNotes[i];
        midiHeldNotes[MIDI_NOTE_STACK-1]=n;
    }
}
bool midiStackRemove(uint8_t n){
    for(int i=0;i<midiHeldCount;i++){
        if(midiHeldNotes[i]==n){
            for(int k=i+1;k<midiHeldCount;k++)midiHeldNotes[k-1]=midiHeldNotes[k];
            midiHeldCount--; return true;
        }
    }
    return false;
}

// ---- Theremin (v0.999) ----
//
// A VL53L1X distance sensor on the Grove port plays pitch by hand height,
// the way a theremin's pitch antenna does. Volume stays on the IMU tilt,
// so one unit is enough — which was the constraint from the start.
//
// It drives currentFreq and the envelope exactly as the MIDI note path
// does, so the filter, FX, the arpeggiator and note-out all follow with no
// special cases. The sensor is just another way to say "play this pitch".
//
// Wire, NOT Wire1 (corrected v0.99904).
//
// Wire1 is the board's own bus: the keyboard controller and the IMU live
// on it at SDA=8/SCL=9, which is why the first scan found 0x18, 0x34 and
// 0x69. Repointing it at the Grove pins took that bus away from them — and
// M5's library reads the IMU every frame, so it claimed the bus straight
// back. The sensor read once after a rescan and then never again, which is
// exactly how it presented.
//
// Port 0 is free, so the sensor gets it and Wire1 is left alone.
#ifndef CPS_TOF_SDA_PIN
#define CPS_TOF_SDA_PIN 2    // Grove, yellow
#endif
#ifndef CPS_TOF_SCL_PIN
#define CPS_TOF_SCL_PIN 1    // Grove, white
#endif

VL53L1X tofSensor;
bool  tofPresent=false;
// Which I2C peripheral to probe (v0.99909). Selectable because it cannot
// be settled from documentation and it MATTERS: M5's own library owns one
// of these for the keyboard and the IMU, and taking it away makes the IMU
// read nonsense — which is heard as parameters moving on their own, the
// waveform changing, and notes appearing with nothing in front of the
// sensor. All of which were reported with no sensor attached at all.
// 0 = Grove main port (G1/G2, port Wire). 1 = the Cap LoRa-1262's own
// Grove port (v0.99913) — its silkscreen reads G8 SDA / G9 SCL, the exact
// pins M5's library already uses for the keyboard and IMU. So this is not
// a second bus: it is a tap on the SAME internal one, which I2C supports
// (multi-drop) in a way UART never could. Selecting this does not call
// begin() at all — M5 already has that bus running, and re-configuring it
// is what caused the IMU corruption in v0.99904. The sensor is simply
// attached to the bus that is already there.
int   tofBusIndex=0;
bool  thereminFullScan=false;   // set true only by Rescan (v0.99912)
TwoWire *tofBus(){return tofBusIndex==0?&Wire:&Wire1;}
bool  thereminEnabled=false;
// Playing range in mm. Near is the top note: moving the hand AWAY lowers
// the pitch, which is how a real theremin behaves.
// Far defaults to 250mm, not 400 (v0.99909): Short mode reaches much
// further in the dark but only about 280mm in room light, and a window
// wider than the sensor can see is a window whose top half is silence.
int   thereminNearMm=60,thereminFarMm=250;
int   thereminOctaves=2;
// Semitones from C4 to the TOP of the theremin's range (v0.9992). C4 is
// note 0 here, matching row1Freqs[0]'s own reference pitch, so this reads
// the same way the rest of the synth already thinks about pitch.
int   thereminTopSemis=12;   // default C5
bool  thereminQuantize=false;
bool  thereminActive=false;
float thereminSmoothHz=0.f;
unsigned long thereminLastMs=0;
int   thereminLastMm=0;
unsigned long thereminLastGoodMs=0;
// Grace period after (re)enabling, before the watchdog starts counting at
// all (v0.9992). Reported only on Grove, never on Cap: with nothing in
// front of the sensor yet, right after startContinuous(), dataReady() can
// go a beat longer than usual before it first trips — and once ANY real
// reading has come through, the connection stays solid until the next
// toggle. That pattern is a settling window, not a fault, and 3 seconds
// was occasionally not enough of one on Grove's marginally different
// electrical path. Cap never showed it, plausibly because that bus is
// already running continuously rather than just having been reconfigured.
constexpr unsigned long THEREMIN_STARTUP_GRACE_MS=2500;
unsigned long thereminEnabledAtMs=0;
unsigned long thereminLastRetryMs=0;
int  thereminRetryCount=0;   // reset on every successful probe (v0.99944)
bool  thereminLostConnection=false;   // explicit flag for the menu (v0.99917)
float thereminRawHz=0.f;   // pre-quantize, smoothed continuous pitch (v0.99912)
// Consecutive good/bad readings, so one glitch cannot start or stop a note.
int   thereminGoodCount=0,thereminBadCount=0;

// One place that stops the theremin, because stopping it means clearing
// four things and doing three of them was the v0.99906 bug (v0.99907).
//
// portaFreq is the one that was missed. Portamento glides it toward
// currentFreq, so leaving it at the last pitch while currentFreq went to
// zero made it slide down to nothing on its own — heard as the pitch
// wandering after the hand had left. Pressing a key then started the glide
// over from there, which is the loop that was reported. Zero is this
// codebase's "unset" for portaFreq: the note path tests portaFreq<=0 to
// decide whether to seed it.
void thereminStop(){
    thereminActive=false;
    currentFreq=0.f;
    thereminSmoothHz=0.f;
    thereminRawHz=0.f;
    portaFreq=0.f;
}

// Which Grove pin is SDA cannot be settled from documentation any more
// than the MIDI RX pin could (v0.9991), so both orders are tried — and
// unlike the MIDI case this runs ONCE at boot, so there is no search to
// leave running.
//
// The bus is also scanned and the addresses logged. "Not found" has
// several causes that look identical from outside — wrong pins, no power,
// a sensor at an unexpected address — and the scan separates them: any
// device at all means the wiring is right and the address is the problem,
// while an empty bus means it is not.
// What the boot scan found, kept so the menu can show it (v0.9992).
//
// The scan log was written before the serial monitor could attach and was
// unreadable in practice — which is the same trap the SD and MIDI
// diagnostics fell into. A result the player can read on the device needs
// no timing luck at all.
char tofScanResult[24]="not scanned";
int  tofScanDevices=0;

// The VL53L1X answers at 0x29 unless it has been told otherwise, so look
// for THAT rather than for any device at all (v0.99903).
//
// Scanning for "something, anything" reported six devices with nothing
// plugged in — three addresses found twice, and those three were the
// board's own 0x18/0x34/0x69. The bus was the internal one: M5Cardputer
// initialises Wire1 during its own startup, and a later begin() with
// different pins does not move an already-started bus. Wire.end() first
// forces it. Looking for a specific address would have made that obvious
// immediately instead of reading as a wiring problem.
constexpr uint8_t VL53L1X_ADDR=0x29;

// fullScan=true walks every address for the diagnostic log and the
// Rescan row's count; fullScan=false checks only the sensor's own address
// (v0.99912). Every ON toggle used to run the full 126-address sweep, and
// each address is a transaction with the platform's I2C timeout on a
// no-answer — on this hardware that added up to several seconds, which is
// the freeze reported on Theremin ON/OFF. Reserving the full sweep for an
// explicit Rescan means enabling Theremin normally costs one transaction,
// not 126.
bool thereminTryBus(int sda,int scl,bool fullScan=false){
    TwoWire *bus=tofBus();
    // Wire1 (the Cap's port) is never begin()/end()'d — it is M5's bus,
    // already running, and touching it is the exact mistake v0.99904
    // fixed. Wire (Grove main) is still owned by this code and gets the
    // usual reconfigure (v0.99913).
    if(tofBusIndex==0){
        bus->end();
        delay(5);
        bus->begin(sda,scl,400000);
        delay(10);
    }

    if(!fullScan){
        bus->beginTransmission(VL53L1X_ADDR);
        if(bus->endTransmission()!=0){
            if(tofBusIndex==0)bus->end();
            return false;
        }
        tofScanDevices=1;
        snprintf(tofScanResult,sizeof(tofScanResult),"probing SDA=%d",sda);
    } else {
        int found=0; bool sawSensor=false;
        for(uint8_t addr=1;addr<127;addr++){
            bus->beginTransmission(addr);
            if(bus->endTransmission()==0){
                Serial.printf("[ToF] i2c device at 0x%02X (SDA=%d SCL=%d)\n",addr,sda,scl);
                found++;
                if(addr==VL53L1X_ADDR)sawSensor=true;
            }
        }
        tofScanDevices+=found;
        if(!sawSensor){
            // Hand the peripheral back rather than sitting on it with the
            // wrong pins configured (v0.99909) — except the shared bus,
            // which this code does not own and must not tear down.
            if(tofBusIndex==0)bus->end();
            if(found>0)snprintf(tofScanResult,sizeof(tofScanResult),"%d dev, no 0x29",found);
            return false;
        }
    }
    tofSensor.setBus(bus);
    // Lowered from 200ms (v0.99914): the freeze reported after toggling
    // Theremin on turned out not to be the toggle itself — the log showed
    // "[ToF] toggle took 0ms" — but a stream of "i2cRead returned Error
    // 263" that followed it indefinitely. Each failed transaction was
    // blocking the main loop for up to the configured timeout, over and
    // over, with no limit. 50ms shortens each individual stall.
    tofSensor.setTimeout(50);
    bool ok=tofSensor.init();
    snprintf(tofScanResult,sizeof(tofScanResult),ok?"ok SDA=%d":"init fail SDA=%d",sda);
    return ok;
}

void midiSerialSuspend();   // defined with the MIDI transport, below
void midiSerialResume();

void thereminBegin(){
    thereminLostConnection=false;   // a fresh probe supersedes the old verdict
    // The MIDI/Theremin pin conflict from v0.99910 had one gap: this
    // function is the one that actually touches GPIO1/2, but only
    // thereminToggle() suspended MIDI first. Called directly from boot
    // when a saved setting restored Theremin as already on, the UART was
    // never suspended, and the conflict happened exactly as before — the
    // owner's report ("worked after a second reset") is consistent with a
    // peripheral left in a bad state by that conflict rather than with
    // software alone, which a full power cycle clears and a soft reset may
    // not (v0.99911).
    //
    // Suspending here instead of at each caller means every path — boot,
    // the toggle, Rescan — is covered by the one place that owns the pins.
    // Failing to find a sensor gives MIDI back immediately: there is no
    // reason to keep it suspended for a bus nothing is using.
    // Only Grove actually conflicts with MIDI's pins — Cap shares an
    // entirely different bus (G8/9) and never touches GPIO1/2 at all
    // (v0.99922). Suspending unconditionally meant restoring a saved
    // Theremin-ON state on the CAP bus silenced MIDI at boot for no
    // reason: nothing was ever going to collide, so there was nothing to
    // protect against. This is very likely what the owner's Cap+Grove-MIDI
    // test actually hit — MIDI came back only after happening to toggle
    // Theremin off and on, which is a coincidence of a different bug
    // (tofPresent not being cleared on OFF skipped the re-probe on ON, so
    // MIDI was never re-suspended the second time) rather than a real fix.
    if(tofBusIndex==0)midiSerialSuspend();

    // Reset, or a rescan adds to the previous count instead of replacing
    // it — which is why the row climbed by six every press (v0.99903).
    tofScanDevices=0;
    snprintf(tofScanResult,sizeof(tofScanResult),"no i2c device");
    // Grove's pin pair could not be confirmed from documentation, and the
    // EXT header is the other place a unit can be attached, so try both —
    // still once per call, with a definite end.
    // Grove only, both orders (v0.99908). The EXT header's pins were in
    // this list on the chance a unit was attached there, but configuring
    // I2C on pins that may be wired to something else is a real risk for
    // no benefit — the sensor was found on Grove, and a board with no
    // sensor at all was misbehaving.
    static const int PIN_PAIRS[][2]={{CPS_TOF_SDA_PIN,CPS_TOF_SCL_PIN},
                                     {CPS_TOF_SCL_PIN,CPS_TOF_SDA_PIN}};
    bool found=false;
    for(auto &pr:PIN_PAIRS){if(thereminTryBus(pr[0],pr[1],thereminFullScan)){found=true;break;}}
    if(!found){
        Serial.println("[ToF] no sensor found - check the unit and the Grove 5V switch");
        tofPresent=false;
        midiSerialResume();   // nothing is using the pins; give them back
        return;
    }
    // Short mode: less range than this sensor can do, but far better
    // immunity to ambient light and a faster update — a theremin is played
    // within arm's reach, and latency matters more than reach.
    tofSensor.setDistanceMode(VL53L1X::Short);
    // The measurement takes 20ms, so the repeat interval has to be longer
    // than that (v0.99905). Setting both to 20 left no gap: the sensor was
    // asked for a new reading before it had finished the last, so it
    // returned stale or invalid data. That is why the pitch froze at
    // whatever height the hand first appeared at — only the first
    // measurement of each pass was real.
    tofSensor.setMeasurementTimingBudget(20000);
    tofSensor.startContinuous(33);
    // Discard the first several readings after (re)starting continuous
    // mode (v0.99911). VL53L1X datasheets note the first measurements
    // after a mode/timing change can be unreliable while the sensor
    // settles, and a stray reading here is a stray note at connect time —
    // the 1-2 seconds of pitch reported with nothing in the sensor's field
    // matches roughly this many 33ms cycles. Counted down in
    // thereminUpdate() rather than delay()'d here, so boot is not blocked.
    thereminGoodCount=-6;
    thereminBadCount=0;
    tofPresent=true;
    thereminEnabledAtMs=millis();   // starts the startup grace window
    thereminRetryCount=0;            // a real find resets the give-up counter
    thereminLastGoodMs=0;
    Serial.println("[ToF] ready");
}

// Time-based, not flag-based (v0.99917). The two versions before this
// counted consecutive did_timeout flags, and the count never advanced no
// matter how long the "i2cRead Error 263" stream ran. The reason is in
// Pololu's own source: did_timeout is only ever set inside read(true)'s
// blocking wait loop —
//
//   if (blocking) { startTimeout(); while (!dataReady()) {
//       if (checkTimeoutExpired()) { did_timeout = true; return 0; } } }
//
// — and this code calls read(false), the non-blocking form, specifically
// so a stalled sensor cannot block the main loop. That path skips the
// loop entirely, so did_timeout is never touched here regardless of
// whether the underlying I2C transaction succeeded. The counter was
// watching a flag that this call was never going to set; it was not that
// failures were rare, it was that none of them were being counted at all.
// Which also means the Grove-only bus-conflict theory this was chasing
// may never have been necessary — a connector or cable that glitches
// occasionally is enough on its own once failures go uncounted forever.
//
// So this tracks the one thing read(false) actually can't hide: how long
// it has been since a reading last succeeded, regardless of which
// internal call produced the last error.
constexpr unsigned long THEREMIN_LOST_MS=3000;


// Snaps to the nearest step of the active Pro Style scale, rather than to
// a flat chromatic semitone (v0.99918).
//
// Chromatic snapping made every scale sound the same through the
// theremin, which defeats the point of having set one in Play Style —
// Semitone mode existed to make the instrument playable in tune, and
// "in tune" should mean the scale the rest of the synth is using, not
// every semitone regardless of it. EZ Style has no Scale setting to
// draw on, so it falls back to the chromatic snap this replaces.
//
// Searches every degree across a couple of octaves either side of the
// theremin's own range and keeps the closest, the same brute-force
// approach recomputeKeyNotes() already uses for the keyboard rows — at
// under a dozen scale degrees and a handful of octaves, cheap enough for
// every 33ms reading.
float thereminQuantizeToHz(float hz,float topHz){
    if(playMode!=PlayMode::PRO){
        float semis=roundf(12.f*log2f(hz/topHz));
        return topHz*powf(2.f,semis/12.f);
    }
    const ScaleDef &sc=SCALES[currentScaleIndex];
    float target=12.f*log2f(hz/topHz);   // semitones below topHz, negative
    float bestSemis=0.f,bestDist=1e9f;
    for(int oct=-(thereminOctaves+1);oct<=1;oct++){
        for(int deg=0;deg<sc.length;deg++){
            float semis=(float)sc.intervals[deg]+oct*12.f;
            float dist=fabsf(semis-target);
            if(dist<bestDist){bestDist=dist;bestSemis=semis;}
        }
    }
    return topHz*powf(2.f,bestSemis/12.f);
}

void thereminUpdate(){
    // !tofPresent no longer exits here (v0.99922): it now means "lost,
    // retrying" rather than "off", and the retry logic below needs to run
    // while it is set. thereminEnabled is still the real off switch.
    if(!thereminEnabled)return;
    if(millis()-thereminLastMs<15)return;   // poll faster than the 33ms period
    thereminLastMs=millis();
    if(!tofPresent){
        // Retried on a slower cadence than the normal 15ms poll, not every
        // cycle (v0.99922): thereminBegin() on Grove does two blocking
        // delays (bus->end()/begin() with a settle pause), and running
        // that every 15ms would spend nearly all of the loop's time
        // waiting rather than doing anything else. Once every 500ms is
        // frequent enough that a reconnect is noticed quickly without
        // costing meaningful CPU time in between attempts.
        //
        // Bounded now (v0.99944): with no unit plugged in at all — not a
        // transient hiccup, just Theremin left ON from a saved setting —
        // this retried forever, spamming "no sensor found" every 500ms
        // with no way to stop it short of the SETTING menu. 40 attempts
        // (~20s) is enough to catch a genuine reconnect but not so long
        // it feels stuck; past that it disables outright, same as the
        // lost-connection path, and needs an explicit switch back on.
        if(thereminRetryCount>=40){
            Serial.println("[ToF] giving up after repeated failures - Theremin disabled");
            thereminEnabled=false;
            midiSerialResume();
            return;
        }
        if(millis()-thereminLastRetryMs>=500){
            thereminLastRetryMs=millis();
            thereminRetryCount++;
            thereminBegin();
        }
        return;
    }
    if(thereminLastGoodMs==0)thereminLastGoodMs=millis();

    // "Still talking to the sensor" and "a target is in range" are
    // different questions, and v0.99917 conflated them (v0.99919). A
    // RangeStatus other than Valid — nothing detected, a weak signal — is
    // a completely ordinary result: it is what a working sensor reports
    // whenever nothing is in front of it, which happens constantly during
    // normal playing (a hand lifted between notes, adjusting position).
    // Feeding that into the same watchdog as a genuine communication
    // failure meant a few seconds of ordinary silence disabled Theremin
    // outright — on Grove AND on Cap alike, since this was a logic error
    // with nothing to do with which bus was in use, and unaffected by
    // reseating any cable.
    //
    // dataReady() returning true is the actual evidence of a live
    // connection: it means a fresh measurement was successfully read over
    // I2C this cycle, whatever that measurement says. The watchdog now
    // resets on that alone. Whether the measurement is USABLE for a note
    // stays exactly the separate question it always was, decided below.
    if(thereminLastGoodMs==0)thereminLastGoodMs=millis();   // first tick after enable
    if(!tofSensor.dataReady()){
        bool pastGrace=(millis()-thereminEnabledAtMs>=THEREMIN_STARTUP_GRACE_MS);
        if(pastGrace&&millis()-thereminLastGoodMs>=THEREMIN_LOST_MS&&!thereminLostConnection){
            // Marked lost, but Theremin itself is left ON and kept polling
            // below — no manual toggle required to recover (v0.99922).
            // Requiring one meant a brief real hiccup needed the same
            // fix as a genuine unplug, and the owner asked, reasonably,
            // why Grove couldn't just keep going the way Cap does. Cap's
            // tolerance comes from its bus being kept alive by unrelated
            // keyboard/IMU traffic; Grove's dataReady() is the only signal
            // this bus has, so silently ignoring failures here would also
            // hide a real disconnect forever, which the earlier cable-pull
            // test relied on catching. Retrying instead keeps both: a
            // transient miss clears itself the moment a reading succeeds,
            // and a genuine outage stays silent for as long as it lasts
            // without ever needing to be told to stop.
            Serial.println("[ToF] sensor not responding - retrying");
            tofPresent=false; thereminLostConnection=true;
            if(thereminActive)thereminStop();
            if(tofBusIndex==0)midiSerialResume();
        }
        return;
    }
    thereminLastGoodMs=millis();
    tofPresent=true;
    int mm=(int)tofSensor.read(false);

    // One decision, made once (v0.99908).
    //
    // Previously "is this a real measurement" and "is the hand inside the
    // playing window" were separate tests with separate early exits, and
    // only the first reset the good-reading counter. At the edges the two
    // disagreed several times a second, so the note stopped and started
    // repeatedly — and every restart is an ATTACK at whatever pitch the
    // next reading gave, which is where the bursts of high notes came
    // from. A hand hovering at the far limit, or closer than the near
    // limit, sat exactly on that boundary.
    //
    // Now a reading is simply usable or it is not, and it takes several in
    // a row to change state either way. Between those states the pitch is
    // left alone rather than being recomputed from a reading that has
    // already been judged unusable.
    constexpr int THEREMIN_CONFIRM=3;
    bool statusOk=(tofSensor.ranging_data.range_status==VL53L1X::RangeValid);
    bool inWindow=(mm>=thereminNearMm&&mm<=thereminFarMm);
    bool usable=statusOk&&mm>=20&&mm<=2000&&inWindow;

    if(!usable){
        thereminGoodCount=0;
        if(++thereminBadCount>=THEREMIN_CONFIRM){
            thereminLastMm=0;
            if(thereminActive)thereminStop();
        }
        return;
    }
    thereminBadCount=0;
    thereminLastMm=mm;
    // Still settling: count up through the negative range from
    // thereminBegin() before the usual confirm count applies (v0.99911).
    if(thereminGoodCount<0){thereminGoodCount++;return;}
    if(++thereminGoodCount<THEREMIN_CONFIRM)return;
    thereminGoodCount=THEREMIN_CONFIRM;   // don't let it wrap

    float t=constrain((float)(mm-thereminNearMm)/(float)(thereminFarMm-thereminNearMm),0.f,1.f);

    // The top note is its own setting now, independent of the keyboard's
    // live octave/transpose (v0.9992). Deriving it from params.octaveShift
    // meant the playable range moved whenever the keyboard's own octave
    // did, and pushing the keyboard up to reach a higher theremin range
    // pushed the KEYBOARD out of a useful register at the same time —
    // there was no way to reach a high theremin range and a comfortable
    // keyboard range together. thereminTopSemis is a plain semitone offset
    // from C4, set on its own page, unaffected by anything else in the
    // synth.
    // Transpose still applies, octave does not (v0.99921). Octave was
    // dropped in v0.9992 because it dragged the whole theremin range
    // along with the keyboard's own register. Transpose is different: it
    // is a small, deliberate key-of-the-song shift, and a scale locked to
    // C regardless of Transpose defeats the point of setting one — the
    // scale itself should move with the song's key, the same way it does
    // for the keyboard.
    float topHz=261.63f*powf(2.f,(float)thereminTopSemis/12.f)
                *powf(2.f,(float)transposeSemitones/12.f);
    float hz=topHz*powf(2.f,-t*(float)thereminOctaves);

    // Smoothing runs on the RAW continuous pitch, before quantizing
    // (v0.99912). Doing it the other way — smoothing the ALREADY-snapped
    // value — meant the output was always gliding toward whichever
    // semitone had just been picked and never actually arrived cleanly, so
    // Semitone mode sounded like a slightly stepped version of Smooth
    // rather than like real steps. Quantizing after smoothing removes that
    // glide entirely: once the smoothed raw pitch crosses a semitone
    // boundary the output jumps straight to the new note.
    if(thereminRawHz<=0.f)thereminRawHz=hz;
    else thereminRawHz+=(hz-thereminRawHz)*0.25f;

    thereminSmoothHz=thereminRawHz;
    if(thereminQuantize){
        // Continuous is the authentic theremin behaviour, but the
        // instrument is famously hard to play in tune, and this makes it
        // usable alongside the rest of the synth. Snapped to the active
        // Play Style scale in Pro Style (v0.99918), chromatic otherwise.
        thereminSmoothHz=thereminQuantizeToHz(thereminSmoothHz,topHz);
    }

    // playingFreq is what the oscillator reads; audioTask only copies
    // currentFreq into it at the moment a note attacks, which is right for
    // every input that retriggers per note and wrong for this one
    // (v0.99906).
    currentFreq=thereminSmoothHz;
    playingFreq=thereminSmoothHz;
    if(portaEnabled)portaFreq=thereminSmoothHz;

    if(!thereminActive){
        thereminActive=true;
        if(envPhase==EnvPhase::IDLE)envLevel=0.f;
        envPhase=EnvPhase::ATTACK;
        filterEnvPhase=EnvPhase::ATTACK;
    }
}

void midiNoteOn(uint8_t note,uint8_t vel){
    // Velocity 0 is Note Off — the convention almost every keyboard uses
    // for note-off, and forgetting it leaves notes stuck on forever.
    if(vel==0){
        if(!midiStackRemove(note))return;
        if(midiHeldCount==0){midiNoteActive=false;currentFreq=0.f;}
        else currentFreq=midiNoteToHz(midiHeldNotes[midiHeldCount-1]);
        return;
    }
    midiStackPush(note);
    if(arpLatchEnabledFwd())midiLatchToggle(note);
    float f=midiNoteToHz(note);
    currentFreq=f;
    if(portaEnabled&&portaFreq<=0.f)portaFreq=f;
    // Retrigger, as the local keyboard does. seqVelocityMult is the same
    // scaling the sequencer's per-step velocity uses, so MIDI velocity
    // lands on an existing control rather than needing a new one.
    seqVelocityMult=constrain((float)vel/127.f,0.f,1.f);
    if(envPhase==EnvPhase::IDLE)envLevel=0.f;
    envPhase=EnvPhase::ATTACK;
    filterEnvPhase=EnvPhase::ATTACK;
    midiNoteActive=true;
    rebuildArpChord();   // v0.99931: direct call, no flag to poll
}

void midiNoteOff(uint8_t note){
    // Pedal down: remember it and keep sounding (v0.99861). Checked before
    // the stack is touched, or the note would be gone by the time the
    // pedal lifts.
    if(midiSustain){
        for(int i=0;i<midiSustainedCount;i++)if(midiSustainedNotes[i]==note)return;
        if(midiSustainedCount<MIDI_NOTE_STACK)midiSustainedNotes[midiSustainedCount++]=note;
        return;
    }
    if(!midiStackRemove(note))return;
    // Releasing the last MIDI note must not silence a note the LOCAL
    // keyboard is still holding (v0.9986) — that was reported as the
    // built-in key's sound vanishing when the MIDI hand lifted. The local
    // key path reasserts currentFreq on its next event, so simply leaving
    // it alone is enough.
    if(midiHeldCount==0){midiNoteActive=false;if(resolveFreqFromKeys()<=0)currentFreq=0.f;}
    else currentFreq=midiNoteToHz(midiHeldNotes[midiHeldCount-1]);
    rebuildArpChord();
}

// Held down: note-offs are deferred instead of sounding. Released: every
// deferred note is let go at once (v0.99861).
void midiSustainSet(bool on){
    if(midiSustain==on)return;
    midiSustain=on;
    if(on){
        // The pedal also holds notes played on the BUILT-IN keyboard
        // (v0.99863). It only deferred MIDI note-offs before, which was
        // not a decision so much as a consequence of where the code sat —
        // and a pedal is a performance control, so it should hold whatever
        // is being played rather than only what arrived over a cable.
        //
        // Driven through the existing noteHeld/heldFreq mechanism (the H
        // key's Hold) rather than a parallel one, so both routes share the
        // same state and the "H:ON" readout stays truthful about what the
        // synth is doing.
        //
        // pedalTookHold remembers whether the pedal is the reason Hold is
        // on: lifting it must not cancel a Hold the player set with the H
        // key themselves.
        if(!noteHeld){
            noteHeld=true;
            heldFreq=(playingFreq>0)?playingFreq:currentFreq;
            midiPedalTookHold=true;
        }
    } else {
        for(int i=0;i<midiSustainedCount;i++)midiNoteOff(midiSustainedNotes[i]);
        midiSustainedCount=0;
        if(midiPedalTookHold){
            noteHeld=false; heldFreq=0.f; midiPedalTookHold=false;
            midiPedalTookHold=false;
        }
    }
}

// ---- CC in (v0.9988) ----
//
// Two assignable slots, each mapping an incoming CC number onto one of the
// existing IMU targets. Reusing ImuTarget rather than inventing a parallel
// list is the whole trick: every target already knows how to apply itself
// through applyImuValue(), already has a name for the menu, already has a
// sensible range, and already writes to an OFFSET so the patch's own
// setting stays the base. A separate CC-target enum would have meant
// duplicating all of that and keeping the two in step forever.
//
// Consequence worth stating: an external knob and the IMU write to the
// same offset, so pointing both at one target means whichever moved last
// wins. That is the honest behaviour for two controls wired to one
// parameter, and it is why the assignments are visible in a menu rather
// than hidden.
// ---- Switch targets (v0.99892) ----
//
// Two more CC slots, but for things that TOGGLE rather than sweep:
// portamento, hold, the arpeggiator and its latch. A knob mapped to one of
// those would be useless, and the CC destination list is built from
// ImuTarget, which by definition only contains continuous parameters — so
// these needed their own small list rather than being forced into it.
//
// Treated as momentary switches: a CC value of 64 or more is "pressed",
// below that is "released", which is what pedals and pads send. The toggle
// fires on the press only, so holding a pad down does not flip the setting
// repeatedly.
enum class MidiSwitchFn : uint8_t { NONE, PORTAMENTO, HOLD, ARP, ARP_LATCH, FN_COUNT };
const char *midiSwitchFnName(MidiSwitchFn f){
    switch(f){
        case MidiSwitchFn::PORTAMENTO: return "Porta";
        case MidiSwitchFn::HOLD:       return "Hold";
        case MidiSwitchFn::ARP:        return "Arp";
        case MidiSwitchFn::ARP_LATCH:  return "Arp Latch";
        default:                       return "--";
    }
}
uint8_t      midiSwNum[2]={80,81};   // GP1/GP2, the general-purpose switches
MidiSwitchFn midiSwFn[2]={MidiSwitchFn::NONE,MidiSwitchFn::NONE};
bool         midiSwWasDown[2]={false,false};

// All four are defined further down; this block sits above
// midiControlChange() because that is where the switches are dispatched.
void portaToggle();
void arpLatchToggle();
void noteHoldToggleFwd();

// Controllers send these two different ways and neither is wrong
// (v0.99893):
//
//   Momentary — a pad or pedal: 127 while held, 0 on release. The press is
//   the event; the release means nothing.
//   Latching — a button in latch mode: 127, then 0 on the NEXT press. The
//   value IS the state, and both edges are events.
//
// Assuming momentary made a latching button need two presses per change:
// the 0 was discarded, so only every other press did anything. Assuming
// latching would have been just as wrong the other way, leaving a pad's
// release to switch things off. So the mode is per slot — a sustain pedal
// on one and a panel button on the other is an ordinary setup.
enum class MidiSwMode : uint8_t { TOGGLE, DIRECT, MODE_COUNT };
MidiSwMode midiSwMode[2]={MidiSwMode::TOGGLE,MidiSwMode::TOGGLE};
const char *midiSwModeName(MidiSwMode m){
    return (m==MidiSwMode::DIRECT)?"Latch":"Moment";
}

// Current state of whatever a slot points at, so DIRECT can compare rather
// than blindly set — that way it still goes through the existing toggle
// functions and keeps all their side effects (capturing the held
// frequency, clearing portaFreq, resetting the arp's step) instead of
// duplicating them.
bool midiSwitchState(MidiSwitchFn f){
    switch(f){
        case MidiSwitchFn::PORTAMENTO: return portaEnabled;
        case MidiSwitchFn::HOLD:       return noteHeld;
        case MidiSwitchFn::ARP:        return arpEnabled;
        case MidiSwitchFn::ARP_LATCH:  return arpLatchEnabled;
        default: return false;
    }
}
void midiSwitchDo(MidiSwitchFn f){
    switch(f){
        case MidiSwitchFn::PORTAMENTO: portaToggle();       break;
        case MidiSwitchFn::HOLD:       noteHoldToggleFwd(); break;
        case MidiSwitchFn::ARP:        arpToggle();         break;
        case MidiSwitchFn::ARP_LATCH:  arpLatchToggle();    break;
        default: break;
    }
    // Ask for the arpeggiator's chord to be rebuilt (v0.99894).
    //
    // arpLatchToggle() empties the latched lists, but the notes the arp is
    // actually PLAYING live in arpHeldFreqs[], which is only rebuilt inside
    // the keyChanged branch of the main loop. Driven from a key that branch
    // runs immediately; driven from MIDI nothing asked for it, so switching
    // Latch off updated the display and left the old chord arpeggiating
    // until any key happened to be pressed. Same shape as the v0.9986 fault
    // — the arp's chord has two ways in and only one of them triggered a
    // rebuild.
    //
    // Called directly for every function here, not just the latch one:
    // they all change what should be sounding (v0.99931 — was a flag set
    // here and polled from loop(); now a direct call, since the whole
    // point of unifying the entry point was to stop needing a flag at
    // all).
    rebuildArpChord();
}

void midiSwitchApply(int slot,uint8_t val){
    bool down=(val>=64);
    if(down==midiSwWasDown[slot])return;   // edge only, either mode
    midiSwWasDown[slot]=down;
    MidiSwitchFn f=midiSwFn[slot];
    if(midiSwMode[slot]==MidiSwMode::DIRECT){
        // The value is the state: bring the setting to match it.
        if(midiSwitchState(f)!=down)midiSwitchDo(f);
    } else {
        if(!down)return;                   // momentary: act on the press
        midiSwitchDo(f);
    }
}

bool     midiCcInEnabled=false;
uint8_t  midiCcInNum[2]={74,71};                 // Cutoff, Resonance
ImuTarget midiCcInTarget[2]={ImuTarget::FILTER_CUTOFF,ImuTarget::RESONANCE};

void midiCcInApply(int slot,uint8_t val){
    ImuTarget t=midiCcInTarget[slot];
    if(t==ImuTarget::NONE)return;
    // 0-127 onto the -1..+1 the targets expect for bipolar parameters, or
    // 0..1 for the rest — imuBipolarAuto() already knows which is which,
    // so a CC behaves like a tilt of the same target rather than needing
    // its own rules. cfg=false because a knob has no axis to inherit an
    // inversion setting from.
    float n=(float)val/127.f;
    float v=imuBipolarAuto(t,false)?(n*2.f-1.f):n;
    applyImuValue(t,v);
}

void midiControlChange(uint8_t cc,uint8_t val){
    // Checked before the fixed assignments below, so a slot pointed at
    // CC1 or CC64 overrides the built-in meaning rather than fighting it.
    if(midiCcInEnabled){
        // Switches first: a slot aimed at a controller should win over a
        // continuous slot aimed at the same number, since the switch is
        // the more specific intent (v0.99892).
        for(int i=0;i<2;i++)
            if(midiSwFn[i]!=MidiSwitchFn::NONE&&cc==midiSwNum[i]){midiSwitchApply(i,val);return;}
        for(int i=0;i<2;i++)if(cc==midiCcInNum[i]){midiCcInApply(i,val);return;}
    }
    switch(cc){
        case CC_MODULATION:
            // Onto the vibrato OFFSET, so the menu's own vibrato setting
            // stays the base and the wheel adds to it — exactly how the
            // IMU targets behave.
            params.vibratoDepthOffsetTarget=(val/127.f)*MIDI_MOD_MAX_VIBRATO;
            break;
        case CC_SUSTAIN:
            midiSustainSet(val>=64);   // the standard threshold
            break;
        case CC_ALL_NOTES_OFF:
        case 120:                      // All Sound Off
            // Worth honouring: this is what a host sends when a player
            // hits panic, and ignoring it leaves notes stuck with no way
            // to clear them short of a reboot.
            midiHeldCount=0; midiSustainedCount=0;
            midiNoteActive=false; currentFreq=0.f;
            rebuildArpChord();
            break;
        default: break;
    }
}

// Program Change selects a morph slot, so an external keyboard or a host
// can change sound — and it MORPHS rather than switching, since that is
// what this synth does. Programs are 1-based on most hardware and 0-based
// in the protocol; program 0 is slot 1 here, which lines up with the
// Shift+1..0 keys.
void midiProgramChange(uint8_t program){
    if(program<NUM_MORPH_SLOTS)morphStart(program);
}

void midiPitchBend(int value14){
    // Centre is 8192. Mapped onto the existing bend range so a MIDI wheel
    // and the local bend keys reach the same place.
    float norm=((float)value14-8192.f)/8192.f;
    keyBendGoal=norm*keyBendMaxCents;
}

#if CPS_USB_MIDI
// USB transport (v0.9961). Everything above this point is transport-free;
// this is the only part that knows how the bytes arrived, which is what
// makes the DIN socket on a MIDI unit a matter of calling
// midiProcessByte() from somewhere else rather than a rewrite.
Adafruit_USBD_MIDI usbMidi;
#endif

// Byte-level parser with running status, which real keyboards rely on.
uint8_t midiStatus=0,midiData[2]={0,0};
int     midiDataIdx=0,midiDataNeeded=0;

// ---- MIDI clock in (v0.9989) ----
//
// Follows an external tempo, which is what makes the sequencer and
// arpeggiator usable alongside other gear rather than only on their own.
//
// The protocol sends 24 clocks per quarter note. Rather than counting
// clocks and stepping on every 24th — which locks the resolution to a
// quarter and drifts if a clock is dropped — this measures the interval
// between clocks and derives a BPM from it. The existing timing code then
// carries on exactly as before, just reading a tempo that happens to come
// from outside. Nothing in the sequencer had to change.
//
// Averaged over 24 clocks (one beat). A single interval is far too noisy:
// serial jitter alone would swing the reading by several BPM, and a tempo
// display flickering by that much looks broken even when the timing is
// fine.
bool  midiClockEnabled=false;
bool  midiClockLocked=false;     // true once a usable tempo has been seen
float midiClockBpm=120.f;
unsigned long midiClockLastMs=0;
unsigned long midiClockAccumUs=0;
int   midiClockCount=0;

// Start/Continue/Stop drive the sequencer transport, so pressing play on
// the master starts this too — which is the point of syncing.
void seqTogglePlayFwd();
bool seqIsPlayingFwd();

void midiClockTick(){
    unsigned long now=micros();
    if(midiClockLastMs!=0){
        unsigned long dt=now-midiClockLastMs;
        // Ignore absurd gaps: the first clock after a pause, or a stall.
        // 24 clocks at 40-300bpm is roughly 8-62ms per clock, so anything
        // outside a generous window is not a tempo, it is a restart.
        if(dt>1000&&dt<200000){
            midiClockAccumUs+=dt;
            if(++midiClockCount>=24){
                float usPerClock=(float)midiClockAccumUs/24.f;
                float bpm=60000000.f/(usPerClock*24.f);
                if(bpm>=20.f&&bpm<=300.f){
                    bpm=constrain(bpm,40.f,240.f);
                    // Smoothed across beats, not applied raw (v0.99932,
                    // raised v0.99933). 0.3 removed the wobble but made a
                    // deliberate tempo change on the master feel sluggish
                    // to follow — the same filter fighting both the noise
                    // and the signal, since it cannot distinguish a real
                    // tempo change from beat-to-beat jitter; it can only
                    // trade how much of each gets through. 0.6 leans
                    // toward following: at 120bpm a full jump settles in
                    // roughly 3-4 beats instead of 7-8, while still
                    // averaging enough beats to keep a held tempo from
                    // visibly wobbling.
                    midiClockBpm=midiClockLocked?(midiClockBpm+(bpm-midiClockBpm)*0.6f):bpm;
                    midiClockLocked=true;
                }
                midiClockAccumUs=0; midiClockCount=0;
            }
        } else {
            midiClockAccumUs=0; midiClockCount=0;
        }
    }
    midiClockLastMs=now;
}

void midiClockReset(){
    midiClockLastMs=0; midiClockAccumUs=0; midiClockCount=0;
    midiClockLocked=false;
}

void midiProcessByte(uint8_t b){
    // Realtime messages can appear BETWEEN the data bytes of another
    // message, so they are handled here and deliberately do not touch
    // midiStatus or midiDataIdx — treating them like any other status
    // byte would corrupt whatever note was mid-transmission.
    if(b>=0xF8){
        if(!midiClockEnabled)return;
        switch(b){
            case 0xF8: midiClockTick(); break;                  // Clock
            case 0xFA:                                          // Start
                midiClockReset();
                if(!seqIsPlayingFwd())seqTogglePlayFwd();
                break;
            case 0xFB:                                          // Continue
                if(!seqIsPlayingFwd())seqTogglePlayFwd();
                break;
            case 0xFC:                                          // Stop
                if(seqIsPlayingFwd())seqTogglePlayFwd();
                midiClockReset();
                break;
            default: break;                                     // 0xFE etc
        }
        return;
    }
    if(b&0x80){                   // status byte
        uint8_t hi=b&0xF0;
        if(hi==MIDI_NOTE_ON||hi==MIDI_NOTE_OFF||hi==MIDI_PITCH_BEND||hi==MIDI_CC){
            midiStatus=b; midiDataIdx=0; midiDataNeeded=2;
        } else if(hi==MIDI_PROGRAM){
            // One data byte, not two (v0.99861). Getting this wrong would
            // not just lose Program Change — the parser would wait for a
            // byte that never comes and swallow the message after it.
            midiStatus=b; midiDataIdx=0; midiDataNeeded=1;
        } else {
            midiStatus=0;         // ignore what we don't handle
        }
        return;
    }
    if(!midiStatus)return;        // data with no status yet
    midiData[midiDataIdx++]=b;
    if(midiDataIdx<midiDataNeeded)return;
    midiDataIdx=0;                // running status: keep midiStatus
    switch(midiStatus&0xF0){
        case MIDI_NOTE_ON:   midiNoteOn(midiData[0],midiData[1]); break;
        case MIDI_NOTE_OFF:  midiNoteOff(midiData[0]);            break;
        case MIDI_PITCH_BEND:midiPitchBend(((int)midiData[1]<<7)|midiData[0]); break;
        case MIDI_CC:        midiControlChange(midiData[0],midiData[1]);       break;
        case MIDI_PROGRAM:   midiProgramChange(midiData[0]);                   break;
    }
}

// Drain whatever has arrived. Called from loop(), not audioTask: MIDI
// timing lives in milliseconds and the audio path must not take on work it
// does not need. Bounded per call so a flood of data — a controller
// sweeping a wheel, say — cannot stall the UI (v0.9961).
// ---- Serial MIDI transport, for the M5Stack Unit MIDI (v0.998) ----
//
// The unit is a plain UART bridge to a pair of DIN sockets (plus a SAM2695
// synth on the other direction, which this does not use). So MIDI IN costs
// one HardwareSerial and nothing else — no USB stack, none of the ~19KB of
// DRAM that made USB MIDI fail in v0.9962, and no reset loop.
//
// Wiring: HY2.0-4P Grove. Black GND, Red 5V, YELLOW is the unit's UART_RX,
// WHITE is the unit's UART_TX. The unit's TX is what carries notes arriving
// at its INPUT socket, so that is what this board must READ.
//
// The unit's DIP switch must be in BYPASS. M5's own description is explicit
// that only in Bypass does the controller's RX pin receive the INPUT
// signal; in Separate that pin does nothing, which would look exactly like
// a broken cable or wrong pin.
//
// Baud is 31250, the MIDI 1.0 standard. M5's spec table says 31520 — that
// is a typo in their docs, repeated in several places. 31250 is correct and
// 31520 would be a 0.9% error, which UARTs tolerate, so the wrong figure
// may even appear to work while being wrong.
// There is only ONE Grove port on this board, and the ToF unit for the
// theremin idea wants it too (I2C). The EXT 2.54-14P header carries its own
// UART, so the two can coexist by moving one of them there — G13/G15 rather
// than G1/G2. Both options are here; define CPS_MIDI_RX_PIN/TX_PIN in
// platformio.ini to override without touching this file.
#ifndef CPS_MIDI_RX_PIN
#define CPS_MIDI_RX_PIN 1    // Grove G1  (EXT alternative: 13)
#endif
#ifndef CPS_MIDI_TX_PIN
#define CPS_MIDI_TX_PIN 2    // Grove G2  (EXT alternative: 15)
#endif
constexpr uint32_t MIDI_BAUD=31250;

HardwareSerial midiSerial(1);   // UART1; UART0 is the console
bool midiSerialReady=false;

// Diagnostics, because the first thing to establish tomorrow is whether
// BYTES are arriving at all — that separates a wiring/pin/DIP problem from
// a parsing one, and they look identical from the outside (silence).
volatile uint32_t midiRxBytes=0;
unsigned long midiLastRxMs=0;

// Which of the Grove pair is RX cannot be settled from documentation, and
// getting it wrong looks exactly like every other failure here: silence.
// So rather than asking someone to edit a constant, rebuild and reflash to
// test the other option, this alternates between the two every few seconds
// until a byte actually arrives, then locks onto whichever pin delivered it
// (v0.9981). Costs nothing once data is flowing, and turns a
// rebuild-and-guess cycle into waiting a few seconds.
const int MIDI_PIN_CANDIDATES[2][2]={
    {CPS_MIDI_RX_PIN,CPS_MIDI_TX_PIN},   // Grove G1 as RX
    {CPS_MIDI_TX_PIN,CPS_MIDI_RX_PIN},   // swapped
};
int  midiPinChoice=0;
bool midiPinLocked=false;
unsigned long midiPinTryMs=0;

// Suspend/resume for the Theremin's benefit (v0.99910): the UART and the
// I2C bus cannot both own GPIO1/2 at once, so Theremin releases this
// before claiming the pins and restores it when switched off.
void midiSerialBeginWith(int choice);   // defined just below

void midiSerialSuspend(){
    if(!midiSerialReady)return;
    midiSerial.end();
    midiSerialReady=false;
}
void midiSerialResume(){
    if(midiSerialReady)return;
    midiSerialBeginWith(midiPinChoice);
}

void midiSerialBeginWith(int choice){
    midiSerial.end();
    midiSerial.begin(MIDI_BAUD,SERIAL_8N1,
        MIDI_PIN_CANDIDATES[choice][0],MIDI_PIN_CANDIDATES[choice][1]);
    midiSerialReady=true;
    midiPinTryMs=millis();
    Serial.printf("[MIDI] listening on RX=%d TX=%d @%u baud\n",
        MIDI_PIN_CANDIDATES[choice][0],MIDI_PIN_CANDIDATES[choice][1],
        (unsigned)MIDI_BAUD);
}

void midiSerialBegin(){
    midiSerialBeginWith(0);
    Serial.println("[MIDI] Unit MIDI checklist: DIP switch = BYPASS,");
    Serial.println("[MIDI]   and the Grove 5V direction switch must be set");
    Serial.println("[MIDI]   to POWER OUT, or the unit gets no power at all.");
    Serial.println("[MIDI] RX pin auto-swaps every 3s until bytes arrive.");
}

// Called from the poll loop while nothing has been received yet.
// Bounded, and suspended while anything is transmitting (v0.99896).
//
// This was written to find which Grove pin is RX and then get out of the
// way, but it had no way to stop when nothing ever arrives — and nothing
// ever arrives with the unit's DIP in Separate, where the RX pin is not
// connected at all. So it swapped forever, every three seconds, and each
// swap calls midiSerial.end()/begin(): the UART is torn down mid-send and
// the TX pin moves with it. Transmission worked roughly half the time, in
// three-second slices. That is exactly how it presented — a sequencer that
// responded sometimes and never followed the tempo.
//
// Two limits now. It never runs while any send feature is on, because
// tearing down the UART to look for input is not worth breaking output
// for. And it gives up after 30 seconds, settling on choice 0 — G1 as RX,
// which is what the hardware actually uses.
// Declared with the clock-out code further down, which needs the
// sequencer state that is itself declared after this point.
extern bool midiClockOutEnabled;

constexpr unsigned long MIDI_PIN_SEARCH_MS=30000;
unsigned long midiPinSearchStartMs=0;
bool midiPinSearchDone=false;

void midiPinAutoTry(){
    if(!midiSerialReady)return;   // suspended, e.g. for the Theremin (v0.99910)
    if(midiPinLocked||midiPinSearchDone||midiRxBytes>0)return;
    if(midiNoteOutEnabled||midiCcOutEnabled||midiClockOutEnabled)return;
    if(midiPinSearchStartMs==0)midiPinSearchStartMs=millis();
    if(millis()-midiPinSearchStartMs>MIDI_PIN_SEARCH_MS){
        midiPinSearchDone=true;
        if(midiPinChoice!=0){midiPinChoice=0;midiSerialBeginWith(0);}
        Serial.println("[MIDI] no input seen - settled on RX=1 (send unaffected)");
        return;
    }
    if(millis()-midiPinTryMs<3000)return;
    midiPinChoice^=1;
    midiSerialBeginWith(midiPinChoice);
}

void midiPoll(){
#if CPS_USB_MIDI
    int guard=256;
    while(usbMidi.available()&&guard-->0)midiProcessByte((uint8_t)usbMidi.read());
#endif
    if(!midiSerialReady)return;
    // Bounded per call for the same reason as the USB path: a controller
    // sweeping a wheel sends a lot, and the UI must not stall behind it.
    int guard2=256;
    while(midiSerial.available()&&guard2-->0){
        uint8_t b=(uint8_t)midiSerial.read();
        if(!midiPinLocked){
            midiPinLocked=true;
            Serial.printf("[MIDI] data on RX=%d - locked\n",
                MIDI_PIN_CANDIDATES[midiPinChoice][0]);
        }
        midiRxBytes++; midiLastRxMs=millis();
        // Raw dump of everything that is NOT Active Sensing (v0.9982).
        //
        // The first hardware test received a steady four bytes a second
        // whether or not a key was pressed, which is what Active Sensing
        // alone looks like — 0xFE every ~250ms, exactly what a Roland
        // sends to say it is still there. So the cable, the unit, the DIP
        // switch, the pin and the baud rate are all proven correct, and
        // the notes are simply not being transmitted.
        //
        // Filtering out 0xFE leaves a log that is silent until something
        // real arrives, so pressing one key either prints bytes or prints
        // nothing — and that distinguishes "the keyboard is not sending"
        // from "the parser is not understanding" without guessing.
        midiProcessByte(b);
    }
    midiPinAutoTry();
}

// Printed once a second while data is flowing, and once when it stops.
// Deliberately not per byte: at 31250 baud a held chord would flood the log
// and the flooding itself would change the timing being measured.
void midiDiagTick(){
    static uint32_t lastCount=0;
    static unsigned long lastMs=0;
    static bool wasActive=false;
    unsigned long now=millis();
    if(now-lastMs<1000)return;
    lastMs=now;
    uint32_t c=midiRxBytes;
    if(c!=lastCount){
#if CPS_LOG_MIDI
        Serial.printf("[MIDI] rx %u bytes total (+%u)\n",
            (unsigned)c,(unsigned)(c-lastCount));
#endif
        lastCount=c; wasActive=true;
    } else if(wasActive){
#if CPS_LOG_MIDI
        Serial.println("[MIDI] idle");
#endif
        wasActive=false;
    }
}

// Force-retriggers the envelope for this step, even if the frequency
// happens to repeat — that's what gives the arpeggio its percussive,
// stepped character rather than a smooth glide between notes.
void triggerArpStep(float freq){
    currentFreq=freq;
    if(envPhase==EnvPhase::IDLE)envLevel=0.f;
    envPhase=EnvPhase::ATTACK;
    filterEnvPhase=EnvPhase::ATTACK;
    if(portaEnabled&&portaFreq<=0.f)portaFreq=freq;
}

// Rate: note length each arp step represents, relative to BPM's quarter
// note (e.g. Rate=1/8 means 2 steps per beat instead of 1).
struct ArpRateOption { const char *label; float mult; };
const ArpRateOption ARP_RATES[]={
    {"1/1",  4.0f},{"1/2",  2.0f},{"1/4",  1.0f},{"1/8",  0.5f},
    {"1/16", 0.25f},{"1/32",0.125f},{"1/8T", 1.f/3.f},{"1/16T",1.f/6.f},
};
constexpr int NUM_ARP_RATES=sizeof(ARP_RATES)/sizeof(ARP_RATES[0]);
int arpRateIndex=2; // default 1/4, matching the original fixed behavior

float arpLastTriggeredFreq=0.f; // for MAIN screen: which held note is currently sounding

void updateArpTiming(){
    if(arpHeldCount==0){
        // Same exception as the plain note path (v0.9983): with the
        // arpeggiator on and no chord held locally, this ran every loop
        // and zeroed currentFreq — which silenced MIDI notes continuously
        // rather than just on a key event.
        if(currentFreq!=0.f&&!midiNoteActive)currentFreq=0.f; // let it release naturally
        return;
    }
    unsigned long now=millis();
    // External clock overrides the local tempo when it is running
    // (v0.9989). The IMU/CC offset still applies on top, so a tilt can
    // push against the incoming tempo — which is a deliberate effect
    // rather than a conflict, and costs nothing to allow.
    float baseBpm=(midiClockEnabled&&midiClockLocked)?midiClockBpm:arpTempoBpm;
    float bpm=constrain(baseBpm+arpTempoOffset,40.f,240.f);
    float baseStepMs=60000.0f/bpm*ARP_RATES[arpRateIndex].mult;
    float swingFactor=constrain(arpSwing+arpSwingOffset,-100.f,100.f)/100.f;
    bool isOffBeat=(arpStepIndex%2==1);
    // Positive Swing delays the off-beat step (classic long-short swing
    // feel); negative pushes it earlier instead (anticipated/"pushed" feel).
    float stepMs=isOffBeat?baseStepMs*(1.f+swingFactor*0.5f):baseStepMs*(1.f-swingFactor*0.5f);
    // The [arpTiming] diagnostic that used to sit here (v0.9997x) is
    // retired (UI/UX diagnostic pass) — it did its job: confirmed
    // stepMs/bpm/arpRateIndex stay healthy even during a freeze, which
    // ruled out this function as the cause and pointed to the actual
    // one — the Cardputer ADV's TCA8418 keyboard chip, a hardware/library
    // quirk outside this file, mitigated by the keyboard watchdog rather
    // than fixable here. No ongoing reason to print this every second.
    if(now-arpLastStepMs>=(unsigned long)stepMs){
        arpLastStepMs=now;
        int idx=nextArpIndex();
        bool pitchOrdered=(arpType==ArpType::UP||arpType==ArpType::DOWN||arpType==ArpType::UP_DOWN);
        float freq=pitchOrdered?arpSortedFreqs[idx]:arpHeldFreqs[idx];
        triggerArpStep(freq);
        arpLastTriggeredFreq=freq;
        arpStepIndex++;
    }
}

// ---------------------------------------------------------
// Step Sequencer (16 steps, CardputerADV only for hardware auto-detect
// purposes only — the Sequencer itself works on both boards). Has its
// own independent Tempo/Swing, separate from the Arpeggiator's.
// ---------------------------------------------------------
struct SeqStep {
    float freq=0.f;       // 0 = rest
    uint8_t velocity=100; // base velocity; Accent boosts this further
    bool tie=false;       // extend the previous note — no retrigger, no pitch change
    bool slide=false;     // glide from the previous pitch to this one — no retrigger
    bool accent=false;    // boost velocity + filter cutoff for this step
};
constexpr int SEQ_NUM_STEPS=16;
SeqStep seqSteps[SEQ_NUM_STEPS];
int  seqCursorStep=0;    // which step is being edited
int  seqPlayStep=0;      // current playback position
bool seqPlaying=false;
// Two separate editing "focuses": STEP (this step's Velocity/Gate) and
// PATTERN (the whole sequence's Tempo/Swing) — kept conceptually separate
// per user feedback, rather than one flat 4-way cycle. 'b' toggles focus;
// 'g' cycles the 2-way choice within whichever focus is currently active.
// All four values (Vel/Gate/Tempo/Swing) are always shown on screen
// regardless of focus; only the active one is marked.
enum class SeqFocus : uint8_t { STEP, PATTERN };
SeqFocus seqFocus=SeqFocus::STEP;
enum class SeqStepTarget : uint8_t { VELOCITY, TIE, SLIDE, ACCENT };
SeqStepTarget seqStepTarget=SeqStepTarget::VELOCITY;
enum class SeqPatternTarget : uint8_t { TEMPO, SWING };
SeqPatternTarget seqPatternTarget=SeqPatternTarget::TEMPO;
bool prevSeqFocusKeyPressed=false;
unsigned long seqLastStepMs=0;
float prevSeqEntryFreq=0.f;
bool prevSeqDelPressed=false;
bool prevSeqCursorLeftPressed=false, prevSeqCursorRightPressed=false;
bool prevSeqVelIncPressed=false, prevSeqVelDecPressed=false;
bool prevSeqGateKeyPressed=false, prevSeqPlayKeyPressed=false;
float seqTempoBpm=120.0f; // 40-240, independent of Arp's Tempo
uint8_t seqLastUsedVelocity=100; // carries forward to new note entries, so you're not stuck starting at 100 every time

// Song playback (declared early — updateSeqTiming() below reads these to
// apply per-entry Transpose and detect when to advance to the next
// entry; the rest of Song's state/logic lives further down near its
// data model and editor UI).
bool  songPlaying=false;
float songTransposeMult=1.0f; // current entry's chromatic Transpose, applied on top of each step's stored freq
void  songAdvanceOnPassComplete(); // forward declaration — defined near the rest of Song's playback logic below

// Copy/Cut/Paste: 'V' marks/confirms a step-range selection, Shift+C
// copies it, Shift+X cuts it, Enter pastes at the cursor. seqSelStart/End
// are -1 when there's no selection; while actively marking (seqSelMarking)
// they track [min(anchor,cursor), max(anchor,cursor)] live as the cursor
// moves, then freeze in place once confirmed.
int  seqSelAnchor=-1, seqSelStart=-1, seqSelEnd=-1;
bool seqSelMarking=false;
SeqStep seqClipboard[SEQ_NUM_STEPS];
int  seqClipboardLen=0;
bool prevSeqMarkKeyPressed=false, prevSeqCopyKeyPressed=false, prevSeqCutKeyPressed=false, prevSeqPasteKeyPressed=false;
float seqSwing=0.0f;      // -100 to +100%, independent of Arp's Swing

// Forwarders for the MIDI clock handler, which sits above this (v0.9989).
bool seqIsPlayingFwd(){return seqPlaying;}
void seqTogglePlayFwd();

void seqTogglePlay(){
    seqPlaying=!seqPlaying;
    if(seqPlaying){
        seqPlayStep=0;
        seqLastStepMs=millis();
    } else {
        currentFreq=0.f;
        seqSliding=false;
        seqAccentCutoffBoostTarget=0.f;
        seqAccentResoBoostTarget=0.f;
        seqVelocityMult=1.0f;
    }
}

void seqTogglePlayFwd(){seqTogglePlay();}

// ---- MIDI clock out (v0.99895) ----
//
// The counterpart to clock in: other gear follows C.P.S.'s tempo instead
// of setting it. Placed here rather than with the other send code because
// it needs seqPlaying and the tempos, which are declared just above.
//
// Note this only reaches external gear with the unit's DIP in Separate —
// in Bypass the controller's TX goes to the SAM2695 alone. The chip
// ignores clock, so sending it there is harmless, just pointless.
bool midiClockOutEnabled=false;
unsigned long midiClockOutNextUs=0;
bool midiClockOutWasPlaying=false;

void midiSendRealtime(uint8_t b){
    if(!midiSerialReady)return;
    midiSerial.write(&b,1);
}

void midiClockOutUpdate(){
    if(!midiClockOutEnabled){midiClockOutNextUs=0;return;}
    // Never generate a clock while following one: two masters on a wire is
    // not a tempo, it is a fight. Clock in wins because something else
    // asked for it explicitly.
    if(midiClockEnabled&&midiClockLocked){midiClockOutNextUs=0;return;}

    // Transport, so pressing play here starts the other machine — the same
    // courtesy clock in extends to us.
    if(seqPlaying!=midiClockOutWasPlaying){
        midiClockOutWasPlaying=seqPlaying;
        midiSendRealtime(seqPlaying?0xFA:0xFC);
        if(seqPlaying)midiClockOutNextUs=micros();
    }

    // Whichever tempo is actually driving: the sequencer's while it plays,
    // the arpeggiator's otherwise.
    float bpm=constrain(seqPlaying?(seqTempoBpm+seqTempoOffset)
                                  :(arpTempoBpm+arpTempoOffset),40.f,240.f);
    unsigned long usPerClock=(unsigned long)(60000000.f/(bpm*24.f));
    unsigned long now=micros();
    if(midiClockOutNextUs==0){midiClockOutNextUs=now;return;}

    // Advance the deadline by exactly one interval rather than resetting it
    // to now, or every late call would push the tempo permanently flat.
    int guard=0;
    while((long)(now-midiClockOutNextUs)>=0&&guard++<8){
        midiSendRealtime(0xF8);
        midiClockOutNextUs+=usPerClock;
    }
    // If it fell far behind — a long redraw, a card write — give up on the
    // missed clocks instead of firing a burst that would sound like a
    // stumble at the receiving end.
    if(guard>=8)midiClockOutNextUs=now+usPerClock;
}

// Reuses the existing note-key tables directly (not resolveFreqFromKeys(),
// since that treats Backspace as a 13th note — here Backspace instead
// means "clear the selected step", handled separately below).
float seqResolveFreqExcludingDel(){
    auto s=M5Cardputer.Keyboard.keysState();
    float mult=powf(2.f,(float)params.octaveShift)*powf(2.f,(float)transposeSemitones/12.f);
    for(auto it=s.word.rbegin();it!=s.word.rend();++it){
        float base;
        if(noteKeyBaseFreq(*it,base))return base*mult;
    }
    return 0.f;
}

bool prevSeqOctUpPressed=false, prevSeqOctDownPressed=false;
bool prevSeqTrUpPressed=false, prevSeqTrDownPressed=false;

void updateSeqEditing(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool curL=false,curR=false,vInc=false,vDec=false,gateKey=false,playKey=false,focusKey=false;
    bool octUp=false,octDown=false,trUp=false,trDown=false;
    bool markKey=false,copyKey=false,cutKey=false;
    bool pasteKey=s.enter; // Enter: paste clipboard at cursor (checked directly, not in the s.word loop below, since Enter alone wouldn't otherwise populate s.word)
    for(char c:s.word){
        if(s.shift){
            // Shift + the same physical keys PLAY uses for Octave/Transpose,
            // so the muscle memory carries over even though SEQ needs
            // those same keys (unshifted) for its own step/vel/gate controls.
            if(c==';')octUp=true;   if(c=='.')octDown=true;
            if(c=='/')trUp=true;    if(c==',')trDown=true;
            if(c=='c'||c=='C')copyKey=true; // Shift+C: copy selection
            if(c=='x'||c=='X')cutKey=true;  // Shift+X: cut selection
        } else {
            if(c==',')curL=true;    if(c=='/')curR=true;
            if(c==';')vInc=true;    if(c=='.')vDec=true;
            if(c=='v')markKey=true; // V: mark/confirm/clear a step selection
        }
        // Defensive fallback in case the keyboard reports the shifted
        // symbol directly instead of a separate shift flag + base char.
        if(c==':')octUp=true; if(c=='>')octDown=true;
        if(c=='?')trUp=true;  if(c=='<')trDown=true;
        if(c=='g')gateKey=true;
        if(c=='f')focusKey=true; // toggle STEP <-> PATTERN focus
        if(c==' ')playKey=true;
    }

    if(curL&&!prevSeqCursorLeftPressed) seqCursorStep=(seqCursorStep-1+SEQ_NUM_STEPS)%SEQ_NUM_STEPS;
    if(curR&&!prevSeqCursorRightPressed)seqCursorStep=(seqCursorStep+1)%SEQ_NUM_STEPS;
    prevSeqCursorLeftPressed=curL;prevSeqCursorRightPressed=curR;

    // Copy/Cut/Paste/Mark: kept independent of STEP/PATTERN focus, since
    // selection is inherently about steps regardless of what ;/. adjusts.
    if(seqSelMarking){
        seqSelStart=min(seqSelAnchor,seqCursorStep);
        seqSelEnd=max(seqSelAnchor,seqCursorStep);
    }
    if(markKey&&!prevSeqMarkKeyPressed){
        if(seqSelAnchor<0){
            // No selection yet — start marking from here.
            seqSelAnchor=seqCursorStep; seqSelStart=seqSelEnd=seqCursorStep; seqSelMarking=true;
        } else if(seqSelMarking){
            // Currently marking — confirm/freeze the current range.
            seqSelMarking=false;
        } else {
            // Already confirmed — clear it.
            seqSelAnchor=seqSelStart=seqSelEnd=-1;
        }
    }
    prevSeqMarkKeyPressed=markKey;

    if(copyKey&&!prevSeqCopyKeyPressed&&seqSelStart>=0){
        seqClipboardLen=seqSelEnd-seqSelStart+1;
        for(int i=0;i<seqClipboardLen;i++)seqClipboard[i]=seqSteps[seqSelStart+i];
    }
    prevSeqCopyKeyPressed=copyKey;

    if(cutKey&&!prevSeqCutKeyPressed&&seqSelStart>=0){
        seqClipboardLen=seqSelEnd-seqSelStart+1;
        for(int i=0;i<seqClipboardLen;i++){
            seqClipboard[i]=seqSteps[seqSelStart+i];
            seqSteps[seqSelStart+i]=SeqStep();
        }
    }
    prevSeqCutKeyPressed=cutKey;

    if(pasteKey&&!prevSeqPasteKeyPressed&&seqClipboardLen>0){
        int n=min(seqClipboardLen,SEQ_NUM_STEPS-seqCursorStep); // truncate at step 16
        for(int i=0;i<n;i++)seqSteps[seqCursorStep+i]=seqClipboard[i];
    }
    prevSeqPasteKeyPressed=pasteKey;

    if(focusKey&&!prevSeqFocusKeyPressed)seqFocus=(seqFocus==SeqFocus::STEP)?SeqFocus::PATTERN:SeqFocus::STEP;
    prevSeqFocusKeyPressed=focusKey;

    if(gateKey&&!prevSeqGateKeyPressed){
        if(seqFocus==SeqFocus::STEP)seqStepTarget=(SeqStepTarget)(((uint8_t)seqStepTarget+1)%4);
        else                        seqPatternTarget=(seqPatternTarget==SeqPatternTarget::TEMPO)?SeqPatternTarget::SWING:SeqPatternTarget::TEMPO;
    }
    prevSeqGateKeyPressed=gateKey;

    if(seqFocus==SeqFocus::STEP){
        SeqStep &st=seqSteps[seqCursorStep];
        switch(seqStepTarget){
            case SeqStepTarget::VELOCITY:
                // Step 5->1, hold-to-repeat added (v0.9996x) — same
                // request as global Volume and ARP/SEQ/SONG's Tempo/
                // Swing. Reuses menuUpHeldMs/menuDownHeldMs, the pair
                // that actually shares ';'/'.' with vInc/vDec here (see
                // the Tempo/Swing case below in this same function for
                // why the OTHER pair, menuIncHeldMs/menuDecHeldMs, would
                // be wrong).
                if(menuKeyFire(vInc,prevSeqVelIncPressed,menuUpHeldMs,menuUpLastMs)){st.velocity=min((int)st.velocity+1,100);seqLastUsedVelocity=st.velocity;}
                if(menuKeyFire(vDec,prevSeqVelDecPressed,menuDownHeldMs,menuDownLastMs)){st.velocity=max((int)st.velocity-1,0);seqLastUsedVelocity=st.velocity;}
                break;
            case SeqStepTarget::TIE:
                if((vInc&&!prevSeqVelIncPressed)||(vDec&&!prevSeqVelDecPressed))st.tie=!st.tie;
                break;
            case SeqStepTarget::SLIDE:
                if((vInc&&!prevSeqVelIncPressed)||(vDec&&!prevSeqVelDecPressed))st.slide=!st.slide;
                break;
            case SeqStepTarget::ACCENT:
                if((vInc&&!prevSeqVelIncPressed)||(vDec&&!prevSeqVelDecPressed))st.accent=!st.accent;
                break;
        }
    } else {
        // Step 5->1, hold-to-repeat added (v0.9996x, corrected). Reusing
        // menuIncHeldMs/menuDecHeldMs was WRONG — those track '/' and ','
        // (updateMenuNavigation()'s mI/mDe), not ';' and '.', which is
        // what vInc/vDec actually are here. updateMenuNavigation() runs
        // unconditionally every frame regardless of appMode and clears
        // menuIncHeldMs whenever '/' isn't down — which while holding ';'
        // for SEQ's Tempo is always, so it zeroed the timer out from
        // under this on literally the next frame: the initial press fired
        // (menuKeyFire's own !prev branch), nothing after did. Correct
        // pairing is menuUpHeldMs (tracks ';', same key as vInc) and
        // menuDownHeldMs (tracks '.', same key as vDec) — the reset logic
        // stays in sync because it is now watching the actual key being
        // held, not an unrelated one.
        if(seqPatternTarget==SeqPatternTarget::TEMPO){
            if(menuKeyFire(vInc,prevSeqVelIncPressed,menuUpHeldMs,menuUpLastMs))seqTempoBpm=min(seqTempoBpm+1.f,240.f);
            if(menuKeyFire(vDec,prevSeqVelDecPressed,menuDownHeldMs,menuDownLastMs))seqTempoBpm=max(seqTempoBpm-1.f,40.f);
        } else {
            if(menuKeyFire(vInc,prevSeqVelIncPressed,menuUpHeldMs,menuUpLastMs))seqSwing=min(seqSwing+1.f,100.f);
            if(menuKeyFire(vDec,prevSeqVelDecPressed,menuDownHeldMs,menuDownLastMs))seqSwing=max(seqSwing-1.f,-100.f);
        }
    }
    prevSeqVelIncPressed=vInc;prevSeqVelDecPressed=vDec;

    if(octUp&&!prevSeqOctUpPressed&&params.octaveShift<2)     params.octaveShift++;
    if(octDown&&!prevSeqOctDownPressed&&params.octaveShift>-2) params.octaveShift--;
    prevSeqOctUpPressed=octUp;prevSeqOctDownPressed=octDown;
    if(trUp&&!prevSeqTrUpPressed&&transposeSemitones<TRANSPOSE_MAX)    transposeSemitones++;
    if(trDown&&!prevSeqTrDownPressed&&transposeSemitones>TRANSPOSE_MIN)transposeSemitones--;
    prevSeqTrUpPressed=trUp;prevSeqTrDownPressed=trDown;

    if(playKey&&!prevSeqPlayKeyPressed)seqTogglePlay();
    prevSeqPlayKeyPressed=playKey;

    // Backspace/del clears the selected step entirely (note + tie/slide/
    // accent flags) back to a plain rest. Shift+Backspace clears the
    // whole 16-step pattern at once.
    if(s.del&&!prevSeqDelPressed){
        if(s.shift){
            for(int i=0;i<SEQ_NUM_STEPS;i++)seqSteps[i]=SeqStep();
        } else {
            seqSteps[seqCursorStep]=SeqStep();
        }
    }
    prevSeqDelPressed=s.del;

    // Any note key press assigns that pitch to the selected step, plays
    // a brief preview so you can hear what you're entering, and auto-
    // advances the cursor to the next step (use ,// to skip a step and
    // leave it as a rest, rather than pressing a note key for it).
    // MIDI notes enter steps too (v0.99891). seqResolveFreqExcludingDel()
    // reads the built-in keyboard only, so step entry silently ignored an
    // external keyboard — the one input where being able to play the pitch
    // you want, in the octave you want, matters most, and where the
    // built-in three-key limit is least relevant.
    //
    // Local keys take precedence when both are down, matching the rule the
    // note path already uses: whoever is actually holding a key is
    // playing. The velocity of the MIDI note is used as the step's
    // velocity, since a keyboard that sends it is expressing exactly what
    // that step should be.
    float curFreq=seqResolveFreqExcludingDel();
    bool fromMidi=false;
    if(curFreq<=0.f&&midiHeldCount>0){
        curFreq=midiNoteToHz(midiHeldNotes[midiHeldCount-1]);
        fromMidi=true;
    }
    if(curFreq>0.f&&curFreq!=prevSeqEntryFreq){
        seqSteps[seqCursorStep].freq=curFreq;
        seqSteps[seqCursorStep].velocity=fromMidi
            ? (int)constrain((int)(seqVelocityMult*100.f),1,100)
            : seqLastUsedVelocity;
        if(!seqPlaying){
            currentFreq=curFreq;
            portaFreq=curFreq; // preview should be instantly accurate, not mid-glide
            seqVelocityMult=1.0f;
            if(envPhase==EnvPhase::IDLE)envLevel=0.f;
            envPhase=EnvPhase::ATTACK;
            filterEnvPhase=EnvPhase::ATTACK;
        }
        seqCursorStep=(seqCursorStep+1)%SEQ_NUM_STEPS;
    } else if(curFreq<=0.f&&prevSeqEntryFreq>0.f&&!seqPlaying){
        // Key released — stop the preview.
        currentFreq=0.f;
    }
    prevSeqEntryFreq=curFreq;
}

void updateSeqTiming(){
    if(!seqPlaying)return;
    unsigned long now=millis();
    float seqBase=(midiClockEnabled&&midiClockLocked)?midiClockBpm:seqTempoBpm;
    float bpm=constrain(seqBase+seqTempoOffset,40.f,240.f);   // v0.9989
    float baseStepMs=60000.0f/bpm/4.0f; // 16th notes: 16 steps = one bar at this tempo
    float swingFactor=constrain(seqSwing+seqSwingOffset,-100.f,100.f)/100.f;
    bool isOffBeat=(seqPlayStep%2==1);
    float stepMs=isOffBeat?baseStepMs*(1.f+swingFactor*0.5f):baseStepMs*(1.f-swingFactor*0.5f);
    if(now-seqLastStepMs>=(unsigned long)stepMs){
        seqLastStepMs=now;
        SeqStep &st=seqSteps[seqPlayStep];
        float tMult=songPlaying?songTransposeMult:1.0f; // Song's per-entry chromatic Transpose; 1.0 (no-op) outside Song playback
        if(st.tie&&currentFreq>0.f){
            // Extend the currently-sounding note — no retrigger, no pitch
            // change, no envelope reset. Checked before the Rest check
            // below so a Tie step doesn't need its own note assigned —
            // it just continues whatever's already sounding.
        } else if(st.freq<=0.f){
            // Rest — silence, let the amp envelope's own Release handle the tail.
            currentFreq=0.f;
            seqSliding=false;
            seqAccentCutoffBoostTarget=0.f;
            seqAccentResoBoostTarget=0.f;
        } else {
            float vel=st.velocity;
            if(st.accent)vel=min(100.f,vel*SEQ_ACCENT_VELOCITY_MULT);
            seqVelocityMult=constrain(vel/100.f,0.f,1.f);
            seqAccentCutoffBoostTarget=st.accent?SEQ_ACCENT_CUTOFF_BOOST:0.f;
            seqAccentResoBoostTarget=st.accent?SEQ_ACCENT_RESO_BOOST:0.f;
            if(st.slide&&currentFreq>0.f){
                // Glide from the current pitch to the new one — no
                // retrigger, so the envelope/amplitude just continues.
                seqSlideFreq=currentFreq;
                seqSliding=true;
                currentFreq=st.freq*tMult;
            } else {
                // Normal note-on (or a tie/slide with nothing previously
                // sounding to extend/glide from) — full retrigger.
                seqSliding=false;
                currentFreq=st.freq*tMult;
                if(envPhase==EnvPhase::IDLE)envLevel=0.f;
                envPhase=EnvPhase::ATTACK;
                filterEnvPhase=EnvPhase::ATTACK;
            }
        }
        seqPlayStep=(seqPlayStep+1)%SEQ_NUM_STEPS;
        if(songPlaying&&seqPlayStep==0)songAdvanceOnPassComplete();
    }
}

// ---- Tap Tempo (v0.9993x) ----
//
// Shift+Enter, tracked here rather than inside a menu, so it works from
// PLAY and SEQ without spending one of the few keys those screens still
// have free — the request that made this worth doing. Sets whichever
// tempo is actually in use: seqTempoBpm while SEQ is playing, arpTempoBpm
// otherwise, matching how the two are already independent everywhere
// else in this firmware.
//
// Each tap after the first computes an instantaneous BPM from the gap
// since the last one and blends it into a running average — a single
// gap is accepted immediately (there is nothing to average yet), but a
// human tapping a beat is never perfectly even, and averaging the last
// few intervals is what every hardware tap-tempo button actually does.
// A tap more than 2 seconds after the last one starts a new average
// rather than blending against a stale one — indistinguishable from
// deciding to tap a new, much slower tempo otherwise.
bool  tapTempoActive=false;
unsigned long tapTempoLastMs=0;
float tapTempoAvgMs=0.f;
int   tapTempoCount=0;

void tapTempoHit(){
    unsigned long now=millis();
    if(tapTempoActive&&now-tapTempoLastMs<2000){
        float gapMs=(float)(now-tapTempoLastMs);
        tapTempoAvgMs=(tapTempoCount==0)?gapMs:(tapTempoAvgMs+(gapMs-tapTempoAvgMs)*0.35f);
        tapTempoCount++;
        float bpm=constrain(60000.f/tapTempoAvgMs,40.f,240.f);
        if(seqPlaying)seqTempoBpm=bpm; else arpTempoBpm=bpm;
    } else {
        tapTempoCount=0;   // first tap of a new sequence: nothing to average yet
    }
    tapTempoActive=true;
    tapTempoLastMs=now;
}

void updateOctaveAndVolume(){
    if(appMode==AppMode::PATCH||appMode==AppMode::PATTERN||appMode==AppMode::TIMBRE)return;
    auto s=M5Cardputer.Keyboard.keysState();
    // Edge-triggered: firing on every frame the key is held would average
    // garbage. Static because this function's `s` is a fresh local each
    // call, so the previous frame's press state has to live outside it.
    static bool prevTapKeyPressed=false;
    bool tapKeyNow=s.shift&&s.enter;
    if(tapKeyNow&&!prevTapKeyPressed)tapTempoHit();
    prevTapKeyPressed=tapKeyNow;
    bool oU=false,oD=false,bD=false,bU=false;   // vU/vD moved to updateVolumeRepeat() (v0.9996x)
    bool iXH=false,iYH=false,nH=false,pOn=false,hKey=false,seqPlayKey=false;
    // v0.9921: shifted variants of the IMU hold keys disable/enable the
    // axis itself, and Shift+H latches the help overlay open.
    bool iXEn=false,iYEn=false,hLatchKey=false;
    int  morphKeySlot=-1;   // v0.995: set by Shift+1..0 below
    bool trU=false,trD=false,latchKey=false,arpToggleKey=false;
    for(char c:s.word){
        if(isCardputerAdv){
            // ';'/'.'/'/'/',' double as SEQ's own step-cursor/velocity/gate
            // keys, so only let them mean octave/transpose while on PLAY.
            if(appMode==AppMode::PLAY){
                if(c==';')oU=true;  if(c=='.')oD=true;
                if(c=='/')trU=true; if(c==',')trD=true;
            }
            if(s.shift&&c=='v')arpToggleKey=true; // Shift+V: Arp on/off, from anywhere except Patch
            else if(c=='v'&&appMode!=AppMode::SEQ)latchKey=true; // V (unshifted): Arp latch toggle — in SEQ, plain V instead marks/confirms a step selection (see updateSeqEditing())
            if(c=='V')arpToggleKey=true;          // defensive: in case shifted letters are reported uppercase directly
        } else {
            // Original Cardputer: ;/./,// are reserved for PAD (virtual
            // tilt) control instead, so octave/transpose move to keys
            // that don't collide with that — nor with SEQ's own keys.
            if(c=='j')oU=true;  if(c=='n')oD=true;
            if(c=='m')trU=true; if(c=='b')trD=true;
        }
        // Shift+L/Shift+S are reserved for SONG's Load/Save (see
        // updateSongEditor()) — skip the plain volume/IMU-hold meaning
        // there so they don't also fire alongside Load/Save.
        // Volume itself moved out to updateVolumeRepeat() (v0.9996x) —
        // no longer collected here at all; see that function.
        // Shift+C/Shift+X are reserved for SEQ's Copy/Cut (see
        // updateSeqEditing()) — skip the plain portamento/bend meaning
        // there so they don't also fire alongside Copy/Cut.
        if(c=='z')bD=true;
        if(c=='x'&&!(appMode==AppMode::SEQ&&s.shift))bU=true;
        // Shift+A / Shift+S switch that IMU axis off entirely, the same
        // shape as Shift+V for the arpeggiator. Unshifted stays the hold
        // toggle it has always been.
        //
        // The keyboard reports the SHIFTED character, so a shifted A
        // arrives as 'A' and not as 'a' with s.shift set — testing s.shift
        // alone never fired (v0.9922). Shift+V had a note about exactly
        // this and a defensive uppercase test; that lesson did not get
        // carried over here. Both forms are accepted now.
        if(c=='A'||(c=='a'&&s.shift))iXEn=true;
        else if(c=='a')iXH=true;
        if(!(appMode==AppMode::SONG&&s.shift)){
            if(c=='S'||(c=='s'&&s.shift))iYEn=true;
            else if(c=='s')iYH=true;
        }
        if(c=='d')nH=true;
        if(c=='c'&&!(appMode==AppMode::SEQ&&s.shift))pOn=true;
        // H alone still shows the overlay only while held — that can never
        // strand you, since letting go always closes it. Shift+H latches it
        // so it can be read hands-free. Uppercase accepted for the same
        // reason as A/S above (v0.9922).
        if(c=='H'||(c=='h'&&s.shift))hLatchKey=true;
        else if(c=='h')hKey=true;
        // Shift+1..0 fires a patch morph (v0.995). The keyboard reports
        // the SHIFTED character, so a shifted 1 arrives as '!' — the same
        // lesson as Shift+A arriving as 'A' (v0.9922). Both forms are
        // accepted, since the digit-with-shift path is what some layouts
        // produce. Digits alone stay note keys, untouched.
        //
        // Its own statement, NOT part of the if/else chain above: dropping
        // a braced block into the middle of one is what broke the build in
        // v0.995 (v0.9951).
        {
            static const char SH[10]={'!','@','#','$','%','^','&','*','(',')'};
            static const char DG[10]={'1','2','3','4','5','6','7','8','9','0'};
            for(int k=0;k<10;k++){
                if(c==SH[k]||(c==DG[k]&&s.shift)){morphKeySlot=k;break;}
            }
        }
        // Space: Sequencer Play/Stop from anywhere except Patch. SEQ and
        // SONG each already handle Space themselves (updateSeqEditing()/
        // updateSongEditor()), so skip it here to avoid double-toggling
        // in the same frame.
        if(c==' '&&appMode!=AppMode::SEQ&&appMode!=AppMode::SONG)seqPlayKey=true;
    }

    // Octave (edge-triggered)
    if(oU&&!prevOctaveUpPressed   &&params.octaveShift<2) params.octaveShift++;
    if(oD&&!prevOctaveDownPressed &&params.octaveShift>-2)params.octaveShift--;
    prevOctaveUpPressed=oU;prevOctaveDownPressed=oD;

    // Transpose (edge-triggered): ',' down / '/' up
    if(trU&&!prevTransposeUpPressed  &&transposeSemitones<TRANSPOSE_MAX)transposeSemitones++;
    if(trD&&!prevTransposeDownPressed&&transposeSemitones>TRANSPOSE_MIN)transposeSemitones--;
    prevTransposeUpPressed=trU;prevTransposeDownPressed=trD;


    // Volume (edge-triggered)

    // Bend
    if(bD&&!bU)keyBendGoal=-keyBendMaxCents;
    else if(bU&&!bD)keyBendGoal=+keyBendMaxCents;
    else keyBendGoal=0;

    // IMU X hold
    if(iXH&&!prevImuXHoldPressed)imuXHeld=!imuXHeld;
    prevImuXHoldPressed=iXH;

    // IMU Y hold
    if(iYH&&!prevImuYHoldPressed)imuYHeld=!imuYHeld;
    prevImuYHoldPressed=iYH;

    // Axis enable/disable (v0.9921). Switching an axis off has to clear
    // its offset as well: the value it was contributing would otherwise
    // freeze at whatever tilt you happened to be at, leaving the sound
    // altered by a control that now reads as off. Hold is released too,
    // since a held value with the axis disabled is the same trap.
    if(iXEn&&!prevImuXEnablePressed){
        imuXEnabled=!imuXEnabled;
        if(!imuXEnabled){resetParamToDefault(imuAxisX.target);imuXHeld=false;}
    }
    prevImuXEnablePressed=iXEn;
    if(iYEn&&!prevImuYEnablePressed){
        imuYEnabled=!imuYEnabled;
        if(!imuYEnabled){resetParamToDefault(imuAxisY.target);imuYHeld=false;}
    }
    prevImuYEnablePressed=iYEn;

    // Shift+H latches the overlay; H alone is momentary as before. Latching
    // wins while it is set, so releasing H does not close a latched overlay.
    if(hLatchKey&&!prevHelpLatchPressed)helpLatched=!helpLatched;
    prevHelpLatchPressed=hLatchKey;

    // Fire the morph on the press only, so holding the key does not restart
    // it every frame (v0.995). Ignored while a text field is open, where a
    // shifted digit is a character being typed rather than a command.
    static int prevMorphKeySlot=-1;
    if(morphKeySlot>=0&&morphKeySlot!=prevMorphKeySlot
       &&appMode!=AppMode::PATCH&&appMode!=AppMode::SONG)
        morphStart(morphKeySlot);
    prevMorphKeySlot=morphKeySlot;

    // Note hold
    if(nH&&!prevNoteHoldPressed){
        noteHeld=!noteHeld;
        midiPedalTookHold=false;   // v0.99863: the player owns Hold now
        if(noteHeld)heldFreq=(playingFreq>0)?playingFreq:currentFreq;
        else{heldFreq=0;if(currentFreq==0)envPhase=EnvPhase::RELEASE;}
    }
    prevNoteHoldPressed=nH;

    void portaToggle();   // defined with the other menu actions further down
// Portamento toggle (C key). Calls the shared helper rather than
    // repeating its body — with the menu row gone (v0.994) this is the only
    // caller, and two copies of the same two lines is how they drift.
    if(pOn&&!prevPortaPressed)portaToggle();
    prevPortaPressed=pOn;

    // Arp latch toggle (V key, ADV only)
    if(latchKey&&!prevArpLatchPressed)arpLatchToggle();
    prevArpLatchPressed=latchKey;

    // Arp on/off toggle (Shift+V, ADV only) — usable anywhere except
    // Patch. This is now the ONLY way to toggle it (the redundant
    // SETTING > Arp entry was removed once this covered every screen).
    if(arpToggleKey&&!prevArpToggleKeyPressed)arpToggle();
    prevArpToggleKeyPressed=arpToggleKey;

    // Sequencer Play/Stop (Space) — usable anywhere except Patch/SEQ
    // itself (which has its own handling), so playback can be started
    // or stopped while tweaking VCO/VCF/etc without going back to SEQ.
    if(seqPlayKey&&!prevSeqPlayKeyPressedGlobal)seqTogglePlay();
    prevSeqPlayKeyPressedGlobal=seqPlayKey;

    // Help overlay (H key: show while held, hide on release)
    helpVisible=hKey||helpLatched;
}

// The hold toggle as a callable, for the MIDI switch slots (v0.99892).
// Mirrors what the H key does, including capturing the frequency, rather
// than just flipping the flag — a Hold with no note captured does nothing.
void noteHoldToggleFwd(){
    noteHeld=!noteHeld;
    if(noteHeld)heldFreq=(playingFreq>0)?playingFreq:currentFreq;
    else heldFreq=0.f;
    midiPedalTookHold=false;   // the player owns Hold now
}

// ==========================================================
// SD card
// ==========================================================
// The Cap LoRa-1262's own SPI chip select, GPIO5 (v0.99916). Confirmed
// from M5's own Cap LoRa868/1262 tutorial: "SX1262 PIN NSS, IRQ, RST,
// BUSY" constructed as Module(GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_3,
// GPIO_NUM_6) — NSS is the chip select, and it is GPIO5.
//
// This firmware never touches it, so with the Cap attached it is left
// floating. SPI is a shared bus by design — every device's CS has to be
// held deselected (HIGH, active-low being the near-universal convention)
// or it may respond to traffic meant for someone else. A floating CS on
// the LoRa chip is exactly that: it can read as asserted, and the SX1262
// answering SD commands is a plausible match for "GO_IDLE_STATE failed"
// and crc errors from the very first command. The owner's observation
// that the Launcher mounts the SD fine with the Cap attached, on the
// same cold boot, is what points here rather than back at electrical
// interference — the Launcher almost certainly deselects this pin as
// part of supporting the Cap's family of expansion modules, and nothing
// here ever did.
constexpr int CAP_LORA_CS_PIN=5;

bool initSDCard(){
    // Deselect the Cap's LoRa chip before any SPI traffic starts, so it
    // cannot answer for the SD card. Harmless with no Cap attached: an
    // unused output pin costs nothing.
    pinMode(CAP_LORA_CS_PIN,OUTPUT);
    digitalWrite(CAP_LORA_CS_PIN,HIGH);

    // An extra settle delay on a genuinely cold boot only (v0.99913). The
    // owner reported the card failing to mount when the board is powered
    // on directly with the LoRa Cap attached, but mounting fine when
    // launched through the Launcher — and those two differ in exactly
    // this respect: a direct power-on has to establish 3.3V from nothing,
    // while power is already stable by the time the Launcher hands off to
    // an app. A LoRa module's inrush current is the kind of load that
    // dips a rail right at power-on, for long enough to make the SD
    // negotiate in the first tens of milliseconds harder than usual — and
    // that is exactly the window the retry above already handles for a
    // marginal card, but was not designed to cover a marginal RAIL. Two
    // different problems that fail the same way.
    //
    // esp_reset_reason() distinguishes a cold POWERON_RESET from a
    // Launcher-triggered soft reset, so the extra wait applies only where
    // the theory says it is needed and costs nothing on every other boot.
    if(esp_reset_reason()==ESP_RST_POWERON)delay(150);
    SPI.begin(SD_SPI_SCK_PIN,SD_SPI_MISO_PIN,SD_SPI_MOSI_PIN,SD_SPI_CS_PIN);
    // Retry, and drop the clock on the way (v0.9953). A single attempt at
    // 25MHz was the whole of it before, so one marginal mount meant every
    // setting silently loaded as default and every save failed with "File
    // system is not mounted" — with nothing on screen to say the card had
    // not come up. Cards vary in how fast they will negotiate straight
    // after power-on; 25 then 16 then 4MHz costs a few milliseconds in the
    // bad case and nothing at all in the good one.
    // One more, slower rate added for CRC errors that keep recurring
    // through the whole sequence (v0.99914) — persistent crc errors at
    // every rate, all the way through the retry window, look like
    // continuous interference on the SPI lines rather than a one-off
    // startup transient. That is a wiring/shielding question this retry
    // cannot solve outright, but a slower clock and a slightly longer gap
    // between attempts costs nothing on hardware without the problem and
    // gives noisy hardware a bit more margin.
    const uint32_t rates[]={25000000,16000000,4000000,1000000};
    for(int i=0;i<4;i++){
        if(SD.begin(SD_SPI_CS_PIN,SPI,rates[i])){
            if(i>0)Serial.printf("[SD] mounted at %u Hz (attempt %d)\n",(unsigned)rates[i],i+1);
            return true;
        }
        SD.end();
        delay(80);
    }
    return false;
}
bool ensureCpsFolder(){return SD.exists(CPS_FOLDER_PATH)||SD.mkdir(CPS_FOLDER_PATH);}

static const char *SETTINGS_FILE_PATH="/CPS/settings.json";

const char *filterTypeName(FilterType t){
    switch(t){
        case FilterType::LPF:  return "LPF";
        case FilterType::HPF:  return "HPF";
        case FilterType::BPF:  return "BPF";
        case FilterType::NOTCH:return "Notch";
        case FilterType::NONE: return "None";
        default:               return "?";
    }
}

// Writes just the Sequencer pattern fields (tempo/swing/16 steps) to an
// already-open file — shared by the main settings save and Pattern Bank
// slot saves, so the exact field format only needs to be defined once.
// Settings are built in RAM and written to the card in one go (v0.9913).
//
// This used to be ~100 separate f.printf() calls straight to the File, and
// each one reaches the FAT layer and the SPI driver on its own. On the
// buffer where a save landed, audioTask's worst case jumped from the usual
// ~22-23ms to 31ms — well past the 23.22ms budget — and the surrounding
// second logged several late buffers. Assembling the text first turns all
// of that into a single write, so the card is touched once, not a hundred
// times.
//
// Static, not a stack local: this is several KB and the task stacks here
// are not the place for it. Only ever called from the main task.
char settingsBuf[SETTINGS_BUF_SIZE];
int  settingsBufLen=0;
void sbReset(){settingsBufLen=0;settingsBuf[0]=0;}
// Stops appending if the buffer fills rather than overflowing —
// saveSettingsToFile() checks the length afterwards and refuses to write a
// truncated file, which is far better than writing a corrupt one.
void sbAppend(const char *fmt,...){
    if(settingsBufLen>=SETTINGS_BUF_SIZE-1)return;
    va_list ap; va_start(ap,fmt);
    int n=vsnprintf(settingsBuf+settingsBufLen,SETTINGS_BUF_SIZE-settingsBufLen,fmt,ap);
    va_end(ap);
    if(n>0)settingsBufLen+=min(n,SETTINGS_BUF_SIZE-1-settingsBufLen);
}

// Bumped only when the MEANING of the stored keys changes, not per
// release — see the stamp written in saveSettingsToFile().
constexpr int CPS_PATCH_FORMAT = 1;

// Write-side mirror of loadingPatch (v0.9971). If a key is ignored when a
// patch is LOADED, writing it into the patch was pointless: it bloats a
// file people are about to share, and invites the obvious question of why
// someone else's morph-slot assignments are sitting inside a sound. Kept
// as a separate flag rather than one shared "patch mode" — loading and
// saving happen at different moments and conflating them would be a bug
// waiting to happen.
bool savingPatch=false;

bool saveSettingsToFile(const char *path);
bool savePatchToFile(const char *path){
    savingPatch=true;
    bool ok=saveSettingsToFile(path);
    savingPatch=false;
    return ok;
}


void writeSeqPatternFields();

void writeSeqPatternFields(){
    sbAppend("  \"seq_tempo\": %.1f,\n",seqTempoBpm);
    sbAppend("  \"seq_swing\": %.1f,\n",seqSwing);
    for(int i=0;i<SEQ_NUM_STEPS;i++){
        sbAppend("  \"seq%d_freq\": %.2f,\n",i,seqSteps[i].freq);
        sbAppend("  \"seq%d_vel\": %d,\n",i,(int)seqSteps[i].velocity);
        sbAppend("  \"seq%d_tie\": %d,\n",i,(int)seqSteps[i].tie);
        sbAppend("  \"seq%d_slide\": %d,\n",i,(int)seqSteps[i].slide);
        sbAppend("  \"seq%d_accent\": %d%s\n",i,(int)seqSteps[i].accent,(i==SEQ_NUM_STEPS-1)?"":",");
    }
}

bool saveSettingsToFile(const char *path){
    sbReset();
    sbAppend("{\n");
    // Format stamp (v0.997). Patches are about to be shared between people
    // — posted on Reddit and dropped into /CPS/Patch — and until now a
    // file said nothing about what wrote it. A reader that finds no
    // cps_format at all knows the file predates this, which is itself
    // useful; one that finds a HIGHER number than it understands can say
    // so instead of loading a file it may only half understand.
    //
    // Deliberately a format number, not the firmware version: it only
    // needs to change when the meaning of the keys changes, and bumping it
    // on every release would make it noise.
    sbAppend("  \"cps_format\": %d,\n",CPS_PATCH_FORMAT);
    sbAppend("  \"imu_x_target\": %u,\n",(unsigned)imuAxisX.target);
    sbAppend("  \"imu_y_target\": %u,\n",(unsigned)imuAxisY.target);
    sbAppend("  \"imu_x_held\": %d,\n",(int)imuXHeld);
    sbAppend("  \"imu_x_en\": %d,\n",(int)imuXEnabled);
    sbAppend("  \"imu_x_held_norm\": %.4f,\n",imuXLastNorm);
    sbAppend("  \"imu_y_held\": %d,\n",(int)imuYHeld);
    sbAppend("  \"imu_y_en\": %d,\n",(int)imuYEnabled);
    sbAppend("  \"imu_y_held_norm\": %.4f,\n",imuYLastNorm);
    sbAppend("  \"imu_x_sens\": %.2f,\n",imuAxisX.sensitivity);
    sbAppend("  \"imu_x_invert\": %d,\n",(int)imuAxisX.invert);
    sbAppend("  \"imu_x_curve\": %d,\n",(int)imuAxisX.exponential);
    sbAppend("  \"imu_x_deadzone\": %.3f,\n",imuAxisX.deadzone);
    sbAppend("  \"imu_x_cal\": %.3f,\n",imuAxisX.calOffsetDeg);
    sbAppend("  \"imu_y_sens\": %.2f,\n",imuAxisY.sensitivity);
    sbAppend("  \"imu_y_invert\": %d,\n",(int)imuAxisY.invert);
    sbAppend("  \"imu_y_curve\": %d,\n",(int)imuAxisY.exponential);
    sbAppend("  \"imu_y_deadzone\": %.3f,\n",imuAxisY.deadzone);
    sbAppend("  \"imu_y_cal\": %.3f,\n",imuAxisY.calOffsetDeg);
    sbAppend("  \"imu_calibrated\": %d,\n",(int)imuCalibrated);
    sbAppend("  \"bend_max_cents\": %.2f,\n",keyBendMaxCents);
    sbAppend("  \"bend_attack\": %.6f,\n",keyBendAttackSmooth);
    sbAppend("  \"bend_release\": %.6f,\n",keyBendReleaseSmooth);
    if(!savingPatch){   // performance/operational, not the sound —
                         // requested explicitly alongside Arp above,
                         // same reasoning (v0.9996x).
        sbAppend("  \"porta_enabled\": %d,\n",(int)portaEnabled);
        sbAppend("  \"porta_speed\": %.6f,\n",portaSpeed);
    }
    sbAppend("  \"adsr_attack\": %.3f,\n",adsr.attackTime);
    sbAppend("  \"adsr_decay\": %.3f,\n",adsr.decayTime);
    sbAppend("  \"adsr_sustain\": %.3f,\n",adsr.sustainLevel);
    sbAppend("  \"adsr_release\": %.3f,\n",adsr.releaseTime);
    sbAppend("  \"filter_type\": %u,\n",(unsigned)filterParams.type);
    sbAppend("  \"filter_cutoff\": %.1f,\n",filterParams.cutoffHz);
    sbAppend("  \"filter_q\": %.2f,\n",filterParams.resonanceQ);
    sbAppend("  \"filter_tracking\": %.2f,\n",filterParams.keyTracking);
    sbAppend("  \"fenv_depth\": %.1f,\n",filterEnv.depth);
    sbAppend("  \"fenv_attack\": %.3f,\n",filterEnv.attackTime);
    sbAppend("  \"fenv_decay\": %.3f,\n",filterEnv.decayTime);
    sbAppend("  \"fenv_sustain\": %.3f,\n",filterEnv.sustainLvl);
    sbAppend("  \"fenv_release\": %.3f,\n",filterEnv.releaseTime);
    sbAppend("  \"vco_timbre\": %.2f,\n",params.timbreMorph);
    sbAppend("  \"vco_shape\": %.2f,\n",params.oscShape);
    sbAppend("  \"vco_detune\": %.1f,\n",params.detuneCents);
    sbAppend("  \"vco_fine\": %.1f,\n",params.fineTuneCents);
    sbAppend("  \"vco_sub_level\": %.2f,\n",params.subOscLevel);
    sbAppend("  \"vco_sub_oct\": %d,\n",params.subOscOctave);
    sbAppend("  \"vco_noise\": %.2f,\n",params.noiseLevel);
    sbAppend("  \"fx_ring_rate\": %.1f,\n",params.ringModRateHz);
    sbAppend("  \"fx_ring_mix\": %.2f,\n",params.ringModMix);
    sbAppend("  \"fx_limiter_drive\": %.2f,\n",params.limiterDrive);
    sbAppend("  \"fx_limiter_mix\": %.2f,\n",params.limiterMix);
    sbAppend("  \"fx_chorus_rate\": %.2f,\n",params.chorusRateHz);
    sbAppend("  \"fx_chorus_depth\": %.1f,\n",params.chorusDepthMs);
    sbAppend("  \"fx_chorus_mix\": %.2f,\n",params.chorusMix);
    sbAppend("  \"fx_delay_time\": %.1f,\n",params.delayTimeMs);
    sbAppend("  \"fx_delay_feedback\": %.2f,\n",params.delayFeedback);
    sbAppend("  \"fx_delay_mix\": %.2f,\n",params.delayMix);
    sbAppend("  \"fx_reverb_room\": %.2f,\n",params.reverbRoomSize);
    sbAppend("  \"fx_reverb_damp\": %.2f,\n",params.reverbDamping);
    sbAppend("  \"fx_reverb_mix\": %.2f,\n",params.reverbMix);
    sbAppend("  \"fx_bitcrush\": %.2f,\n",params.bitcrush);
    sbAppend("  \"vco_vib_depth\": %.2f,\n",params.vibratoDepth);
    sbAppend("  \"vco_vib_rate\": %.2f,\n",params.vibratoRateHz);
    sbAppend("  \"vca_tremolo\": %.2f,\n",params.tremoloDepth);
    sbAppend("  \"osc2_level\": %.2f,\n",params.osc2Level);
    sbAppend("  \"osc2_wave\": %d,\n",(int)params.osc2Waveform);
    sbAppend("  \"osc2_shape\": %.2f,\n",params.osc2Shape);
    sbAppend("  \"osc2_detune\": %.2f,\n",params.osc2DetuneCents);
    sbAppend("  \"osc2_fine\": %.2f,\n",params.osc2FineCents);
    sbAppend("  \"osc2_octave\": %d,\n",params.osc2OctaveShift);
    sbAppend("  \"osc2_semi\": %d,\n",params.osc2Semitones);
    if(!savingPatch){   // appearance is the device's, not the sound's
        sbAppend("  \"ui_theme\": %d,\n",uiThemeIndex);
        sbAppend("  \"ui_bright\": %d,\n",(int)uiBrightness);
        sbAppend("  \"midi_cc_out\": %d,\n",(int)midiCcOutEnabled);
        sbAppend("  \"midi_cc_x\": %d,\n",(int)midiCcOutNumX);
        sbAppend("  \"midi_cc_y\": %d,\n",(int)midiCcOutNumY);
        sbAppend("  \"midi_cc_ch\": %d,\n",(int)midiCcOutChannel);
        sbAppend("  \"midi_note_out\": %d,\n",(int)midiNoteOutEnabled);
        sbAppend("  \"midi_gm\": %d,\n",(int)midiGmProgram);
        sbAppend("  \"midi_cc_in\": %d,\n",(int)midiCcInEnabled);
        sbAppend("  \"midi_cc_in0\": %d,\n",(int)midiCcInNum[0]);
        sbAppend("  \"midi_cc_in1\": %d,\n",(int)midiCcInNum[1]);
        sbAppend("  \"midi_cc_in0t\": %u,\n",(unsigned)midiCcInTarget[0]);
        sbAppend("  \"midi_cc_in1t\": %u,\n",(unsigned)midiCcInTarget[1]);
        sbAppend("  \"midi_clock_in\": %d,\n",(int)midiClockEnabled);
        sbAppend("  \"midi_clock_out\": %d,\n",(int)midiClockOutEnabled);
        sbAppend("  \"thr_on\": %d,\n",(int)thereminEnabled);
        sbAppend("  \"thr_quant\": %d,\n",(int)thereminQuantize);
        sbAppend("  \"thr_oct\": %d,\n",thereminOctaves);
        sbAppend("  \"thr_top\": %d,\n",thereminTopSemis);
        sbAppend("  \"thr_bus\": %d,\n",tofBusIndex);
        sbAppend("  \"thr_far\": %d,\n",thereminFarMm);
        sbAppend("  \"midi_sw0\": %d,\n",(int)midiSwNum[0]);
        sbAppend("  \"midi_sw1\": %d,\n",(int)midiSwNum[1]);
        sbAppend("  \"midi_sw0f\": %u,\n",(unsigned)midiSwFn[0]);
        sbAppend("  \"midi_sw1f\": %u,\n",(unsigned)midiSwFn[1]);
        sbAppend("  \"midi_sw0m\": %u,\n",(unsigned)midiSwMode[0]);
        sbAppend("  \"midi_sw1m\": %u,\n",(unsigned)midiSwMode[1]);
    }
    if(!savingPatch){   // playing style, not sound — see the load side
        sbAppend("  \"drift_on\": %d,\n",(int)analogDriftOn);
        sbAppend("  \"drift_amt\": %.2f,\n",analogDriftAmount);
    }
    // Morph slots and their timing describe THIS device's setup — which
    // ten sounds are on the number keys — not a sound. They stay in
    // settings.json and out of patches (v0.9971).
    if(!savingPatch){
        sbAppend("  \"morph_time\": %.2f,\n",morphTimeSec);
        for(int i=0;i<NUM_MORPH_SLOTS;i++)
            if(morphSlotPatch[i].length())
                sbAppend("  \"morph_slot%d\": \"%s\",\n",i,morphSlotPatch[i].c_str());
    }
    sbAppend("  \"lfo_wave\": %u,\n",(unsigned)lfo.wave);
    sbAppend("  \"lfo_rate\": %.3f,\n",lfo.rateHz);
    sbAppend("  \"lfo_depth\": %.3f,\n",lfo.depth);
    sbAppend("  \"lfo_target\": %u,\n",(unsigned)lfo.target);
    if(!savingPatch){   // output level and playing style are the session's
        sbAppend("  \"key_volume\": %.3f,\n",params.keyVolume);
        sbAppend("  \"play_mode\": %u,\n",(unsigned)playMode);
        sbAppend("  \"scale\": %d,\n",currentScaleIndex);
    }
    if(!savingPatch){   // performance/operational, not the sound — same
                         // reasoning as key_volume/play_mode/scale above
                         // (v0.9996x). Requested explicitly: Arp's own
                         // on/off, type and Tempo/Swing/Rate are things
                         // the player sets for how they are playing right
                         // now, not something a patch should carry or
                         // overwrite when loaded.
        sbAppend("  \"arp_enabled\": %d,\n",(int)arpEnabled);
        sbAppend("  \"arp_type\": %u,\n",(unsigned)arpType);
        sbAppend("  \"arp_tempo\": %.1f,\n",arpTempoBpm);
        sbAppend("  \"arp_swing\": %.1f,\n",arpSwing);
        sbAppend("  \"arp_rate\": %d,\n",arpRateIndex);
    }
    sbAppend("  \"morph_chain_len\": %d,\n",morphChainLen);
    for(int i=0;i<morphChainLen;i++)sbAppend("  \"morph_chain%d\": %d,\n",i,(int)morphChain[i]);
    // The sequencer pattern has its own bank; a patch carrying all sixteen
    // steps was ~2KB of a shared file describing something the reader now
    // ignores anyway (v0.9971).
    if(!savingPatch)writeSeqPatternFields();
    sbAppend("}\n");
    if(settingsBufLen>=SETTINGS_BUF_SIZE-1){
        Serial.println("[Settings] buffer full - not written");
        return false;   // better no write at all than a truncated one
    }
    File f=SD.open(path,FILE_WRITE);
    if(!f){Serial.println("[Settings] open failed");return false;}
    f.write((const uint8_t*)settingsBuf,settingsBufLen);
    f.close();
    Serial.printf("[Settings] saved to %s (%d bytes, 1 write)\n",path,settingsBufLen);
    return true;
}
bool saveSettings(){return saveSettingsToFile(SETTINGS_FILE_PATH);}

// Set while a PATCH is being parsed, so parseSettingLine() can skip the
// handful of keys that belong to the session rather than to the sound
// (v0.9955).
bool loadingPatch=false;

void parseSettingLine(const String &line){
    int q1=line.indexOf('"');if(q1<0)return;
    int q2=line.indexOf('"',q1+1);if(q2<0)return;
    String key=line.substring(q1+1,q2);
    int col=line.indexOf(':',q2);if(col<0)return;
    String vs=line.substring(col+1);vs.trim();
    if(vs.endsWith(","))vs.remove(vs.length()-1);vs.trim();
    if(!vs.length())return;
    float v=vs.toFloat();
    if(key=="imu_x_target"){uint8_t u=(uint8_t)v;if(u<(uint8_t)ImuTarget::TARGET_COUNT)imuAxisX.target=(ImuTarget)u;}
    else if(key=="imu_y_target"){uint8_t u=(uint8_t)v;if(u<(uint8_t)ImuTarget::TARGET_COUNT)imuAxisY.target=(ImuTarget)u;}
    else if(key=="imu_x_held")imuXHeld=(bool)(int)v;
    else if(key=="imu_x_en")imuXEnabled=(bool)(int)v;
    else if(key=="imu_x_held_norm")imuXLastNorm=v;
    else if(key=="imu_y_held")imuYHeld=(bool)(int)v;
    else if(key=="imu_y_en")imuYEnabled=(bool)(int)v;
    else if(key=="imu_y_held_norm")imuYLastNorm=v;
    else if(key=="imu_x_sens")imuAxisX.sensitivity=constrain(v,0.3f,3.0f);
    else if(key=="imu_x_invert")imuAxisX.invert=(bool)(int)v;
    else if(key=="imu_x_curve")imuAxisX.exponential=(bool)(int)v;
    else if(key=="imu_x_deadzone")imuAxisX.deadzone=constrain(v,0.f,0.3f);
    else if(key=="imu_x_cal")imuAxisX.calOffsetDeg=v;
    else if(key=="imu_y_sens")imuAxisY.sensitivity=constrain(v,0.3f,3.0f);
    else if(key=="imu_y_invert")imuAxisY.invert=(bool)(int)v;
    else if(key=="imu_y_curve")imuAxisY.exponential=(bool)(int)v;
    else if(key=="imu_y_deadzone")imuAxisY.deadzone=constrain(v,0.f,0.3f);
    else if(key=="imu_y_cal")imuAxisY.calOffsetDeg=v;
    else if(key=="imu_calibrated")imuCalibrated=(bool)(int)v;
    else if(key=="bend_max_cents")keyBendMaxCents=v;
    else if(key=="bend_attack")keyBendAttackSmooth=v;
    else if(key=="bend_release")keyBendReleaseSmooth=v;
    else if(key=="porta_enabled"){if(!loadingPatch)portaEnabled=(bool)(int)v;}
    else if(key=="porta_speed"){if(!loadingPatch)portaSpeed=v;}
    else if(key=="adsr_attack")adsr.attackTime=v;
    else if(key=="adsr_decay")adsr.decayTime=v;
    else if(key=="adsr_sustain")adsr.sustainLevel=v;
    else if(key=="adsr_release")adsr.releaseTime=v;
    else if(key=="filter_type"){uint8_t u=(uint8_t)v;if(u<=(uint8_t)FilterType::NONE)filterParams.type=(FilterType)u;}
    else if(key=="filter_cutoff")filterParams.cutoffHz=v;
    else if(key=="filter_q")filterParams.resonanceQ=v;
    else if(key=="filter_tracking")filterParams.keyTracking=constrain(v,0.f,1.f);
    else if(key=="fenv_depth")filterEnv.depth=v;
    else if(key=="fenv_attack")filterEnv.attackTime=v;
    else if(key=="fenv_decay")filterEnv.decayTime=v;
    else if(key=="fenv_sustain")filterEnv.sustainLvl=v;
    else if(key=="fenv_release")filterEnv.releaseTime=v;
    else if(key=="vco_timbre"){params.timbreMorph=params.timbreMorphTarget=v;}
    else if(key=="vco_shape")params.oscShape=constrain(v,0.f,1.f);
    else if(key=="vco_detune")params.detuneCents=constrain(v,-50.f,50.f);
    else if(key=="vco_fine")params.fineTuneCents=constrain(v,-100.f,100.f);
    else if(key=="vco_sub_level")params.subOscLevel=constrain(v,0.f,1.f);
    else if(key=="vco_sub_oct")params.subOscOctave=constrain((int)v,-2,-1);
    else if(key=="vco_noise")params.noiseLevel=constrain(v,0.f,1.f);
    else if(key=="fx_ring_rate")params.ringModRateHz=constrain(v,20.f,2000.f);
    else if(key=="fx_ring_mix")params.ringModMix=constrain(v,0.f,1.f);
    else if(key=="fx_limiter_drive")params.limiterDrive=constrain(v,1.f,5.f);
    else if(key=="fx_limiter_mix")params.limiterMix=constrain(v,0.f,1.f);
    else if(key=="fx_chorus_rate")params.chorusRateHz=constrain(v,0.1f,5.f);
    else if(key=="fx_chorus_depth")params.chorusDepthMs=constrain(v,0.f,20.f);
    else if(key=="fx_chorus_mix")params.chorusMix=constrain(v,0.f,1.f);
    else if(key=="fx_delay_time")params.delayTimeMs=constrain(v,50.f,DELAY_MAX_MS);
    else if(key=="fx_delay_feedback")params.delayFeedback=constrain(v,0.f,0.9f);
    else if(key=="fx_delay_mix")params.delayMix=constrain(v,0.f,1.f);
    else if(key=="fx_reverb_room")params.reverbRoomSize=constrain(v,0.f,1.f);
    else if(key=="fx_reverb_damp")params.reverbDamping=constrain(v,0.f,1.f);
    else if(key=="fx_reverb_mix")params.reverbMix=constrain(v,0.f,1.f);
    else if(key=="fx_bitcrush")params.bitcrush=constrain(v,0.f,1.f);
    else if(key=="vco_vib_depth")params.vibratoDepth=constrain(v,0.f,1.f);
    else if(key=="vco_vib_rate")params.vibratoRateHz=constrain(v,1.f,10.f);
    else if(key=="vca_tremolo")params.tremoloDepth=constrain(v,0.f,1.f);
    else if(key=="osc2_level")params.osc2Level=constrain(v,0.f,1.f);
    else if(key=="osc2_wave"){int n=(int)v;if(n>=0&&n<NUM_OSC_WAVEFORMS)params.osc2Waveform=(OscWaveform)n;}
    else if(key=="osc2_shape")params.osc2Shape=constrain(v,0.f,1.f);
    else if(key=="osc2_detune")params.osc2DetuneCents=constrain(v,-50.f,50.f);
    else if(key=="osc2_fine")params.osc2FineCents=constrain(v,-10.f,10.f);
    else if(key=="osc2_octave"){int n=(int)v;if(n>=-2&&n<=2)params.osc2OctaveShift=n;}
    else if(key=="osc2_semi"){int n=(int)v;if(n>=-12&&n<=12)params.osc2Semitones=n;}
    // Appearance and morph-slot assignments belong to the DEVICE, not to a
    // sound (v0.997). A shared patch that repaints someone's synth, or
    // fills their morph slots with the names of patches they do not have,
    // is doing something no one asked it to.
    else if(key=="ui_theme"){if(!loadingPatch){int n=(int)v;if(n>=0&&n<NUM_UI_THEMES){uiThemeIndex=n;applyUiTheme();}}}
    else if(key=="ui_bright"){if(!loadingPatch){int n=(int)v;uiBrightness=(uint8_t)constrain(n,(int)UI_BRIGHT_MIN,(int)UI_BRIGHT_MAX);applyUiBrightness();}}
    // MIDI settings belong to the device, not to a sound (v0.99871).
    else if(key=="midi_cc_out"){if(!loadingPatch)midiCcOutEnabled=(bool)(int)v;}
    else if(key=="midi_cc_x"){if(!loadingPatch)midiCcOutNumX=(uint8_t)constrain((int)v,0,127);}
    else if(key=="midi_cc_y"){if(!loadingPatch)midiCcOutNumY=(uint8_t)constrain((int)v,0,127);}
    else if(key=="midi_cc_ch"){if(!loadingPatch)midiCcOutChannel=(uint8_t)constrain((int)v,0,15);}
    else if(key=="midi_note_out"){if(!loadingPatch)midiNoteOutEnabled=(bool)(int)v;}
    else if(key=="midi_gm"){if(!loadingPatch)midiGmProgram=(uint8_t)constrain((int)v,0,127);}
    else if(key=="midi_cc_in"){if(!loadingPatch)midiCcInEnabled=(bool)(int)v;}
    else if(key=="midi_cc_in0"){if(!loadingPatch)midiCcInNum[0]=(uint8_t)constrain((int)v,0,127);}
    else if(key=="midi_cc_in1"){if(!loadingPatch)midiCcInNum[1]=(uint8_t)constrain((int)v,0,127);}
    else if(key=="midi_cc_in0t"){if(!loadingPatch){uint8_t u=(uint8_t)v;if(u<(uint8_t)ImuTarget::TARGET_COUNT)midiCcInTarget[0]=(ImuTarget)u;}}
    else if(key=="midi_cc_in1t"){if(!loadingPatch){uint8_t u=(uint8_t)v;if(u<(uint8_t)ImuTarget::TARGET_COUNT)midiCcInTarget[1]=(ImuTarget)u;}}
    else if(key=="midi_clock_in"){if(!loadingPatch)midiClockEnabled=(bool)(int)v;}
    else if(key=="midi_clock_out"){if(!loadingPatch)midiClockOutEnabled=(bool)(int)v;}
    else if(key=="thr_on"){if(!loadingPatch)thereminEnabled=(bool)(int)v;}
    else if(key=="thr_quant"){if(!loadingPatch)thereminQuantize=(bool)(int)v;}
    else if(key=="thr_oct"){if(!loadingPatch)thereminOctaves=constrain((int)v,1,4);}
    else if(key=="thr_top"){if(!loadingPatch)thereminTopSemis=constrain((int)v,-24,36);}
    else if(key=="thr_bus"){if(!loadingPatch)tofBusIndex=constrain((int)v,0,1);}
    else if(key=="thr_far"){if(!loadingPatch)thereminFarMm=constrain((int)v,160,1000);}
    else if(key=="midi_sw0"){if(!loadingPatch)midiSwNum[0]=(uint8_t)constrain((int)v,0,127);}
    else if(key=="midi_sw1"){if(!loadingPatch)midiSwNum[1]=(uint8_t)constrain((int)v,0,127);}
    else if(key=="midi_sw0f"){if(!loadingPatch){uint8_t u=(uint8_t)v;if(u<(uint8_t)MidiSwitchFn::FN_COUNT)midiSwFn[0]=(MidiSwitchFn)u;}}
    else if(key=="midi_sw1f"){if(!loadingPatch){uint8_t u=(uint8_t)v;if(u<(uint8_t)MidiSwitchFn::FN_COUNT)midiSwFn[1]=(MidiSwitchFn)u;}}
    else if(key=="midi_sw0m"){if(!loadingPatch){uint8_t u=(uint8_t)v;if(u<(uint8_t)MidiSwMode::MODE_COUNT)midiSwMode[0]=(MidiSwMode)u;}}
    else if(key=="midi_sw1m"){if(!loadingPatch){uint8_t u=(uint8_t)v;if(u<(uint8_t)MidiSwMode::MODE_COUNT)midiSwMode[1]=(MidiSwMode)u;}}
    else if(key=="drift_on"){if(!loadingPatch)analogDriftOn=(bool)(int)v;}
    else if(key=="drift_amt"){if(!loadingPatch)analogDriftAmount=constrain(v,0.f,1.f);}
    else if(key=="cps_format"){
        // Only reported, never enforced (v0.997). Refusing to load would be
        // worse than loading: the keys this firmware does not recognise are
        // ignored anyway, and the ones it does still mean what they say.
        // The note is for the log, so an odd-sounding shared patch has an
        // explanation waiting rather than looking like a bug.
        int f=(int)v;
        if(f>CPS_PATCH_FORMAT)
            Serial.printf("[Patch] made by a newer version (format %d > %d) - unknown settings ignored\n",f,CPS_PATCH_FORMAT);
    }
    else if(key=="morph_time"){if(!loadingPatch)morphTimeSec=constrain(v,0.f,10.f);}
    else if(key.startsWith("morph_slot")&&!loadingPatch){
        // String-valued, so it needs the quotes stripped rather than
        // toFloat() — the only key here that is not a number (v0.995).
        int idx=key.substring(10).toInt();
        int a1=vs.indexOf('"'),a2=vs.lastIndexOf('"');
        if(idx>=0&&idx<NUM_MORPH_SLOTS&&a1>=0&&a2>a1)
            morphSlotPatch[idx]=vs.substring(a1+1,a2);
    }
    else if(key=="lfo_wave"){uint8_t u=(uint8_t)v;if(u<5)lfo.wave=(LfoWave)u;}
    else if(key=="lfo_rate")lfo.rateHz=constrain(v,LFO_RATE_MIN,LFO_RATE_MAX);
    else if(key=="lfo_depth")lfo.depth=constrain(v,0.f,1.f);
    else if(key=="lfo_target"){uint8_t u=(uint8_t)v;if(u<(uint8_t)LfoTarget::TARGET_COUNT)lfo.target=(LfoTarget)u;}
    else if(key=="key_volume"){
        // Ignored when loading a PATCH (v0.9955). Output level is a
        // property of the session, not of the sound: having a patch jump
        // the volume is startling at best and, at a low stored value,
        // looks like the patch failed to load. settings.json still
        // restores it, so the level you left the synth at comes back.
        if(!loadingPatch)params.keyVolume=constrain(v,0.f,1.f);
    }
    // Play Style, Scale and Drift are session settings, not part of a
    // sound — the same reasoning as key_volume above (v0.9956). A patch
    // that silently flips the keyboard into Pro Style on a different
    // scale, or switches the tuning instability on, changes how the
    // instrument PLAYS rather than how it sounds. settings.json still
    // carries them, so the synth comes back the way it was left.
    else if(key=="play_mode"){if(!loadingPatch){uint8_t u=(uint8_t)v;if(u<=(uint8_t)PlayMode::PRO)playMode=(PlayMode)u;}}
    else if(key=="scale"){if(!loadingPatch){int idx=(int)v;if(idx>=0&&idx<NUM_SCALES)currentScaleIndex=idx;}}
    else if(key=="arp_enabled"){if(!loadingPatch)arpEnabled=(bool)(int)v;}
    else if(key=="arp_type"){if(!loadingPatch){uint8_t u=(uint8_t)v;if(u<=(uint8_t)ArpType::RANDOM)arpType=(ArpType)u;}}
    else if(key=="arp_tempo"){if(!loadingPatch)arpTempoBpm=constrain(v,40.f,240.f);}
    else if(key=="arp_swing"){if(!loadingPatch)arpSwing=constrain(v,-100.f,100.f);}
    else if(key=="arp_rate"){if(!loadingPatch){int idx=(int)v;if(idx>=0&&idx<NUM_ARP_RATES)arpRateIndex=idx;}}
    else if(key=="morph_chain_len"){int n=(int)v;if(n>=MIN_MORPH_SLOTS&&n<=MAX_MORPH_SLOTS)morphChainLen=n;}
    else if(key.startsWith("morph_chain")&&key!="morph_chain_len"){
        int idx=key.substring(11).toInt(); // "morph_chain" is 11 chars
        int wv=(int)v;
        if(idx>=0&&idx<MAX_MORPH_SLOTS&&wv>=0&&wv<NUM_OSC_WAVEFORMS)morphChain[idx]=(OscWaveform)wv;
    }
    // The sequencer pattern is NOT part of a sound (v0.997). Save writes
    // the whole synth state to both settings.json and patch files through
    // one function, so every patch carries all sixteen steps — meaning
    // auditioning someone else's patch silently replaced the pattern you
    // were working on. Patterns have their own save/load in the Pattern
    // bank; that is where they belong.
    else if(key=="seq_tempo"){if(!loadingPatch)seqTempoBpm=constrain(v,40.f,240.f);}
    else if(key=="seq_swing"){if(!loadingPatch)seqSwing=constrain(v,-100.f,100.f);}
    else if(key.startsWith("seq")&&!loadingPatch){
        // "seqN_freq" / "seqN_vel" / "seqN_gate"
        int us=key.indexOf('_');
        if(us>3){
            int idx=key.substring(3,us).toInt();
            if(idx>=0&&idx<SEQ_NUM_STEPS){
                String field=key.substring(us+1);
                if(field=="freq")seqSteps[idx].freq=v;
                else if(field=="vel")seqSteps[idx].velocity=(uint8_t)constrain(v,0.f,100.f);
                else if(field=="tie")seqSteps[idx].tie=(bool)(int)v;
                else if(field=="slide")seqSteps[idx].slide=(bool)(int)v;
                else if(field=="accent")seqSteps[idx].accent=(bool)(int)v;
            }
        }
    }
}

// Forward declaration: performPatchToneReset() is defined further down
// with the other menu actions, but is needed here (v0.9932).
void performPatchToneReset();
bool loadSettingsFromFile(const char *path,bool resetFirst=false);

// resetFirst: clear tone parameters to their defaults before parsing.
//
// The parser only ever ASSIGNS keys it finds, and silently leaves alone
// anything the file does not mention. That is correct for settings.json,
// which is always written by the current firmware and always complete.
// It is wrong for a patch: a patch saved before oscillator 2, the Morph
// chain or the FX tab existed has no keys for them, so loading it left
// whatever happened to be set at the time — you got the old patch's tone
// plus the current patch's oscillator 2, Morph chain and effects mixed
// together (v0.9932).
//
// Resetting first makes an absent key mean "default" instead of "keep",
// which is what loading a patch has always implied. Old patches now load
// as they sounded when they were saved.
bool loadSettingsFromFile(const char *path,bool resetFirst){   // default in the prototype above
    if(!SD.exists(path)){Serial.println("[Settings] not found");return false;}
    File f=SD.open(path,FILE_READ);
    if(!f)return false;
    if(resetFirst)performPatchToneReset();
    // resetFirst is only ever true for a patch load, so it doubles as the
    // "this is a patch" flag.
    loadingPatch=resetFirst;
    while(f.available())parseSettingLine(f.readStringUntil('\n'));
    loadingPatch=false;
    f.close();
    // Held IMU axes don't get live tilt updates, so reconstruct whatever
    // target value was frozen at save time from the stored normalized input.
    if(imuXEnabled&&imuXHeld&&imuAxisX.target!=ImuTarget::NONE)applyImuValue(imuAxisX.target,imuXLastNorm);
    if(imuYEnabled&&imuYHeld&&imuAxisY.target!=ImuTarget::NONE)applyImuValue(imuAxisY.target,imuYLastNorm);
    updateFilterCoefficients();
    Serial.printf("[Settings] loaded from %s\n",path);
    return true;
}
bool loadSettings(){return loadSettingsFromFile(SETTINGS_FILE_PATH);}

// ---------------------------------------------------------
// Pattern Bank: 8 lettered banks (A-H) x 8 numbered slots (1-8) = 64
// pattern slots, each a saved 16-step Sequencer pattern (steps + tempo +
// swing — the same fields as the main settings file's seq_*/seqN_*
// entries, just scoped to one file per slot under /CPS/Pattern instead
// of the whole synth's live state). Save reuses writeSeqPatternFields();
// load reuses parseSettingLine() line-by-line, since the field names are
// identical — a pattern file is really just a tiny settings file that
// only touches Sequencer fields.
// ---------------------------------------------------------
static const char *PATTERN_FOLDER_PATH = "/CPS/Pattern";
constexpr int NUM_PATTERN_BANKS = 8;         // A-H
constexpr int NUM_PATTERNS_PER_BANK = 8;     // 1-8

bool ensurePatternFolder(){return SD.exists(PATTERN_FOLDER_PATH)||SD.mkdir(PATTERN_FOLDER_PATH);}

String patternFilePath(int bank,int slot){
    char buf[40];
    snprintf(buf,sizeof(buf),"%s/%c%d.json",PATTERN_FOLDER_PATH,'A'+bank,slot+1);
    return String(buf);
}

bool patternSlotExists(int bank,int slot){
    return SD.exists(patternFilePath(bank,slot));
}

// Cached occupancy for the 8x8 grid (v0.9926).
//
// drawPatternBankScreen() called patternSlotExists() for every cell, so
// every redraw of that screen meant 64 SD.exists() calls — 64 card
// transactions, several times a second. That is why the grid crawled under
// the cursor keys, and why opening it during playback dragged the tempo
// down: the card work was blocking long enough to interfere with the audio
// task, the same mechanism as the settings-save spike fixed in v0.9913.
//
// The card is now read once when the screen is entered and after any save
// or delete, which are the only things that can change occupancy.
bool patternSlotCache[NUM_PATTERN_BANKS][NUM_PATTERNS_PER_BANK]={{false}};
void refreshPatternSlotCache(){
    for(int b=0;b<NUM_PATTERN_BANKS;b++)
        for(int sl=0;sl<NUM_PATTERNS_PER_BANK;sl++)
            patternSlotCache[b][sl]=patternSlotExists(b,sl);
}

bool savePatternToSlot(int bank,int slot){
    if(!ensurePatternFolder()){Serial.println("[Pattern] folder create failed");return false;}
    // Same single-write treatment as the settings file (v0.9913) — this
    // one runs on leaving SEQ, so it fires often enough to matter.
    sbReset();
    sbAppend("{\n");
    writeSeqPatternFields();
    sbAppend("}\n");
    if(settingsBufLen>=SETTINGS_BUF_SIZE-1){
        Serial.println("[Pattern] buffer full - not written");
        return false;
    }
    File f=SD.open(patternFilePath(bank,slot),FILE_WRITE);
    if(!f){Serial.println("[Pattern] open failed");return false;}
    f.write((const uint8_t*)settingsBuf,settingsBufLen);
    f.close();
    Serial.printf("[Pattern] saved to %s\n",patternFilePath(bank,slot).c_str());
    return true;
}

bool loadPatternFromSlot(int bank,int slot){
    String path=patternFilePath(bank,slot);
    if(!SD.exists(path)){Serial.println("[Pattern] not found");return false;}
    File f=SD.open(path,FILE_READ);
    if(!f)return false;
    while(f.available())parseSettingLine(f.readStringUntil('\n'));
    f.close();
    Serial.printf("[Pattern] loaded from %s\n",path.c_str());
    return true;
}

bool deletePatternSlot(int bank,int slot){
    return SD.remove(patternFilePath(bank,slot));
}

// Song UI preview: reads a pattern's step shape (freq/tie/accent/slide
// only — velocity doesn't matter for a tiny preview) into a dedicated
// buffer, separate from the live seqSteps[] so scrolling through Song
// entries in the editor never disturbs whatever's actually loaded or
// playing. Cached by (bank,slot) so it only re-reads the SD card when
// the previewed entry actually changes.
SeqStep songPreviewSteps[SEQ_NUM_STEPS];
bool songPreviewValid=false;
int  songPreviewLoadedBank=-1, songPreviewLoadedSlot=-1;

void loadPatternPreview(int bank,int slot){
    if(songPreviewValid&&songPreviewLoadedBank==bank&&songPreviewLoadedSlot==slot)return;
    for(int i=0;i<SEQ_NUM_STEPS;i++)songPreviewSteps[i]=SeqStep();
    songPreviewLoadedBank=bank;songPreviewLoadedSlot=slot;
    String path=patternFilePath(bank,slot);
    if(!SD.exists(path)){songPreviewValid=false;return;}
    File f=SD.open(path,FILE_READ);
    if(!f){songPreviewValid=false;return;}
    while(f.available()){
        String line=f.readStringUntil('\n');
        line.trim();
        if(line.length()<3)continue;
        int c=line.indexOf(':');
        if(c<0)continue;
        String key=line.substring(0,c);key.trim();key.replace("\"","");
        String valStr=line.substring(c+1);valStr.trim();
        if(valStr.endsWith(","))valStr=valStr.substring(0,valStr.length()-1);
        float v=valStr.toFloat();
        if(key.length()>3&&key.startsWith("seq")&&key[3]>='0'&&key[3]<='9'){
            int us=key.indexOf('_');
            if(us<4)continue;
            int idx=key.substring(3,us).toInt();
            String field=key.substring(us+1);
            if(idx<0||idx>=SEQ_NUM_STEPS)continue;
            if(field=="freq")songPreviewSteps[idx].freq=v;
            else if(field=="tie")songPreviewSteps[idx].tie=(bool)(int)v;
            else if(field=="accent")songPreviewSteps[idx].accent=(bool)(int)v;
            else if(field=="slide")songPreviewSteps[idx].slide=(bool)(int)v;
        }
    }
    f.close();
    songPreviewValid=true;
}

// One fixed color per Bank letter (A-H), used by the Song timeline so
// each block is recognizable by pattern bank at a glance. Placeholder
// values here; recomputed properly in setup() via color565().
uint16_t songBankColors[NUM_PATTERN_BANKS];

enum class PatternBankMode : uint8_t { LOAD, SAVE };
PatternBankMode patternBankMode = PatternBankMode::LOAD;
int  patternSelBank=0;   // 0-7 (A-H)
int  patternSelSlot=0;   // 0-7 (1-8)
bool patternConfirmDelete=false;
bool patternConfirmOverwrite=false;
bool prevPatternUpPressed=false, prevPatternDownPressed=false;
bool prevPatternLeftPressed=false, prevPatternRightPressed=false;
bool prevPatternConfirmPressed=false, prevPatternDeleteKeyPressed=false, prevPatternTabPressed=false;

void enterPatternBank(PatternBankMode mode){
    patternBankMode=mode;
    patternConfirmDelete=false;patternConfirmOverwrite=false;
    // Seed edge-trackers with whatever is CURRENTLY held so the key press
    // that opened this screen isn't immediately re-read as a fresh press
    // inside the browser (same reasoning as enterPatchBrowser()).
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldUp=false,heldDown=false,heldLeft=false,heldRight=false;
    for(char c:s.word){
        if(c==';')heldUp=true;   if(c=='.')heldDown=true;
        if(c==',')heldLeft=true; if(c=='/')heldRight=true;
    }
    prevPatternUpPressed=heldUp; prevPatternDownPressed=heldDown;
    prevPatternLeftPressed=heldLeft; prevPatternRightPressed=heldRight;
    prevPatternConfirmPressed=s.enter;
    prevPatternDeleteKeyPressed=s.del;
    prevPatternTabPressed=s.tab;
    // Read the card once here rather than 64 times per redraw (v0.9926).
    refreshPatternSlotCache();
    appMode=AppMode::PATTERN;
}
void patternBankSaveEnter(){enterPatternBank(PatternBankMode::SAVE);}
void patternBankLoadEnter(){enterPatternBank(PatternBankMode::LOAD);}
const char *patternBankEnterLabel(){return "Select>";}

void updatePatternBank(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool up=false,down=false,left=false,right=false;
    for(char c:s.word){
        if(c==';')up=true;   if(c=='.')down=true;
        if(c==',')left=true; if(c=='/')right=true;
    }
    bool confirmKey=s.enter, deleteKey=s.del, tabKey=s.tab;

    if(patternConfirmDelete){
        for(char c:s.word){
            if(c=='y'||c=='Y'){deletePatternSlot(patternSelBank,patternSelSlot);refreshPatternSlotCache();patternConfirmDelete=false;}
            if(c=='n'||c=='N')patternConfirmDelete=false;
        }
        return;
    }
    if(patternConfirmOverwrite){
        for(char c:s.word){
            if(c=='y'||c=='Y'){savePatternToSlot(patternSelBank,patternSelSlot);refreshPatternSlotCache();patternConfirmOverwrite=false;appMode=lastMainMode;}
            if(c=='n'||c=='N')patternConfirmOverwrite=false;
        }
        return;
    }

    if(up&&!prevPatternUpPressed)patternSelBank=(patternSelBank-1+NUM_PATTERN_BANKS)%NUM_PATTERN_BANKS;
    if(down&&!prevPatternDownPressed)patternSelBank=(patternSelBank+1)%NUM_PATTERN_BANKS;
    if(left&&!prevPatternLeftPressed)patternSelSlot=(patternSelSlot-1+NUM_PATTERNS_PER_BANK)%NUM_PATTERNS_PER_BANK;
    if(right&&!prevPatternRightPressed)patternSelSlot=(patternSelSlot+1)%NUM_PATTERNS_PER_BANK;
    prevPatternUpPressed=up;prevPatternDownPressed=down;
    prevPatternLeftPressed=left;prevPatternRightPressed=right;

    if(confirmKey&&!prevPatternConfirmPressed){
        if(patternBankMode==PatternBankMode::LOAD){
            if(patternSlotCache[patternSelBank][patternSelSlot]){
                loadPatternFromSlot(patternSelBank,patternSelSlot);
                appMode=lastMainMode;
            }
        } else {
            if(patternSlotCache[patternSelBank][patternSelSlot])patternConfirmOverwrite=true;
            else{savePatternToSlot(patternSelBank,patternSelSlot);refreshPatternSlotCache();appMode=lastMainMode;}
        }
    }
    prevPatternConfirmPressed=confirmKey;

    if(deleteKey&&!prevPatternDeleteKeyPressed){
        if(patternSlotCache[patternSelBank][patternSelSlot])patternConfirmDelete=true;
    }
    prevPatternDeleteKeyPressed=deleteKey;

    if(tabKey&&!prevPatternTabPressed)appMode=AppMode::CATEGORY;
    prevPatternTabPressed=tabKey;
}

// ==========================================================
// Song mode: arranges saved Pattern Bank patterns into a sequence for
// playback (playback engine is a later step — this covers the data
// model, save/load, and the arrangement editor). Each entry references
// a Pattern Bank slot (bank+slot) plus a per-entry Transpose (semitones,
// chromatic — consistent with how Octave/Transpose already works
// elsewhere rather than a scale-aware diatonic shift) and Repeat count
// (how many times through before advancing). Two global settings:
// whether each entry uses its own saved pattern's Tempo/Swing
// (songInheritTempoSwing) or keeps whatever the Sequencer's current
// Tempo/Swing already is throughout the whole song, and whether the
// song loops back to the start after the last entry or stops there.
// ==========================================================
constexpr int SONG_MAX_ENTRIES=32;
struct SongEntry {
    uint8_t bank=0;       // 0-7 (A-H), which Pattern Bank slot this entry plays
    uint8_t slot=0;       // 0-7 (1-8)
    int8_t  transpose=0;  // semitones, chromatic, -24..+24
    uint8_t repeat=1;     // 1-16 times through the 16 steps before advancing
};
SongEntry songEntries[SONG_MAX_ENTRIES];
int  songLen=0;
bool songInheritTempoSwing=true; // true: each entry uses its own saved pattern's Tempo/Swing; false: keep the Sequencer's current Tempo/Swing throughout
bool songLoopAtEnd=true;         // true: loop back to entry 0 after the last one; false: stop there
float songTempoBpm=120.0f, songSwing=0.0f; // used instead of each pattern's own saved Tempo/Swing whenever songInheritTempoSwing is off

static const char *SONG_FOLDER_PATH="/CPS/Song";
constexpr int NUM_SONG_SLOTS=8; // 1-8, no lettered banks needed — fewer songs than patterns expected

bool ensureSongFolder(){return SD.exists(SONG_FOLDER_PATH)||SD.mkdir(SONG_FOLDER_PATH);}

String songFilePath(int slot){
    char buf[32];
    snprintf(buf,sizeof(buf),"%s/%d.json",SONG_FOLDER_PATH,slot+1);
    return String(buf);
}

bool songSlotExists(int slot){return SD.exists(songFilePath(slot));}

bool saveSongToSlot(int slot){
    if(!ensureSongFolder()){Serial.println("[Song] folder create failed");return false;}
    File f=SD.open(songFilePath(slot),FILE_WRITE);
    if(!f){Serial.println("[Song] open failed");return false;}
    f.println("{");
    f.printf("  \"song_len\": %d,\n",songLen);
    f.printf("  \"song_inherit\": %d,\n",(int)songInheritTempoSwing);
    f.printf("  \"song_loop\": %d,\n",(int)songLoopAtEnd);
    f.printf("  \"song_tempo\": %.1f,\n",songTempoBpm);
    f.printf("  \"song_swing\": %.1f,\n",songSwing);
    for(int i=0;i<songLen;i++){
        f.printf("  \"song%d_bank\": %d,\n",i,songEntries[i].bank);
        f.printf("  \"song%d_slot\": %d,\n",i,songEntries[i].slot);
        f.printf("  \"song%d_transpose\": %d,\n",i,songEntries[i].transpose);
        f.printf("  \"song%d_repeat\": %d%s\n",i,songEntries[i].repeat,(i==songLen-1)?"":",");
    }
    f.println("}");
    f.close();
    Serial.printf("[Song] saved to %s\n",songFilePath(slot).c_str());
    return true;
}

bool loadSongFromSlot(int slot){
    String path=songFilePath(slot);
    if(!SD.exists(path)){Serial.println("[Song] not found");return false;}
    File f=SD.open(path,FILE_READ);
    if(!f)return false;
    songLen=0;
    while(f.available()){
        String line=f.readStringUntil('\n');
        line.trim();
        if(line.length()<3)continue;
        int c=line.indexOf(':');
        if(c<0)continue;
        String key=line.substring(0,c); key.trim(); key.replace("\"","");
        String valStr=line.substring(c+1); valStr.trim();
        if(valStr.endsWith(","))valStr=valStr.substring(0,valStr.length()-1);
        float v=valStr.toFloat();
        if(key=="song_len"){songLen=constrain((int)v,0,SONG_MAX_ENTRIES);continue;}
        if(key=="song_inherit"){songInheritTempoSwing=(bool)(int)v;continue;}
        if(key=="song_loop"){songLoopAtEnd=(bool)(int)v;continue;}
        if(key=="song_tempo"){songTempoBpm=constrain(v,40.f,240.f);continue;}
        if(key=="song_swing"){songSwing=constrain(v,-100.f,100.f);continue;}
        if(key.startsWith("song")){
            int us=key.indexOf('_');
            if(us<4)continue;
            int idx=key.substring(4,us).toInt();
            String field=key.substring(us+1);
            if(idx<0||idx>=SONG_MAX_ENTRIES)continue;
            if(field=="bank")songEntries[idx].bank=(uint8_t)constrain((int)v,0,NUM_PATTERN_BANKS-1);
            else if(field=="slot")songEntries[idx].slot=(uint8_t)constrain((int)v,0,NUM_PATTERNS_PER_BANK-1);
            else if(field=="transpose")songEntries[idx].transpose=(int8_t)constrain((int)v,-24,24);
            else if(field=="repeat")songEntries[idx].repeat=(uint8_t)constrain((int)v,1,16);
        }
    }
    f.close();
    Serial.printf("[Song] loaded from %s\n",path.c_str());
    return true;
}

bool deleteSongSlot(int slot){return SD.remove(songFilePath(slot));}

// ---- Song editor UI ----
enum class SongField : uint8_t { BANK, SLOT, TRANSPOSE, REPEAT };
enum class SongFocus : uint8_t { ENTRY, SETTINGS };
SongFocus songFocus=SongFocus::ENTRY;
enum class SongSettingsField : uint8_t { TEMPO, SWING };
SongSettingsField songSettingsField=SongSettingsField::TEMPO;
bool prevSongFocusKeyPressed=false;
int  songCursorEntry=0;
SongField songField=SongField::BANK;
enum class SongIoMode : uint8_t { SAVE, LOAD };
bool songIoPickerOpen=false;
SongIoMode songIoMode=SongIoMode::SAVE;
int  songIoSelSlot=0;
bool songIoConfirmDelete=false, songIoConfirmOverwrite=false;
bool prevSongUpPressed=false, prevSongDownPressed=false, prevSongLeftPressed=false, prevSongRightPressed=false;
bool prevSongInsertKeyPressed=false, prevSongDeleteKeyPressed=false, prevSongFieldKeyPressed=false;
bool prevSongSaveKeyPressed=false, prevSongLoadKeyPressed=false, prevSongInheritKeyPressed=false, prevSongLoopKeyPressed=false;
bool prevSongIoConfirmPressed=false, prevSongIoDeleteKeyPressed=false, prevSongIoTabPressed=false;
bool prevSongIoUpPressed=false, prevSongIoDownPressed=false;
bool prevSongTabPressed=false;
bool prevSongPlayKeyPressed=false;

void updateSongIoPicker(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool up=false,down=false;
    for(char c:s.word){if(c==';')up=true; if(c=='.')down=true;}
    bool confirmKey=s.enter, deleteKey=s.del, tabKey=s.tab;

    if(songIoConfirmDelete){
        for(char c:s.word){
            if(c=='y'||c=='Y'){deleteSongSlot(songIoSelSlot);songIoConfirmDelete=false;}
            if(c=='n'||c=='N')songIoConfirmDelete=false;
        }
        return;
    }
    if(songIoConfirmOverwrite){
        for(char c:s.word){
            if(c=='y'||c=='Y'){saveSongToSlot(songIoSelSlot);songIoConfirmOverwrite=false;songIoPickerOpen=false;}
            if(c=='n'||c=='N')songIoConfirmOverwrite=false;
        }
        return;
    }

    if(up&&!prevSongIoUpPressed)songIoSelSlot=(songIoSelSlot-1+NUM_SONG_SLOTS)%NUM_SONG_SLOTS;
    if(down&&!prevSongIoDownPressed)songIoSelSlot=(songIoSelSlot+1)%NUM_SONG_SLOTS;
    prevSongIoUpPressed=up;prevSongIoDownPressed=down;

    if(confirmKey&&!prevSongIoConfirmPressed){
        if(songIoMode==SongIoMode::LOAD){
            if(songSlotExists(songIoSelSlot)){loadSongFromSlot(songIoSelSlot);songIoPickerOpen=false;}
        } else {
            if(songSlotExists(songIoSelSlot))songIoConfirmOverwrite=true;
            else{saveSongToSlot(songIoSelSlot);songIoPickerOpen=false;}
        }
    }
    prevSongIoConfirmPressed=confirmKey;

    if(deleteKey&&!prevSongIoDeleteKeyPressed){
        if(songSlotExists(songIoSelSlot))songIoConfirmDelete=true;
    }
    prevSongIoDeleteKeyPressed=deleteKey;

    if(tabKey&&!prevSongIoTabPressed)songIoPickerOpen=false;
    prevSongIoTabPressed=tabKey;
}

// ---- Song playback engine ----
// Reuses SEQ's own step-timing engine (updateSeqTiming(), seqPlaying)
// entirely — Song just loads each entry's pattern into seqSteps[] in
// turn, applies that entry's Transpose as a multiplier songTransposeMult
// (read by updateSeqTiming()), and advances entries via
// songAdvanceOnPassComplete() (called from updateSeqTiming() whenever a
// full 16-step pass completes). Deliberately simple for a first pass —
// each entry transition does a synchronous SD card read via
// loadPatternFromSlot(), no pre-fetch/double-buffering — flagged as a
// possible source of a small timing hiccup right at entry boundaries;
// worth listening for specifically there once this is on hardware.
int   songPlayEntry=0;
int   songPlayRepeatsDone=0;

void songLoadEntry(int idx){
    if(idx<0||idx>=songLen){songPlaying=false;seqPlaying=false;return;}
    SongEntry &e=songEntries[idx];
    loadPatternFromSlot(e.bank,e.slot); // also sets seqTempoBpm/seqSwing from the pattern file
    if(!songInheritTempoSwing){seqTempoBpm=songTempoBpm;seqSwing=songSwing;}
    songTransposeMult=powf(2.f,(float)e.transpose/12.f);
    songPlayEntry=idx;
    songPlayRepeatsDone=0;
    seqPlayStep=0;
    seqLastStepMs=millis();
}

void songAdvanceOnPassComplete(){
    songPlayRepeatsDone++;
    if(songPlayRepeatsDone<songEntries[songPlayEntry].repeat)return; // keep repeating the current entry
    int next=songPlayEntry+1;
    if(next>=songLen){
        if(songLoopAtEnd){next=0;}
        else{
            songPlaying=false;
            seqPlaying=false;
            currentFreq=0.f;
            return;
        }
    }
    songLoadEntry(next);
}

void songTogglePlay(){
    if(songLen==0)return;
    if(songPlaying){
        songPlaying=false;
        seqPlaying=false;
        currentFreq=0.f;
        seqSliding=false;
        seqAccentCutoffBoostTarget=0.f;
        seqAccentResoBoostTarget=0.f;
        seqVelocityMult=1.0f;
    } else {
        songPlaying=true;
        seqPlaying=true;
        songLoadEntry(0);
    }
}

void updateSongEditor(){
    if(songIoPickerOpen){updateSongIoPicker();return;}
    auto s=M5Cardputer.Keyboard.keysState();
    bool up=false,down=false,left=false,right=false,fieldKey=false,focusKey=false;
    bool saveKey=false,loadKey=false,inheritKey=false,loopKey=false,hKey=false,playKey=false;
    bool hLatchKey=false;
    for(char c:s.word){
        if(c==',')up=true;   if(c=='/')down=true;
        if(c==';')left=true; if(c=='.')right=true;
        if(c=='g')fieldKey=true;
        if(c=='f')focusKey=true;
        if(s.shift&&(c=='s'||c=='S'))saveKey=true;
        if(s.shift&&(c=='l'||c=='L'))loadKey=true;
        if(c=='i'||c=='I')inheritKey=true;
        if(c=='o'||c=='O')loopKey=true;
        // Sh+H:Latch, matching PLAY/SEQ exactly (v0.9996x) — SONG only
        // had hold before, on request for parity across all three.
        // helpLatched/prevHelpLatchPressed are the same globals PLAY/SEQ
        // use, so a latch set in one screen carries over to this one too,
        // which is the unified behaviour that was actually asked for.
        if(c=='H'||(c=='h'&&s.shift))hLatchKey=true;
        else if(c=='h')hKey=true;
        if(c==' ')playKey=true;
    }
    if(hLatchKey&&!prevHelpLatchPressed)helpLatched=!helpLatched;
    prevHelpLatchPressed=hLatchKey;
    helpVisible=hKey||helpLatched; // hold OR latch, same as PLAY/SEQ
    bool insertKey=s.enter, deleteKey=s.del, tabKey=s.tab;

    if(playKey&&!prevSongPlayKeyPressed)songTogglePlay();
    prevSongPlayKeyPressed=playKey;

    if(saveKey&&!prevSongSaveKeyPressed){songIoMode=SongIoMode::SAVE;songIoPickerOpen=true;}
    prevSongSaveKeyPressed=saveKey;
    if(loadKey&&!prevSongLoadKeyPressed){songIoMode=SongIoMode::LOAD;songIoPickerOpen=true;}
    prevSongLoadKeyPressed=loadKey;
    if(inheritKey&&!prevSongInheritKeyPressed)songInheritTempoSwing=!songInheritTempoSwing;
    prevSongInheritKeyPressed=inheritKey;
    if(loopKey&&!prevSongLoopKeyPressed)songLoopAtEnd=!songLoopAtEnd;
    prevSongLoopKeyPressed=loopKey;

    if(up&&!prevSongUpPressed&&songLen>0)songCursorEntry=(songCursorEntry-1+songLen)%songLen;
    if(down&&!prevSongDownPressed&&songLen>0)songCursorEntry=(songCursorEntry+1)%songLen;
    prevSongUpPressed=up;prevSongDownPressed=down;

    if(focusKey&&!prevSongFocusKeyPressed)songFocus=(songFocus==SongFocus::ENTRY)?SongFocus::SETTINGS:SongFocus::ENTRY;
    prevSongFocusKeyPressed=focusKey;

    if(fieldKey&&!prevSongFieldKeyPressed){
        if(songFocus==SongFocus::ENTRY)songField=(SongField)(((uint8_t)songField+1)%4);
        else                           songSettingsField=(songSettingsField==SongSettingsField::TEMPO)?SongSettingsField::SWING:SongSettingsField::TEMPO;
    }
    prevSongFieldKeyPressed=fieldKey;

    if(songFocus==SongFocus::SETTINGS){
        // Step 5->1, hold-to-repeat added (v0.9996x, corrected). Reusing
        // menuIncHeldMs/menuDecHeldMs was WRONG — those track '/' and ','
        // (updateMenuNavigation()'s mI/mDe), not ';'/'.' , which is what
        // left/right actually are here. updateMenuNavigation() runs
        // unconditionally every frame regardless of appMode and clears
        // menuIncHeldMs whenever '/' isn't down — which while holding
        // '.' for right is always, so it zeroed the timer out from under
        // this on literally the next frame: the initial press fired
        // (menuKeyFire's own !prev branch), nothing after did. Correct
        // pairing is menuDownHeldMs (tracks '.', same key as right) and
        // menuUpHeldMs (tracks ';', same key as left) — the reset logic
        // stays in sync because it is now watching the actual key being
        // held, not an unrelated one.
        bool edgeInc=right&&!prevSongRightPressed, edgeDec=left&&!prevSongLeftPressed;
        if(songSettingsField==SongSettingsField::TEMPO){
            if(menuKeyFire(right,prevSongRightPressed,menuDownHeldMs,menuDownLastMs))songTempoBpm=min(240.f,songTempoBpm+1.f);
            if(menuKeyFire(left,prevSongLeftPressed,menuUpHeldMs,menuUpLastMs))songTempoBpm=max(40.f,songTempoBpm-1.f);
        } else {
            if(menuKeyFire(right,prevSongRightPressed,menuDownHeldMs,menuDownLastMs))songSwing=min(100.f,songSwing+1.f);
            if(menuKeyFire(left,prevSongLeftPressed,menuUpHeldMs,menuUpLastMs))songSwing=max(-100.f,songSwing-1.f);
        }
    } else if(songLen>0&&(left||right)){
        SongEntry &e=songEntries[songCursorEntry];
        bool inc=right,dec=left;
        bool edgeInc=inc&&!prevSongRightPressed, edgeDec=dec&&!prevSongLeftPressed;
        switch(songField){
            case SongField::BANK:
                if(edgeInc)e.bank=(e.bank+1)%NUM_PATTERN_BANKS;
                if(edgeDec)e.bank=(e.bank-1+NUM_PATTERN_BANKS)%NUM_PATTERN_BANKS;
                break;
            case SongField::SLOT:
                if(edgeInc)e.slot=(e.slot+1)%NUM_PATTERNS_PER_BANK;
                if(edgeDec)e.slot=(e.slot-1+NUM_PATTERNS_PER_BANK)%NUM_PATTERNS_PER_BANK;
                break;
            case SongField::TRANSPOSE:
                if(edgeInc)e.transpose=(int8_t)min(24,(int)e.transpose+1);
                if(edgeDec)e.transpose=(int8_t)max(-24,(int)e.transpose-1);
                break;
            case SongField::REPEAT:
                if(edgeInc)e.repeat=(uint8_t)min(16,(int)e.repeat+1);
                if(edgeDec)e.repeat=(uint8_t)max(1,(int)e.repeat-1);
                break;
        }
    }
    prevSongLeftPressed=left;prevSongRightPressed=right;

    // Enter inserts a new entry right after the cursor (or as the first
    // entry if the song is empty), inheriting the Bank/Slot the cursor
    // was already on (so building up a song from similar patterns is
    // quick) but resetting Transpose/Repeat to their defaults, since
    // those are specific to whichever entry they came from and carrying
    // them forward was more surprising than helpful.
    if(insertKey&&!prevSongInsertKeyPressed&&songLen<SONG_MAX_ENTRIES){
        int insertAt=(songLen>0)?songCursorEntry+1:0;
        SongEntry newEntry;
        if(songLen>0){
            newEntry.bank=songEntries[songCursorEntry].bank;
            newEntry.slot=songEntries[songCursorEntry].slot;
        }
        for(int i=songLen;i>insertAt;i--)songEntries[i]=songEntries[i-1];
        songEntries[insertAt]=newEntry;
        songLen++;
        songCursorEntry=insertAt;
    }
    prevSongInsertKeyPressed=insertKey;

    if(deleteKey&&!prevSongDeleteKeyPressed&&songLen>0){
        for(int i=songCursorEntry;i<songLen-1;i++)songEntries[i]=songEntries[i+1];
        songLen--;
        if(songCursorEntry>=songLen)songCursorEntry=max(0,songLen-1);
    }
    prevSongDeleteKeyPressed=deleteKey;

    if(tabKey&&!prevSongTabPressed)appMode=lastMainMode;
    prevSongTabPressed=tabKey;
}

// ==========================================================
// Timbre Morph Order (v0.984)
// ==========================================================
// ;/. moves the library-list cursor; Enter toggles whether the
// highlighted waveform is included in the active morph chain (subject
// to MIN_MORPH_SLOTS/MAX_MORPH_SLOTS); ,// reorders it earlier/later
// within the chain (only meaningful while it's included). Tab saves and
// returns to SETTINGS, same pattern as the other category sub-screens.
int  timbreCursor=0;
bool prevTimbreUpPressed=false, prevTimbreDownPressed=false;
bool prevTimbreLeftPressed=false, prevTimbreRightPressed=false;
bool prevTimbreToggleKeyPressed=false, prevTimbreTabPressed=false;

void openTimbreScreen(){
    timbreCursor=0;
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldUp=false,heldDown=false,heldLeft=false,heldRight=false;
    for(char c:s.word){
        if(c==';')heldUp=true;   if(c=='.')heldDown=true;
        if(c==',')heldLeft=true; if(c=='/')heldRight=true;
    }
    prevTimbreUpPressed=heldUp;prevTimbreDownPressed=heldDown;
    prevTimbreLeftPressed=heldLeft;prevTimbreRightPressed=heldRight;
    prevTimbreToggleKeyPressed=s.enter;
    prevTimbreTabPressed=s.tab;
    appMode=AppMode::TIMBRE;
}

void updateTimbreScreen(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool up=false,down=false,left=false,right=false;
    for(char c:s.word){
        if(c==';')up=true;   if(c=='.')down=true;
        if(c==',')left=true; if(c=='/')right=true;
    }
    bool toggleKey=s.enter, tabKey=s.tab;

    if(up&&!prevTimbreUpPressed)timbreCursor=(timbreCursor-1+NUM_OSC_WAVEFORMS)%NUM_OSC_WAVEFORMS;
    if(down&&!prevTimbreDownPressed)timbreCursor=(timbreCursor+1)%NUM_OSC_WAVEFORMS;
    prevTimbreUpPressed=up;prevTimbreDownPressed=down;

    OscWaveform w=(OscWaveform)timbreCursor;

    if(toggleKey&&!prevTimbreToggleKeyPressed){
        // Locked (v0.99943): this screen edits the live chain while it is
        // actively sounding — the same audioTask race morphStart() has,
        // just triggered by hand instead of by picking a slot. See
        // morphChainMux's declaration.
        portENTER_CRITICAL(&morphChainMux);
        int slot=morphChainSlotOf(w);
        if(slot>=0){
            // Remove — but never below the minimum, so there's always
            // something to morph between.
            if(morphChainLen>MIN_MORPH_SLOTS){
                for(int i=slot;i<morphChainLen-1;i++)morphChain[i]=morphChain[i+1];
                morphChainLen--;
                params.timbreMorph=constrain(params.timbreMorph,0.f,(float)(morphChainLen-1));
                params.timbreMorphTarget=params.timbreMorph;
            }
        } else if(morphChainLen<MAX_MORPH_SLOTS){
            morphChain[morphChainLen]=w;
            morphChainLen++;
        }
        portEXIT_CRITICAL(&morphChainMux);
        refreshMorphTablePtrs();
    }
    prevTimbreToggleKeyPressed=toggleKey;

    if((left||right)){
        int slot=morphChainSlotOf(w);
        if(slot>=0){
            // Locked for the same reason as the add/remove branch above.
            portENTER_CRITICAL(&morphChainMux);
            if(left&&!prevTimbreLeftPressed&&slot>0){
                OscWaveform tmp=morphChain[slot-1];morphChain[slot-1]=morphChain[slot];morphChain[slot]=tmp;
            }
            if(right&&!prevTimbreRightPressed&&slot<morphChainLen-1){
                OscWaveform tmp=morphChain[slot+1];morphChain[slot+1]=morphChain[slot];morphChain[slot]=tmp;
            }
            portEXIT_CRITICAL(&morphChainMux);
            if(left||right)refreshMorphTablePtrs();
        }
    }
    prevTimbreLeftPressed=left;prevTimbreRightPressed=right;

    if(tabKey&&!prevTimbreTabPressed){saveSettings();appMode=AppMode::SETTINGS;}
    prevTimbreTabPressed=tabKey;
}

// ==========================================================
// Patch Bank
// ==========================================================
// Patches are stored as individual settings.json-style files under
// /CPS/Patch/<name>.json. The app never lets the user browse outside
// this folder, by design (see PATCH_FOLDER_PATH usage below).
static const char *PATCH_FOLDER_PATH = "/CPS/Patch";
constexpr int PATCH_NAME_MAX_LEN = 20;
// Raised from 32 (v0.997). Patch sharing means collecting other people's
// files, and 32 is easy to pass; over the limit scanPatches() silently
// stopped adding, so the extras simply did not appear with nothing to say
// why. Each entry is a String, so this costs little until the slots are
// actually used.
constexpr int MAX_PATCHES = 64;

bool ensurePatchFolder(){return SD.exists(PATCH_FOLDER_PATH)||SD.mkdir(PATCH_FOLDER_PATH);}

enum class PatchMode   : uint8_t { LOAD, SAVE };
enum class PatchUiState: uint8_t { BROWSE, NAME_ENTRY, CONFIRM_DELETE, CONFIRM_OVERWRITE };

PatchMode    patchMode      = PatchMode::LOAD;
PatchUiState patchUiState   = PatchUiState::BROWSE;
String       patchNames[MAX_PATCHES];
int          patchCount     = 0;
int          selectedPatchIndex = 0;
int          patchActionIndex   = -1;   // index into patchNames[] being renamed/duplicated/deleted/overwritten
String       patchNameBuffer;
bool         patchRenaming    = false;
bool         patchDuplicating = false;

bool prevPatchUpPressed=false, prevPatchDownPressed=false;
bool prevPatchConfirmPressed=false, prevPatchTabPressed=false;
bool prevPatchRenamePressed=false, prevPatchDupPressed=false, prevPatchDeleteKeyPressed=false;
bool prevPatchEnterPressed=false, prevPatchDelPressed=false;
std::vector<char> prevPatchTypedWord;

bool isValidPatchChar(char c){
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c==' ';
}

// Defined with the morph code further down; the patch browser above it is
// the caller (v0.99881).
void morphRefreshSlotsNamed(const String &name);

String patchFilePath(const String &name){return String(PATCH_FOLDER_PATH)+"/"+name+".json";}

void scanPatches(){
    patchCount=0;
    File dir=SD.open(PATCH_FOLDER_PATH);
    if(!dir)return;
    File f=dir.openNextFile();
    while(f){
        if(!f.isDirectory()){
            String n=String(f.name());
            int slash=n.lastIndexOf('/');if(slash>=0)n=n.substring(slash+1);
            if(n.endsWith(".json")&&patchCount<MAX_PATCHES)
                patchNames[patchCount++]=n.substring(0,n.length()-5);
        }
        f=dir.openNextFile();
    }
    dir.close();
    // Alphabetical sort (small N, simple insertion-style bubble sort is fine)
    for(int i=0;i<patchCount-1;i++)
        for(int j=0;j<patchCount-1-i;j++)
            if(patchNames[j]>patchNames[j+1]){String t=patchNames[j];patchNames[j]=patchNames[j+1];patchNames[j+1]=t;}
}

// Number of selectable rows in the browse list. SAVE mode has an extra
// "<New Patch>" placeholder row at the top.
int patchListCount(){return patchCount+(patchMode==PatchMode::SAVE?1:0);}
bool patchIsNewRow(int row){return patchMode==PatchMode::SAVE&&row==0;}
int  patchRowToNameIndex(int row){return patchMode==PatchMode::SAVE?row-1:row;}

void clampPatchSelection(){
    int c=patchListCount();
    if(selectedPatchIndex>=c)selectedPatchIndex=max(0,c-1);
    if(selectedPatchIndex<0)selectedPatchIndex=0;
}

void enterPatchBrowser(PatchMode mode){
    patchMode=mode;
    scanPatches();
    selectedPatchIndex=0;
    patchUiState=PatchUiState::BROWSE;
    patchRenaming=false;patchDuplicating=false;

    // Seed edge-trackers with whatever is CURRENTLY held so the very key
    // press that opened this screen (e.g. '/' or ',' on the SETTINGS menu)
    // isn't immediately re-read as a brand-new press inside the browser
    // (which previously caused an instant delete-confirm / false-select).
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldUp=false,heldDown=false,heldConfirm=s.enter,heldDelete=false,heldRename=false,heldDup=false;
    for(char c:s.word){
        if(c==';')heldUp=true;   if(c=='.')heldDown=true;
        if(c=='/')heldConfirm=true;
        if(c==',')heldDelete=true;
        if(c=='r')heldRename=true;
        if(c=='c')heldDup=true;
    }
    prevPatchUpPressed=heldUp; prevPatchDownPressed=heldDown;
    prevPatchConfirmPressed=heldConfirm; prevPatchTabPressed=s.tab;
    prevPatchRenamePressed=heldRename; prevPatchDupPressed=heldDup; prevPatchDeleteKeyPressed=heldDelete;
    prevPatchEnterPressed=s.enter; prevPatchDelPressed=s.del;
    prevPatchTypedWord=s.word;
    appMode=AppMode::PATCH;
}

void patchSaveEnter(){enterPatchBrowser(PatchMode::SAVE);}
void patchLoadEnter(){enterPatchBrowser(PatchMode::LOAD);}
const char *patchEnterLabel(){return "Select>";}

void updatePatchBrowser(){
    auto s=M5Cardputer.Keyboard.keysState();

    if(patchUiState==PatchUiState::NAME_ENTRY){
        for(char c:s.word){
            bool wasPressed=false;
            for(char p:prevPatchTypedWord)if(p==c){wasPressed=true;break;}
            if(!wasPressed&&isValidPatchChar(c)&&patchNameBuffer.length()<PATCH_NAME_MAX_LEN)
                patchNameBuffer+=c;
        }
        prevPatchTypedWord=s.word;

        if(s.del&&!prevPatchDelPressed&&patchNameBuffer.length()>0)
            patchNameBuffer.remove(patchNameBuffer.length()-1);
        prevPatchDelPressed=s.del;

        if(s.enter&&!prevPatchEnterPressed){
            String trimmed=patchNameBuffer;trimmed.trim();
            if(trimmed.length()>0){
                if(patchRenaming){
                    SD.rename(patchFilePath(patchNames[patchActionIndex]).c_str(),patchFilePath(trimmed).c_str());
                } else if(patchDuplicating){
                    File src=SD.open(patchFilePath(patchNames[patchActionIndex]).c_str(),FILE_READ);
                    File dst=SD.open(patchFilePath(trimmed).c_str(),FILE_WRITE);
                    if(src&&dst)while(src.available())dst.write(src.read());
                    if(src)src.close();
                    if(dst)dst.close();
                } else {
                    savePatchToFile(patchFilePath(trimmed).c_str());
                    morphRefreshSlotsNamed(trimmed);   // v0.99881
                }
            }
            scanPatches();clampPatchSelection();
            patchUiState=PatchUiState::BROWSE;
            patchRenaming=false;patchDuplicating=false;
        }
        prevPatchEnterPressed=s.enter;

        if(s.tab&&!prevPatchTabPressed){
            patchUiState=PatchUiState::BROWSE;
            patchRenaming=false;patchDuplicating=false;
        }
        prevPatchTabPressed=s.tab;
        return;
    }

    if(patchUiState==PatchUiState::CONFIRM_DELETE||patchUiState==PatchUiState::CONFIRM_OVERWRITE){
        bool confirm=s.enter,cancel=s.tab;
        for(char c:s.word)if(c=='/')confirm=true;
        if(confirm&&!prevPatchConfirmPressed){
            if(patchUiState==PatchUiState::CONFIRM_DELETE){
                SD.remove(patchFilePath(patchNames[patchActionIndex]).c_str());
            } else {
                savePatchToFile(patchFilePath(patchNames[patchActionIndex]).c_str());
                morphRefreshSlotsNamed(patchNames[patchActionIndex]);   // v0.99881
            }
            scanPatches();clampPatchSelection();
            patchUiState=PatchUiState::BROWSE;
        }
        prevPatchConfirmPressed=confirm;
        if(cancel&&!prevPatchTabPressed)patchUiState=PatchUiState::BROWSE;
        prevPatchTabPressed=cancel;
        return;
    }

    // BROWSE
    bool mU=false,mD=false,confirm=s.enter,doDelete=false,doRename=false,doDup=false,cancel=s.tab;
    for(char c:s.word){
        if(c==';')mU=true; if(c=='.')mD=true;
        if(c=='/')confirm=true;
        if(c==',')doDelete=true;
        if(c=='r')doRename=true;
        if(c=='c')doDup=true;
    }

    int count=patchListCount();
    if(mU&&!prevPatchUpPressed&&count>0)selectedPatchIndex=(selectedPatchIndex-1+count)%count;
    if(mD&&!prevPatchDownPressed&&count>0)selectedPatchIndex=(selectedPatchIndex+1)%count;
    prevPatchUpPressed=mU;prevPatchDownPressed=mD;

    if(confirm&&!prevPatchConfirmPressed&&count>0){
        if(patchIsNewRow(selectedPatchIndex)){
            patchNameBuffer="";
            patchRenaming=false;patchDuplicating=false;
            prevPatchTypedWord=s.word;
            prevPatchEnterPressed=s.enter;prevPatchDelPressed=s.del;
            patchUiState=PatchUiState::NAME_ENTRY;
        } else {
            int ni=patchRowToNameIndex(selectedPatchIndex);
            if(patchMode==PatchMode::LOAD){
                // A patch replaces the tone wholesale, so absent keys must
                // mean default rather than keep (v0.9932).
                loadSettingsFromFile(patchFilePath(patchNames[ni]).c_str(),true);
                appMode=AppMode::CATEGORY;
            } else {
                patchActionIndex=ni;
                patchUiState=PatchUiState::CONFIRM_OVERWRITE;
            }
        }
    }
    prevPatchConfirmPressed=confirm;

    if(doRename&&!prevPatchRenamePressed&&count>0&&!patchIsNewRow(selectedPatchIndex)){
        patchActionIndex=patchRowToNameIndex(selectedPatchIndex);
        patchNameBuffer=patchNames[patchActionIndex];
        patchRenaming=true;patchDuplicating=false;
        prevPatchTypedWord=s.word;
        prevPatchEnterPressed=s.enter;prevPatchDelPressed=s.del;
        patchUiState=PatchUiState::NAME_ENTRY;
    }
    prevPatchRenamePressed=doRename;

    if(doDup&&!prevPatchDupPressed&&count>0&&!patchIsNewRow(selectedPatchIndex)){
        patchActionIndex=patchRowToNameIndex(selectedPatchIndex);
        String suggested=patchNames[patchActionIndex]+"_copy";
        if(suggested.length()>PATCH_NAME_MAX_LEN)suggested=suggested.substring(0,PATCH_NAME_MAX_LEN);
        patchNameBuffer=suggested;
        patchRenaming=false;patchDuplicating=true;
        prevPatchTypedWord=s.word;
        prevPatchEnterPressed=s.enter;prevPatchDelPressed=s.del;
        patchUiState=PatchUiState::NAME_ENTRY;
    }
    prevPatchDupPressed=doDup;

    if(doDelete&&!prevPatchDeleteKeyPressed&&count>0&&!patchIsNewRow(selectedPatchIndex)){
        patchActionIndex=patchRowToNameIndex(selectedPatchIndex);
        patchUiState=PatchUiState::CONFIRM_DELETE;
    }
    prevPatchDeleteKeyPressed=doDelete;

    if(cancel&&!prevPatchTabPressed)appMode=AppMode::CATEGORY;
    prevPatchTabPressed=cancel;
}

// ==========================================================
// SETTING menu items
// ==========================================================
const char *imuXLabel(){return imuTargetName(imuAxisX.target);}
const char *imuYLabel(){return imuTargetName(imuAxisY.target);}

// ---------------------------------------------------------
// IMU target picker (scrollable list w/ category dividers)
// ---------------------------------------------------------
// Display order for the picker, grouped by category. This is independent
// from the ImuTarget enum's numeric order (which must stay stable for
// backward-compatible save files) — it's purely a visual arrangement.
// group: non-null starts a new section and names it (v0.992). The list is
// 29 rows deep with 7 on screen, so dividers alone left you scrolling with
// no idea which part of the synth you were in; the name of the current
// section now shows in the title bar.
struct ImuPickerRow { ImuTarget target; bool divider; const char *group; };
const ImuPickerRow IMU_PICKER_ORDER[]={
    // Section order follows the TAB BAR (v0.9934): VCO, VCF, VCA, LFO, FX,
    // then the two that have no tab of their own. FX was near the top for
    // a while because its effects are the easiest to hear, but now that
    // the list is split into sections there is no longer a long scroll to
    // save anyone from — so matching the tabs is worth more than
    // prioritising by impact, and there is one order to learn instead of
    // two.
    {ImuTarget::TIMBRE,        false, "VCO"},
    {ImuTarget::SHAPE,         false, nullptr},
    {ImuTarget::OSC_MIX,       false, nullptr},
    {ImuTarget::OSC2_SHAPE,    false, nullptr},
    {ImuTarget::DETUNE,        false, nullptr},
    {ImuTarget::NOISE,         false, nullptr},
    {ImuTarget::SUB_LEVEL,     false, nullptr},
    {ImuTarget::FILTER_CUTOFF, true , "VCF"},
    {ImuTarget::RESONANCE,     false, nullptr},
    {ImuTarget::VOLUME,        true , "VCA"},
    {ImuTarget::TREMOLO,       false, nullptr},
    {ImuTarget::LFO_RATE,      true , "LFO"},
    {ImuTarget::LFO_DEPTH,     false, nullptr},
    // FX in the same order as the pads on the FX tab.
    {ImuTarget::FX_RING_RATE,    true , "FX"},
    {ImuTarget::FX_RING_MIX,     false, nullptr},
    {ImuTarget::FX_LIMIT_DRIVE,  false, nullptr},
    {ImuTarget::FX_CHORUS_DEPTH, false, nullptr},
    {ImuTarget::FX_CHORUS_MIX,   false, nullptr},
    {ImuTarget::FX_DELAY_FB,     false, nullptr},
    {ImuTarget::FX_DELAY_MIX,    false, nullptr},
    {ImuTarget::FX_REVERB_ROOM,  false, nullptr},
    {ImuTarget::FX_REVERB_MIX,   false, nullptr},
    {ImuTarget::BITCRUSH,        false, nullptr},
    // No tab of their own, so they follow the tabbed sections.
    {ImuTarget::VIBRATO_DEPTH, true , "Pitch"},
    {ImuTarget::VIBRATO_RATE,  false, nullptr},
    {ImuTarget::PITCH_BEND,    false, nullptr},
    {ImuTarget::BEND_UP,       false, nullptr},
    {ImuTarget::BEND_DOWN,     false, nullptr},
    {ImuTarget::ARP_TEMPO,     true , "ARP / SEQ"},
    {ImuTarget::ARP_SWING,     false, nullptr},
};
constexpr int IMU_PICKER_COUNT=sizeof(IMU_PICKER_ORDER)/sizeof(IMU_PICKER_ORDER[0]);

// Section table, derived from the group labels above rather than written
// out separately — one list to keep in step instead of two.
struct ImuPickerSection { const char *name; int first,count; };
ImuPickerSection IMU_PICKER_SECTIONS[12];
int IMU_PICKER_SECTION_COUNT=0;
void buildImuPickerSections(){
    IMU_PICKER_SECTION_COUNT=0;
    for(int i=0;i<IMU_PICKER_COUNT;i++){
        if(IMU_PICKER_ORDER[i].group&&IMU_PICKER_SECTION_COUNT<12){
            IMU_PICKER_SECTIONS[IMU_PICKER_SECTION_COUNT].name=IMU_PICKER_ORDER[i].group;
            IMU_PICKER_SECTIONS[IMU_PICKER_SECTION_COUNT].first=i;
            IMU_PICKER_SECTIONS[IMU_PICKER_SECTION_COUNT].count=0;
            IMU_PICKER_SECTION_COUNT++;
        }
        if(IMU_PICKER_SECTION_COUNT>0)IMU_PICKER_SECTIONS[IMU_PICKER_SECTION_COUNT-1].count++;
    }
}
int imuPickerSectionOfRow(int row){
    for(int sIdx=IMU_PICKER_SECTION_COUNT-1;sIdx>=0;sIdx--)
        if(row>=IMU_PICKER_SECTIONS[sIdx].first)return sIdx;
    return 0;
}

bool imuPickerOpen=false;
// 0 = X, 1 = Y, or 2/3 for the two CC-in destination slots (v0.99882).
// The picker is reused rather than duplicated: it already presents the
// targets in sections with names, which is exactly what the CC
// destinations needed once there were thirty of them to scroll past. A
// second picker would have meant two lists to keep in step.
int  imuPickerAxis=0;
constexpr int IMU_PICKER_AXIS_CC0=2,IMU_PICKER_AXIS_CC1=3;
int  imuPickerIndex=0;  // row index into IMU_PICKER_ORDER
// Two levels, like the scale picker (v0.993). 28 targets in one flat list
// meant scrolling four screens to reach the far end; picking the section
// first turns that into two short lists. Same shape the scale picker has
// used since v0.954, so it is a structure already learned here.
int  imuPickerLevel=0;      // 0 = section list, 1 = targets within a section
int  imuPickerSection=0;    // index into IMU_PICKER_SECTIONS
int  imuPickerSectionRow=0; // cursor within the section list

bool prevImuPickerUpPressed=false, prevImuPickerDownPressed=false;
bool prevImuPickerConfirmPressed=false, prevImuPickerTabPressed=false;

int imuPickerRowForTarget(ImuTarget t){
    for(int i=0;i<IMU_PICKER_COUNT;i++)if(IMU_PICKER_ORDER[i].target==t)return i;
    return 0;
}

void openImuPicker(int axis){
    imuPickerAxis=axis;
    buildImuPickerSections();
    ImuTarget cur = (axis==0)                  ? imuAxisX.target
                  : (axis==1)                  ? imuAxisY.target
                  : (axis==IMU_PICKER_AXIS_CC0)? midiCcInTarget[0]
                                               : midiCcInTarget[1];
    imuPickerIndex=imuPickerRowForTarget(cur);
    // Open on the section list, but positioned at whatever is currently
    // assigned, so the axis's existing target is one keypress away rather
    // than something to go hunting for.
    if(imuPickerIndex<0)imuPickerIndex=0;
    imuPickerSection=imuPickerSectionOfRow(imuPickerIndex);
    imuPickerSectionRow=imuPickerSection;
    imuPickerLevel=0;
    // Seed edge-trackers with whatever is CURRENTLY held, so the same '/'
    // keypress that opened this picker isn't immediately re-read as a
    // fresh confirm (same fix as the Patch Bank carry-over bug).
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldUp=false,heldDown=false,heldConfirm=s.enter;
    for(char c:s.word){if(c==';')heldUp=true;if(c=='.')heldDown=true;if(c=='/')heldConfirm=true;}
    prevImuPickerUpPressed=heldUp;prevImuPickerDownPressed=heldDown;
    prevImuPickerConfirmPressed=heldConfirm;prevImuPickerTabPressed=s.tab;
    imuPickerOpen=true;
}
void imuXOpenPicker(){openImuPicker(0);}
void imuYOpenPicker(){openImuPicker(1);}

// Everything OUTSIDE SynthParams that parseSettingLine() can write.
//
// morphLoadSlot() loads a patch over the live state to snapshot it, then
// puts the live state back. Restoring only params/filter/ADSR was not
// enough (v0.9955): a patch file also carries Play Style, Scale, Drift,
// key volume, the IMU mapping, the LFO, arp and portamento settings, and
// all of those were left holding whatever the LAST slot's patch said. The
// next settings save then wrote that to settings.json, which is why Play
// Style kept coming back as Pro/Hirajoshi and Drift as ON at 100% after
// every reboot — values from a morph slot's patch, made permanent.
struct GlobalStateBackup {
    ImuAxisConfig ax,ay;
    bool imuXE,imuYE,imuXH,imuYH,imuCal;
    float imuXN,imuYN;
    float bendMax,bendAtk,bendRel;
    bool porta; float portaSp;
    FilterEnvParams fenv;
    LfoParams lfoCfg;
    PlayMode pm; int scaleIdx;
    bool driftOn; float driftAmt;
    bool arpOn; ArpType arpTy; float arpTempo,arpSw; int arpRate;
    float seqTempo,seqSw;
    int themeIdx; uint8_t bright;
};
void backupGlobals(GlobalStateBackup &b){
    b.ax=imuAxisX; b.ay=imuAxisY;
    b.imuXE=imuXEnabled; b.imuYE=imuYEnabled;
    b.imuXH=imuXHeld;    b.imuYH=imuYHeld;
    b.imuCal=imuCalibrated; b.imuXN=imuXLastNorm; b.imuYN=imuYLastNorm;
    b.bendMax=keyBendMaxCents; b.bendAtk=keyBendAttackSmooth; b.bendRel=keyBendReleaseSmooth;
    b.porta=portaEnabled; b.portaSp=portaSpeed;
    b.fenv=filterEnv; b.lfoCfg=lfo;
    b.pm=playMode; b.scaleIdx=currentScaleIndex;
    b.driftOn=analogDriftOn; b.driftAmt=analogDriftAmount;
    b.arpOn=arpEnabled; b.arpTy=arpType; b.arpTempo=arpTempoBpm; b.arpSw=arpSwing; b.arpRate=arpRateIndex;
    b.seqTempo=seqTempoBpm; b.seqSw=seqSwing;
    b.themeIdx=uiThemeIndex; b.bright=uiBrightness;
}
void restoreGlobals(const GlobalStateBackup &b){
    imuAxisX=b.ax; imuAxisY=b.ay;
    imuXEnabled=b.imuXE; imuYEnabled=b.imuYE;
    imuXHeld=b.imuXH;    imuYHeld=b.imuYH;
    imuCalibrated=b.imuCal; imuXLastNorm=b.imuXN; imuYLastNorm=b.imuYN;
    keyBendMaxCents=b.bendMax; keyBendAttackSmooth=b.bendAtk; keyBendReleaseSmooth=b.bendRel;
    portaEnabled=b.porta; portaSpeed=b.portaSp;
    filterEnv=b.fenv; lfo=b.lfoCfg;
    playMode=b.pm; currentScaleIndex=b.scaleIdx;
    analogDriftOn=b.driftOn; analogDriftAmount=b.driftAmt;
    // Bounds-checked (v0.9997x) — this was the only one of four
    // arpRateIndex write sites without one; unlikely to be reachable in
    // practice since b.arpRate is captured from an already-valid live
    // value, but cheap to guard regardless.
    arpEnabled=b.arpOn; arpType=b.arpTy; arpTempoBpm=b.arpTempo; arpSwing=b.arpSw;
    if(b.arpRate>=0&&b.arpRate<NUM_ARP_RATES)arpRateIndex=b.arpRate;
    seqTempoBpm=b.seqTempo; seqSwing=b.seqSw;
    uiThemeIndex=b.themeIdx; uiBrightness=b.bright;
    applyUiTheme(); applyUiBrightness();
    recomputeKeyNotes();   // play mode / scale may have been clobbered
}

// Rebuild a slot's snapshot from its patch file. Runs at startup and when
// an assignment changes — never while playing (v0.995).
void morphLoadSlot(int i){
    if(!morphSlotsReady())return;
    morphSlots[i].used=false;
    if(i<0||i>=NUM_MORPH_SLOTS||!morphSlotPatch[i].length())return;
    String path=patchFilePath(morphSlotPatch[i]);
    if(!SD.exists(path.c_str())){morphSlotPatch[i]="";return;}
    // Snapshot the live sound, load the patch over the top, snapshot THAT,
    // then put the live sound back. Roundabout, but it reuses the existing
    // loader instead of teaching the parser to write somewhere else — and
    // it only ever runs while stopped, so the brief detour is unheard.
    PatchSnapshot keep; morphCapture(keep);
    GlobalStateBackup gb; backupGlobals(gb);
    loadSettingsFromFile(path.c_str(),true);
    morphCapture(morphSlots[i]);
    snprintf(morphSlots[i].name,sizeof(morphSlots[i].name),"%s",morphSlotPatch[i].c_str());
    morphSlots[i].used=true;
    // Two values that identify a patch at a glance, logged so a snapshot
    // can be compared against the file it came from (v0.99881). If a slot
    // still reports the pre-edit numbers after a reboot, the fault is in
    // reading the file rather than in when the snapshot was taken — and
    // those two look identical from the outside.
#if CPS_LOG_PATCH
    Serial.printf("[Morph] slot %d <- %s (cutoff %.0f, atk %.3f)\n",
        i+1,morphSlotPatch[i].c_str(),morphSlots[i].cutoffHz,morphSlots[i].adsrA);
#endif
    // Restore, discrete state included.
    params=keep.p;
    // Locked for the same reason as morphStart() (v0.99943).
    portENTER_CRITICAL(&morphChainMux);
    for(int k=0;k<MAX_MORPH_SLOTS;k++)morphChain[k]=keep.chain[k];
    morphChainLen=keep.chainLen;
    portEXIT_CRITICAL(&morphChainMux);
    refreshMorphTablePtrs();
    filterParams.type=(FilterType)keep.filterType;
    filterParams.cutoffHz=keep.cutoffHz; filterParams.resonanceQ=keep.resonanceQ;
    filterParams.keyTracking=keep.keyTracking; filterEnv.depth=keep.fEnvDepth;
    adsr.attackTime=keep.adsrA; adsr.decayTime=keep.adsrD;
    adsr.sustainLevel=keep.adsrS; adsr.releaseTime=keep.adsrR;
    restoreGlobals(gb);
    updateFilterCoefficients();
}
// Rebuild any slot pointing at this patch name (v0.99881).
//
// Snapshots were only built at boot and when an assignment changed, so
// overwriting a patch left every slot holding the OLD sound: Load read the
// updated file, morphing replayed what was captured before the edit. The
// two disagreeing about the same patch name is exactly the sort of thing
// that looks like corruption rather than a missing refresh.
//
// Called after a successful save. Comparing by name rather than by slot
// index because the same patch can occupy several slots.
void morphRefreshSlotsNamed(const String &name){
    if(!morphSlotsReady()||!name.length())return;
    for(int i=0;i<NUM_MORPH_SLOTS;i++){
        if(morphSlotPatch[i]==name){
            morphLoadSlot(i);
            Serial.printf("[Morph] slot %d refreshed from %s\n",i+1,name.c_str());
        }
    }
}

void morphLoadAllSlots(){for(int i=0;i<NUM_MORPH_SLOTS;i++)morphLoadSlot(i);}

void openMorphSlotScreen(){
    // The list of patches to choose from comes from patchNames[], which is
    // only filled when the Load/Save browser is entered — so opening this
    // screen on a fresh boot offered nothing to assign (v0.9954). Scanned
    // here for the same reason the browser scans on entry. This is NOT the
    // boot-time scan removed in v0.9952: that one ran before any other
    // open of the folder and left the handle in a state the next scan came
    // back empty from. Scanning on entering a screen is the established
    // pattern here and has always worked.
    scanPatches();
    morphSlotCursor=0;
    auto st=M5Cardputer.Keyboard.keysState();
    prevMorphUpPressed=false;prevMorphDownPressed=false;
    prevMorphLeftPressed=false;prevMorphRightPressed=false;
    for(char c:st.word){
        if(c==';')prevMorphUpPressed=true;   if(c=='.')prevMorphDownPressed=true;
        if(c==',')prevMorphLeftPressed=true; if(c=='/')prevMorphRightPressed=true;
    }
    prevMorphEnterPressed=st.enter;prevMorphTabPressed=st.tab;prevMorphDelPressed=st.del;
    morphSlotScreenOpen=true;
}

void updateMorphSlotScreen(){
    auto st=M5Cardputer.Keyboard.keysState();
    bool up=false,down=false,left=false,right=false;
    for(char c:st.word){
        if(c==';')up=true;   if(c=='.')down=true;
        if(c==',')left=true; if(c=='/')right=true;
    }
    const int ROWS=NUM_MORPH_SLOTS+1;   // ten slots plus the Time row
    if(up&&!prevMorphUpPressed)  morphSlotCursor=(morphSlotCursor-1+ROWS)%ROWS;
    if(down&&!prevMorphDownPressed)morphSlotCursor=(morphSlotCursor+1)%ROWS;
    prevMorphUpPressed=up;prevMorphDownPressed=down;

    if(morphSlotCursor==NUM_MORPH_SLOTS){
        // Time row. Repeats, since it is an ordinary value.
        if(menuKeyFire(right,prevMorphRightPressed,morphIncHeldMs,morphIncLastMs))
            morphTimeSec=min(morphTimeSec+0.25f,10.f);
        if(menuKeyFire(left,prevMorphLeftPressed,morphDecHeldMs,morphDecLastMs))
            morphTimeSec=max(morphTimeSec-0.25f,0.f);
    } else {
        // ,// step through the available patches for this slot. Edge
        // triggered: each press is a deliberate choice, and repeating
        // would reload a snapshot from the card on every step.
        if((left&&!prevMorphLeftPressed)||(right&&!prevMorphRightPressed)){
            if(patchCount>0){
                int cur=-1;
                for(int k=0;k<patchCount;k++)
                    if(patchNames[k]==morphSlotPatch[morphSlotCursor]){cur=k;break;}
                int n=(right&&!prevMorphRightPressed)?1:-1;
                // -1 is the empty slot, so the cycle runs -1..patchCount-1.
                cur+=n;
                if(cur< -1)cur=patchCount-1;
                if(cur>=patchCount)cur=-1;
                morphSlotPatch[morphSlotCursor]=(cur<0)?String(""):patchNames[cur];
                morphLoadSlot(morphSlotCursor);
            }
        }
        if(st.del&&!prevMorphDelPressed){
            morphSlotPatch[morphSlotCursor]="";
            if(morphSlotsReady())morphSlots[morphSlotCursor].used=false;
        }
    }
    prevMorphLeftPressed=left;prevMorphRightPressed=right;
    prevMorphDelPressed=st.del;

    // Enter auditions the slot, so it can be judged without leaving.
    if(st.enter&&!prevMorphEnterPressed&&morphSlotCursor<NUM_MORPH_SLOTS)
        morphStart(morphSlotCursor);
    prevMorphEnterPressed=st.enter;

    if(st.tab&&!prevMorphTabPressed)morphSlotScreenOpen=false;
    prevMorphTabPressed=st.tab;
}

void updateThemePicker(){
    auto st=M5Cardputer.Keyboard.keysState();
    bool up=false,down=false;
    for(char c:st.word){if(c==';')up=true;if(c=='.')down=true;}
    bool confirm=st.enter;bool cancel=st.tab;

    if(up&&!prevThemeUpPressed)  themePickerIndex=(themePickerIndex-1+NUM_UI_THEMES)%NUM_UI_THEMES;
    if(down&&!prevThemeDownPressed)themePickerIndex=(themePickerIndex+1)%NUM_UI_THEMES;
    prevThemeUpPressed=up;prevThemeDownPressed=down;

    if(confirm&&!prevThemeConfirmPressed){
        uiThemeIndex=themePickerIndex;
        applyUiTheme();uiThemeDirty=true;
        themePickerOpen=false;
    }
    prevThemeConfirmPressed=confirm;

    if(cancel&&!prevThemeTabPressed)themePickerOpen=false;
    prevThemeTabPressed=cancel;
}

void updateImuPicker(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool mU=false,mD=false,confirm=s.enter,cancel=s.tab;
    for(char c:s.word){if(c==';')mU=true;if(c=='.')mD=true;if(c=='/')confirm=true;}

    if(imuPickerLevel==0){
        int n=max(1,IMU_PICKER_SECTION_COUNT);
        if(mU&&!prevImuPickerUpPressed)  imuPickerSectionRow=(imuPickerSectionRow-1+n)%n;
        if(mD&&!prevImuPickerDownPressed)imuPickerSectionRow=(imuPickerSectionRow+1)%n;
    } else {
        const ImuPickerSection &sec=IMU_PICKER_SECTIONS[imuPickerSection];
        int rel=imuPickerIndex-sec.first;
        if(mU&&!prevImuPickerUpPressed)  rel=(rel-1+sec.count)%sec.count;
        if(mD&&!prevImuPickerDownPressed)rel=(rel+1)%sec.count;
        imuPickerIndex=sec.first+rel;
    }
    prevImuPickerUpPressed=mU;prevImuPickerDownPressed=mD;

    if(confirm&&!prevImuPickerConfirmPressed){
        if(imuPickerLevel==0){
            // Drill into the section, landing on its currently assigned
            // target if that is the section we are in.
            imuPickerSection=imuPickerSectionRow;
            const ImuPickerSection &sec=IMU_PICKER_SECTIONS[imuPickerSection];
            if(imuPickerIndex<sec.first||imuPickerIndex>=sec.first+sec.count)
                imuPickerIndex=sec.first;
            imuPickerLevel=1;
        } else {
            ImuTarget newTarget=IMU_PICKER_ORDER[imuPickerIndex].target;
            // Clearing the outgoing target's offset applies to the CC
            // slots for the same reason it applies to an axis: leaving it
            // would freeze that parameter at whatever last drove it.
            if(imuPickerAxis==0){resetParamToDefault(imuAxisX.target);imuAxisX.target=newTarget;}
            else if(imuPickerAxis==1){resetParamToDefault(imuAxisY.target);imuAxisY.target=newTarget;}
            else{
                int slot=(imuPickerAxis==IMU_PICKER_AXIS_CC0)?0:1;
                resetParamToDefault(midiCcInTarget[slot]);
                midiCcInTarget[slot]=newTarget;
            }
            imuPickerOpen=false;
        }
    }
    prevImuPickerConfirmPressed=confirm;

    // Tab backs out one level before closing, matching the scale picker.
    if(cancel&&!prevImuPickerTabPressed){
        if(imuPickerLevel==1)imuPickerLevel=0;
        else                 imuPickerOpen=false;
    }
    prevImuPickerTabPressed=cancel;
}

void bendWInc(){keyBendMaxCents=min(keyBendMaxCents+100.f,1200.f);}
void bendWDec(){keyBendMaxCents=max(keyBendMaxCents-100.f,0.f);}
char bwBuf[12];const char *bendWLabel(){snprintf(bwBuf,sizeof(bwBuf),"%.0fst",keyBendMaxCents/100);return bwBuf;}
void bendAInc(){keyBendAttackSmooth=min(keyBendAttackSmooth*1.3f,0.01f);}
void bendADec(){keyBendAttackSmooth=max(keyBendAttackSmooth/1.3f,0.00005f);}
char baBuf[12];const char *bendALabel(){snprintf(baBuf,sizeof(baBuf),"%.4f",keyBendAttackSmooth);return baBuf;}
void bendRInc(){keyBendReleaseSmooth=min(keyBendReleaseSmooth*1.3f,0.02f);}
void bendRDec(){keyBendReleaseSmooth=max(keyBendReleaseSmooth/1.3f,0.0005f);}
char brBuf[12];const char *bendRLabel(){snprintf(brBuf,sizeof(brBuf),"%.4f",keyBendReleaseSmooth);return brBuf;}

void portaToggle(){portaEnabled=!portaEnabled;if(!portaEnabled)portaFreq=0;}
void portaSpdInc(){portaSpeed=min(portaSpeed*1.3f,0.1f);}
void portaSpdDec(){portaSpeed=max(portaSpeed/1.3f,0.0001f);}
char ptSBuf[12];const char *portaSpdLabel(){snprintf(ptSBuf,sizeof(ptSBuf),"%.4f",portaSpeed);return ptSBuf;}

// ---------------------------------------------------------
// IMU per-axis fine controls (v0.8 Phase 3)
// ---------------------------------------------------------
void imuXSensInc(){imuAxisX.sensitivity=min(imuAxisX.sensitivity+0.1f,3.0f);}
void imuXSensDec(){imuAxisX.sensitivity=max(imuAxisX.sensitivity-0.1f,0.3f);}
char imuXSensBuf[8];const char *imuXSensLabel(){snprintf(imuXSensBuf,sizeof(imuXSensBuf),"%.1fx",imuAxisX.sensitivity);return imuXSensBuf;}
void imuXInvertToggle(){imuAxisX.invert=!imuAxisX.invert;}
const char *imuXInvertLabel(){return imuAxisX.invert?"Inverted":"Normal";}
void imuXCurveToggle(){imuAxisX.exponential=!imuAxisX.exponential;}
const char *imuXCurveLabel(){return imuAxisX.exponential?"Expo":"Linear";}
void imuXDeadzoneInc(){imuAxisX.deadzone=min(imuAxisX.deadzone+0.05f,0.3f);}
void imuXDeadzoneDec(){imuAxisX.deadzone=max(imuAxisX.deadzone-0.05f,0.f);}
char imuXDzBuf[8];const char *imuXDzLabel(){snprintf(imuXDzBuf,sizeof(imuXDzBuf),"%.0f%%",imuAxisX.deadzone*100);return imuXDzBuf;}

void imuYSensInc(){imuAxisY.sensitivity=min(imuAxisY.sensitivity+0.1f,3.0f);}
void imuYSensDec(){imuAxisY.sensitivity=max(imuAxisY.sensitivity-0.1f,0.3f);}
char imuYSensBuf[8];const char *imuYSensLabel(){snprintf(imuYSensBuf,sizeof(imuYSensBuf),"%.1fx",imuAxisY.sensitivity);return imuYSensBuf;}
void imuYInvertToggle(){imuAxisY.invert=!imuAxisY.invert;}
const char *imuYInvertLabel(){return imuAxisY.invert?"Inverted":"Normal";}
void imuYCurveToggle(){imuAxisY.exponential=!imuAxisY.exponential;}
const char *imuYCurveLabel(){return imuAxisY.exponential?"Expo":"Linear";}
void imuYDeadzoneInc(){imuAxisY.deadzone=min(imuAxisY.deadzone+0.05f,0.3f);}
void imuYDeadzoneDec(){imuAxisY.deadzone=max(imuAxisY.deadzone-0.05f,0.f);}
char imuYDzBuf[8];const char *imuYDzLabel(){snprintf(imuYDzBuf,sizeof(imuYDzBuf),"%.0f%%",imuAxisY.deadzone*100);return imuYDzBuf;}

// Calibrate: zeroes both axes to whatever tilt the device currently has.
// Gated behind a confirmation overlay (same pattern as Patch Bank's delete).
bool imuCalibrateConfirmOpen=false;
bool prevImuCalConfirmPressed=false, prevImuCalTabPressed=false;

void openCalibrateConfirm(){
    imuCalibrateConfirmOpen=true;
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldConfirm=s.enter;
    for(char c:s.word)if(c=='/')heldConfirm=true;
    prevImuCalConfirmPressed=heldConfirm;
    prevImuCalTabPressed=s.tab;
}
const char *calibrateEnterLabel(){return "Select>";}

void calibrateToggle(){
    if(imuCalibrated){
        // Turn OFF: immediately reset, no confirmation needed (non-destructive,
        // just returns both axes to their raw zero point)
        imuAxisX.calOffsetDeg=0.f;
        imuAxisY.calOffsetDeg=0.f;
        imuCalibrated=false;
    } else {
        // Turn ON: open the confirmation dialog; calibration is actually
        // applied when the user confirms (see updateImuCalibrateConfirm)
        openCalibrateConfirm();
    }
}
const char *calibrateOnOffLabel(){return imuCalibrated?"ON":"OFF";}

void updateImuCalibrateConfirm(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool confirm=s.enter,cancel=s.tab;
    for(char c:s.word)if(c=='/')confirm=true;
    if(confirm&&!prevImuCalConfirmPressed){
        imuAxisX.calOffsetDeg=lastAngleXDeg;
        imuAxisY.calOffsetDeg=lastAngleYDeg;
        imuCalibrated=true;
        imuCalibrateConfirmOpen=false;
    }
    prevImuCalConfirmPressed=confirm;
    if(cancel&&!prevImuCalTabPressed)imuCalibrateConfirmOpen=false;
    prevImuCalTabPressed=cancel;
}

// ---------------------------------------------------------
// Category sub-menus (Patch / IMU / Bend / Portamento)
// ---------------------------------------------------------
// SETTING is now just an entry point into these four dedicated screens,
// each following the same list-of-SettingItem pattern used elsewhere.
// SCREEN, not DISPLAY: M5Unified defines Display as a macro, so an
// enumerator of that name is substituted before the compiler ever sees it
// and the enum fails to parse — with the error reported on the line
// itself, which makes it look like a syntax mistake (v0.9938).
enum class SettingsCategory : uint8_t { PATCH, IMU, BEND, PORTAMENTO, PLAYMODE, ARP, PATTERN, SCREEN, MIDI, MIDI_OUT, MIDI_IN, THEREMIN };
SettingsCategory currentCategory = SettingsCategory::PATCH;
int selectedCategoryIndex = 0;

// ---------------------------------------------------------
// Reset-to-default (Phase 4)
// ---------------------------------------------------------
// Shared confirmation overlay for Patch(tone)/Bend/Portamento resets —
// same pattern as the Patch Bank delete-confirm and IMU Calibrate-confirm.
enum class ResetKind : uint8_t { PATCH_TONE, BEND, PORTAMENTO, PATCH_RANDOM, PATTERN_RANDOM };
bool resetConfirmOpen=false;
ResetKind resetConfirmKind=ResetKind::PATCH_TONE;
bool prevResetConfirmPressed=false, prevResetTabPressed=false;

void openResetConfirm(ResetKind kind){
    resetConfirmKind=kind;
    resetConfirmOpen=true;
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldConfirm=s.enter;
    for(char c:s.word)if(c=='/')heldConfirm=true;
    prevResetConfirmPressed=heldConfirm;
    prevResetTabPressed=s.tab;
}
void openPatchToneReset(){openResetConfirm(ResetKind::PATCH_TONE);}
void openPatchRandomize(){openResetConfirm(ResetKind::PATCH_RANDOM);}
void openPatternRandomize(){openResetConfirm(ResetKind::PATTERN_RANDOM);}
void openBendReset(){openResetConfirm(ResetKind::BEND);}
void openPortaReset(){openResetConfirm(ResetKind::PORTAMENTO);}
const char *resetEnterLabel(){return "Select>";}

float randRange(float lo,float hi){
    return lo+(hi-lo)*(float)random(0,10001)/10000.0f;
}

// Resets everything tone-related (VCO/VCF/VCA/LFO/IMU + note hold) to a
// simple, predictable starting point. Performance-time state (Bend,
// Portamento, octave, transpose) is deliberately NOT touched here — those
// get their own separate resets in their own category screens.
// Capture the sound as it is right now into a snapshot (v0.995).
void morphCapture(PatchSnapshot &d){
    d.p=params;
    for(int i=0;i<MAX_MORPH_SLOTS;i++)d.chain[i]=morphChain[i];
    d.chainLen=morphChainLen;
    d.osc2Wave=params.osc2Waveform;
    d.osc2Oct=params.osc2OctaveShift; d.osc2Semi=params.osc2Semitones;
    d.subOct=params.subOscOctave;
    d.lfoWave=(uint8_t)lfo.wave; d.lfoTarget=(uint8_t)lfo.target;
    d.lfoRate=lfo.rateHz;        d.lfoDepth=lfo.depth;
    d.imuXTarget=(uint8_t)imuAxisX.target; d.imuYTarget=(uint8_t)imuAxisY.target;
    d.imuXEn=imuXEnabled; d.imuYEn=imuYEnabled;
    d.filterType=(int)filterParams.type;
    d.cutoffHz=filterParams.cutoffHz; d.resonanceQ=filterParams.resonanceQ;
    d.keyTracking=filterParams.keyTracking; d.fEnvDepth=filterEnv.depth;
    d.adsrA=adsr.attackTime; d.adsrD=adsr.decayTime;
    d.adsrS=adsr.sustainLevel; d.adsrR=adsr.releaseTime;
    d.used=true;
}

inline float mlerp(float a,float b,float t){return a+(b-a)*t;}

// Write the morph at position t (0 = from, 1 = to) into the live sound.
//
// Discrete state is NOT interpolated — waveforms, the Morph chain, filter
// type, octave and semitone offsets. There is no halfway between a saw and
// a square, and a chain that changed slot by slot mid-morph would sound
// like a fault. They switch once, at the START, so the morph is heard as
// the new waveform's tone arriving rather than as something breaking. That
// does mean the first instant of a morph is audible; it is a smaller cost
// than a step in the middle, and it lines up with the target patch's
// character being what you are morphing toward.
// Where timbreMorph's sweep begins, set once by morphStart() and read by
// morphApply() every tick — a fixed reference point for the whole morph's
// duration, since morphApply() only ever sees the current t (v0.99942).
float morphTimbreStart=0.f;

void morphApply(float t){
    const SynthParams &A=morphFrom.p,&B=morphTo.p;
    params.oscShape      =mlerp(A.oscShape,B.oscShape,t);
    params.detuneCents   =mlerp(A.detuneCents,B.detuneCents,t);
    params.fineTuneCents =mlerp(A.fineTuneCents,B.fineTuneCents,t);
    params.subOscLevel   =mlerp(A.subOscLevel,B.subOscLevel,t);
    params.noiseLevel    =mlerp(A.noiseLevel,B.noiseLevel,t);
    // Interpolated from morphTimbreStart (an end of the CURRENT chain,
    // set once in morphStart()) toward B's own value, in lockstep with
    // this whole function's t — not from the outgoing patch's value, and
    // not via the separate fast SM-based smoothing morphStart() leaves
    // alone for exactly this reason (v0.99942). See the note in
    // morphStart() for what this replaced and why.
    params.timbreMorph=mlerp(morphTimbreStart,params.timbreMorphTarget,t);
    params.vibratoDepth  =mlerp(A.vibratoDepth,B.vibratoDepth,t);
    params.vibratoRateHz =mlerp(A.vibratoRateHz,B.vibratoRateHz,t);
    params.tremoloDepth  =mlerp(A.tremoloDepth,B.tremoloDepth,t);
    params.bitcrush      =mlerp(A.bitcrush,B.bitcrush,t);
    params.osc2Level     =mlerp(A.osc2Level,B.osc2Level,t);
    params.osc2Shape     =mlerp(A.osc2Shape,B.osc2Shape,t);
    params.osc2DetuneCents=mlerp(A.osc2DetuneCents,B.osc2DetuneCents,t);
    params.osc2FineCents =mlerp(A.osc2FineCents,B.osc2FineCents,t);
    params.ringModRateHz =mlerp(A.ringModRateHz,B.ringModRateHz,t);
    params.ringModMix    =mlerp(A.ringModMix,B.ringModMix,t);
    params.limiterDrive  =mlerp(A.limiterDrive,B.limiterDrive,t);
    params.limiterMix    =mlerp(A.limiterMix,B.limiterMix,t);
    params.chorusRateHz  =mlerp(A.chorusRateHz,B.chorusRateHz,t);
    params.chorusDepthMs =mlerp(A.chorusDepthMs,B.chorusDepthMs,t);
    params.chorusMix     =mlerp(A.chorusMix,B.chorusMix,t);
    params.delayTimeMs   =mlerp(A.delayTimeMs,B.delayTimeMs,t);
    params.delayFeedback =mlerp(A.delayFeedback,B.delayFeedback,t);
    params.delayMix      =mlerp(A.delayMix,B.delayMix,t);
    params.reverbRoomSize=mlerp(A.reverbRoomSize,B.reverbRoomSize,t);
    params.reverbDamping =mlerp(A.reverbDamping,B.reverbDamping,t);
    params.reverbMix     =mlerp(A.reverbMix,B.reverbMix,t);
    filterParams.cutoffHz   =mlerp(morphFrom.cutoffHz,morphTo.cutoffHz,t);
    filterParams.resonanceQ =mlerp(morphFrom.resonanceQ,morphTo.resonanceQ,t);
    filterParams.keyTracking=mlerp(morphFrom.keyTracking,morphTo.keyTracking,t);
    filterEnv.depth         =mlerp(morphFrom.fEnvDepth,morphTo.fEnvDepth,t);
    adsr.attackTime =mlerp(morphFrom.adsrA,morphTo.adsrA,t);
    adsr.decayTime  =mlerp(morphFrom.adsrD,morphTo.adsrD,t);
    adsr.sustainLevel=mlerp(morphFrom.adsrS,morphTo.adsrS,t);
    adsr.releaseTime=mlerp(morphFrom.adsrR,morphTo.adsrR,t);
    lfo.rateHz=mlerp(morphFrom.lfoRate,morphTo.lfoRate,t);
    lfo.depth =mlerp(morphFrom.lfoDepth,morphTo.lfoDepth,t);
    updateFilterCoefficients();
}

// Start a morph toward a slot. morphFrom is captured from the LIVE sound,
// not from the previously selected slot, so pressing a new slot part-way
// through continues from where the sound actually is (v0.995).
void morphStart(int slot){
    if(!morphSlotsReady())return;
    if(slot<0||slot>=NUM_MORPH_SLOTS||!morphSlots[slot].used)return;
    // Redirecting to a new target before the previous morph finished
    // never reset morphDiagCount, so repeated redirects kept appending to
    // the SAME ring buffer across all of them until it hit MORPH_DIAG_CAP
    // (48) and stopped recording anything further — a known gap noted
    // when the diagnostic was built, now confirmed to actually matter: a
    // log showing exactly 48 samples, spanning several redirects, was the
    // first clue in a serious lockup report. Reset here so every morph
    // attempt — including a redirect — starts its own clean window.
    morphDiagCount=0;
    morphCapture(morphFrom);
    morphTo=morphSlots[slot];
    // Compared BEFORE the overwrite below, since afterward there is
    // nothing left to compare against (v0.99945). Re-morphing to the same
    // patch — or to a different patch that happens to share the same
    // chain — was sweeping through the whole chain regardless, because
    // the "start from a far end" logic ran unconditionally with no way
    // to tell the chain had not actually changed.
    bool sameChain=(morphChainLen==morphTo.chainLen);
    if(sameChain)for(int i=0;i<morphChainLen;i++)if(morphChain[i]!=morphTo.chain[i]){sameChain=false;break;}

    // Discrete state switches now — see morphApply().
    // Guarded below, inside refreshMorphTablePtrs() itself, and around
    // the array/length writes here too (v0.99943) — see morphChainMux's
    // declaration.
    portENTER_CRITICAL(&morphChainMux);
    for(int i=0;i<MAX_MORPH_SLOTS;i++)morphChain[i]=morphTo.chain[i];
    morphChainLen=morphTo.chainLen;
    portEXIT_CRITICAL(&morphChainMux);
    refreshMorphTablePtrs();
    // timbreMorph sweeps from an END of the NEW chain toward the target,
    // in lockstep with the whole morph's own timeline via morphApply()'s
    // mlerp below — not the outgoing patch's value, and not the separate
    // fast timbreMorphTarget/SM smoothing used for live IMU tweaks
    // (v0.99942, revised from v0.9995's instant snap).
    //
    // The snap in v0.9995 killed the harsh mismatch but also killed the
    // gradual "sweep through waveforms" the morph is supposed to sound
    // like. That sweep was never actually the problem —
    // getMorphedSample() already clamps its indices to morphChainLen
    // internally, so nothing was misreading memory; the old code just
    // started sweeping from a position that belonged to a chain which no
    // longer existed, an arbitrary jump with no relationship to the chain
    // that had just gone live. Starting from the far end of the SAME new
    // chain instead — whichever end sits farther from this patch's own
    // timbreMorph target — keeps every frame of the sweep inside one
    // currently-active chain (consecutive waveforms of the same set, so
    // always coherent audio) and gives the longest, most audible sweep
    // rather than a token nudge.
    {
        int chainMax=max(0,morphChainLen-1);
        float target=constrain(morphTo.p.timbreMorph,0.f,(float)chainMax);
        if(sameChain){
            // Nothing to sweep through — the chain did not change, so
            // starting from wherever the sound already is IS the correct
            // "morph," which for an unchanged chain is no audible change
            // at all. Re-morphing to the same patch, or to a different
            // patch that shares this chain, now does nothing rather than
            // touring every waveform in it for no reason.
            morphTimbreStart=constrain(params.timbreMorph,0.f,(float)chainMax);
        } else {
            morphTimbreStart=(target>(float)chainMax*0.5f)?0.f:(float)chainMax;
        }
        params.timbreMorph=morphTimbreStart;
        params.timbreMorphTarget=target;   // where it lands once morphApply() finishes
    }
    params.osc2Waveform=morphTo.osc2Wave;
    params.osc2OctaveShift=morphTo.osc2Oct;
    params.osc2Semitones=morphTo.osc2Semi;
    params.subOscOctave=morphTo.subOct;
    filterParams.type=(FilterType)morphTo.filterType;
    // LFO wave and target are discrete — there is no halfway between a
    // sine and a square, or between modulating pitch and modulating the
    // filter (v0.99874).
    lfo.wave=(LfoWave)morphTo.lfoWave;
    lfo.target=(LfoTarget)morphTo.lfoTarget;
    // Repoint the IMU axes, ALWAYS clearing the outgoing target's offset
    // first and releasing any hold (v0.9956).
    //
    // v0.9954 only cleared when the target changed, which left a real way
    // to lose all sound: if the target stayed the same but the incoming
    // patch had that axis DISABLED, the offset it had been contributing
    // was frozen in place with nothing left to update it. With the axis on
    // Volume and the device tilted, that freezes the output at zero and
    // nothing short of a reboot brings it back — the synth keeps
    // generating audio, the screen keeps animating, and it is silent.
    // Which is exactly the reported fault.
    //
    // Clearing unconditionally costs nothing: resetParamToDefault() only
    // zeroes an offset, and the axis is about to be re-driven from live
    // tilt anyway.
    resetParamToDefault(imuAxisX.target);
    resetParamToDefault(imuAxisY.target);
    imuXHeld=false; imuYHeld=false;
    imuAxisX.target=(ImuTarget)morphTo.imuXTarget;
    imuAxisY.target=(ImuTarget)morphTo.imuYTarget;
    imuXEnabled=morphTo.imuXEn; imuYEnabled=morphTo.imuYEn;
    // And clear the INCOMING target too when its axis arrives disabled:
    // otherwise a stale offset on that parameter has nothing to move it.
    if(!imuXEnabled)resetParamToDefault(imuAxisX.target);
    if(!imuYEnabled)resetParamToDefault(imuAxisY.target);
    if(morphTimeSec<=0.01f){morphApply(1.f);morphActive=false;return;}
    morphPos=0.f; morphActive=true;
}

// Advanced from loop(), which runs far faster than the ear needs and
// leaves audioTask untouched.
// Buffered rather than printed live (v0.99948, corrected from v0.99947).
// The v0.99947 build stopped reproducing the click/silence entirely —
// not fixed, just avoided: a Serial.printf() call is slow enough that
// adding one every 150ms shifted loop()'s timing just enough to dodge
// whatever narrow window causes it, which is itself useful confirmation
// this is timing-sensitive rather than a plain logic error. Recording
// into a small in-memory ring buffer (a handful of array writes, no I/O)
// and printing the WHOLE captured sequence only after the morph finishes
// keeps the diagnostic from perturbing the real-time window it needs to
// observe. envPhase is stored as a raw uint8_t since it is an enum class
// (IDLE=0, ATTACK=1, DECAY=2, SUSTAIN=3, RELEASE=4 in declaration order).
struct MorphDiagEntry{
    unsigned long tMs; float pos; uint8_t envPhase; float envLevel;
    float currentFreq; float playingFreq; float sustainLevel;
    float filterEnvLevel; float cutoffHz;
    // Added v0.99949: filterEnvPhase is a SEPARATE state machine from
    // envPhase (advanceEnvelope() runs two independent switch blocks),
    // and the diagnostic had only ever logged the amp one — a brief
    // filterEnvLevel excursion could have been the filter envelope
    // retriggering entirely on its own, invisible until now. noteHeld
    // distinguishes a genuine key-state change from something retriggering
    // the envelope while Hold's own state never moved at all.
    uint8_t filterEnvPhase; bool noteHeld;
};
// v0.99953's addition of reverbMix/chorusMix/chorusDepthMs/lfoDepth to
// this struct is reverted here (v0.99954): SD mounting failed on every
// boot immediately after that build, with no Cap attached, on both boot
// paths, reproducibly — and the only thing that build actually changed
// was this struct growing. setup() (where SD mounts) runs entirely
// before loop() is ever called, so morphTick()'s own code could not have
// executed yet at the point SD fails; the connection has to be indirect
// — a memory-layout shift from this array growing large enough to expose
// an out-of-bounds write elsewhere that happens to land on whatever the
// VFS layer needs, not a direct causal link. The exact culprit elsewhere
// is not yet found; reverting the only thing that changed is the safe,
// testable move while it is.
constexpr int MORPH_DIAG_CAP=48;   // 48*~150ms ≈ 7s, comfortably over morphTimeSec's usual range
MorphDiagEntry morphDiagLog[MORPH_DIAG_CAP];
// morphDiagCount itself moved earlier in the file (v0.9997x fix) —
// morphStart() is defined before this point and now resets it too; see
// the declaration next to loopHeartbeatMs for where it actually lives.

void morphTick(){
    if(!morphActive)return;
    static unsigned long lastMs=0;
    unsigned long now=millis();
    if(lastMs==0||now-lastMs>200)lastMs=now;   // first call, or resumed
    float dt=(now-lastMs)/1000.f; lastMs=now;
    morphPos+=dt/max(0.01f,morphTimeSec);
    bool justFinished=false;
    if(morphPos>=1.f){morphPos=1.f;morphActive=false;justFinished=true;}
    morphApply(morphPos);
#if CPS_LOG_PATCH
    static unsigned long lastLogMs=0;
    if(now-lastLogMs>=150&&morphDiagCount<MORPH_DIAG_CAP){
        lastLogMs=now;
        MorphDiagEntry &e=morphDiagLog[morphDiagCount++];
        e.tMs=now; e.pos=morphPos; e.envPhase=(uint8_t)envPhase; e.envLevel=envLevel;
        e.currentFreq=currentFreq; e.playingFreq=playingFreq;
        e.sustainLevel=adsr.sustainLevel; e.filterEnvLevel=filterEnvLevel;
        e.cutoffHz=filterParams.cutoffHz;
        e.filterEnvPhase=(uint8_t)filterEnvPhase; e.noteHeld=noteHeld;
    }
    if(justFinished){
        Serial.printf("[morph] finished, %d samples:\n",morphDiagCount);
        for(int i=0;i<morphDiagCount;i++){
            MorphDiagEntry &e=morphDiagLog[i];
            Serial.printf("  +%4lums pos=%.2f envPhase=%d fEnvPhase=%d hold=%d envLevel=%.3f "
                          "cur=%.1f play=%.1f sustain=%.2f fEnvLvl=%.3f cutoff=%.0f\n",
                e.tMs-morphDiagLog[0].tMs,e.pos,e.envPhase,e.filterEnvPhase,(int)e.noteHeld,
                e.envLevel,e.currentFreq,e.playingFreq,e.sustainLevel,e.filterEnvLevel,e.cutoffHz);
        }
        morphDiagCount=0;
    }
#endif
}

void performPatchToneReset(){
    // VCO
    params.timbreMorph=params.timbreMorphTarget=0.f;
    // Also restore the Morph chain itself (which waveforms are active,
    // and in what order) back to the original default — otherwise a
    // customized chain, or an unusual waveform accidentally left
    // selected, would survive a "reset the tone" action. Resetting
    // timbreMorph to 0 alone isn't enough: it only resets the KNOB
    // POSITION within whatever chain happens to be active right now.
    // Same four as the boot default — see the morphChain declaration.
    // Locked for the same reason as morphStart() (v0.99943): Reset can be
    // invoked while a note is sounding.
    portENTER_CRITICAL(&morphChainMux);
    morphChain[0]=OscWaveform::SINE;   morphChain[1]=OscWaveform::TRIANGLE;
    morphChain[2]=OscWaveform::SAWTOOTH;morphChain[3]=OscWaveform::SQUARE;
    morphChainLen=4;
    portEXIT_CRITICAL(&morphChainMux);
    refreshMorphTablePtrs();   // the cached pointers describe the old chain
    params.oscShape=0.5f; params.oscShapeOffset=0.f; params.oscShapeOffsetTarget=0.f;
    params.detuneCents=0.f; params.detuneOffset=0.f; params.detuneOffsetTarget=0.f;
    params.fineTuneCents=0.f;
    params.subOscLevel=0.f; params.subLevelOffset=0.f; params.subLevelOffsetTarget=0.f;
    params.subOscOctave=-1;
    params.noiseLevel=0.f; params.noiseOffset=0.f; params.noiseOffsetTarget=0.f;
    // Bit-crusher (v0.9875): previously left untouched by a tone reset, so
    // a forgotten Bit-crusher setting survived it — the same trap the Morph
    // chain used to be, and just as likely to be read as broken hardware.
    params.bitcrush=0.f; params.bitcrushOffset=params.bitcrushOffsetTarget=0.f;
    // v0.989: now that these are menu-owned and saved, a tone reset has to
    // clear them too — otherwise a leftover Vibrato or Tremolo would
    // survive "reset the tone" the same way the Morph chain used to.
    params.vibratoDepth=0.f;  params.vibratoDepthOffset=params.vibratoDepthOffsetTarget=0.f;
    params.vibratoRateHz=5.f; params.vibratoRateOffset =params.vibratoRateOffsetTarget =0.f;
    params.tremoloDepth=0.f;  params.tremoloDepthOffset=params.tremoloDepthOffsetTarget=0.f;
    // Oscillator 2 back to silent (v0.9911) — a leftover second oscillator
    // would survive "reset the tone" the way the Morph chain used to.
    params.osc2Level=0.f; params.osc2Waveform=OscWaveform::SAWTOOTH; params.osc2Shape=0.5f;
    params.osc2DetuneCents=0.f; params.osc2FineCents=0.f; params.osc2OctaveShift=0;
    params.osc2Semitones=0;   // v0.993
    // Both IMU axes back ON (v0.99873). Patch loads reset first and then
    // parse, so a key the file does not contain means "default" — and
    // without this line these two had no default, they simply kept
    // whatever was live. Patches saved before v0.9921 have no imu_x_en at
    // all, so loading one inherited the previous patch's state; morphing
    // through several could leave an axis switched off with nothing on
    // screen to explain it. Reported as exactly that: the two patches
    // that DO carry the key loaded with the IMU on, the six older ones
    // did not.
    imuXEnabled=true; imuYEnabled=true;
    params.osc2LevelOffset=params.osc2LevelOffsetTarget=0.f;   // v0.9934
    params.osc2ShapeOffset=params.osc2ShapeOffsetTarget=0.f;
    // FX (v0.9875): all four off. Mix=0 is this project's "fully off"
    // convention throughout, so zeroing the four mixes is enough to
    // disable them; the other FX parameters are left at whatever the user
    // had, since they're inaudible while off and re-enabling a pad already
    // restores a sane default mix. Buffers are wiped too — otherwise the
    // previous patch's echoes would still be sitting in the delay line,
    // waiting to be read back out the next time Delay is switched on.
    params.ringModMix=0.f;
    params.limiterMix=0.f;
    params.chorusMix=0.f;
    params.delayMix=0.f;
    params.reverbMix=0.f;
    params.reverbRoomOffset   =params.reverbRoomOffsetTarget   =0.f;
    params.reverbMixOffset    =params.reverbMixOffsetTarget    =0.f;
    params.ringModRateOffset  =params.ringModRateOffsetTarget  =0.f;
    params.ringModMixOffset   =params.ringModMixOffsetTarget   =0.f;
    params.limiterDriveOffset =params.limiterDriveOffsetTarget =0.f;
    params.chorusDepthOffset  =params.chorusDepthOffsetTarget  =0.f;
    params.chorusMixOffset    =params.chorusMixOffsetTarget    =0.f;
    params.delayFeedbackOffset=params.delayFeedbackOffsetTarget=0.f;
    params.delayMixOffset     =params.delayMixOffsetTarget     =0.f;
    updateFxEffective(0.f);
    clearFxBuffers();
    // VCF
    filterParams.type=FilterType::NONE;
    filterParams.cutoffHz=2000.0f;
    filterParams.resonanceQ=0.707f;
    filterParams.keyTracking=0.0f;
    params.resonanceOffset=0.f; params.resonanceOffsetTarget=0.f;
    filterEnv.depth=0.f; filterEnv.attackTime=0.1f; filterEnv.decayTime=0.3f;
    filterEnv.sustainLvl=0.0f; filterEnv.releaseTime=0.3f;
    // VCA (ADSR) — simplest usable starting point: full sustain, quick release
    adsr.attackTime=0.f; adsr.decayTime=0.f; adsr.sustainLevel=1.0f; adsr.releaseTime=0.2f;
    // LFO
    lfo.target=LfoTarget::NONE; lfo.wave=LfoWave::SINE; lfo.rateHz=2.0f; lfo.depth=0.f;
    lfoRateOffset=0.f; lfoRateOffsetTarget=0.f; lfoDepthOffset=0.f; lfoDepthOffsetTarget=0.f;
    // IMU
    // Must stay in sync with the imuAxisX/imuAxisY declarations near the
    // top of the file (see the note there) — Y is Shape rather than Volume
    // so that a default patch can never be silenced just by how the device
    // is being held.
    imuAxisX.target=ImuTarget::TIMBRE;
    imuAxisY.target=ImuTarget::SHAPE;
    imuAxisX.sensitivity=1.0f; imuAxisY.sensitivity=1.0f;
    imuAxisX.invert=false; imuAxisY.invert=false;
    imuAxisX.exponential=false; imuAxisY.exponential=false;
    imuAxisX.deadzone=0.f; imuAxisY.deadzone=0.f;
    imuAxisX.calOffsetDeg=0.f; imuAxisY.calOffsetDeg=0.f;
    imuCalibrated=false;
    imuXHeld=false; imuYHeld=false;
    params.volumeScale=1.0f; params.volumeScaleTarget=1.0f;
    // Note hold
    noteHeld=false; heldFreq=0.f; midiPedalTookHold=false;
    updateFilterCoefficients();
}

// Randomizes every tone-related parameter (same scope as the tone reset,
// including "type" parameters like filter type / LFO target+wave / IMU
// targets, per user's request). Calibration is left alone — it's a
// physical setup thing, not a creative/tone parameter.
void performPatchRandomize(){
    // VCO
    params.timbreMorph=params.timbreMorphTarget=randRange(0.f,(float)max(1,morphChainLen-1));
    params.oscShape=randRange(0.f,1.f); params.oscShapeOffset=0.f; params.oscShapeOffsetTarget=0.f;
    params.detuneCents=randRange(-50.f,50.f); params.detuneOffset=0.f; params.detuneOffsetTarget=0.f;
    params.fineTuneCents=randRange(-100.f,100.f);
    params.subOscLevel=randRange(0.f,1.f); params.subLevelOffset=0.f; params.subLevelOffsetTarget=0.f;
    params.subOscOctave=(random(0,2)==0)?-1:-2;
    // Noise tends to mask pitch clarity noticeably even at fairly low
    // levels, so it's much rarer and much subtler here than the other
    // randomized parameters — most random patches should have none at all.
    params.noiseLevel=(random(0,100)<15)?randRange(0.05f,0.2f):0.f;
    params.noiseOffset=0.f; params.noiseOffsetTarget=0.f;
    // VCF
    filterParams.type=(FilterType)random(0,5); // LPF/HPF/BPF/NOTCH/NONE
    filterParams.cutoffHz=randRange(FILTER_CUTOFF_MIN,FILTER_CUTOFF_MAX);
    filterParams.resonanceQ=randRange(FILTER_Q_MIN,FILTER_Q_MAX);
    filterParams.keyTracking=randRange(0.f,1.f);
    params.resonanceOffset=0.f; params.resonanceOffsetTarget=0.f;
    filterEnv.depth=randRange(-3900.f,3900.f);
    filterEnv.attackTime=randRange(0.f,1.5f);
    filterEnv.decayTime=randRange(0.f,1.5f);
    filterEnv.sustainLvl=randRange(0.f,1.f);
    filterEnv.releaseTime=randRange(0.f,1.5f);
    // VCA (kept within a musically reasonable range, not the full 0-5s max)
    adsr.attackTime=randRange(0.f,1.5f);
    adsr.decayTime=randRange(0.f,1.5f);
    adsr.sustainLevel=randRange(0.f,1.f);
    adsr.releaseTime=randRange(0.f,1.5f);
    // LFO
    lfo.target=(LfoTarget)random(0,(int)LfoTarget::TARGET_COUNT);
    lfo.wave=(LfoWave)random(0,4);
    lfo.rateHz=randRange(LFO_RATE_MIN,LFO_RATE_MAX);
    lfo.depth=randRange(0.f,1.f);
    lfoRateOffset=0.f; lfoRateOffsetTarget=0.f; lfoDepthOffset=0.f; lfoDepthOffsetTarget=0.f;
    // IMU
    imuAxisX.target=(ImuTarget)random(0,(int)ImuTarget::TARGET_COUNT);
    imuAxisY.target=(ImuTarget)random(0,(int)ImuTarget::TARGET_COUNT);
    imuAxisX.sensitivity=randRange(0.3f,3.0f);
    imuAxisY.sensitivity=randRange(0.3f,3.0f);
    imuAxisX.invert=(random(0,2)==1);
    imuAxisY.invert=(random(0,2)==1);
    imuAxisX.exponential=(random(0,2)==1);
    imuAxisY.exponential=(random(0,2)==1);
    imuAxisX.deadzone=randRange(0.f,0.3f);
    imuAxisY.deadzone=randRange(0.f,0.3f);
    imuXHeld=false; imuYHeld=false;
    imuXEnabled=true; imuYEnabled=true;
    // Everything below was added to the synth after this function was
    // written and never got included here, so Randomize quietly ignored it
    // (v0.993). Audited against the parameter list in one pass.
    //
    // Oscillator 2 is present but usually silent. A second oscillator on
    // every random patch would make them all thick in the same way; the
    // interesting ones are the patches where it IS there. So it appears
    // about a third of the time, and when it does it leans toward musical
    // intervals rather than an arbitrary semitone offset.
    if(random(0,100)<35){
        params.osc2Level=randRange(0.2f,0.7f);
        params.osc2Waveform=(OscWaveform)random(0,NUM_OSC_WAVEFORMS);
        params.osc2Shape=randRange(0.f,1.f);
        static const int INTERVALS[]={0,0,3,4,5,7,7,12,-12};
        params.osc2Semitones=INTERVALS[random(0,(int)(sizeof(INTERVALS)/sizeof(INTERVALS[0])))];
        params.osc2DetuneCents=randRange(-12.f,12.f);
        params.osc2FineCents=randRange(-5.f,5.f);
        params.osc2OctaveShift=(random(0,100)<25)?((random(0,2)==0)?-1:1):0;
    } else {
        params.osc2Level=0.f;
        params.osc2Waveform=OscWaveform::SAWTOOTH;
        params.osc2Shape=0.5f; params.osc2Semitones=0;
        params.osc2DetuneCents=0.f; params.osc2FineCents=0.f; params.osc2OctaveShift=0;
    }
    params.osc2LevelOffset=params.osc2LevelOffsetTarget=0.f;   // v0.9934
    params.osc2ShapeOffset=params.osc2ShapeOffsetTarget=0.f;
    // Vibrato / Tremolo are modulation depths that make a patch seasick at
    // full value, so they are occasional and shallow — the same reasoning
    // that keeps Noise rare above.
    params.vibratoDepth=(random(0,100)<30)?randRange(0.05f,0.35f):0.f;
    params.vibratoRateHz=randRange(2.f,8.f);
    params.tremoloDepth=(random(0,100)<25)?randRange(0.05f,0.4f):0.f;
    params.vibratoDepthOffset=params.vibratoDepthOffsetTarget=0.f;
    params.vibratoRateOffset =params.vibratoRateOffsetTarget =0.f;
    params.tremoloDepthOffset=params.tremoloDepthOffsetTarget=0.f;
    // Bit-crusher destroys pitch clarity faster than Noise does, so rarer
    // still and capped well below the top of its range.
    params.bitcrush=(random(0,100)<20)?randRange(0.1f,0.45f):0.f;
    params.bitcrushOffset=params.bitcrushOffsetTarget=0.f;
    // FX: each pad rolled independently. Turning them all on at once would
    // both bury the patch and put every random patch at the top of the CPU
    // budget. Reverb is the most expensive and the most likely to swamp
    // everything else, so it is among the least likely to appear.
    params.ringModMix   =(random(0,100)<20)?randRange(0.15f,0.5f):0.f;
    params.ringModRateHz=randRange(20.f,800.f);
    params.limiterMix   =(random(0,100)<30)?randRange(0.3f,0.8f):0.f;
    params.limiterDrive =randRange(1.5f,4.f);
    params.chorusMix    =(random(0,100)<30)?randRange(0.2f,0.6f):0.f;
    params.chorusRateHz =randRange(0.2f,3.f);
    params.chorusDepthMs=randRange(5.f,18.f);
    params.delayMix     =(random(0,100)<25)?randRange(0.15f,0.45f):0.f;
    params.delayTimeMs  =randRange(80.f,min(600.f,DELAY_MAX_MS));   // v0.9965
    params.delayFeedback=randRange(0.1f,0.6f);
    params.reverbMix    =(random(0,100)<20)?randRange(0.15f,0.4f):0.f;
    params.reverbRoomSize=randRange(0.3f,0.9f);
    params.reverbDamping =randRange(0.2f,0.8f);
    params.ringModRateOffset  =params.ringModRateOffsetTarget  =0.f;
    params.ringModMixOffset   =params.ringModMixOffsetTarget   =0.f;
    params.limiterDriveOffset =params.limiterDriveOffsetTarget =0.f;
    params.chorusDepthOffset  =params.chorusDepthOffsetTarget  =0.f;
    params.chorusMixOffset    =params.chorusMixOffsetTarget    =0.f;
    params.delayFeedbackOffset=params.delayFeedbackOffsetTarget=0.f;
    params.delayMixOffset     =params.delayMixOffsetTarget     =0.f;
    params.reverbRoomOffset   =params.reverbRoomOffsetTarget   =0.f;
    params.reverbMixOffset    =params.reverbMixOffsetTarget    =0.f;
    clearFxBuffers(); clearReverbState();
    // Portamento and Arp (on/off, type, Tempo/Swing/Rate) were briefly
    // added here (v0.99962) after being found missing from Randomize's
    // coverage, then explicitly withdrawn (v0.9996x): both are now
    // performance/operational settings, not part of a patch at all — see
    // the save/load gating above — so Randomize, which randomizes a
    // PATCH, correctly has nothing to do with either any more.
    params.volumeScale=1.0f; params.volumeScaleTarget=1.0f;
    // Note hold
    noteHeld=false; heldFreq=0.f; midiPedalTookHold=false;
    updateFilterCoefficients();
}

// Generates a random 16-step pattern using the CURRENT scale's notes (so
// it's always in-key, EZ or Pro), with Tie/Accent/Slide included so the
// result actually shows off the TB-303-style features, not just pitches.
// Tempo/Swing are left untouched — those are pattern-level, not part of
// what "randomize the steps" implies.
void performPatternRandomize(){
    bool inNote=false;
    for(int i=0;i<SEQ_NUM_STEPS;i++){
        seqSteps[i]=SeqStep();
        int roll=random(0,100);
        if(inNote&&roll<30){
            // Continue the previous note via Tie.
            seqSteps[i].tie=true;
        } else if(roll<(inNote?80:65)){
            // New note, picked from the current scale's key tables (25
            // notes total across both rows) so it's always in-key.
            int idx=random(0,12+13);
            float freq=(idx<12)?row1Freqs[idx]:row2Freqs[idx-12];
            seqSteps[i].freq=freq;
            seqSteps[i].velocity=(uint8_t)random(40,101);
            seqSteps[i].accent=(random(0,100)<18);
            seqSteps[i].slide=(random(0,100)<15);
            inNote=true;
        } else {
            // Rest.
            inNote=false;
        }
    }
    seqCursorStep=0;
}

void performBendReset(){
    keyBendMaxCents=200.0f;
    keyBendAttackSmooth=KEY_BEND_ATTACK_SMOOTH_DEFAULT;
    keyBendReleaseSmooth=KEY_BEND_RELEASE_SMOOTH_DEFAULT;
}

void performPortaReset(){
    portaEnabled=false;
    portaSpeed=0.005f;
    portaFreq=0.f;
}

void updateResetConfirm(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool confirm=s.enter,cancel=s.tab;
    for(char c:s.word)if(c=='/')confirm=true;
    if(confirm&&!prevResetConfirmPressed){
        switch(resetConfirmKind){
            case ResetKind::PATCH_TONE:   performPatchToneReset(); break;
            case ResetKind::PATCH_RANDOM: performPatchRandomize(); break;
            case ResetKind::PATTERN_RANDOM: performPatternRandomize(); break;
            case ResetKind::BEND:         performBendReset(); break;
            case ResetKind::PORTAMENTO:   performPortaReset(); break;
        }
        resetConfirmOpen=false;
    }
    prevResetConfirmPressed=confirm;
    if(cancel&&!prevResetTabPressed)resetConfirmOpen=false;
    prevResetTabPressed=cancel;
}

SettingItem patchMenuItems[]={
    {"Save", patchSaveEnter, patchSaveEnter, patchEnterLabel},
    {"Load", patchLoadEnter, patchLoadEnter, patchEnterLabel},
    {"Reset", openPatchToneReset, openPatchToneReset, resetEnterLabel},
    {"Random", openPatchRandomize, openPatchRandomize, resetEnterLabel},
    {"Morph", openMorphSlotScreen, openMorphSlotScreen, patchEnterLabel},
};
// 2-column layout (splitCol=5 in getCategoryItems): left = X's 5 items,
// right = Y's 5 items + Calibrate as a 6th row. (ADV only — see
// imuMenuItemsOriginal for the reduced original-Cardputer "PAD" version.)
SettingItem imuMenuItemsAdv[]={
    {"IMU X",   imuXOpenPicker,    imuXOpenPicker,    imuXLabel},
    {"X Sens",  imuXSensInc,       imuXSensDec,       imuXSensLabel},
    {"X Invert",imuXInvertToggle,  imuXInvertToggle,  imuXInvertLabel},
    {"X Curve", imuXCurveToggle,   imuXCurveToggle,   imuXCurveLabel},
    {"X Dead",  imuXDeadzoneInc,   imuXDeadzoneDec,   imuXDzLabel},
    {"IMU Y",   imuYOpenPicker,    imuYOpenPicker,    imuYLabel},
    {"Y Sens",  imuYSensInc,       imuYSensDec,       imuYSensLabel},
    {"Y Invert",imuYInvertToggle,  imuYInvertToggle,  imuYInvertLabel},
    {"Y Curve", imuYCurveToggle,   imuYCurveToggle,   imuYCurveLabel},
    {"Y Dead",  imuYDeadzoneInc,   imuYDeadzoneDec,   imuYDzLabel},
    {"Calibrate",calibrateToggle,calibrateToggle,calibrateOnOffLabel},
};
// Original Cardputer: no Deadzone (nothing to filter out — it's a clean
// key-driven signal, not a noisy sensor) and no Calibrate (no physical
// zero-point to correct on a virtual axis). 2-column, splitCol=4.
SettingItem imuMenuItemsOriginal[]={
    {"PAD X",   imuXOpenPicker,    imuXOpenPicker,    imuXLabel},
    {"X Sens",  imuXSensInc,       imuXSensDec,       imuXSensLabel},
    {"X Invert",imuXInvertToggle,  imuXInvertToggle,  imuXInvertLabel},
    {"X Curve", imuXCurveToggle,   imuXCurveToggle,   imuXCurveLabel},
    {"PAD Y",   imuYOpenPicker,    imuYOpenPicker,    imuYLabel},
    {"Y Sens",  imuYSensInc,       imuYSensDec,       imuYSensLabel},
    {"Y Invert",imuYInvertToggle,  imuYInvertToggle,  imuYInvertLabel},
    {"Y Curve", imuYCurveToggle,   imuYCurveToggle,   imuYCurveLabel},
};
// MIDI gets its own category (v0.99871) rather than four more rows on the
// IMU page, which is already eleven items across two columns. It will also
// be the natural home for CC receive and clock sync when those arrive.
// Zero is NOT the neutral value for every controller (v0.99874).
//
// Sending 0 to release a controller is right for an effect depth, where 0
// means "none". It is catastrophic for CC7 (Channel Volume) and CC11
// (Expression), where 0 means SILENCE — and since nothing sends them again
// afterwards, the receiver stays mute until it is power-cycled. Reported
// exactly that way: moving off CC7 or CC11 killed the sound until reset.
//
// So each controller is released to whatever ITS idle value is: full for
// the two volume controls, centre for pan, zero for the rest.
uint8_t midiCcReleaseValue(uint8_t cc){
    switch(cc){
        case 7:      // Channel Volume
        case 11:     // Expression
            return 127;
        case 10:     // Pan — centre, not hard left
            return 64;
        default:
            return 0;
    }
}

void midiCcRelease(uint8_t cc){
    if(midiCcOutEnabled)midiSendCC(midiCcOutChannel,cc,midiCcReleaseValue(cc));
}

void midiCcOutToggle(){
    // Release both controllers on the way out, for the same reason
    // (v0.99873): a latched CC left at its last value keeps affecting the
    // receiver after this has been switched off.
    if(midiCcOutEnabled){
        midiCcRelease(midiCcOutNumX);
        midiCcRelease(midiCcOutNumY);
        midiCcLastSentX=midiCcLastSentY=-1;
    }
    midiCcOutEnabled=!midiCcOutEnabled;
}
const char *midiCcOutLabel(){return midiCcOutEnabled?"ON":"off";}

// CC numbers step by one across the full 0-127 range. Holding the key
// repeats, so crossing the range is not the chore that implies.
// Zero the OLD controller before moving to a new one (v0.99873).
//
// A CC is a value a receiver latches and holds, not a momentary command.
// So retargeting X from Chorus to Reverb used to leave the chorus at
// whatever depth it was last sent — the two effects stacked, and the only
// way back was to steer the CC to that number again and wind it down by
// hand. Sending 0 on the way out is what the player means by "not that
// one any more".
//
// Also resets the last-sent value, so the new controller receives its
// first update immediately rather than waiting for the tilt to cross into
// a different 7-bit step.
void midiCcRetarget(uint8_t &num,int &lastSent,int delta){
    midiCcRelease(num);
    num=(uint8_t)((num+delta+128)&0x7F);
    lastSent=-1;
}
void midiCcXInc(){midiCcRetarget(midiCcOutNumX,midiCcLastSentX,+1);}
void midiCcXDec(){midiCcRetarget(midiCcOutNumX,midiCcLastSentX,-1);}
void midiCcYInc(){midiCcRetarget(midiCcOutNumY,midiCcLastSentY,+1);}
void midiCcYDec(){midiCcRetarget(midiCcOutNumY,midiCcLastSentY,-1);}

// Named where a name is near-universal, so the common choices are
// recognisable instead of being bare numbers.
const char *midiCcName(uint8_t cc){
    switch(cc){
        case 1:  return "Mod";
        case 7:  return "Vol";
        case 10: return "Pan";
        case 11: return "Expr";
        case 71: return "Reso";
        case 74: return "Cutoff";
        case 91: return "Reverb";
        case 93: return "Chorus";
        default: return nullptr;
    }
}
char midiCcXBuf[16];const char *midiCcXLabel(){
    const char *n=midiCcName(midiCcOutNumX);
    if(n)snprintf(midiCcXBuf,sizeof(midiCcXBuf),"%d %s",midiCcOutNumX,n);
    else snprintf(midiCcXBuf,sizeof(midiCcXBuf),"CC%d",midiCcOutNumX);
    return midiCcXBuf;}
char midiCcYBuf[16];const char *midiCcYLabel(){
    const char *n=midiCcName(midiCcOutNumY);
    if(n)snprintf(midiCcYBuf,sizeof(midiCcYBuf),"%d %s",midiCcOutNumY,n);
    else snprintf(midiCcYBuf,sizeof(midiCcYBuf),"CC%d",midiCcOutNumY);
    return midiCcYBuf;}

void midiChInc(){midiCcOutChannel=(uint8_t)((midiCcOutChannel+1)&0x0F);}
void midiChDec(){midiCcOutChannel=(uint8_t)((midiCcOutChannel+15)&0x0F);}
char midiChBuf[8];const char *midiChLabel(){
    // Stored 0-15, shown 1-16 — every piece of hardware labels them that
    // way, and showing the internal number would be a needless trap.
    snprintf(midiChBuf,sizeof(midiChBuf),"%d",midiCcOutChannel+1);
    return midiChBuf;}

void midiNoteOutToggle(){
    midiNoteOutEnabled=!midiNoteOutEnabled;
    // Send the instrument on switch-on, so the GM synth is playing the
    // chosen sound from the first note rather than whatever it defaulted
    // to (v0.99872).
    if(midiNoteOutEnabled)midiSendProgram(midiCcOutChannel,midiGmProgram);
}
const char *midiNoteOutLabel(){return midiNoteOutEnabled?"ON":"off";}

// The 16 GM instrument families, which is a far more useful granularity
// than 128 numbered programs on a four-row menu. Stepping picks the first
// program of each family.
const char *GM_FAMILIES[16]={
    "Piano","Chrom Perc","Organ","Guitar","Bass","Strings","Ensemble","Brass",
    "Reed","Pipe","Synth Lead","Synth Pad","Synth FX","Ethnic","Percussive","SFX"};
void midiGmInc(){
    midiGmProgram=(uint8_t)((midiGmProgram+8)&0x7F);
    if(midiNoteOutEnabled)midiSendProgram(midiCcOutChannel,midiGmProgram);
}
void midiGmDec(){
    midiGmProgram=(uint8_t)((midiGmProgram+120)&0x7F);
    if(midiNoteOutEnabled)midiSendProgram(midiCcOutChannel,midiGmProgram);
}
char midiGmBuf[20];const char *midiGmLabel(){
    snprintf(midiGmBuf,sizeof(midiGmBuf),"%s",GM_FAMILIES[(midiGmProgram>>3)&0x0F]);
    return midiGmBuf;}

void thereminBegin();   // defined above with the sensor code
void midiSerialSuspend();   // defined with the MIDI transport, below
void midiSerialResume();

void thereminToggle(){
    // Timed rather than guessed at (v0.99913): the fast single-address
    // probe should cost under a millisecond, so if the freeze is still
    // several seconds after that fix, the probe was not the whole story.
    // This prints exactly where the time goes on the next attempt instead
    // of trading one guess for another.
    unsigned long t0=millis();
    // Fast targeted probe — see thereminFullScan below.
    thereminFullScan=false;
    thereminEnabled=!thereminEnabled;
    // The Grove port has exactly two signal pins, and CPS_TOF_SDA_PIN/
    // SCL_PIN are the SAME physical pins as CPS_MIDI_RX_PIN/TX_PIN
    // (v0.99910) — one Grove connector, and MIDI serial starts on those
    // pins unconditionally at boot regardless of what is actually plugged
    // in. Configuring I2C on top of a running UART on the same GPIOs is a
    // genuine electrical conflict, not a software race: corrupted bytes on
    // the MIDI side read as random Note On/Off and CC messages, which is
    // where the phantom notes, the runaway pitch, and parameters moving on
    // their own all actually came from — including with no ToF unit
    // attached, since the UART alone was enough to misbehave once I2C
    // reconfigured the pins under it.
    //
    // So the two take turns. Enabling Theremin suspends the MIDI UART
    // before touching the pins; disabling it hands the UART back.
    if(thereminEnabled){
        // thereminBegin() suspends MIDI itself now (v0.99911), so every
        // caller is covered without needing to remember to do it first.
        if(!tofPresent)thereminBegin();
    } else {
        midiSerialResume();
    }
    // Release the note when switching off, or the last pitch the hand was
    // at would hang (v0.999).
    if(!thereminEnabled&&thereminActive)thereminStop();
    Serial.printf("[ToF] toggle took %lums\n",millis()-t0);
}
const char *thereminLabel(){
    // Says WHY when there is no sensor, rather than only that there is
    // none (v0.9992): "no i2c device" means nothing answered on the bus at
    // all — wiring or power — while an address plus "init fail" means
    // something is there but is not this sensor.
    if(!tofPresent){
        // Distinct from the boot-time reasons: this one means the sensor
        // WAS working and then stopped (v0.99914) — a wiring, power, or
        // interference issue during use rather than at startup.
        if(thereminLostConnection)return "lost connection";
        return tofScanResult;
    }
    return thereminEnabled?"ON":"off";
}

// Rescan on demand, so a unit can be plugged in without rebooting and the
// result read straight off the same row.
void thereminBegin();
void thereminRescan(){
    // The one place that asks for the diagnostic sweep, since it is the
    // one place someone is actually looking at the result (v0.99912).
    thereminFullScan=true;
    thereminBegin();
}
void thereminQuantToggle(){thereminQuantize=!thereminQuantize;}
const char *thereminQuantLabel(){return thereminQuantize?"Semitone":"Smooth";}
void thereminOctInc(){thereminOctaves=min(thereminOctaves+1,4);}
void thereminOctDec(){thereminOctaves=max(thereminOctaves-1,1);}
char thereminOctBuf[8];const char *thereminOctLabel(){
    snprintf(thereminOctBuf,sizeof(thereminOctBuf),"%d oct",thereminOctaves);
    return thereminOctBuf;}

// The top of the range, as its own setting (v0.9992). Steps a full octave
// at a time — semitone precision at the TOP note is not the point, and a
// coarser step covers the -24..+36 range in fewer presses. The note name
// is what actually matters when picking this, so the label shows that
// rather than a raw semitone count.
const char *getNoteName(float freq);   // defined much further down

void thereminTopInc(){thereminTopSemis=min(thereminTopSemis+12,36);}
void thereminTopDec(){thereminTopSemis=max(thereminTopSemis-12,-24);}
char thereminTopBuf[8];const char *thereminTopLabel(){
    float hz=261.63f*powf(2.f,(float)thereminTopSemis/12.f);
    snprintf(thereminTopBuf,sizeof(thereminTopBuf),"%s",getNoteName(hz));
    return thereminTopBuf;}
void thereminFarInc(){thereminFarMm=min(thereminFarMm+50,1000);}
void thereminFarDec(){thereminFarMm=max(thereminFarMm-50,thereminNearMm+100);}
char thereminFarBuf[10];const char *thereminFarLabel(){
    snprintf(thereminFarBuf,sizeof(thereminFarBuf),"%dmm",thereminFarMm);
    return thereminFarBuf;}
// Live reading, so the range can be set by holding a hand where you want
// the limit rather than by guessing millimetres. Read-only, but with a
// real no-op handler rather than nullptr — every other row's handler is
// called unconditionally, and a null one would crash on the first press.
void thereminNoop(){}
void thereminBusToggle(){
    tofBusIndex^=1;
    tofPresent=false;
    snprintf(tofScanResult,sizeof(tofScanResult),"press Rescan");
}
const char *thereminBusLabel(){return tofBusIndex==0?"Grove":"Cap G8/9";}
char thereminScanBuf[16];const char *thereminScanLabel(){
    // On the Cap's shared bus, "found" legitimately includes the
    // keyboard controller, the IMU and whatever else already lives there
    // — not a miscount (v0.99919). Grove's bus carries only the sensor,
    // so it shows a plain count; Cap's is labelled "shared" so a higher
    // number there does not read as an error.
    if(tofBusIndex==1)
        snprintf(thereminScanBuf,sizeof(thereminScanBuf),"%d shared",tofScanDevices);
    else
        snprintf(thereminScanBuf,sizeof(thereminScanBuf),"%d found",tofScanDevices);
    return thereminScanBuf;}
char thereminDistBuf[12];const char *thereminDistLabel(){
    if(!tofPresent||thereminLastMm<=0)return "--";
    snprintf(thereminDistBuf,sizeof(thereminDistBuf),"%dmm",thereminLastMm);
    return thereminDistBuf;}

SettingItem thereminMenuItems[]={
    {"Theremin",thereminToggle,     thereminToggle,     thereminLabel},
    {"Pitch",   thereminQuantToggle,thereminQuantToggle,thereminQuantLabel},
    {"Top",     thereminTopInc,     thereminTopDec,     thereminTopLabel},
    {"Span",    thereminOctInc,     thereminOctDec,     thereminOctLabel},
    {"Far",     thereminFarInc,     thereminFarDec,     thereminFarLabel},
    {"Reading", thereminNoop,       thereminNoop,       thereminDistLabel},
    {"Bus",     thereminBusToggle,  thereminBusToggle,  thereminBusLabel},
    {"Rescan",  thereminRescan,     thereminRescan,     thereminScanLabel},
};

void midiClockOutToggle(){
    midiClockOutEnabled=!midiClockOutEnabled;
    // Tell the other machine to stop rather than leaving it running with
    // no clock arriving (v0.99895).
    if(!midiClockOutEnabled&&midiClockOutWasPlaying){
        midiSendRealtime(0xFC);
        midiClockOutWasPlaying=false;
    }
}
const char *midiClockOutLabel(){return midiClockOutEnabled?"ON":"off";}

void midiClockToggle(){
    midiClockEnabled=!midiClockEnabled;
    if(!midiClockEnabled)midiClockReset();
}
char midiClockBuf[14];const char *midiClockLabel(){
    if(!midiClockEnabled)return "off";
    // Show the received tempo once there is one, so it is obvious whether
    // a master is actually sending — "ON" alone cannot distinguish a
    // working link from a silent cable.
    if(!midiClockLocked)return "ON (wait)";
    snprintf(midiClockBuf,sizeof(midiClockBuf),"%.0f BPM",midiClockBpm);
    return midiClockBuf;}

void midiCcInToggle(){midiCcInEnabled=!midiCcInEnabled;}
const char *midiCcInLabel(){return midiCcInEnabled?"ON":"off";}

// Slot CC numbers reuse the same naming helper as the send side, so a
// number the player recognises reads the same in both directions.
char midiCcIn0Buf[16];const char *midiCcIn0Label(){
    const char *n=midiCcName(midiCcInNum[0]);
    if(n)snprintf(midiCcIn0Buf,sizeof(midiCcIn0Buf),"%d %s",midiCcInNum[0],n);
    else snprintf(midiCcIn0Buf,sizeof(midiCcIn0Buf),"CC%d",midiCcInNum[0]);
    return midiCcIn0Buf;}
char midiCcIn1Buf[16];const char *midiCcIn1Label(){
    const char *n=midiCcName(midiCcInNum[1]);
    if(n)snprintf(midiCcIn1Buf,sizeof(midiCcIn1Buf),"%d %s",midiCcInNum[1],n);
    else snprintf(midiCcIn1Buf,sizeof(midiCcIn1Buf),"CC%d",midiCcInNum[1]);
    return midiCcIn1Buf;}
void midiCcIn0Inc(){midiCcInNum[0]=(uint8_t)((midiCcInNum[0]+1)&0x7F);}
void midiCcIn0Dec(){midiCcInNum[0]=(uint8_t)((midiCcInNum[0]+127)&0x7F);}
void midiCcIn1Inc(){midiCcInNum[1]=(uint8_t)((midiCcInNum[1]+1)&0x7F);}
void midiCcIn1Dec(){midiCcInNum[1]=(uint8_t)((midiCcInNum[1]+127)&0x7F);}

// Destination steps through IMU_PICKER_ORDER rather than the raw enum, so
// the choices appear in the same order and grouping the IMU picker uses —
// and unassignable entries never come up. Clearing the offset on the way
// out matters: leaving it behind would freeze the old parameter at
// whatever the knob last sent, the same trap as the morph IMU handover.
// Opens the shared picker rather than cycling through thirty targets one
// key press at a time (v0.99882) — which is what the IMU page moved away
// from for the same reason.
void midiCcIn0TgtOpen(){openImuPicker(IMU_PICKER_AXIS_CC0);}
void midiCcIn1TgtOpen(){openImuPicker(IMU_PICKER_AXIS_CC1);}
const char *midiCcIn0TgtLabel(){return imuTargetName(midiCcInTarget[0]);}
const char *midiCcIn1TgtLabel(){return imuTargetName(midiCcInTarget[1]);}

// Split into Out and In (v0.99892). One MIDI page had reached twelve rows
// and the next feature would not fit, but the reason to split is not just
// room: sending and receiving are two different jobs and were only sharing
// a page because they share a word. The channel setting stays with Out,
// since that is the only direction it applies to — reception is Omni.
void midiSw0Inc(){midiSwNum[0]=(uint8_t)((midiSwNum[0]+1)&0x7F);}
void midiSw0Dec(){midiSwNum[0]=(uint8_t)((midiSwNum[0]+127)&0x7F);}
void midiSw1Inc(){midiSwNum[1]=(uint8_t)((midiSwNum[1]+1)&0x7F);}
void midiSw1Dec(){midiSwNum[1]=(uint8_t)((midiSwNum[1]+127)&0x7F);}
char midiSw0Buf[16];const char *midiSw0Label(){
    const char *n=midiCcName(midiSwNum[0]);
    if(n)snprintf(midiSw0Buf,sizeof(midiSw0Buf),"%d %s",midiSwNum[0],n);
    else snprintf(midiSw0Buf,sizeof(midiSw0Buf),"CC%d",midiSwNum[0]);
    return midiSw0Buf;}
char midiSw1Buf[16];const char *midiSw1Label(){
    const char *n=midiCcName(midiSwNum[1]);
    if(n)snprintf(midiSw1Buf,sizeof(midiSw1Buf),"%d %s",midiSwNum[1],n);
    else snprintf(midiSw1Buf,sizeof(midiSw1Buf),"CC%d",midiSwNum[1]);
    return midiSw1Buf;}

// Only five choices, so cycling is right here — a picker would be more
// ceremony than the list deserves.
void midiSwFnStep(int slot,int d){
    int n=((int)midiSwFn[slot]+d+(int)MidiSwitchFn::FN_COUNT)%(int)MidiSwitchFn::FN_COUNT;
    midiSwFn[slot]=(MidiSwitchFn)n;
}
void midiSwModeStep(int slot){
    midiSwMode[slot]=(midiSwMode[slot]==MidiSwMode::TOGGLE)?MidiSwMode::DIRECT
                                                           :MidiSwMode::TOGGLE;
    // Forget the last level: the two modes read it differently, and a
    // stale one would swallow the first press after a change (v0.99893).
    midiSwWasDown[slot]=false;
}
void midiSw0ModeTgl(){midiSwModeStep(0);}
void midiSw1ModeTgl(){midiSwModeStep(1);}
const char *midiSw0ModeLabel(){return midiSwModeName(midiSwMode[0]);}
const char *midiSw1ModeLabel(){return midiSwModeName(midiSwMode[1]);}

void midiSw0FnInc(){midiSwFnStep(0,+1);}
void midiSw0FnDec(){midiSwFnStep(0,-1);}
void midiSw1FnInc(){midiSwFnStep(1,+1);}
void midiSw1FnDec(){midiSwFnStep(1,-1);}
const char *midiSw0FnLabel(){return midiSwitchFnName(midiSwFn[0]);}
const char *midiSw1FnLabel(){return midiSwitchFnName(midiSwFn[1]);}

void openMidiOutCategory();
void openMidiInCategory();

SettingItem midiOutMenuItems[]={
    {"Note Out",midiNoteOutToggle,midiNoteOutToggle,midiNoteOutLabel},
    {"GM Sound",midiGmInc,       midiGmDec,        midiGmLabel},
    {"IMU->CC", midiCcOutToggle, midiCcOutToggle, midiCcOutLabel},
    {"X CC",    midiCcXInc,      midiCcXDec,      midiCcXLabel},
    {"Y CC",    midiCcYInc,      midiCcYDec,      midiCcYLabel},
    {"Clock Out",midiClockOutToggle,midiClockOutToggle,midiClockOutLabel},
    {"Channel", midiChInc,       midiChDec,       midiChLabel},
};
SettingItem midiInMenuItems[]={
    {"Clock In",midiClockToggle, midiClockToggle, midiClockLabel},
    {"CC In",   midiCcInToggle,  midiCcInToggle,  midiCcInLabel},
    {"In1 CC",  midiCcIn0Inc,    midiCcIn0Dec,    midiCcIn0Label},
    {"In1 Dest",midiCcIn0TgtOpen,midiCcIn0TgtOpen,midiCcIn0TgtLabel},
    {"In2 CC",  midiCcIn1Inc,    midiCcIn1Dec,    midiCcIn1Label},
    {"In2 Dest",midiCcIn1TgtOpen,midiCcIn1TgtOpen,midiCcIn1TgtLabel},
    {"Sw1 CC",  midiSw0Inc,      midiSw0Dec,      midiSw0Label},
    {"Sw1 Fn",  midiSw0FnInc,    midiSw0FnDec,    midiSw0FnLabel},
    {"Sw1 Mode",midiSw0ModeTgl,  midiSw0ModeTgl,  midiSw0ModeLabel},
    {"Sw2 CC",  midiSw1Inc,      midiSw1Dec,      midiSw1Label},
    {"Sw2 Fn",  midiSw1FnInc,    midiSw1FnDec,    midiSw1FnLabel},
    {"Sw2 Mode",midiSw1ModeTgl,  midiSw1ModeTgl,  midiSw1ModeLabel},
};

SettingItem bendMenuItems[]={
    {"Bend wid", bendWInc, bendWDec, bendWLabel},
    {"Bend atk", bendAInc, bendADec, bendALabel},
    {"Bend rel", bendRInc, bendRDec, bendRLabel},
    {"Reset",    openBendReset, openBendReset, resetEnterLabel},
};
SettingItem portaMenuItems[]={
    // The ON/OFF row is gone (v0.994): portamento already toggles from a
    // performance key, exactly as the IMU axes do, and a menu row that
    // duplicates a shortcut is one more thing to scroll past. Speed and
    // Reset stay, since neither has a key.
    {"Porta spd",  portaSpdInc,  portaSpdDec,  portaSpdLabel},
    {"Reset",      openPortaReset, openPortaReset, resetEnterLabel},
};

// Returns the item array/count/title for the currently-open category screen.
void playModeToggle(){playMode=(playMode==PlayMode::EZ)?PlayMode::PRO:PlayMode::EZ;recomputeKeyNotes();}
// "Style" rather than "Mode" (v0.9924). When this was written there was
// no MODE here; PLAY / SEQ / SONG came later and took the word, leaving
// two different things called Mode. This one is a playing style.
const char *playModeLabel(){return playMode==PlayMode::EZ?"EZ Style":"Pro Style";}

// ---------------------------------------------------------
// Scale picker (Play Mode > Scale) — a genuine 2-level menu: pick a
// category first, then a scale within it. Unlike the IMU target picker's
// single flat list, this uses two full steps since the scale count is
// large enough that even category-grouped flat scrolling would be a lot
// to page through. Selecting a scale takes effect IMMEDIATELY (live
// preview) so the user can hold a note key while scrolling to hear it;
// Tab from the category list fully cancels back to whatever scale was
// active before the picker was opened.
bool scalePickerOpen=false;
int  scalePickerLevel=0;            // 0 = category list, 1 = scale list within a category
int  scalePickerCategoryIndex=0;
int  scalePickerRowIndex=0;
int  scalePickerOriginalScaleIndex=0;

bool prevScalePickerUpPressed=false, prevScalePickerDownPressed=false;
bool prevScalePickerConfirmPressed=false, prevScalePickerTabPressed=false;

int getScalesInCategory(int cat,int *outIndices,int maxOut){
    int n=0;
    for(int i=0;i<NUM_SCALES&&n<maxOut;i++)if(SCALES[i].category==cat)outIndices[n++]=i;
    return n;
}

void openScalePicker(){
    scalePickerOpen=true;
    scalePickerLevel=0;
    scalePickerRowIndex=0;
    scalePickerCategoryIndex=0;
    scalePickerOriginalScaleIndex=currentScaleIndex;
    auto s=M5Cardputer.Keyboard.keysState();
    bool heldUp=false,heldDown=false,heldConfirm=s.enter;
    for(char c:s.word){if(c==';')heldUp=true;if(c=='.')heldDown=true;if(c=='/')heldConfirm=true;}
    prevScalePickerUpPressed=heldUp;prevScalePickerDownPressed=heldDown;
    prevScalePickerConfirmPressed=heldConfirm;prevScalePickerTabPressed=s.tab;
}
const char *scalePickerEnterLabel(){return "Select>";}
const char *currentScaleLabel(){return SCALES[currentScaleIndex].name;}

void updateScalePicker(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool mU=false,mD=false,confirm=s.enter,cancel=s.tab;
    for(char c:s.word){if(c==';')mU=true;if(c=='.')mD=true;if(c=='/')confirm=true;}

    if(scalePickerLevel==0){
        if(mU&&!prevScalePickerUpPressed)  scalePickerRowIndex=(scalePickerRowIndex-1+NUM_SCALE_CATEGORIES)%NUM_SCALE_CATEGORIES;
        if(mD&&!prevScalePickerDownPressed)scalePickerRowIndex=(scalePickerRowIndex+1)%NUM_SCALE_CATEGORIES;
        prevScalePickerUpPressed=mU;prevScalePickerDownPressed=mD;
        if(confirm&&!prevScalePickerConfirmPressed){
            scalePickerCategoryIndex=scalePickerRowIndex;
            int indices[32];int n=getScalesInCategory(scalePickerCategoryIndex,indices,32);
            scalePickerRowIndex=0;
            for(int i=0;i<n;i++)if(indices[i]==currentScaleIndex){scalePickerRowIndex=i;break;}
            // Actually apply the highlighted scale now (not just move the
            // cursor) — otherwise confirming immediately without scrolling
            // first (the only option for a single-item category, e.g.
            // Chromatic) would close the picker without ever having
            // switched currentScaleIndex, silently keeping the old scale.
            currentScaleIndex=indices[scalePickerRowIndex];
            recomputeKeyNotes();
            scalePickerLevel=1;
        }
        prevScalePickerConfirmPressed=confirm;
        if(cancel&&!prevScalePickerTabPressed){
            currentScaleIndex=scalePickerOriginalScaleIndex;
            recomputeKeyNotes();
            scalePickerOpen=false;
        }
        prevScalePickerTabPressed=cancel;
    } else {
        int indices[32];int n=getScalesInCategory(scalePickerCategoryIndex,indices,32);
        if(mU&&!prevScalePickerUpPressed){
            scalePickerRowIndex=(scalePickerRowIndex-1+n)%n;
            currentScaleIndex=indices[scalePickerRowIndex];recomputeKeyNotes();
        }
        if(mD&&!prevScalePickerDownPressed){
            scalePickerRowIndex=(scalePickerRowIndex+1)%n;
            currentScaleIndex=indices[scalePickerRowIndex];recomputeKeyNotes();
        }
        prevScalePickerUpPressed=mU;prevScalePickerDownPressed=mD;
        if(confirm&&!prevScalePickerConfirmPressed){
            scalePickerOpen=false; // keep currentScaleIndex — already live-previewed
        }
        prevScalePickerConfirmPressed=confirm;
        if(cancel&&!prevScalePickerTabPressed){
            scalePickerLevel=0; // one level back, keeping the current preview
            scalePickerRowIndex=scalePickerCategoryIndex;
        }
        prevScalePickerTabPressed=cancel;
    }
}

SettingItem playModeMenuItemsEZ[]={
    {"Style", playModeToggle, playModeToggle, playModeLabel},
};
// Analog Drift sits with Scale on the Pro page only (v0.9941): it is a
// deliberate loss of stability, which is the opposite of what EZ Style is
// for. The warning line under the list says as much, since "Drift ON" on
// its own does not tell you the tuning is about to wander.
void analogDriftToggle(){analogDriftOn=!analogDriftOn;}
const char *analogDriftLabel(){return analogDriftOn?"ON":"off";}
void driftAmtInc(){analogDriftAmount=min(analogDriftAmount+0.05f,1.f);}
void driftAmtDec(){analogDriftAmount=max(analogDriftAmount-0.05f,0.f);}
char driftAmtBuf[10];const char *driftAmtLabel(){
    snprintf(driftAmtBuf,sizeof(driftAmtBuf),"%.0f%%",analogDriftAmount*100);
    return driftAmtBuf;}

SettingItem playModeMenuItemsPro[]={
    {"Style", playModeToggle, playModeToggle, playModeLabel},
    {"Scale", openScalePicker,openScalePicker,currentScaleLabel},
    {"Drift", analogDriftToggle, analogDriftToggle, analogDriftLabel},
    {"Amount",driftAmtInc,      driftAmtDec,       driftAmtLabel},
};

void arpToggle(){
    arpEnabled=!arpEnabled;
    if(!arpEnabled){currentFreq=0.f;arpHeldCount=0;}
    else{arpStepIndex=0;arpLastStepMs=millis();}
}
void arpTypeNext(){arpType=(ArpType)(((uint8_t)arpType+1)%5);}
void arpTypePrev(){arpType=(ArpType)(((uint8_t)arpType+4)%5);}
const char *arpTypeLabel(){
    switch(arpType){
        case ArpType::UP:        return "Up";
        case ArpType::DOWN:      return "Down";
        case ArpType::UP_DOWN:   return "Up-Down";
        case ArpType::AS_PLAYED: return "As Played";
        case ArpType::RANDOM:    return "Random";
    }
    return "?";
}
// Step 5->1 (v0.9996x): Tempo/Swing are timing controls, and 5-unit
// steps were too coarse for fine adjustment. Fine-by-default now works
// because the CATEGORY dispatch below gives these two items — and only
// these, via the onIncrement!=onDecrement check that already existed for
// a different reason — the same hold-to-repeat every VCF/VCA/LFO/FX
// value already has, so a single tap moves by 1 and holding covers the
// same large jumps a bare 5-step tap used to.
void arpTempoInc(){arpTempoBpm=min(arpTempoBpm+1.f,240.f);}
void arpTempoDec(){arpTempoBpm=max(arpTempoBpm-1.f,40.f);}
char arpTempoBuf[8];
const char *arpTempoLabel(){snprintf(arpTempoBuf,sizeof(arpTempoBuf),"%.0f",arpTempoBpm);return arpTempoBuf;}
void arpRateNext(){arpRateIndex=(arpRateIndex+1)%NUM_ARP_RATES;}
void arpRatePrev(){arpRateIndex=(arpRateIndex+NUM_ARP_RATES-1)%NUM_ARP_RATES;}
const char *arpRateLabel(){return ARP_RATES[arpRateIndex].label;}
void arpSwingInc(){arpSwing=min(arpSwing+1.f,100.f);}
void arpSwingDec(){arpSwing=max(arpSwing-1.f,-100.f);}
char arpSwingBuf[8];
const char *arpSwingLabel(){snprintf(arpSwingBuf,sizeof(arpSwingBuf),"%+.0f%%",arpSwing);return arpSwingBuf;}

SettingItem arpMenuItems[]={
    {"Type",    arpTypeNext,  arpTypePrev,  arpTypeLabel},
    {"Tempo",   arpTempoInc,  arpTempoDec,  arpTempoLabel},
    {"Rate",    arpRateNext,  arpRatePrev,  arpRateLabel},
    {"Swing", arpSwingInc,arpSwingDec,arpSwingLabel},
};

SettingItem patternMenuItems[]={
    {"Save", patternBankSaveEnter, patternBankSaveEnter, patternBankEnterLabel},
    {"Load", patternBankLoadEnter, patternBankLoadEnter, patternBankEnterLabel},
    {"Random", openPatternRandomize, openPatternRandomize, resetEnterLabel},
};

// UI theme picker (v0.9936). A plain cycling item rather than its own
// screen — there are five entries and the effect is visible the instant it
// changes, so a list to scroll would be more ceremony than it is worth.
// Also forces a full redraw: uiColor is read all over the place and the
// partial-redraw paths only repaint what they think is dirty, so without
// this the new accent would arrive piecemeal — the same class of problem
// as the v0.9922 help-overlay corruption.
void uiThemeNext(){uiThemeIndex=(uiThemeIndex+1)%NUM_UI_THEMES;applyUiTheme();uiThemeDirty=true;}
void uiThemePrev(){uiThemeIndex=(uiThemeIndex+NUM_UI_THEMES-1)%NUM_UI_THEMES;applyUiTheme();uiThemeDirty=true;}
const char *uiThemeLabel(){return UI_THEMES[constrain(uiThemeIndex,0,NUM_UI_THEMES-1)].name;}

// Display category contents, defined here rather than with the other
// menu items because getCategoryItems() below has to see the array
// (v0.9939).
// Theme picker (v0.9937): its own list rather than a value that cycles in
// place. Cycling meant you could only compare a theme against the one
// before it, which is how Ice and Access came to look alike — seen side by
// side they are obviously different. The list also has room for swatches,
// so the colours can be judged without applying them.
void openThemePicker(){
    themePickerIndex=constrain(uiThemeIndex,0,NUM_UI_THEMES-1);
    // Seed the edge-trackers with what is held right now, so the keypress
    // that opened this isn't read again as a fresh press inside it (same
    // reasoning as the patch and pattern browsers).
    auto st=M5Cardputer.Keyboard.keysState();
    prevThemeUpPressed=false;prevThemeDownPressed=false;
    for(char c:st.word){if(c==';')prevThemeUpPressed=true;if(c=='.')prevThemeDownPressed=true;}
    prevThemeConfirmPressed=st.enter;prevThemeTabPressed=st.tab;
    themePickerOpen=true;
}

void uiBrightInc(){uiBrightness=(uint8_t)min((int)uiBrightness+16,(int)UI_BRIGHT_MAX);applyUiBrightness();}
void uiBrightDec(){uiBrightness=(uint8_t)max((int)uiBrightness-16,(int)UI_BRIGHT_MIN);applyUiBrightness();}
char uiBrightBuf[10];const char *uiBrightLabel(){
    snprintf(uiBrightBuf,sizeof(uiBrightBuf),"%d%%",(int)((float)uiBrightness/255.f*100.f+0.5f));
    return uiBrightBuf;}

SettingItem displayMenuItems[]={
    {"Theme",      openThemePicker, openThemePicker, uiThemeLabel},
    {"Brightness", uiBrightInc,     uiBrightDec,     uiBrightLabel},
};

// midiMenuItems is defined after categoryEnterLabel(), which is below
// this function — declared rather than moved, since moving it would just
// shift the problem to categoryEnterLabel().
extern SettingItem midiMenuItems[2];

SettingItem *getCategoryItems(int &count,const char *&title){
    switch(currentCategory){
        case SettingsCategory::PATCH:
            count=sizeof(patchMenuItems)/sizeof(patchMenuItems[0]);
            title="PATCH";
            return patchMenuItems;
        case SettingsCategory::IMU:
            if(isCardputerAdv){
                count=sizeof(imuMenuItemsAdv)/sizeof(imuMenuItemsAdv[0]);
                title="IMU";
                return imuMenuItemsAdv;
            } else {
                count=sizeof(imuMenuItemsOriginal)/sizeof(imuMenuItemsOriginal[0]);
                title="PAD";
                return imuMenuItemsOriginal;
            }
        case SettingsCategory::BEND:
            count=sizeof(bendMenuItems)/sizeof(bendMenuItems[0]);
            title="BEND";
            return bendMenuItems;
        case SettingsCategory::PORTAMENTO:
            count=sizeof(portaMenuItems)/sizeof(portaMenuItems[0]);
            title="PORTAMENTO";
            return portaMenuItems;
        case SettingsCategory::MIDI:
            count=sizeof(midiMenuItems)/sizeof(midiMenuItems[0]);
            title="MIDI";
            return midiMenuItems;
        case SettingsCategory::MIDI_OUT:
            count=sizeof(midiOutMenuItems)/sizeof(midiOutMenuItems[0]);
            title="MIDI > OUT";
            return midiOutMenuItems;
        case SettingsCategory::THEREMIN:
            count=sizeof(thereminMenuItems)/sizeof(thereminMenuItems[0]);
            title="THEREMIN";
            return thereminMenuItems;
        case SettingsCategory::MIDI_IN:
            count=sizeof(midiInMenuItems)/sizeof(midiInMenuItems[0]);
            title="MIDI > IN";
            return midiInMenuItems;
        case SettingsCategory::SCREEN:
            count=sizeof(displayMenuItems)/sizeof(displayMenuItems[0]);
            title="DISPLAY";
            return displayMenuItems;
        case SettingsCategory::PLAYMODE:
            title="PLAY STYLE";
            if(playMode==PlayMode::PRO){
                count=sizeof(playModeMenuItemsPro)/sizeof(playModeMenuItemsPro[0]);
                return playModeMenuItemsPro;
            } else {
                count=sizeof(playModeMenuItemsEZ)/sizeof(playModeMenuItemsEZ[0]);
                return playModeMenuItemsEZ;
            }
        case SettingsCategory::ARP:
            count=sizeof(arpMenuItems)/sizeof(arpMenuItems[0]);
            title="ARP";
            return arpMenuItems;
        case SettingsCategory::PATTERN:
            count=sizeof(patternMenuItems)/sizeof(patternMenuItems[0]);
            title="PATTERN";
            return patternMenuItems;
    }
    count=0;title="";return nullptr;
}

void openCategory(SettingsCategory c){
    currentCategory=c;
    selectedCategoryIndex=0;
    appMode=AppMode::CATEGORY;
}
void openPatchCategory(){openCategory(SettingsCategory::PATCH);}
void openImuCategory(){openCategory(SettingsCategory::IMU);}
void openBendCategory(){openCategory(SettingsCategory::BEND);}
void openPortaCategory(){openCategory(SettingsCategory::PORTAMENTO);}
void openPlayModeCategory(){openCategory(SettingsCategory::PLAYMODE);}
void openArpCategory(){openCategory(SettingsCategory::ARP);}
void openDisplayCategory(){openCategory(SettingsCategory::SCREEN);}
void openMidiCategory(){openCategory(SettingsCategory::MIDI);}
void openThereminCategory(){openCategory(SettingsCategory::THEREMIN);}
void openMidiOutCategory(){openCategory(SettingsCategory::MIDI_OUT);}
void openMidiInCategory(){openCategory(SettingsCategory::MIDI_IN);}
void openPatternCategory(){openCategory(SettingsCategory::PATTERN);}
// Just the arrow (v0.9924). "Select>" was the same word on every row,
// and because the row name prints in a fixed-width field it landed in a
// different column for "Portamento" and "Play Style" than for shorter
// names — a ragged edge for a word carrying no information. The arrow
// alone still says "this opens something", and it lines up.
const char *categoryEnterLabel(){return ">";}

// Kept here rather than with the two sub-menus above: it references
// categoryEnterLabel(), which is declared just above this point.
SettingItem midiMenuItems[]={
    {"Out",     openMidiOutCategory, openMidiOutCategory, categoryEnterLabel},
    {"In",      openMidiInCategory,  openMidiInCategory,  categoryEnterLabel},
};




// Top-level SETTING screen: just the category entry points. Arp is
// CardputerADV only (see the Arpeggiator section above for why), so
// original-Cardputer builds get a shorter list without it.
SettingItem settingItemsAdv[]={
    {"Patch",      openPatchCategory, openPatchCategory, categoryEnterLabel},
    {"Morph",      openTimbreScreen,  openTimbreScreen,  categoryEnterLabel},
    {"IMU",        openImuCategory,   openImuCategory,   categoryEnterLabel},
    {"Bend",       openBendCategory,  openBendCategory,  categoryEnterLabel},
    {"Portamento", openPortaCategory, openPortaCategory, categoryEnterLabel},
    {"Play Style", openPlayModeCategory, openPlayModeCategory, categoryEnterLabel},
    {"Arp",        openArpCategory,   openArpCategory,   categoryEnterLabel},
    {"Display",    openDisplayCategory, openDisplayCategory, categoryEnterLabel},
    {"MIDI",       openMidiCategory,    openMidiCategory,    categoryEnterLabel},
    {"Theremin",   openThereminCategory,openThereminCategory,categoryEnterLabel},
};
// Arp only makes sense from PLAY (it needs live chord-holding, which SEQ
// mode suppresses while a pattern is playing) — hidden when SEQ is the
// active home mode. Pattern is the inverse — only meaningful from SEQ
// (it saves/loads Sequencer step patterns) — hidden when PLAY is home.
SettingItem settingItemsAdvNoArp[]={
    {"Patch",      openPatchCategory, openPatchCategory, categoryEnterLabel},
    {"Morph",      openTimbreScreen,  openTimbreScreen,  categoryEnterLabel},
    {"Pattern",    openPatternCategory, openPatternCategory, categoryEnterLabel},
    {"IMU",        openImuCategory,   openImuCategory,   categoryEnterLabel},
    {"Bend",       openBendCategory,  openBendCategory,  categoryEnterLabel},
    {"Portamento", openPortaCategory, openPortaCategory, categoryEnterLabel},
    {"Play Style", openPlayModeCategory, openPlayModeCategory, categoryEnterLabel},
    {"Display",    openDisplayCategory, openDisplayCategory, categoryEnterLabel},
    {"MIDI",       openMidiCategory,    openMidiCategory,    categoryEnterLabel},
    {"Theremin",   openThereminCategory,openThereminCategory,categoryEnterLabel},
};
// Original Cardputer has no Arp at all, but still needs the same
// Pattern-only-from-SEQ split as the ADV lists above.
SettingItem settingItemsOriginalPlay[]={
    {"Patch",      openPatchCategory, openPatchCategory, categoryEnterLabel},
    {"Morph",      openTimbreScreen,  openTimbreScreen,  categoryEnterLabel},
    {"PAD",        openImuCategory,   openImuCategory,   categoryEnterLabel},
    {"Bend",       openBendCategory,  openBendCategory,  categoryEnterLabel},
    {"Portamento", openPortaCategory, openPortaCategory, categoryEnterLabel},
    {"Play Style", openPlayModeCategory, openPlayModeCategory, categoryEnterLabel},
    {"Display",    openDisplayCategory, openDisplayCategory, categoryEnterLabel},
    {"MIDI",       openMidiCategory,    openMidiCategory,    categoryEnterLabel},
    {"Theremin",   openThereminCategory,openThereminCategory,categoryEnterLabel},
};
SettingItem settingItemsOriginalSeq[]={
    {"Patch",      openPatchCategory, openPatchCategory, categoryEnterLabel},
    {"Morph",      openTimbreScreen,  openTimbreScreen,  categoryEnterLabel},
    {"Pattern",    openPatternCategory, openPatternCategory, categoryEnterLabel},
    {"PAD",        openImuCategory,   openImuCategory,   categoryEnterLabel},
    {"Bend",       openBendCategory,  openBendCategory,  categoryEnterLabel},
    {"Portamento", openPortaCategory, openPortaCategory, categoryEnterLabel},
    {"Play Style", openPlayModeCategory, openPlayModeCategory, categoryEnterLabel},
    {"Display",    openDisplayCategory, openDisplayCategory, categoryEnterLabel},
    {"MIDI",       openMidiCategory,    openMidiCategory,    categoryEnterLabel},
    {"Theremin",   openThereminCategory,openThereminCategory,categoryEnterLabel},
};
// Selected in setup() and whenever lastMainMode changes (see refreshSettingItems());
// kept as a plain pointer + variable (not compile-time const) so all the
// existing settingItems[i]/NUM_SETTING_ITEMS call sites elsewhere keep
// working unchanged.
SettingItem *settingItems=settingItemsAdv;
int NUM_SETTING_ITEMS=sizeof(settingItemsAdv)/sizeof(settingItemsAdv[0]);
int selectedSettingIndex=0;

void refreshSettingItems(){
    if(!isCardputerAdv){
        if(lastMainMode==AppMode::SEQ){
            settingItems=settingItemsOriginalSeq;
            NUM_SETTING_ITEMS=sizeof(settingItemsOriginalSeq)/sizeof(settingItemsOriginalSeq[0]);
        } else {
            settingItems=settingItemsOriginalPlay;
            NUM_SETTING_ITEMS=sizeof(settingItemsOriginalPlay)/sizeof(settingItemsOriginalPlay[0]);
        }
    } else if(lastMainMode==AppMode::SEQ){
        settingItems=settingItemsAdvNoArp;
        NUM_SETTING_ITEMS=sizeof(settingItemsAdvNoArp)/sizeof(settingItemsAdvNoArp[0]);
    } else {
        settingItems=settingItemsAdv;
        NUM_SETTING_ITEMS=sizeof(settingItemsAdv)/sizeof(settingItemsAdv[0]);
    }
    if(selectedSettingIndex>=NUM_SETTING_ITEMS)selectedSettingIndex=0;
}

// ==========================================================
// VCO menu items
// ==========================================================
void timbreInc(){params.timbreMorph=params.timbreMorphTarget=min(params.timbreMorph+0.1f,(float)max(1,morphChainLen-1));}
void timbreDec(){params.timbreMorph=params.timbreMorphTarget=max(params.timbreMorph-0.1f,0.f);}
// Short abbreviations for the VCO tab's tight value column — same
// widths as before this feature, unlike the full OSC_WAVEFORM_NAMES
// used on the dedicated Timbre settings screen where there's more room.
const char *oscWaveformAbbrev(OscWaveform w){
    switch(w){
        case OscWaveform::SINE:       return "Sine";
        case OscWaveform::TRIANGLE:   return "Tri";
        case OscWaveform::SAWTOOTH:   return "Saw";
        case OscWaveform::SQUARE:     return "Sq";
        case OscWaveform::WAVEFOLDER: return "Fold";
        case OscWaveform::HALFSINE:   return "HSin";
        case OscWaveform::PARABOLIC:  return "Para";
        case OscWaveform::ESAW:       return "ESaw";
        case OscWaveform::SQUEEZE:    return "Sqz";
        case OscWaveform::ESQUARE:    return "ESq";
        case OscWaveform::SAW2:       return "Saw2";
        case OscWaveform::SQUARE2:    return "Sq2";
        default:                      return "?";
    }
}
char timBuf[24];const char *timbreLabel(){
    int slot=constrain((int)params.timbreMorph,0,max(0,morphChainLen-1));
    snprintf(timBuf,sizeof(timBuf),"%s(%.1f)",oscWaveformAbbrev(morphChain[slot]),params.timbreMorph);
    return timBuf;
}
void shapeInc(){params.oscShape=min(params.oscShape+0.05f,1.f);}
void shapeDec(){params.oscShape=max(params.oscShape-0.05f,0.f);}
char shapeBuf[10];const char *shapeLabel(){snprintf(shapeBuf,sizeof(shapeBuf),"%.0f%%",params.oscShape*100);return shapeBuf;}
void detInc(){params.detuneCents=min(params.detuneCents+1.f,50.f);}
void detDec(){params.detuneCents=max(params.detuneCents-1.f,-50.f);}
char detBuf[10];const char *detuneLabel(){snprintf(detBuf,sizeof(detBuf),"%+.0fc",params.detuneCents);return detBuf;}
void finInc(){params.fineTuneCents=min(params.fineTuneCents+1.f,100.f);}
void finDec(){params.fineTuneCents=max(params.fineTuneCents-1.f,-100.f);}
char finBuf[10];const char *fineLabel(){snprintf(finBuf,sizeof(finBuf),"%+.0fc",params.fineTuneCents);return finBuf;}

// Sub oscillator
void subLInc(){params.subOscLevel=min(params.subOscLevel+0.05f,1.f);}
void subLDec(){params.subOscLevel=max(params.subOscLevel-0.05f,0.f);}
char subLBuf[10];const char *subLLabel(){snprintf(subLBuf,sizeof(subLBuf),"%.0f%%",params.subOscLevel*100);return subLBuf;}
void subOInc(){params.subOscOctave=min(params.subOscOctave+1,-1);}
void subODec(){params.subOscOctave=max(params.subOscOctave-1,-2);}
char subOBuf[8];const char *subOLabel(){snprintf(subOBuf,sizeof(subOBuf),"%d oct",params.subOscOctave);return subOBuf;}
// Noise blend
void noiseInc(){params.noiseLevel=min(params.noiseLevel+0.05f,1.f);}
void noiseDec(){params.noiseLevel=max(params.noiseLevel-0.05f,0.f);}
char noiseBuf[10];const char *noiseLabel(){snprintf(noiseBuf,sizeof(noiseBuf),"%.0f%%",params.noiseLevel*100);return noiseBuf;}

// Vibrato (v0.989). Pitch modulation, so it lives on the VCO tab rather
// than in FX — an effects menu is the wrong home for a modulation source,
// and the general LFO already offers the same routing via LfoTarget::PITCH.
// Until now these could only be reached by tilting the device.
void vibDepthInc(){params.vibratoDepth=min(params.vibratoDepth+0.05f,1.f);}
void vibDepthDec(){params.vibratoDepth=max(params.vibratoDepth-0.05f,0.f);}
char vibDepthBuf[10];const char *vibDepthLabel(){snprintf(vibDepthBuf,sizeof(vibDepthBuf),"%.0f%%",params.vibratoDepth*100);return vibDepthBuf;}
void vibRateInc(){params.vibratoRateHz=min(params.vibratoRateHz+0.5f,10.f);}
void vibRateDec(){params.vibratoRateHz=max(params.vibratoRateHz-0.5f,1.f);}
char vibRateBuf[10];const char *vibRateLabel(){snprintf(vibRateBuf,sizeof(vibRateBuf),"%.1fHz",params.vibratoRateHz);return vibRateBuf;}

// Forward declarations (v0.9911). vcoItems is defined before the VCO 2
// block below but needs the page-flip and Mix controls from it, and this
// project has been bitten before by use-before-declare after a refactor.
void vcoPageToggleFwd();
const char *vcoPage1LabelFwd();
void osc2LevelIncFwd();
void osc2LevelDecFwd();
const char *osc2LevelLabelFwd();

SettingItem vcoItems[]={
    // Left column (0-4): Osc page, Timbre, Shape, Detune, FineTune
    {"Osc",      vcoPageToggleFwd, vcoPageToggleFwd, vcoPage1LabelFwd},
    {"Timbre",   timbreInc, timbreDec, timbreLabel ,ImuTarget::TIMBRE,LfoTarget::TIMBRE},
    {"Shape",    shapeInc,    shapeDec,    shapeLabel ,ImuTarget::SHAPE,LfoTarget::SHAPE},
    {"Detune",   detInc,    detDec,    detuneLabel ,ImuTarget::DETUNE,LfoTarget::NONE},
    {"FineTune", finInc,    finDec,    fineLabel},
    {"VibDepth", vibDepthInc, vibDepthDec, vibDepthLabel ,ImuTarget::VIBRATO_DEPTH,LfoTarget::NONE},
    // Right column (5-8): Sub, Sub Oct, Noise, Vibrato Rate
    {"Sub Lvl",  subLInc,   subLDec,   subLLabel ,ImuTarget::SUB_LEVEL,LfoTarget::NONE},
    {"Sub Oct",  subOInc,   subODec,   subOLabel},
    {"Noise",    noiseInc,  noiseDec,  noiseLabel ,ImuTarget::NOISE,LfoTarget::NONE},
    {"VibRate",  vibRateInc,  vibRateDec,  vibRateLabel ,ImuTarget::VIBRATO_RATE,LfoTarget::NONE},
    {"Osc2 Mix", osc2LevelIncFwd, osc2LevelDecFwd, osc2LevelLabelFwd},
};
const int NUM_VCO_ITEMS=sizeof(vcoItems)/sizeof(vcoItems[0]);

// ---- VCO 2 (v0.9911) ----
// Its own page inside the same tab rather than a tab of its own: the tab
// bar is already seven entries wide at 34px, and an eighth would not fit.
// ',' / '/' on the "Osc" row flips pages, so the switch lives where you
// are already looking rather than on a separate key.
//
// Mix is on BOTH pages on purpose. It is the one control you reach for
// while balancing the two, and having to change page to hear the balance
// move would be the wrong way round.
void osc2LevelInc(){params.osc2Level=min(params.osc2Level+0.05f,1.f);}
void osc2LevelDec(){params.osc2Level=max(params.osc2Level-0.05f,0.f);}
char osc2LvlBuf[12];const char *osc2LevelLabel(){
    if(params.osc2Level<0.001f)snprintf(osc2LvlBuf,sizeof(osc2LvlBuf),"1 only");
    else if(params.osc2Level>0.999f)snprintf(osc2LvlBuf,sizeof(osc2LvlBuf),"2 only");
    else snprintf(osc2LvlBuf,sizeof(osc2LvlBuf),"%.0f%%",params.osc2Level*100);
    return osc2LvlBuf;}

// Steps through the entire waveform library, not just the Morph chain.
void osc2WaveInc(){params.osc2Waveform=(OscWaveform)(((int)params.osc2Waveform+1)%NUM_OSC_WAVEFORMS);}
void osc2WaveDec(){params.osc2Waveform=(OscWaveform)(((int)params.osc2Waveform+NUM_OSC_WAVEFORMS-1)%NUM_OSC_WAVEFORMS);}
const char *osc2WaveLabel(){return oscWaveformAbbrev(params.osc2Waveform);}

void osc2ShapeInc(){params.osc2Shape=min(params.osc2Shape+0.05f,1.f);}
void osc2ShapeDec(){params.osc2Shape=max(params.osc2Shape-0.05f,0.f);}
char osc2ShapeBuf[10];const char *osc2ShapeLabel(){snprintf(osc2ShapeBuf,sizeof(osc2ShapeBuf),"%.0f%%",params.osc2Shape*100);return osc2ShapeBuf;}

void osc2DetInc(){params.osc2DetuneCents=min(params.osc2DetuneCents+1.f,50.f);}
void osc2DetDec(){params.osc2DetuneCents=max(params.osc2DetuneCents-1.f,-50.f);}
char osc2DetBuf[10];const char *osc2DetLabel(){snprintf(osc2DetBuf,sizeof(osc2DetBuf),"%+.0fc",params.osc2DetuneCents);return osc2DetBuf;}

void osc2FinInc(){params.osc2FineCents=min(params.osc2FineCents+0.1f,10.f);}
void osc2FinDec(){params.osc2FineCents=max(params.osc2FineCents-0.1f,-10.f);}
char osc2FinBuf[10];const char *osc2FinLabel(){snprintf(osc2FinBuf,sizeof(osc2FinBuf),"%+.1fc",params.osc2FineCents);return osc2FinBuf;}

// Named by interval as well as by number, since the point of this control
// is harmony and "+7" is less use than "+7 5th" when reaching for one.
void osc2SemiInc(){params.osc2Semitones=min(params.osc2Semitones+1,12);}
void osc2SemiDec(){params.osc2Semitones=max(params.osc2Semitones-1,-12);}
char osc2SemiBuf[14];const char *osc2SemiLabel(){
    static const char *NAMES[13]={"uni","m2","M2","m3","M3","4th","tri",
                                  "5th","m6","M6","m7","M7","oct"};
    int n=params.osc2Semitones;
    snprintf(osc2SemiBuf,sizeof(osc2SemiBuf),"%+d %s",n,NAMES[abs(n)]);
    return osc2SemiBuf;}

void osc2OctInc(){params.osc2OctaveShift=min(params.osc2OctaveShift+1,2);}
void osc2OctDec(){params.osc2OctaveShift=max(params.osc2OctaveShift-1,-2);}
char osc2OctBuf[10];const char *osc2OctLabel(){snprintf(osc2OctBuf,sizeof(osc2OctBuf),"%+d oct",params.osc2OctaveShift);return osc2OctBuf;}

// Page indicator, editable from either side so the flip is discoverable.
int vcoPage=0;   // 0 = VCO 1, 1 = VCO 2
void vcoPageToggle(){vcoPage^=1;}
const char *vcoPage1Label(){return "1 >2";}
const char *vcoPage2Label(){return "2 <1";}

SettingItem vco2Items[]={
    // Left column (0-3)
    {"Osc",      vcoPageToggle, vcoPageToggle, vcoPage2Label},
    {"Wave",     osc2WaveInc,   osc2WaveDec,   osc2WaveLabel},
    {"Shape",    osc2ShapeInc,  osc2ShapeDec,  osc2ShapeLabel},
    {"Semitone", osc2SemiInc,   osc2SemiDec,   osc2SemiLabel},
    // Right column (4-7)
    {"Detune",   osc2DetInc,    osc2DetDec,    osc2DetLabel},
    {"FineTune", osc2FinInc,    osc2FinDec,    osc2FinLabel},
    {"Octave",   osc2OctInc,    osc2OctDec,    osc2OctLabel},
    {"Mix",      osc2LevelInc,  osc2LevelDec,  osc2LevelLabel},
};
const int NUM_VCO2_ITEMS=sizeof(vco2Items)/sizeof(vco2Items[0]);

void vcoPageToggleFwd(){vcoPageToggle();}
const char *vcoPage1LabelFwd(){return vcoPage1Label();}
void osc2LevelIncFwd(){osc2LevelInc();}
void osc2LevelDecFwd(){osc2LevelDec();}
const char *osc2LevelLabelFwd(){return osc2LevelLabel();}

int selectedVcoIndex=0;
int selectedVco2Index=0;

// ==========================================================
// VCF menu items
// ==========================================================
void ftNext(){uint8_t v=(uint8_t)filterParams.type;filterParams.type=(FilterType)((v+1)%5);updateFilterCoefficients();}
void ftPrev(){uint8_t v=(uint8_t)filterParams.type;filterParams.type=(FilterType)(v==0?4:v-1);updateFilterCoefficients();}
char ftBuf[8];const char *ftLabel(){snprintf(ftBuf,sizeof(ftBuf),"%s",filterTypeName(filterParams.type));return ftBuf;}
// Multiplicative rather than a fixed 100Hz (v0.990). Pitch is perceived
// logarithmically, so a fixed step is an enormous jump down at 100Hz and
// almost nothing up at 8000Hz — and crossing the range took 79 presses.
// The same 1.15 factor the LFO Rate and Ring Mod Rate controls already
// use: every press moves the same musical interval, low-end resolution
// improves from 100Hz steps to 15Hz, and the full sweep is 31 presses.
void fcInc(){filterParams.cutoffHz=min(filterParams.cutoffHz*1.15f,FILTER_CUTOFF_MAX);updateFilterCoefficients();}
void fcDec(){filterParams.cutoffHz=max(filterParams.cutoffHz/1.15f,FILTER_CUTOFF_MIN);updateFilterCoefficients();}
// The cutoff the synth is actually using, not just the knob position
// (v0.99882). An external CC or a tilt writes filterCutoffOffset, and both
// the readout and the response curve were drawn from the base value alone
// — so turning a knob changed the sound with nothing on screen moving,
// which reads as the control not being connected.
//
// Mirrors the audio path's own scaling exactly (see imuScale in
// audioTask): positive offset pulls the cutoff DOWN, by up to 90%.
float effectiveCutoffHz(){
    float off=params.filterCutoffOffset;
    float scale=(off>0.0001f)?(1.0f-off*0.9f):1.0f;
    return constrain(filterParams.cutoffHz*scale,FILTER_CUTOFF_MIN,FILTER_CUTOFF_MAX);
}
char fcBuf[12];const char *fcLabel(){snprintf(fcBuf,sizeof(fcBuf),"%.0fHz",effectiveCutoffHz());return fcBuf;}

// Resonance got the same treatment as cutoff, one version late (v0.99883).
// Fixing only the cutoff in v0.99882 left the other half of the same page
// still showing the knob position while a CC or a tilt moved the sound.
float effectiveResonanceQ(){
    return constrain(filterParams.resonanceQ+params.resonanceOffset,
                     FILTER_Q_MIN,FILTER_Q_MAX);
}
void fqInc(){filterParams.resonanceQ=min(filterParams.resonanceQ+0.1f,FILTER_Q_MAX);updateFilterCoefficients();}
void fqDec(){filterParams.resonanceQ=max(filterParams.resonanceQ-0.1f,FILTER_Q_MIN);updateFilterCoefficients();}
char fqBuf[10];const char *fqLabel(){snprintf(fqBuf,sizeof(fqBuf),"Q%.1f",effectiveResonanceQ());return fqBuf;}
// Key tracking
void fktInc(){filterParams.keyTracking=min(filterParams.keyTracking+0.1f,1.f);}
void fktDec(){filterParams.keyTracking=max(filterParams.keyTracking-0.1f,0.f);}
char fktBuf[10];const char *fktLabel(){snprintf(fktBuf,sizeof(fktBuf),"%.0f%%",filterParams.keyTracking*100);return fktBuf;}
// Filter envelope
void fedInc(){filterEnv.depth=min(filterEnv.depth+100.f,3900.f);}
void fedDec(){filterEnv.depth=max(filterEnv.depth-100.f,-3900.f);}
char fedBuf[12];const char *fedLabel(){snprintf(fedBuf,sizeof(fedBuf),"%+.0fHz",filterEnv.depth);return fedBuf;}
void feaInc(){filterEnv.attackTime=min(filterEnv.attackTime+0.05f,ADSR_MAX_TIME);}
void feaDec(){filterEnv.attackTime=max(filterEnv.attackTime-0.05f,0.f);}
char feaBuf[10];const char *feaLabel(){snprintf(feaBuf,sizeof(feaBuf),"%.2fs",filterEnv.attackTime);return feaBuf;}
void feddInc(){filterEnv.decayTime=min(filterEnv.decayTime+0.05f,ADSR_MAX_TIME);}
void feddDec(){filterEnv.decayTime=max(filterEnv.decayTime-0.05f,0.f);}
char feddBuf[10];const char *feddLabel(){snprintf(feddBuf,sizeof(feddBuf),"%.2fs",filterEnv.decayTime);return feddBuf;}
void ferInc(){filterEnv.releaseTime=min(filterEnv.releaseTime+0.05f,ADSR_MAX_TIME);}
void ferDec(){filterEnv.releaseTime=max(filterEnv.releaseTime-0.05f,0.f);}
char ferBuf[10];const char *ferLabel(){snprintf(ferBuf,sizeof(ferBuf),"%.2fs",filterEnv.releaseTime);return ferBuf;}

SettingItem vcfItems[]={
    // Left column (0-3): Filter, Cutoff, Resonance, KeyTrack
    {"Filter",   ftNext,  ftPrev,  ftLabel},
    {"Cutoff",   fcInc,   fcDec,   fcLabel ,ImuTarget::FILTER_CUTOFF,LfoTarget::FILTER},
    {"Resonance",fqInc,   fqDec,   fqLabel ,ImuTarget::RESONANCE,LfoTarget::NONE},
    {"KeyTrack", fktInc,  fktDec,  fktLabel},
    // Right column (4-7): FEnv Depth, Atk, Dec, Rel
    {"FEnv Dep", fedInc,  fedDec,  fedLabel},
    {"FEnv Atk", feaInc,  feaDec,  feaLabel},
    {"FEnv Dec", feddInc, feddDec, feddLabel},
    {"FEnv Rel", ferInc,  ferDec,  ferLabel},
};
const int NUM_VCF_ITEMS=sizeof(vcfItems)/sizeof(vcfItems[0]);
int selectedVcfIndex=0;

// ==========================================================
// VCA menu items
// ==========================================================
char adsrBuf[12];
void aaInc(){adsr.attackTime=min(adsr.attackTime+0.05f,ADSR_MAX_TIME);}
void aaDec(){adsr.attackTime=max(adsr.attackTime-0.05f,0.f);}
const char *aaLabel(){snprintf(adsrBuf,sizeof(adsrBuf),"%.2fs",adsr.attackTime);return adsrBuf;}
void adInc(){adsr.decayTime=min(adsr.decayTime+0.05f,ADSR_MAX_TIME);}
void adDec(){adsr.decayTime=max(adsr.decayTime-0.05f,0.f);}
const char *adLabel(){snprintf(adsrBuf,sizeof(adsrBuf),"%.2fs",adsr.decayTime);return adsrBuf;}
void asInc(){adsr.sustainLevel=min(adsr.sustainLevel+0.05f,1.f);}
void asDec(){adsr.sustainLevel=max(adsr.sustainLevel-0.05f,0.f);}
const char *asLabel(){snprintf(adsrBuf,sizeof(adsrBuf),"%d%%",(int)(adsr.sustainLevel*100));return adsrBuf;}
void arInc(){adsr.releaseTime=min(adsr.releaseTime+0.05f,ADSR_MAX_TIME);}
void arDec(){adsr.releaseTime=max(adsr.releaseTime-0.05f,0.f);}
const char *arLabel(){snprintf(adsrBuf,sizeof(adsrBuf),"%.2fs",adsr.releaseTime);return adsrBuf;}

// Tremolo (v0.989). Amplitude modulation, so it belongs with the VCA for
// the same reason Vibrato belongs with the VCO.
void tremInc(){params.tremoloDepth=min(params.tremoloDepth+0.05f,1.f);}
void tremDec(){params.tremoloDepth=max(params.tremoloDepth-0.05f,0.f);}
char tremBuf[10];const char *tremLabel(){snprintf(tremBuf,sizeof(tremBuf),"%.0f%%",params.tremoloDepth*100);return tremBuf;}

SettingItem vcaItems[]={
    {"Attack",  aaInc,aaDec,aaLabel},
    {"Decay",   adInc,adDec,adLabel},
    {"Sustain", asInc,asDec,asLabel},
    {"Release", arInc,arDec,arLabel},
    {"Tremolo", tremInc,tremDec,tremLabel ,ImuTarget::TREMOLO,LfoTarget::NONE},
};
const int NUM_VCA_ITEMS=sizeof(vcaItems)/sizeof(vcaItems[0]);
int selectedVcaIndex=0;

// ==========================================================
// LFO menu items
// ==========================================================
void lfoWaveNext(){uint8_t v=(uint8_t)lfo.wave;lfo.wave=(LfoWave)((v+1)%5);}
void lfoWavePrev(){uint8_t v=(uint8_t)lfo.wave;lfo.wave=(LfoWave)(v==0?4:v-1);}
const char *lfoWaveLabel(){return lfoWaveName(lfo.wave);}
void lfoRateInc(){lfo.rateHz=min(lfo.rateHz*1.15f,LFO_RATE_MAX);}
void lfoRateDec(){lfo.rateHz=max(lfo.rateHz/1.15f,LFO_RATE_MIN);}
char lfoRateBuf[10];const char *lfoRateLabel(){snprintf(lfoRateBuf,sizeof(lfoRateBuf),"%.2fHz",lfo.rateHz);return lfoRateBuf;}
void lfoDepthInc(){lfo.depth=min(lfo.depth+0.05f,1.f);}
void lfoDepthDec(){lfo.depth=max(lfo.depth-0.05f,0.f);}
char lfoDepthBuf[8];const char *lfoDepthLabel(){snprintf(lfoDepthBuf,sizeof(lfoDepthBuf),"%.0f%%",lfo.depth*100);return lfoDepthBuf;}
void lfoTargetNext(){uint8_t v=(uint8_t)lfo.target;lfo.target=(LfoTarget)((v+1)%(uint8_t)LfoTarget::TARGET_COUNT);}
void lfoTargetPrev(){uint8_t v=(uint8_t)lfo.target;lfo.target=(LfoTarget)(v==0?(uint8_t)LfoTarget::TARGET_COUNT-1:v-1);}
const char *lfoTargetLabel(){return lfoTargetName(lfo.target);}

SettingItem lfoItems[]={
    {"Wave",   lfoWaveNext,  lfoWavePrev,  lfoWaveLabel},
    {"Rate",   lfoRateInc,   lfoRateDec,   lfoRateLabel ,ImuTarget::LFO_RATE,LfoTarget::NONE},
    {"Depth",  lfoDepthInc,  lfoDepthDec,  lfoDepthLabel ,ImuTarget::LFO_DEPTH,LfoTarget::NONE},
    {"Target", lfoTargetNext,lfoTargetPrev,lfoTargetLabel},
};
const int NUM_LFO_ITEMS=sizeof(lfoItems)/sizeof(lfoItems[0]);
int selectedLfoIndex=0;

// ---- FX (v0.987): Ring Modulator ----
void ringRateInc(){params.ringModRateHz=min(params.ringModRateHz*1.15f,2000.f);}
void ringRateDec(){params.ringModRateHz=max(params.ringModRateHz*0.87f,20.f);}
char ringRateBuf[10];const char *ringRateLabel(){snprintf(ringRateBuf,sizeof(ringRateBuf),"%.0fHz",params.ringModRateHz);return ringRateBuf;}
void ringMixInc(){params.ringModMix=min(params.ringModMix+0.05f,1.f);}
void ringMixDec(){params.ringModMix=max(params.ringModMix-0.05f,0.f);}
char ringMixBuf[10];const char *ringMixLabel(){snprintf(ringMixBuf,sizeof(ringMixBuf),"%.0f%%",params.ringModMix*100);return ringMixBuf;}

SettingItem fxItems[]={
    {"Ring Rate", ringRateInc, ringRateDec, ringRateLabel ,ImuTarget::FX_RING_RATE,LfoTarget::FX_RING_RATE},
    {"Ring Mix",  ringMixInc,  ringMixDec,  ringMixLabel ,ImuTarget::FX_RING_MIX,LfoTarget::FX_RING_MIX},
};
const int NUM_FX_ITEMS=sizeof(fxItems)/sizeof(fxItems[0]);
int selectedFxIndex=0;

// ---- FX (v0.9872): Soft Limiter ----
void limiterDriveInc(){params.limiterDrive=min(params.limiterDrive+0.25f,5.f);}
void limiterDriveDec(){params.limiterDrive=max(params.limiterDrive-0.25f,1.f);}
char limiterDriveBuf[10];const char *limiterDriveLabel(){snprintf(limiterDriveBuf,sizeof(limiterDriveBuf),"%.2fx",params.limiterDrive);return limiterDriveBuf;}
void limiterMixInc(){params.limiterMix=min(params.limiterMix+0.05f,1.f);}
void limiterMixDec(){params.limiterMix=max(params.limiterMix-0.05f,0.f);}
char limiterMixBuf[10];const char *limiterMixLabel(){snprintf(limiterMixBuf,sizeof(limiterMixBuf),"%.0f%%",params.limiterMix*100);return limiterMixBuf;}

SettingItem limiterItems[]={
    {"Drive", limiterDriveInc, limiterDriveDec, limiterDriveLabel ,ImuTarget::FX_LIMIT_DRIVE,LfoTarget::FX_LIMIT_DRIVE},
    {"Mix",   limiterMixInc,   limiterMixDec,   limiterMixLabel},
};
const int NUM_LIMITER_ITEMS=sizeof(limiterItems)/sizeof(limiterItems[0]);

// ---- FX (v0.9873): Chorus ----
void chorusRateInc(){params.chorusRateHz=min(params.chorusRateHz+0.1f,5.f);}
void chorusRateDec(){params.chorusRateHz=max(params.chorusRateHz-0.1f,0.1f);}
char chorusRateBuf[10];const char *chorusRateLabel(){snprintf(chorusRateBuf,sizeof(chorusRateBuf),"%.1fHz",params.chorusRateHz);return chorusRateBuf;}
void chorusDepthInc(){params.chorusDepthMs=min(params.chorusDepthMs+1.f,20.f);}
void chorusDepthDec(){params.chorusDepthMs=max(params.chorusDepthMs-1.f,0.f);}
char chorusDepthBuf[10];const char *chorusDepthLabel(){snprintf(chorusDepthBuf,sizeof(chorusDepthBuf),"%.0fms",params.chorusDepthMs);return chorusDepthBuf;}
void chorusMixInc(){params.chorusMix=min(params.chorusMix+0.05f,1.f);}
void chorusMixDec(){params.chorusMix=max(params.chorusMix-0.05f,0.f);}
char chorusMixBuf[10];const char *chorusMixLabel(){snprintf(chorusMixBuf,sizeof(chorusMixBuf),"%.0f%%",params.chorusMix*100);return chorusMixBuf;}

SettingItem chorusItems[]={
    {"Rate",  chorusRateInc,  chorusRateDec,  chorusRateLabel},
    {"Depth", chorusDepthInc, chorusDepthDec, chorusDepthLabel ,ImuTarget::FX_CHORUS_DEPTH,LfoTarget::FX_CHORUS_DEPTH},
    {"Mix",   chorusMixInc,   chorusMixDec,   chorusMixLabel ,ImuTarget::FX_CHORUS_MIX,LfoTarget::FX_CHORUS_MIX},
};
const int NUM_CHORUS_ITEMS=sizeof(chorusItems)/sizeof(chorusItems[0]);

// ---- FX (v0.9874): Delay/Echo ----
void delayTimeInc(){params.delayTimeMs=min(params.delayTimeMs+25.f,DELAY_MAX_MS);}
void delayTimeDec(){params.delayTimeMs=max(params.delayTimeMs-25.f,50.f);}
char delayTimeBuf[10];const char *delayTimeLabel(){snprintf(delayTimeBuf,sizeof(delayTimeBuf),"%.0fms",params.delayTimeMs);return delayTimeBuf;}
void delayFeedbackInc(){params.delayFeedback=min(params.delayFeedback+0.05f,0.9f);}
void delayFeedbackDec(){params.delayFeedback=max(params.delayFeedback-0.05f,0.f);}
char delayFeedbackBuf[10];const char *delayFeedbackLabel(){snprintf(delayFeedbackBuf,sizeof(delayFeedbackBuf),"%.0f%%",params.delayFeedback*100);return delayFeedbackBuf;}
void delayMixInc(){params.delayMix=min(params.delayMix+0.05f,1.f);}
void delayMixDec(){params.delayMix=max(params.delayMix-0.05f,0.f);}
char delayMixBuf[10];const char *delayMixLabel(){snprintf(delayMixBuf,sizeof(delayMixBuf),"%.0f%%",params.delayMix*100);return delayMixBuf;}

SettingItem delayItems[]={
    {"Time",     delayTimeInc,     delayTimeDec,     delayTimeLabel},
    {"Feedback", delayFeedbackInc, delayFeedbackDec, delayFeedbackLabel ,ImuTarget::FX_DELAY_FB,LfoTarget::FX_DELAY_FB},
    {"Mix",      delayMixInc,      delayMixDec,      delayMixLabel ,ImuTarget::FX_DELAY_MIX,LfoTarget::FX_DELAY_MIX},
};
const int NUM_DELAY_ITEMS=sizeof(delayItems)/sizeof(delayItems[0]);

// ---- FX (v0.9879): Reverb ----
// Room Size and Damping are stored 0-1 and mapped into their useful
// ranges by updateFxEffective(), so what these edit is the friendly 0-100%
// value the user sees, not the raw coefficients.
void reverbRoomInc(){params.reverbRoomSize=min(params.reverbRoomSize+0.05f,1.f);}
void reverbRoomDec(){params.reverbRoomSize=max(params.reverbRoomSize-0.05f,0.f);}
char reverbRoomBuf[10];const char *reverbRoomLabel(){snprintf(reverbRoomBuf,sizeof(reverbRoomBuf),"%.0f%%",params.reverbRoomSize*100);return reverbRoomBuf;}
void reverbDampInc(){params.reverbDamping=min(params.reverbDamping+0.05f,1.f);}
void reverbDampDec(){params.reverbDamping=max(params.reverbDamping-0.05f,0.f);}
char reverbDampBuf[10];const char *reverbDampLabel(){snprintf(reverbDampBuf,sizeof(reverbDampBuf),"%.0f%%",params.reverbDamping*100);return reverbDampBuf;}
void reverbMixInc(){params.reverbMix=min(params.reverbMix+0.05f,1.f);}
void reverbMixDec(){params.reverbMix=max(params.reverbMix-0.05f,0.f);}
char reverbMixBuf[10];const char *reverbMixLabel(){snprintf(reverbMixBuf,sizeof(reverbMixBuf),"%.0f%%",params.reverbMix*100);return reverbMixBuf;}

SettingItem reverbItems[]={
    {"Room",    reverbRoomInc, reverbRoomDec, reverbRoomLabel ,ImuTarget::FX_REVERB_ROOM,LfoTarget::FX_REVERB_ROOM},
    {"Damping", reverbDampInc, reverbDampDec, reverbDampLabel},
    {"Mix",     reverbMixInc,  reverbMixDec,  reverbMixLabel ,ImuTarget::FX_REVERB_MIX,LfoTarget::FX_REVERB_MIX},
};
const int NUM_REVERB_ITEMS=sizeof(reverbItems)/sizeof(reverbItems[0]);

// ---- FX (v0.989): Bit-crusher ----
// Existed since the earliest IMU work but had no menu of its own, so it
// could only ever be reached by tilting the device. It is a signal
// degradation effect, so the FX tab is where it belongs.
void bitcrushInc(){params.bitcrush=min(params.bitcrush+0.05f,1.f);}
void bitcrushDec(){params.bitcrush=max(params.bitcrush-0.05f,0.f);}
char bitcrushBuf[10];const char *bitcrushLabel(){snprintf(bitcrushBuf,sizeof(bitcrushBuf),"%.0f%%",params.bitcrush*100);return bitcrushBuf;}

SettingItem bitcrushItems[]={
    {"Amount", bitcrushInc, bitcrushDec, bitcrushLabel ,ImuTarget::BITCRUSH,LfoTarget::NONE},
};
const int NUM_BITCRUSH_ITEMS=sizeof(bitcrushItems)/sizeof(bitcrushItems[0]);

// ---- FX pad selector (v0.9871) ----
// The FX tab now has two levels: a row of pads (one per effect, colored
// by on/off) to pick and toggle effects at a glance, and each pad's own
// parameter screen (reusing the same SettingItem-list pattern as VCO/
// VCF/VCA/LFO) for detailed adjustment. Extensible by design — adding a
// future effect (soft limiter, Chorus, Delay) just means one more
// FxEffect enum value plus one more switch case in each of the 3
// functions below; nothing about the pad UI itself needs to change.
enum class FxEffect : uint8_t { RING_MOD, SOFT_LIMIT, CHORUS, DELAY, REVERB, BITCRUSH };
constexpr int NUM_FX_EFFECTS = 6; // grows as effects are added
// Pad names stay short on purpose: the selector lays these out at 44px
// with a 4px gap, so five pads come to 4+5*44+4*4 = 240px — exactly the
// screen width. A sixth effect will need that layout revisited.
const char *FX_EFFECT_NAMES[NUM_FX_EFFECTS] = {"RingMod","Limiter","Chorus","Delay","Reverb","Bitcrush"};

bool fxIsOn(int effectIdx){
    switch((FxEffect)effectIdx){
        case FxEffect::RING_MOD:   return params.ringModMix>0.001f;
        case FxEffect::SOFT_LIMIT: return params.limiterMix>0.001f;
        case FxEffect::CHORUS:     return params.chorusMix>0.001f;
        case FxEffect::DELAY:      return params.delayMix>0.001f;
        case FxEffect::REVERB:     return params.reverbMix>0.001f;
        case FxEffect::BITCRUSH:   return params.bitcrush>0.001f;
        default: return false;
    }
}
// Toggling off always zeroes the effect's own mix/amount (unambiguous
// "off"); toggling on restores a reasonable default rather than
// whatever fractional value it might have been left at.
// Per-effect memory of the last level the user had set (v0.9893).
// Switching an effect off has to zero its Mix — that IS "off" throughout
// this file, and fxIsOn() reads the same value to draw the pad — but until
// now switching back on restored a fixed default instead of what had been
// there. Setting Delay to 50%, toggling off and on, and getting 40% back
// is just the pad quietly discarding your setting. These seed with the
// same defaults as before, so a never-touched effect behaves identically.
float fxLastLevel[NUM_FX_EFFECTS]={0.5f,0.7f,0.5f,0.4f,0.3f,0.4f};

// Bit-crusher's control is Amount rather than Mix, but it uses the same
// zero-means-off convention, so it belongs in the same mechanism.
float *fxLevelPtr(int effectIdx){
    switch((FxEffect)effectIdx){
        case FxEffect::RING_MOD:   return &params.ringModMix;
        case FxEffect::SOFT_LIMIT: return &params.limiterMix;
        case FxEffect::CHORUS:     return &params.chorusMix;
        case FxEffect::DELAY:      return &params.delayMix;
        case FxEffect::REVERB:     return &params.reverbMix;
        case FxEffect::BITCRUSH:   return &params.bitcrush;
        default: return nullptr;
    }
}

void fxToggle(int effectIdx){
    if(effectIdx<0||effectIdx>=NUM_FX_EFFECTS)return;
    float *lvl=fxLevelPtr(effectIdx);
    if(!lvl)return;
    if(*lvl>0.001f){
        fxLastLevel[effectIdx]=*lvl;   // remember before clearing
        *lvl=0.f;
        // Reverb keeps a long tail, so it also has to be emptied or the
        // old room would still be ringing when it is switched back on.
        if((FxEffect)effectIdx==FxEffect::REVERB)clearReverbState();
    } else {
        *lvl=(fxLastLevel[effectIdx]>0.001f)?fxLastLevel[effectIdx]:0.4f;
    }
}
SettingItem *fxGetParamItems(int effectIdx,int &count){
    switch((FxEffect)effectIdx){
        case FxEffect::RING_MOD:   count=NUM_FX_ITEMS;      return fxItems;
        case FxEffect::SOFT_LIMIT: count=NUM_LIMITER_ITEMS; return limiterItems;
        case FxEffect::DELAY:      count=NUM_DELAY_ITEMS;   return delayItems;
        case FxEffect::CHORUS:     count=NUM_CHORUS_ITEMS;  return chorusItems;
        case FxEffect::REVERB:     count=NUM_REVERB_ITEMS;  return reverbItems;
        case FxEffect::BITCRUSH:   count=NUM_BITCRUSH_ITEMS;return bitcrushItems;
        default: count=0; return nullptr;
    }
}

enum class FxViewMode : uint8_t { PAD_SELECTOR, PARAM_EDIT };
FxViewMode fxViewMode=FxViewMode::PAD_SELECTOR;
int  fxPadCursor=0;
bool prevFxPadRowKeyPressed=false;   // v0.990: ';' row movement
// Inline editing for single-parameter effects (v0.992). Opening a whole
// screen to change one number was more ceremony than the number deserved —
// the Bit-crusher's Amount is the only control it has. '.' on such a pad
// edits in place instead of drilling in; ',' and '/' then change the value
// rather than moving the pad cursor, and '.' again, Enter or Tab leaves.
// Effects with more than one parameter still open their screen as before.
bool fxInlineEdit=false;
bool prevFxInlinePressed=false;
int  fxEditingEffect=0;
bool prevFxToggleKeyPressed=false, prevFxDrillKeyPressed=false;
bool prevFxPadLeftPressed=false, prevFxPadRightPressed=false;

// ==========================================================
// Menu navigation
// ==========================================================
// Held-key auto-repeat (v0.990). Every menu action in this file was
// edge-triggered — one press, one step — so changing a value meaningfully
// meant pressing a key dozens of times. This is why editing felt slow far
// more than any individual step size did.
//
// The delay matters as much as the rate: a tap has to stay a single step,
// or precise adjustment becomes impossible. 400ms is comfortably longer
// than a deliberate press and short enough not to feel stuck. At 70ms a
// held key gives ~14 steps/second, which crosses a 0-100% control in
// about 1.4s and the whole filter range in about 2.2s.
//
// Deliberately NOT applied to Enter (toggling an FX pad) or to the FX
// drill-in key: repeating those would just flicker an effect on and off,
// or re-enter a screen already entered.
constexpr unsigned long MENU_REPEAT_DELAY_MS = 400;
constexpr unsigned long MENU_REPEAT_RATE_MS  = 70;
// menuUpHeldMs/menuUpLastMs/menuDownHeldMs/menuDownLastMs moved earlier
// too, alongside menuIncHeldMs (v0.9996x, second fix) — SEQ's and SONG's
// Tempo/Swing were repaired to use these instead of menuIncHeldMs/
// menuDecHeldMs (the actually-correct pairing for ';'/'.' ), but that
// repair reused a pair that was, itself, still only locally visible from
// here — the same forward-visibility problem as before, just on the
// other pair this time.
// menuIncHeldMs/menuIncLastMs/menuDecHeldMs/menuDecLastMs moved earlier
// in the file (v0.9996x fix) — updateSeqEditing() and updateSongEditor()
// are both defined before this point and now use them too, and a plain
// global declaration has no forward visibility the way menuKeyFire()'s
// own prototype does; they have to appear before every function that
// touches them, not just this one. See the declaration next to
// morphIncHeldMs for where they live now.
unsigned long fxPadLeftHeldMs=0,fxPadLeftLastMs=0;
unsigned long fxPadRightHeldMs=0,fxPadRightLastMs=0;
unsigned long fxPadRowHeldMs=0;   // v0.9901: cleared alongside menuUpHeldMs

// prev is read only — the existing trailing assignments still own it, so
// this drops into the established if(key&&!prevKey) pattern unchanged.
bool menuKeyFire(bool now,bool prev,unsigned long &heldMs,unsigned long &lastMs){
    unsigned long t=millis();
    if(!now)return false;
    if(!prev){heldMs=t;lastMs=t;return true;}        // the press itself
    // heldMs==0 means this hold was never started here — it belongs to
    // whatever screen the key was actually pressed on (v0.9901). Without
    // this, pressing '.' on the FX pad selector to drill in would carry a
    // long-stale hold timestamp into the parameter screen, and the very
    // next frame the still-held key would look like it had been down for
    // minutes and start repeating immediately. Same for any key held
    // across a screen change.
    if(heldMs==0)return false;
    if(t-heldMs<MENU_REPEAT_DELAY_MS)return false;   // still inside the hold delay
    if(t-lastMs<MENU_REPEAT_RATE_MS)return false;    // rate limit
    lastMs=t;return true;
}

void updateMenuNavigation(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool tab=s.tab,mU=false,mD=false,mI=false,mDe=false;
    for(char c:s.word){
        if(c==';')mU=true;if(c=='.')mD=true;
        if(c=='/')mI=true;if(c==',')mDe=true;
    }
    // Clear the hold timestamp of any key that is not currently down.
    // This runs before the per-screen dispatch below on purpose: a key can
    // be released while a different screen is active, and if that screen's
    // branch never runs, the timer would otherwise stay set and the next
    // press would repeat instantly (v0.9901).
    if(!mU){menuUpHeldMs=0;fxPadRowHeldMs=0;}
    if(!mD)menuDownHeldMs=0;
    if(!mI){menuIncHeldMs=0;fxPadRightHeldMs=0;}
    if(!mDe){menuDecHeldMs=0;fxPadLeftHeldMs=0;}

    if(tab&&!prevTabPressed){
        // The [tab] diagnostic that used to sit here (v0.99956) is
        // retired (v0.9997x, UI/UX diagnostic pass) — it did its job:
        // found that the crash it was chasing traced to delay() calls
        // shared by every non-PLAY screen, fixed in v0.99957, confirmed
        // gone since. No ongoing reason for every Tab press to print.
        if(appMode==AppMode::CATEGORY){
            if(!imuPickerOpen&&!imuCalibrateConfirmOpen&&!resetConfirmOpen&&!scalePickerOpen&&!themePickerOpen&&!morphSlotScreenOpen)appMode=AppMode::SETTINGS;
            // if the picker/confirm overlay IS open, its own Tab-cancel is
            // handled by updateImuPicker()/updateImuCalibrateConfirm()/updateResetConfirm()
        } else if(appMode==AppMode::FX&&fxViewMode==FxViewMode::PARAM_EDIT){
            // Return to the pad selector instead of proceeding to the next
            // tab — FX is still part of the normal Tab-cycle otherwise
            // (see the PAD_SELECTOR case, which falls through below).
            fxViewMode=FxViewMode::PAD_SELECTOR;
        fxInlineEdit=false;   // v0.992: never resume editing on re-entry
        } else if(appMode!=AppMode::PATCH&&appMode!=AppMode::PATTERN&&appMode!=AppMode::SONG&&appMode!=AppMode::TIMBRE){
            AppMode prev=appMode;
            if(s.shift){
                // Shift+Tab: same cycle, reverse direction.
                switch(appMode){
                    case AppMode::PLAY:     appMode=AppMode::SETTINGS; currentFreq=0;break;
                    case AppMode::SEQ:      appMode=AppMode::SETTINGS; currentFreq=0;break;
                    case AppMode::VCO:      appMode=lastMainMode;      break;
                    case AppMode::VCF:      appMode=AppMode::VCO;      break;
                    case AppMode::VCA:      appMode=AppMode::VCF;      break;
                    case AppMode::LFO:      appMode=AppMode::VCA;      break;
                    case AppMode::FX:       appMode=AppMode::LFO;      break;
                    case AppMode::SETTINGS: appMode=AppMode::FX;       break;
                    default: break;
                }
            } else {
                switch(appMode){
                    case AppMode::PLAY:     appMode=AppMode::VCO;      currentFreq=0;break;
                    case AppMode::SEQ:      appMode=AppMode::VCO;      currentFreq=0;break;
                    case AppMode::VCO:      appMode=AppMode::VCF;      break;
                    case AppMode::VCF:      appMode=AppMode::VCA;      break;
                    case AppMode::VCA:      appMode=AppMode::LFO;      break;
                    case AppMode::LFO:      appMode=AppMode::FX;       break;
                    case AppMode::FX:       appMode=AppMode::SETTINGS; break;
                    case AppMode::SETTINGS: appMode=lastMainMode;      break;
                    default: break;
                }
            }
            if(prev==AppMode::SETTINGS||prev==AppMode::SEQ)saveSettings();
        }
        // AppMode::PATCH's and AppMode::PATTERN's Tab are each handled
        // entirely by their own updatePatchBrowser()/updatePatternBank()
    }
    prevTabPressed=tab;

    if(appMode==AppMode::SETTINGS){
        if(menuKeyFire(mU,prevMenuUpPressed,menuUpHeldMs,menuUpLastMs))  selectedSettingIndex=(selectedSettingIndex-1+NUM_SETTING_ITEMS)%NUM_SETTING_ITEMS;
        if(menuKeyFire(mD,prevMenuDownPressed,menuDownHeldMs,menuDownLastMs))selectedSettingIndex=(selectedSettingIndex+1)%NUM_SETTING_ITEMS;
        // Edge-triggered, not repeating: these open a category screen, so
        // repeating has nothing to add. List movement above does repeat.
        if(mI&&!prevMenuIncPressed) settingItems[selectedSettingIndex].onIncrement();
        if(mDe&&!prevMenuDecPressed)settingItems[selectedSettingIndex].onDecrement();
    }
    else if(appMode==AppMode::CATEGORY&&!imuPickerOpen&&!imuCalibrateConfirmOpen&&!resetConfirmOpen&&!scalePickerOpen&&!themePickerOpen&&!morphSlotScreenOpen){
        int count;const char *title;SettingItem *items=getCategoryItems(count,title);
        if(menuKeyFire(mU,prevMenuUpPressed,menuUpHeldMs,menuUpLastMs))  selectedCategoryIndex=(selectedCategoryIndex-1+count)%count;
        if(menuKeyFire(mD,prevMenuDownPressed,menuDownHeldMs,menuDownLastMs))selectedCategoryIndex=(selectedCategoryIndex+1)%count;
        // Edge-triggered by default, still. Several items here are binary
        // toggles bound to BOTH inc and dec (X Invert, X Curve, ...), and
        // a repeating toggle flips ~14 times a second — you could not land
        // on the state you wanted, it would just be wherever you released.
        // Others open pickers or confirm dialogs. Nothing about those
        // benefits, and the cost of getting a toggle wrong is real, so
        // they stay one-press-one-step.
        // Genuine two-direction values (v0.9996x) — where onIncrement and
        // onDecrement are actually different functions, which a toggle's
        // shared single function never is — get the same hold-to-repeat
        // every VCF/VCA/LFO/FX value already has instead. ARP's Tempo and
        // Swing are the two rows this actually applies to today, requested
        // because their steps were dropped 5->1 for finer control, and a
        // tap-only 1-unit step would make a large change tedious; holding
        // now covers that instead. Reuses the same menuIncHeldMs/
        // menuIncLastMs globals VCF/VCA/LFO/FX already share — only one
        // screen is ever active, so nothing here conflicts with them.
        SettingItem &curItem=items[selectedCategoryIndex];
        bool itemRepeatable=(curItem.onIncrement!=curItem.onDecrement);
        if(itemRepeatable){
            if(menuKeyFire(mI,prevMenuIncPressed,menuIncHeldMs,menuIncLastMs))curItem.onIncrement();
            if(menuKeyFire(mDe,prevMenuDecPressed,menuDecHeldMs,menuDecLastMs))curItem.onDecrement();
        } else {
            if(mI&&!prevMenuIncPressed) curItem.onIncrement();
            if(mDe&&!prevMenuDecPressed)curItem.onDecrement();
        }
    }
    else if(appMode==AppMode::VCO){
        // Both VCO pages run through one code path (v0.9911) — picking the
        // table and cursor up front rather than duplicating the branch.
        SettingItem *vi=(vcoPage==0)?vcoItems:vco2Items;
        int vn=(vcoPage==0)?NUM_VCO_ITEMS:NUM_VCO2_ITEMS;
        int &vsel=(vcoPage==0)?selectedVcoIndex:selectedVco2Index;
        if(menuKeyFire(mU,prevMenuUpPressed,menuUpHeldMs,menuUpLastMs))  vsel=(vsel-1+vn)%vn;
        if(menuKeyFire(mD,prevMenuDownPressed,menuDownHeldMs,menuDownLastMs))vsel=(vsel+1)%vn;
        // The page flip is bound to inc AND dec on the "Osc" row, so it must
        // not auto-repeat — a repeating toggle would flip pages ~14 times a
        // second and land wherever you released, the same reason the
        // CATEGORY screen's toggles are edge-triggered.
        bool pageRow=(vi[vsel].onIncrement==vi[vsel].onDecrement);
        if(pageRow){
            if(mI&&!prevMenuIncPressed) vi[vsel].onIncrement();
            if(mDe&&!prevMenuDecPressed)vi[vsel].onDecrement();
        } else {
            if(menuKeyFire(mI,prevMenuIncPressed,menuIncHeldMs,menuIncLastMs)) vi[vsel].onIncrement();
            if(menuKeyFire(mDe,prevMenuDecPressed,menuDecHeldMs,menuDecLastMs))vi[vsel].onDecrement();
        }
    }
    else if(appMode==AppMode::VCF){
        if(menuKeyFire(mU,prevMenuUpPressed,menuUpHeldMs,menuUpLastMs))  selectedVcfIndex=(selectedVcfIndex-1+NUM_VCF_ITEMS)%NUM_VCF_ITEMS;
        if(menuKeyFire(mD,prevMenuDownPressed,menuDownHeldMs,menuDownLastMs))selectedVcfIndex=(selectedVcfIndex+1)%NUM_VCF_ITEMS;
        if(menuKeyFire(mI,prevMenuIncPressed,menuIncHeldMs,menuIncLastMs)) vcfItems[selectedVcfIndex].onIncrement();
        if(menuKeyFire(mDe,prevMenuDecPressed,menuDecHeldMs,menuDecLastMs))vcfItems[selectedVcfIndex].onDecrement();
    }
    else if(appMode==AppMode::VCA){
        if(menuKeyFire(mU,prevMenuUpPressed,menuUpHeldMs,menuUpLastMs))  selectedVcaIndex=(selectedVcaIndex-1+NUM_VCA_ITEMS)%NUM_VCA_ITEMS;
        if(menuKeyFire(mD,prevMenuDownPressed,menuDownHeldMs,menuDownLastMs))selectedVcaIndex=(selectedVcaIndex+1)%NUM_VCA_ITEMS;
        if(menuKeyFire(mI,prevMenuIncPressed,menuIncHeldMs,menuIncLastMs)) vcaItems[selectedVcaIndex].onIncrement();
        if(menuKeyFire(mDe,prevMenuDecPressed,menuDecHeldMs,menuDecLastMs))vcaItems[selectedVcaIndex].onDecrement();
    }
    else if(appMode==AppMode::LFO){
        if(menuKeyFire(mU,prevMenuUpPressed,menuUpHeldMs,menuUpLastMs))  selectedLfoIndex=(selectedLfoIndex-1+NUM_LFO_ITEMS)%NUM_LFO_ITEMS;
        if(menuKeyFire(mD,prevMenuDownPressed,menuDownHeldMs,menuDownLastMs))selectedLfoIndex=(selectedLfoIndex+1)%NUM_LFO_ITEMS;
        if(menuKeyFire(mI,prevMenuIncPressed,menuIncHeldMs,menuIncLastMs)) lfoItems[selectedLfoIndex].onIncrement();
        if(menuKeyFire(mDe,prevMenuDecPressed,menuDecHeldMs,menuDecLastMs))lfoItems[selectedLfoIndex].onDecrement();
    }
    else if(appMode==AppMode::FX&&fxViewMode==FxViewMode::PAD_SELECTOR){
        int fxCnt=0; SettingItem *fxIt=fxGetParamItems(fxPadCursor,fxCnt);
        bool singleParam=(fxCnt==1);
        if(!singleParam)fxInlineEdit=false;   // cursor is on a multi-param pad
        if(fxInlineEdit&&fxIt){
            // ,// adjust the value while inline editing rather than moving
            // the pad cursor. Auto-repeat applies, as it does in any other
            // value field.
            if(menuKeyFire(mI,prevFxPadRightPressed,fxPadRightHeldMs,fxPadRightLastMs)) fxIt[0].onIncrement();
            if(menuKeyFire(mDe,prevFxPadLeftPressed,fxPadLeftHeldMs,fxPadLeftLastMs))   fxIt[0].onDecrement();
        } else {
            if(menuKeyFire(mDe,prevFxPadLeftPressed,fxPadLeftHeldMs,fxPadLeftLastMs)) fxPadCursor=(fxPadCursor-1+NUM_FX_EFFECTS)%NUM_FX_EFFECTS;
            if(menuKeyFire(mI,prevFxPadRightPressed,fxPadRightHeldMs,fxPadRightLastMs)) fxPadCursor=(fxPadCursor+1)%NUM_FX_EFFECTS;
        }
        prevFxPadLeftPressed=mDe;prevFxPadRightPressed=mI;
        // Row movement (v0.990). The pads became a grid in v0.989 but ,//
        // kept walking all six in a line, so crossing from the top row to
        // the one below took three presses when the eye says it is one
        // step down. ; moves a whole row, wrapping. It is only additive —
        // ,// still walks the full sequence, and . still drills in — so
        // nothing that already worked changed.
        //
        // Written as "up one row with wraparound" rather than "swap rows"
        // so it still behaves sensibly if a 7th effect ever makes it three
        // rows; with two rows the two are the same thing.
        // Leaving inline edit also happens on a row move, so ';' is never
        // swallowed.
        if(mU&&!prevFxPadRowKeyPressed)fxInlineEdit=false;
        if(mU&&!prevFxPadRowKeyPressed&&!fxInlineEdit){
            constexpr int COLS=3;
            int rows=(NUM_FX_EFFECTS+COLS-1)/COLS;
            int col=fxPadCursor%COLS,row=fxPadCursor/COLS;
            for(int k=0;k<rows;k++){                 // skip past any empty cell
                row=(row-1+rows)%rows;
                int cand=row*COLS+col;
                if(cand<NUM_FX_EFFECTS){fxPadCursor=cand;break;}
            }
        }
        prevFxPadRowKeyPressed=mU;
        // Enter leaves inline editing rather than toggling the effect off
        // under you — you were adjusting it, so that is the likelier
        // intent. It still toggles normally when not editing.
        if(s.enter&&!prevFxToggleKeyPressed){
            if(fxInlineEdit)fxInlineEdit=false;
            else            fxToggle(fxPadCursor);
        }
        prevFxToggleKeyPressed=s.enter;
        if(mD&&!prevFxDrillKeyPressed){
            if(singleParam){
                fxInlineEdit=!fxInlineEdit;   // v0.992: no screen for one number
            } else {
                fxEditingEffect=fxPadCursor;
                selectedFxIndex=0;
                fxViewMode=FxViewMode::PARAM_EDIT;
            }
        }
        prevFxDrillKeyPressed=mD;
    }
    else if(appMode==AppMode::FX&&fxViewMode==FxViewMode::PARAM_EDIT){
        int count;SettingItem *items=fxGetParamItems(fxEditingEffect,count);
        if(items&&count>0){
            if(menuKeyFire(mU,prevMenuUpPressed,menuUpHeldMs,menuUpLastMs))  selectedFxIndex=(selectedFxIndex-1+count)%count;
            if(menuKeyFire(mD,prevMenuDownPressed,menuDownHeldMs,menuDownLastMs))selectedFxIndex=(selectedFxIndex+1)%count;
            if(menuKeyFire(mI,prevMenuIncPressed,menuIncHeldMs,menuIncLastMs)) items[selectedFxIndex].onIncrement();
            if(menuKeyFire(mDe,prevMenuDecPressed,menuDecHeldMs,menuDecLastMs))items[selectedFxIndex].onDecrement();
        }
    }
    prevMenuUpPressed=mU;prevMenuDownPressed=mD;
    prevMenuIncPressed=mI;prevMenuDecPressed=mDe;
}

// ==========================================================
// Display helpers
// ==========================================================
void drawTabBar(LovyanGFX &gfx,AppMode cur){
    gfx.fillRect(0,0,240,11,BLACK);
    struct{const char *l;AppMode m;int x;}tabs[]={
        {"PLAY",AppMode::PLAY,0},   {"VCO",AppMode::VCO,34},
        {"VCF",AppMode::VCF,68},    {"VCA",AppMode::VCA,102},
        {"LFO",AppMode::LFO,136},   {"FX",AppMode::FX,170},
        {"SET",AppMode::SETTINGS,204}
    };
    constexpr int TW=34;
    for(auto &t:tabs){
        bool act=(cur==t.m)||(t.m==AppMode::PLAY&&cur==AppMode::SEQ);
        gfx.drawRect(t.x,0,TW,11,uiColor);
        if(act){gfx.fillRect(t.x+1,1,TW-2,9,WHITE);gfx.setTextColor(BLACK,WHITE);}
        else    gfx.setTextColor(uiColor,BLACK);
        int lx=t.x+(TW-(int)strlen(t.l)*6)/2;
        gfx.setCursor(lx,2);gfx.print(t.l);
    }
    gfx.setTextColor(uiColor,BLACK);
}

void drawAdsrGraph(){
    constexpr int GX=0,GY=12,GW=240,GH=60;
    canvas.fillRect(GX,GY,GW,GH,BLACK);
    constexpr float FIXED=ADSR_MAX_TIME,SF=0.15f;
    float sx=(float)GW/(FIXED+FIXED*SF);
    int top=GY+4,bot=GY+GH-4,susY=bot-(int)((bot-top)*adsr.sustainLevel);
    int x0=GX;
    int x1=x0+max(1,(int)(adsr.attackTime*sx));
    int x2=x1+max(1,(int)(adsr.decayTime*sx));
    int x3=x2+(int)(FIXED*SF*sx);
    int x4=x3+max(1,(int)(adsr.releaseTime*sx));
    x1=min(x1,GX+GW-3);x2=min(x2,GX+GW-2);x3=min(x3,GX+GW-1);x4=min(x4,GX+GW);
    canvas.drawLine(x0,bot,x1,top,uiColor);
    canvas.drawLine(x1,top,x2,susY,uiColor);
    canvas.drawLine(x2,susY,x3,susY,uiColor);
    canvas.drawLine(x3,susY,x4,bot,uiColor);
    uint16_t yel=canvas.color565(255,255,0);
    for(auto &p:{std::pair<int,int>{x1,top},{x2,susY},{x3,susY}})
        canvas.fillRect(p.first-1,p.second-1,3,3,yel);
    canvas.setCursor(x0+2,GY+GH-10);canvas.print("A");
    canvas.setCursor(x1+2,GY+GH-10);canvas.print("D");
    canvas.setCursor((x2+x3)/2-3,GY+GH-10);canvas.print("S");
    canvas.setCursor(x3+2,GY+GH-10);canvas.print("R");
    uint16_t dim=canvas.color565(0,64,0);
    int m1=GX+(int)(1.f*sx),m25=GX+(int)(2.5f*sx);
    canvas.drawFastVLine(m1,GY,GH,dim);
    canvas.drawFastVLine(m25,GY,GH,dim);
    canvas.setCursor(m1+1,GY+1);canvas.print("1s");
    canvas.setCursor(m25+1,GY+1);canvas.print("2.5s");
    canvas.drawFastHLine(GX,GY+GH,GW,uiColor);
}

void drawWaveform(LovyanGFX &gfx,float morph,float shape){
    constexpr int GX=0,GY=12,GW=240,GH=43,CY=GY+GH/2,CYCLES=3;
    gfx.fillRect(GX,GY,GW,GH,BLACK);
    gfx.drawFastHLine(GX,CY,GW,gfx.color565(0,64,0));
    int pY=CY;
    for(int px=0;px<GW;px++){
        int idx=(int)((float)px/GW*WAVE_TABLE_SIZE*CYCLES)%WAVE_TABLE_SIZE;
        int16_t s=getMorphedSample(idx,morph,shape);
        int y=constrain(CY-(int)((float)s/32768.f*(GH/2-2)),GY,GY+GH-1);
        if(px>0)gfx.drawLine(px-1,pY,px,y,uiColor);
        pY=y;
    }
    gfx.drawFastHLine(GX,GY+GH,GW,uiColor);
}

// Oscillator 2's preview (v0.9914). Same picture, but reading one chosen
// waveform's Shape=0/Shape=1 pair directly instead of going through the
// Morph chain — osc 2 selects a waveform outright, so there is nothing to
// interpolate between chain slots.
void drawOsc2Waveform(LovyanGFX &gfx,OscWaveform w,float shape){
    constexpr int GX=0,GY=12,GW=240,GH=43,CY=GY+GH/2,CYCLES=3;
    int16_t *tA=oscWaveformTable(w),*tB=oscWaveformTableB(w);
    float sh=constrain(shape,0.f,1.f);
    // Same reasoning as the audio path (v0.9935): Square carries its Shape
    // in the table, so show oscillator 2's own rebuilt copy rather than
    // oscillator 1's.
    if(w==OscWaveform::SQUARE){updateSquareDuty2(sh);tA=tB=squareTable2;}
    gfx.fillRect(GX,GY,GW,GH,BLACK);
    gfx.drawFastHLine(GX,CY,GW,gfx.color565(0,64,0));
    int pY=CY;
    for(int px=0;px<GW;px++){
        int idx=(int)((float)px/GW*WAVE_TABLE_SIZE*CYCLES)%WAVE_TABLE_SIZE;
        int16_t s=(int16_t)(tA[idx]*(1.f-sh)+tB[idx]*sh);
        int y=constrain(CY-(int)((float)s/32768.f*(GH/2-2)),GY,GY+GH-1);
        if(px>0)gfx.drawLine(px-1,pY,px,y,uiColor);
        pY=y;
    }
    gfx.drawFastHLine(GX,GY+GH,GW,uiColor);
}

// Draw a scrollable item list.
// splitCol: if >= 0, items from index 0..(splitCol-1) go left, rest go right.
// splitCol < 0: single column layout.
// rowH is a parameter rather than a constant as of v0.9911: the VCO tab's
// first page reached 11 items once oscillator 2 added a page-flip row and
// a Mix row, and 6 rows at the old 13px pitch put the last one at y=122,
// whose glyphs run to 130 — straight through the nav line at 126. Tighter
// pitch is only needed there, so everything else keeps 13.
void drawItemList(SettingItem *items,int count,int sel,int startY=76,int splitCol=-1,bool showNav=true,int rowH=13){
    const int ROW=rowH;
    constexpr int LX=5,RX=123;
    bool twoCol=(splitCol>0&&splitCol<count);
    canvas.setTextColor(uiColor,BLACK);

    if(twoCol){
        // Vertical divider — sized to the taller of the two columns
        int rows=max(splitCol,count-splitCol);
        canvas.drawFastVLine(119,startY-2,rows*ROW+4,canvas.color565(0,64,0));
    }

    for(int i=0;i<count;i++){
        int x,y;
        if(twoCol){
            bool left=(i<splitCol);
            x=left?LX:RX;
            y=startY+(left?i:(i-splitCol))*ROW;
        } else {
            x=LX; y=startY+i*ROW;
        }
        // Modulation markers (v0.9903): '*' when an IMU axis drives this
        // parameter, '~' when the LFO does, both if both. Appended after
        // the value rather than given a column of their own, since the
        // name field is already 8 characters wide and several names use
        // all 8. Cheap to read at a glance and costs no layout space.
        bool mImu=imuTargetActive(items[i].imuT);
        bool mLfo=lfoTargetActive(items[i].lfoT);
        const char *mark=mImu?(mLfo?" *~":" *"):(mLfo?" ~":"");
        canvas.setCursor(x,y);
        canvas.printf("%c%-8s%s%s",(i==sel)?'>':' ',items[i].name,items[i].valueLabel(),mark);
    }
    if(!showNav)return;
    const char *nav=";/. select  ,// change  Tab:next";
    // Clear the nav row first (v0.9904). The per-screen clears above stop
    // at y=125 (fillRect(0,76,240,50) covers 76-125), so this row was only
    // ever overwritten, never erased. Nav strings differ per screen and are
    // centred, so they start at different x and leave different gaps —
    // cycling through the tabs left fragments of previous screens' text
    // showing through the spaces of the current one. Only a full redraw
    // (fillScreen) cleaned it up, which is why it built up over a lap of
    // the tab cycle. The canvas is 135 tall and the 8px font sits at 126,
    // so 125-134 is the whole row plus a pixel of margin.
    canvas.fillRect(0,125,240,10,BLACK);
    canvas.setCursor((240-(int)strlen(nav)*6)/2,126);
    canvas.print(nav);
}

void drawVcoScreen(bool full){
    canvas.startWrite();
    if(full)drawTabBar(canvas,AppMode::VCO);
    // Square's Shape IS pulse width, and calling it "Shape" cost real
    // confusion once — a user comparing against the pre-Shape firmware had
    // no way to tell from the screen that the same control was in front of
    // them (v0.992). Named for what it does whenever the waveform in play
    // is Square, on either page. Row 2 is Shape on both item tables.
    {
        int lo=constrain((int)(params.timbreMorph+0.5f),0,max(0,morphChainLen-1));
        vcoItems[2].name =(morphChain[lo]==OscWaveform::SQUARE)?"PWM":"Shape";
        vco2Items[2].name=(params.osc2Waveform==OscWaveform::SQUARE)?"PWM":"Shape";
    }
    // The preview follows whichever oscillator is on screen, so flipping
    // pages shows you what you are about to edit (v0.9911).
    if(vcoPage==0)drawWaveform(canvas,params.timbreMorph,params.oscShape);
    else          drawOsc2Waveform(canvas,params.osc2Waveform,params.osc2Shape);
    canvas.fillRect(0,56,240,70,BLACK);
    // Page 1 is 11 items, so 6 left / 5 right at a 12px pitch from y=56:
    // the lowest row lands at 116 and ends at 124, clearing the nav line.
    if(vcoPage==0)drawItemList(vcoItems, NUM_VCO_ITEMS, selectedVcoIndex, 56,6,true,12);
    else          drawItemList(vco2Items,NUM_VCO2_ITEMS,selectedVco2Index,57,4);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawVcfScreen(bool full){
    canvas.startWrite();
    if(full)drawTabBar(canvas,AppMode::VCF);
    constexpr int GX=0,GY=12,GW=240,GH=60;
    canvas.fillRect(GX,GY,GW,GH+4,BLACK);

    // Draw frequency response curve using Biquad magnitude calculation.
    // X axis = log frequency scale (100Hz - 20000Hz mapped to 0 - GW).
    // Y axis = gain in dB (top = +18dB, centre = 0dB, bottom = -48dB).
    constexpr float F_MIN  = 100.0f;
    constexpr float F_MAX  = 20000.0f;
    constexpr float DB_TOP =  18.0f;  // top of graph
    constexpr float DB_BOT = -48.0f;  // bottom of graph
    constexpr float DB_RNG = DB_TOP - DB_BOT;
    int zeroY = GY + (int)((DB_TOP / DB_RNG) * GH); // y coordinate for 0dB

    // 0dB reference line (dim green)
    uint16_t dim = canvas.color565(0,64,0);
    canvas.drawFastHLine(GX, zeroY, GW, dim);

    // Compute Biquad coefficients at current settings
    float cut = constrain(effectiveCutoffHz(), 100.f, SAMPLE_RATE*0.45f);   // v0.99882
    float Q   = effectiveResonanceQ();   // v0.99883
    float omega = 2.0f*PI*cut/SAMPLE_RATE;
    float sinW=sinf(omega),cosW=cosf(omega),alpha=sinW/(2.0f*Q);
    float b0,b1,b2,a0,a1,a2;
    switch(filterParams.type){
        case FilterType::LPF:  b0=(1-cosW)/2;b1=1-cosW;b2=(1-cosW)/2;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::HPF:  b0=(1+cosW)/2;b1=-(1+cosW);b2=(1+cosW)/2;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::BPF:  b0=alpha;b1=0;b2=-alpha;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::NOTCH:b0=1;b1=-2*cosW;b2=1;a0=1+alpha;a1=-2*cosW;a2=1-alpha;break;
        case FilterType::NONE: b0=1;b1=b2=0;a0=1;a1=a2=0;break; // bypass
        default:               b0=1;b1=b2=0;a0=1;a1=a2=0;break;
    }
    // Normalise by a0
    float nb0=b0/a0,nb1=b1/a0,nb2=b2/a0;
    float na1=a1/a0,na2=a2/a0;

    int prevY = -1;
    for(int px=0;px<GW;px++){
        // Map pixel x to frequency (log scale)
        float t   = (float)px / (GW-1);
        float freq= F_MIN * powf(F_MAX/F_MIN, t);
        float w   = 2.0f*PI*freq/SAMPLE_RATE;

        // |H(e^jw)|^2 = |B(e^jw)|^2 / |A(e^jw)|^2
        // B = nb0 + nb1*e^-jw + nb2*e^-2jw
        float Br = nb0 + nb1*cosf(w) + nb2*cosf(2*w);
        float Bi =     - nb1*sinf(w) - nb2*sinf(2*w);
        float Ar = 1.0f + na1*cosf(w) + na2*cosf(2*w);
        float Ai =       - na1*sinf(w) - na2*sinf(2*w);
        float magSq = (Br*Br+Bi*Bi) / (Ar*Ar+Ai*Ai+1e-12f);
        float dB = 10.0f*log10f(magSq+1e-12f);

        // Map dB to y coordinate
        float norm = (DB_TOP - dB) / DB_RNG;
        int y = GY + (int)(norm * GH);
        y = constrain(y, GY, GY+GH-1);

        if(px>0 && prevY>=0)
            canvas.drawLine(px-1, prevY, px, y, uiColor);
        prevY = y;
    }

    // Cutoff marker (yellow vertical line). Drawn from the EFFECTIVE
    // cutoff so it tracks the curve (v0.99883) — it marks where the filter
    // IS, so leaving it at the knob position while the curve moved made it
    // look like the graph and the marker disagreed.
    int cx = GX + (int)((log(effectiveCutoffHz()/F_MIN)/log(F_MAX/F_MIN)) * GW);
    cx = constrain(cx, GX, GX+GW-1);
    canvas.drawFastVLine(cx, GY, GH, canvas.color565(255,255,0));

    // Cutoff frequency label
    char fLabel[12]; snprintf(fLabel,sizeof(fLabel),"%.0fHz",effectiveCutoffHz());   // v0.99883
    int lx = (cx+4 < GX+GW-30) ? cx+2 : cx-28;
    canvas.setCursor(lx, GY+2); canvas.print(fLabel);

    canvas.drawFastHLine(GX,GY+GH,GW,uiColor);
    canvas.fillRect(0,76,240,50,BLACK);
    // Left(0-3): Filter/Cutoff/Resonance/KeyTrack  Right(4-7): FEnv Dep/Atk/Dec/Rel
    drawItemList(vcfItems,NUM_VCF_ITEMS,selectedVcfIndex,76,4);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawVcaScreen(bool full){
    canvas.startWrite();
    if(full)drawTabBar(canvas,AppMode::VCA);
    drawAdsrGraph();
    canvas.fillRect(0,76,240,50,BLACK);
    // v0.989: Tremolo made this a 5th item, and a single column would have
    // put it at y=128 — past the nav line at 126 and off the bottom. Split
    // into two columns instead (ADSR left, Release+Tremolo right), which
    // also matches how the VCO tab already lays its items out.
    // Left 76,89,102 / right 76,89 — both clear the nav line.
    drawItemList(vcaItems,NUM_VCA_ITEMS,selectedVcaIndex,76,3);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

// Live LFO waveform: one full cycle drawn across the screen width, scaled
// by the current depth, plus a moving marker showing the LFO's live phase.
void drawLfoWaveform(){
    constexpr int GX=0,GY=12,GW=240,GH=43,CY=GY+GH/2;
    canvas.fillRect(GX,GY,GW,GH,BLACK);
    canvas.drawFastHLine(GX,CY,GW,canvas.color565(0,64,0));
    int pY=CY;
    for(int px=0;px<GW;px++){
        int idx=(int)((float)px/GW*WAVE_TABLE_SIZE)%WAVE_TABLE_SIZE;
        float s=lfoTableSample(lfo.wave,idx)*lfo.depth;
        int y=constrain(CY-(int)(s*(GH/2-2)),GY,GY+GH-1);
        if(px>0)canvas.drawLine(px-1,pY,px,y,uiColor);
        pY=y;
    }
    // Live phase marker
    int mx=GX+(int)((lfoPhase/(float)WAVE_TABLE_SIZE)*GW);
    mx=constrain(mx,GX,GX+GW-1);
    canvas.drawFastVLine(mx,GY,GH,canvas.color565(255,255,0));
    canvas.drawFastHLine(GX,GY+GH,GW,uiColor);
}

void drawLfoScreen(bool full){
    canvas.startWrite();
    if(full)drawTabBar(canvas,AppMode::LFO);
    drawLfoWaveform();
    canvas.fillRect(0,56,240,70,BLACK);
    drawItemList(lfoItems,NUM_LFO_ITEMS,selectedLfoIndex,76);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawFxScreen(bool full){
    canvas.startWrite();
    if(full)drawTabBar(canvas,AppMode::FX);
    canvas.fillRect(0,12,240,111,BLACK);

    if(fxViewMode==FxViewMode::PAD_SELECTOR){
        // Pads in two rows of three (v0.989). The previous single row of
        // 44px pads filled the 240px width exactly at five effects, so the
        // Bit-crusher had nowhere to go; shrinking them instead would have
        // cut the labels down to five characters. Two rows keep the names
        // readable at 72px and leave room for a 7th and 8th effect.
        // Filled/uiColor when on, outline-only when off, cursor gets a
        // white border. ,// still walks all six in order — the grid is
        // purely visual, so no navigation change was needed.
        constexpr int PAD_COLS=3,PAD_W=72,PAD_H=26,PAD_GAP_X=6,PAD_GAP_Y=6;
        constexpr int PAD_X0=(240-(PAD_COLS*PAD_W+(PAD_COLS-1)*PAD_GAP_X))/2;
        constexpr int PAD_Y0=18,PAD_R=5;
        // Font is 6px wide and 8px tall, so these two centre the label in
        // both axes rather than only horizontally as before.
        for(int i=0;i<NUM_FX_EFFECTS;i++){
            int col=i%PAD_COLS,row=i/PAD_COLS;
            int x=PAD_X0+col*(PAD_W+PAD_GAP_X);
            int y=PAD_Y0+row*(PAD_H+PAD_GAP_Y);
            bool on=fxIsOn(i);
            bool isCursor=(i==fxPadCursor);
            if(on)canvas.fillRoundRect(x,y,PAD_W,PAD_H,PAD_R,uiColor);
            canvas.drawRoundRect(x,y,PAD_W,PAD_H,PAD_R,isCursor?WHITE:uiColor);
            canvas.setTextColor(on?BLACK:uiColor,on?uiColor:BLACK);
            int lx=x+(PAD_W-(int)strlen(FX_EFFECT_NAMES[i])*6)/2;
            int ly=y+(PAD_H-8)/2;
            canvas.setCursor(lx,ly);
            canvas.print(FX_EFFECT_NAMES[i]);
            canvas.setTextColor(uiColor,BLACK);
        }
        // Info line, and for a single-parameter effect its value too, so
        // there is something to read while editing in place (v0.992).
        int pcnt=0;SettingItem *pit=fxGetParamItems(fxPadCursor,pcnt);
        canvas.setCursor(4,86);
        if(pcnt==1&&pit)
            canvas.printf("%s: %s  %s %s",FX_EFFECT_NAMES[fxPadCursor],
                fxIsOn(fxPadCursor)?"ON":"off",pit[0].name,pit[0].valueLabel());
        else
            canvas.printf("%s: %s",FX_EFFECT_NAMES[fxPadCursor],fxIsOn(fxPadCursor)?"ON":"off");
        canvas.setCursor(4,100);
        if(fxInlineEdit)canvas.print(",// value  .:Done");
        else            canvas.print(",// ; move  Ent:On/Off");
        canvas.setCursor(4,110);
        if(fxInlineEdit)canvas.print("");
        else if(pcnt==1)canvas.print(".:Edit value here");
        else            canvas.print(".:Edit params");
    } else {
        int count;SettingItem *items=fxGetParamItems(fxEditingEffect,count);
        canvas.setCursor(4,16);
        canvas.printf("%s params",FX_EFFECT_NAMES[fxEditingEffect]);
        if(items&&count>0)drawItemList(items,count,selectedFxIndex,32);
        canvas.setCursor(4,110);
        canvas.print("Tab: back to pads");
    }

    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawSettingsScreen(bool full){
    canvas.startWrite();
    if(full)drawTabBar(canvas,AppMode::SETTINGS);
    // Clear entire area below tab bar to remove any residual drawing from other screens
    canvas.fillRect(0,12,240,123,BLACK);

    // Single column: Patch / IMU / Bend / Portamento entry points.
    // Each opens its own dedicated category screen (AppMode::CATEGORY).
    // Two columns (v0.994), the same treatment VCO and VCA already use —
    // the list reached eight entries once Display was added, and a single
    // column that long walks the eye down the screen for no reason.
    //
    // Split at 6 rather than down the middle: the name field is 8
    // characters wide, and "Portamento" and "Play Style" are the only
    // entries that fill or exceed it. The right column starts at x=123 and
    // has to fit name plus value inside 117px, so the long names go in the
    // left column where there is room to overrun. That leaves Arp and
    // Display on the right — both short.
    drawItemList(settingItems,NUM_SETTING_ITEMS,selectedSettingIndex,24,6,false);

    const char *n1=";/. select  /:open category";
    const char *n2="Tab: save & return to play";
    canvas.setCursor((240-(int)strlen(n1)*6)/2,115);canvas.print(n1);
    canvas.setCursor((240-(int)strlen(n2)*6)/2,128);canvas.print(n2);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawCategoryScreen(bool full){
    canvas.startWrite();
    int count;const char *title;SettingItem *items=getCategoryItems(count,title);
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        char titleBuf[24];snprintf(titleBuf,sizeof(titleBuf),"SETTING > %s",title);
        canvas.setCursor((240-(int)strlen(titleBuf)*6)/2,2);
        canvas.print(titleBuf);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);
    // MIDI reached 11 rows once CC in was added (v0.9988), which does not
    // fit above the nav lines in one column. Split 6/5 rather than
    // evenly: the left column then holds send and the right holds
    // receive, so the two directions read as two groups.
    int splitCol=(currentCategory==SettingsCategory::IMU)?(isCardputerAdv?5:4)
                :(currentCategory==SettingsCategory::MIDI_IN)?6
                :(currentCategory==SettingsCategory::THEREMIN)?4:-1;   // v0.9992: 8 rows, 4/4
    drawItemList(items,count,selectedCategoryIndex,24,splitCol,false);
    // Warning under the Play Style list (v0.9941). "Drift ON" by itself
    // does not tell you the synth is about to detune itself, and this is
    // the kind of setting someone turns on, forgets, and later reports as
    // a tuning bug. Only shown when it is actually on — a permanent
    // caution line would just become part of the furniture.
    if(currentCategory==SettingsCategory::PLAYMODE&&playMode==PlayMode::PRO&&analogDriftOn){
        const char *w1="Drift: pitch/filter/level";
        const char *w2="wander on purpose";
        canvas.setCursor((240-(int)strlen(w1)*6)/2,88);canvas.print(w1);
        canvas.setCursor((240-(int)strlen(w2)*6)/2,100);canvas.print(w2);
    }
    const char *n1=";/. select  ,// change";
    const char *n2="Tab: back";
    canvas.setCursor((240-(int)strlen(n1)*6)/2,115);canvas.print(n1);
    canvas.setCursor((240-(int)strlen(n2)*6)/2,128);canvas.print(n2);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

// Theme list with swatches (v0.9937). Each row shows the theme's three
// mode accents as blocks beside its name, so the colours can be compared
// against each other before committing to one — which is the whole reason
// this stopped being a cycling value.
void drawMorphSlotScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title="Morph Slots";
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);
    // Two columns of five: ten rows down one side would not fit above the
    // nav lines, and the key that plays a slot is right there on the row.
    constexpr int ROW=11,startY=16;
    for(int i=0;i<NUM_MORPH_SLOTS;i++){
        int col=i/5,row=i%5;
        int x=4+col*118,y=startY+row*ROW;
        canvas.setCursor(x,y);
        // Slot 10 is reached with Shift+0, so it is labelled 0.
        char keyc=(i==9)?'0':('1'+i);
        const char *nm=morphSlotPatch[i].length()?morphSlotPatch[i].c_str():"--";
        if(!morphSlotsReady())nm="(no mem)";
        canvas.printf("%c%c %-13.13s",(i==morphSlotCursor)?'>':' ',keyc,nm);
    }
    int ty=startY+5*ROW+6;
    canvas.setCursor(4,ty);
    canvas.printf("%cMorph time %.2fs",(morphSlotCursor==NUM_MORPH_SLOTS)?'>':' ',morphTimeSec);
    canvas.setCursor(4,ty+11);
    if(morphTimeSec<=0.01f)canvas.print("(0 = switch instantly)");
    else                   canvas.print("Play: Shift+1..0");
    const char *pn1=";/. select  ,// patch  Del:clear";
    const char *pn2="Enter:audition   Tab:back";
    canvas.setCursor((240-(int)strlen(pn1)*6)/2,115);canvas.print(pn1);
    canvas.setCursor((240-(int)strlen(pn2)*6)/2,126);canvas.print(pn2);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawThemePickerScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title="Theme";
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);

    constexpr int ROW=15,startY=20,SW=14,SH=9;
    for(int i=0;i<NUM_UI_THEMES;i++){
        const UiTheme &t=UI_THEMES[i];
        int y=startY+i*ROW;
        canvas.setCursor(6,y);
        // '>' is the cursor; '*' marks the theme actually in use, so it
        // stays findable once the cursor has moved off it.
        canvas.printf("%c%-9s%c",(i==themePickerIndex)?'>':' ',t.name,
                      (i==uiThemeIndex)?'*':' ');
        int sx=112;
        uint16_t sw[3]={canvas.color565(t.playR,t.playG,t.playB),
                        canvas.color565(t.seqR ,t.seqG ,t.seqB ),
                        canvas.color565(t.songR,t.songG,t.songB)};
        for(int k=0;k<3;k++){
            canvas.fillRect(sx+k*(SW+4),y-1,SW,SH,sw[k]);
            // Outlined in grey rather than uiColor: a white or pale
            // swatch would otherwise merge into a pale accent border.
            canvas.drawRect(sx+k*(SW+4),y-1,SW,SH,canvas.color565(90,90,90));
        }
    }
    canvas.setCursor(6,startY+NUM_UI_THEMES*ROW+6);
    canvas.print("PLAY/SEQ/SONG");
    const char *pn1=";/. select  Enter:OK";
    const char *pn2="Tab:Cancel";
    canvas.setCursor((240-(int)strlen(pn1)*6)/2,115);canvas.print(pn1);
    canvas.setCursor((240-(int)strlen(pn2)*6)/2,126);canvas.print(pn2);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawImuPickerScreen(bool full){
    canvas.startWrite();
    // Title names the section being browsed at level 1, and says so at
    // level 0 (v0.993). Redrawn whenever it changes, not only on a full
    // redraw — the whole point is that it tracks the cursor.
    const char *grp=(imuPickerLevel==0)?"Category"
        :(IMU_PICKER_SECTION_COUNT?IMU_PICKER_SECTIONS[imuPickerSection].name:"Target");
    static const char *lastGrp=nullptr;
    if(full||grp!=lastGrp){
        lastGrp=grp;
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        char titleBuf[32];
        if(imuPickerAxis>=IMU_PICKER_AXIS_CC0)
            snprintf(titleBuf,sizeof(titleBuf),"CC In%d > %s",
                imuPickerAxis-IMU_PICKER_AXIS_CC0+1,grp?grp:"Target");
        else
            snprintf(titleBuf,sizeof(titleBuf),"%s %s > %s",
                isCardputerAdv?"IMU":"PAD",imuPickerAxis==0?"X":"Y",grp?grp:"Target");
        canvas.setCursor((240-(int)strlen(titleBuf)*6)/2,2);
        canvas.print(titleBuf);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);

    constexpr int ROW=13,startY=18,maxRows=7;
    if(imuPickerLevel==0){
        // Section list. Short enough that it always fits, so no scrolling.
        for(int i=0;i<IMU_PICKER_SECTION_COUNT&&i<maxRows;i++){
            canvas.setCursor(6,startY+i*ROW);
            if(i==imuPickerSectionRow)canvas.printf(">%s",IMU_PICKER_SECTIONS[i].name);
            else                      canvas.printf(" %s",IMU_PICKER_SECTIONS[i].name);
        }
    } else {
        // Targets within the chosen section only, so the scroll window is
        // over a handful of rows rather than all 28.
        const ImuPickerSection &sec=IMU_PICKER_SECTIONS[imuPickerSection];
        int rel=imuPickerIndex-sec.first;
        int top=constrain(rel-maxRows/2,0,max(0,sec.count-maxRows));
        for(int i=0;i<maxRows&&(top+i)<sec.count;i++){
            int row=sec.first+top+i;
            canvas.setCursor(6,startY+i*ROW);
            const char *label=imuTargetName(IMU_PICKER_ORDER[row].target);
            if(row==imuPickerIndex)canvas.printf(">%s",label);
            else                   canvas.printf(" %s",label);
        }
    }
    const char *pn1=(imuPickerLevel==0)?";/. select  / or Enter:Open"
                                       :";/. select  / or Enter:OK";
    const char *pn2=(imuPickerLevel==0)?"Tab:Cancel":"Tab:Back";
    canvas.setCursor((240-(int)strlen(pn1)*6)/2,115);canvas.print(pn1);
    canvas.setCursor((240-(int)strlen(pn2)*6)/2,126);canvas.print(pn2);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawScalePickerScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title=(scalePickerLevel==0)?"Scale: Category":SCALE_CATEGORY_NAMES[scalePickerCategoryIndex];
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);

    constexpr int ROW=13,startY=18,maxRows=7;
    if(scalePickerLevel==0){
        int top=constrain(scalePickerRowIndex-maxRows/2,0,max(0,NUM_SCALE_CATEGORIES-maxRows));
        for(int i=0;i<maxRows&&(top+i)<NUM_SCALE_CATEGORIES;i++){
            int row=top+i;int y=startY+i*ROW;
            canvas.setCursor(6,y);
            if(row==scalePickerRowIndex)canvas.printf(">%s",SCALE_CATEGORY_NAMES[row]);
            else                        canvas.printf(" %s",SCALE_CATEGORY_NAMES[row]);
        }
    } else {
        int indices[32];int n=getScalesInCategory(scalePickerCategoryIndex,indices,32);
        int top=constrain(scalePickerRowIndex-maxRows/2,0,max(0,n-maxRows));
        for(int i=0;i<maxRows&&(top+i)<n;i++){
            int row=top+i;int y=startY+i*ROW;
            canvas.setCursor(6,y);
            const char *label=SCALES[indices[row]].name;
            if(row==scalePickerRowIndex)canvas.printf(">%s",label);
            else                        canvas.printf(" %s",label);
        }
    }
    const char *pn1=(scalePickerLevel==0)?";/.:Select  / or Enter:Open":";/.:Preview  / or Enter:OK";
    const char *pn2=(scalePickerLevel==0)?"Tab:Cancel":"Tab:Back";
    canvas.setCursor((240-(int)strlen(pn1)*6)/2,115);canvas.print(pn1);
    canvas.setCursor((240-(int)strlen(pn2)*6)/2,126);canvas.print(pn2);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawImuCalibrateConfirmScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title="IMU Calibrate";
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);
    const char *msg1="Set current tilt as zero point?";
    const char *msg2="Hold the device the way you'll play it.";
    canvas.setCursor((240-(int)strlen(msg1)*6)/2,45);canvas.print(msg1);
    canvas.setCursor((240-(int)strlen(msg2)*6)/2,58);canvas.print(msg2);
    const char *nav="/ or Enter:Yes   Tab:No";
    canvas.setCursor((240-(int)strlen(nav)*6)/2,80);canvas.print(nav);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawResetConfirmScreen(bool full){
    canvas.startWrite();
    const char *title, *msg1, *msg2;
    switch(resetConfirmKind){
        case ResetKind::PATCH_TONE:
            title="Reset Patch";
            msg1="Reset VCO/VCF/VCA/LFO/IMU";
            msg2="to default? (unsaved changes lost)";
            break;
        case ResetKind::PATCH_RANDOM:
            title="Randomize Patch";
            msg1="Randomize VCO/VCF/VCA/LFO/IMU?";
            msg2="(unsaved changes lost)";
            break;
        case ResetKind::PATTERN_RANDOM:
            title="Randomize Pattern";
            msg1="Randomize the 16-step Sequencer";
            msg2="pattern? (unsaved changes lost)";
            break;
        case ResetKind::BEND:
            title="Reset Bend";
            msg1="Reset Bend width/attack/release";
            msg2="to default?";
            break;
        case ResetKind::PORTAMENTO:
        default:
            title="Reset Portamento";
            msg1="Reset Portamento on/off + speed";
            msg2="to default?";
            break;
    }
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);
    canvas.setCursor((240-(int)strlen(msg1)*6)/2,45);canvas.print(msg1);
    canvas.setCursor((240-(int)strlen(msg2)*6)/2,58);canvas.print(msg2);
    const char *nav="/ or Enter:Yes   Tab:No";
    canvas.setCursor((240-(int)strlen(nav)*6)/2,80);canvas.print(nav);
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

// ==========================================================
// Patch Bank screen
// ==========================================================
void drawTimbreScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title="TIMBRE MORPH ORDER";
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);

    // ---- Active chain, drawn as boxes left-to-right (same spirit as
    // Song's timeline) — the box holding the library cursor's waveform
    // (if it's currently included) is outlined white.
    constexpr int CH_Y=16,CH_H=22,CH_W=36,CH_GAP=3;
    OscWaveform cursorW=(OscWaveform)timbreCursor;
    for(int i=0;i<morphChainLen;i++){
        int x=2+i*(CH_W+CH_GAP);
        bool isCursorSlot=(morphChain[i]==cursorW);
        canvas.fillRect(x,CH_Y,CH_W,CH_H,uiColor);
        canvas.drawRect(x,CH_Y,CH_W,CH_H,isCursorSlot?WHITE:uiColor);
        canvas.setTextColor(BLACK,uiColor);
        canvas.setCursor(x+3,CH_Y+7);
        canvas.print(oscWaveformAbbrev(morphChain[i]));
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.setCursor(4,44);
    canvas.printf("%d/%d slots used (min %d, max %d)",morphChainLen,MAX_MORPH_SLOTS,MIN_MORPH_SLOTS,MAX_MORPH_SLOTS);

    // ---- Library list: every known waveform, showing its slot number
    // if included, or a blank dash if not. No separate header label —
    // saves the vertical space needed to keep the last row clear of the
    // nav line below. Scrolls (centered on the cursor) now that the
    // library is bigger than safely fits in the available space at once.
    constexpr int visibleRows=7;
    int startIdx=max(0,min(timbreCursor-visibleRows/2,NUM_OSC_WAVEFORMS-visibleRows));
    for(int row=0;row<visibleRows&&startIdx+row<NUM_OSC_WAVEFORMS;row++){
        int i=startIdx+row;
        int y=52+row*9;
        bool isCursorRow=(i==timbreCursor);
        int slot=morphChainSlotOf((OscWaveform)i);
        canvas.setTextColor(isCursorRow?WHITE:uiColor,BLACK);
        canvas.setCursor(4,y);
        if(slot>=0)canvas.printf("%s %d: %s",isCursorRow?">":" ",slot+1,OSC_WAVEFORM_NAMES[i]);
        else       canvas.printf("%s -: %s",isCursorRow?">":" ",OSC_WAVEFORM_NAMES[i]);
    }
    canvas.setTextColor(uiColor,BLACK);

    canvas.setCursor(4,124);
    canvas.print("Ent:Add/Remove ,//:Reorder Tab:Back");

    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawPatternBankScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title=(patternBankMode==PatternBankMode::SAVE)?"PATTERN BANK - SAVE":"PATTERN BANK - LOAD";
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);

    // Grid: 8 banks (rows, A-H) x 8 slots (columns, 1-8). Filled = has
    // data, outline only = empty. Selection shown in WHITE.
    constexpr int GX=20,GY=18,CW=26,CH=13;
    canvas.setCursor(GX,GY-10);
    for(int col=0;col<NUM_PATTERNS_PER_BANK;col++)canvas.printf("%-4d",col+1);
    for(int b=0;b<NUM_PATTERN_BANKS;b++){
        canvas.setCursor(4,GY+b*CH+3);
        canvas.printf("%c",'A'+b);
        for(int sl=0;sl<NUM_PATTERNS_PER_BANK;sl++){
            int cx=GX+sl*CW,cy=GY+b*CH;
            bool occupied=patternSlotCache[b][sl];   // v0.9926: no SD access while drawing
            bool isSel=(b==patternSelBank&&sl==patternSelSlot);
            uint16_t col2=isSel?WHITE:uiColor;
            if(occupied)canvas.fillRect(cx,cy,CW-4,CH-3,col2);
            else        canvas.drawRect(cx,cy,CW-4,CH-3,col2);
        }
    }

    if(patternConfirmDelete||patternConfirmOverwrite){
        canvas.fillRect(20,50,200,26,BLACK);
        canvas.drawRect(20,50,200,26,uiColor);
        canvas.setCursor(28,58);
        canvas.printf("%c%d: %s? (Y/N)",'A'+patternSelBank,patternSelSlot+1,
            patternConfirmDelete?"Delete":"Overwrite");
    } else {
        const char *nav=(patternBankMode==PatternBankMode::SAVE)
            ?"Enter:Save  Bksp:Clear  Tab:Cancel"
            :"Enter:Load  Bksp:Clear  Tab:Cancel";
        canvas.setCursor((240-(int)strlen(nav)*6)/2,124);
        canvas.print(nav);
    }
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawSongScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title="SONG";
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    if(helpVisible){
        drawHelpOverlay(canvas);
        canvas.endWrite();
        canvas.pushSprite(0,0);
        return;
    }
    canvas.fillRect(0,12,240,123,BLACK);

    if(songIoPickerOpen){
        canvas.setCursor(10,16);
        canvas.print(songIoMode==SongIoMode::SAVE?"SAVE SONG":"LOAD SONG");
        for(int i=0;i<NUM_SONG_SLOTS;i++){
            int y=28+i*11;
            bool occupied=songSlotExists(i);
            bool isSel=(i==songIoSelSlot);
            canvas.setTextColor(isSel?WHITE:uiColor,BLACK);
            canvas.setCursor(20,y);
            canvas.printf("%s%d %s",isSel?">":" ",i+1,occupied?"[X]":"[ ]");
        }
        canvas.setTextColor(uiColor,BLACK);
        if(songIoConfirmDelete||songIoConfirmOverwrite){
            canvas.fillRect(20,50,200,26,BLACK);
            canvas.drawRect(20,50,200,26,uiColor);
            canvas.setCursor(28,58);
            canvas.printf("Song %d: %s? (Y/N)",songIoSelSlot+1,songIoConfirmDelete?"Delete":"Overwrite");
        } else {
            canvas.setCursor(10,120);
            canvas.print("Enter:OK  Bksp:Clear  Tab:Back");
        }
        canvas.endWrite();
        canvas.pushSprite(0,0);
        return;
    }

    canvas.setCursor(4,14);
    canvas.printf("Bpm:%3.0f Swg:%+3.0f%%  Vol:%d%%",songTempoBpm,songSwing,(int)(params.keyVolume*100));
    canvas.setCursor(4,23);
    if(songFocus==SongFocus::ENTRY){
        const char *fieldNames[]={"Bank","Slot","Transpose","Repeat"};
        canvas.printf("Focus:Entry Field:%-9s %s",fieldNames[(int)songField],songPlaying?"PLAYING":"STOPPED");
    } else {
        canvas.printf("Focus:Song  Field:%-9s %s",songSettingsField==SongSettingsField::TEMPO?"Tempo":"Swing",songPlaying?"PLAYING":"STOPPED");
    }

    if(songLen==0){
        canvas.setCursor(4,60);
        canvas.print("(empty - Enter adds a step)");
    } else {
        // ---- Timeline (Option A): each entry as a fixed-width block,
        // colored by Bank letter, with a thin bar underneath whose width
        // is proportional to Repeat count. The playing entry is
        // highlighted white; the edit cursor gets a white outline
        // instead (so both can be shown distinctly even on the same
        // block).
        constexpr int TL_Y=33,TL_BH=18,TL_BW=32,TL_GAP=3,TL_STRIDE=TL_BW+TL_GAP;
        int visibleBlocks=240/TL_STRIDE;
        int tlStart=max(0,min(songCursorEntry-visibleBlocks/2,max(0,songLen-visibleBlocks)));
        for(int i=0;i<visibleBlocks&&tlStart+i<songLen;i++){
            int idx=tlStart+i;
            SongEntry &e=songEntries[idx];
            int x=2+i*TL_STRIDE;
            bool isCursor=(idx==songCursorEntry);
            bool isPlaying=(songPlaying&&idx==songPlayEntry);
            uint16_t blockColor=isPlaying?WHITE:songBankColors[e.bank];
            canvas.fillRect(x,TL_Y,TL_BW,TL_BH,blockColor);
            canvas.drawRect(x,TL_Y,TL_BW,TL_BH,isCursor?WHITE:uiColor);
            canvas.setTextColor(BLACK,blockColor);
            canvas.setCursor(x+3,TL_Y+4);
            canvas.printf("%c%d",'A'+e.bank,e.slot+1);
            canvas.setTextColor(uiColor,BLACK);
            int repW=map(e.repeat,1,16,3,TL_BW-2);
            canvas.fillRect(x+1,TL_Y+TL_BH+2,repW,3,isCursor?WHITE:songBankColors[e.bank]);
        }

        // ---- Mini step-grid preview (Option B): shows the actual step
        // shape of whichever entry is playing (or, when stopped, the
        // entry the cursor is on) — filled = note, half-height = tie.
        int previewIdx=songPlaying?songPlayEntry:songCursorEntry;
        SongEntry &pe=songEntries[previewIdx];
        loadPatternPreview(pe.bank,pe.slot);
        constexpr int PG_Y=68,PG_H=26,PG_SW=14,PG_BW=12;
        canvas.setCursor(4,58);
        canvas.printf("Preview: %c%d",'A'+pe.bank,pe.slot+1);
        for(int i=0;i<SEQ_NUM_STEPS;i++){
            int x=4+i*PG_SW;
            SeqStep &ps=songPreviewSteps[i];
            canvas.drawRect(x,PG_Y,PG_BW,PG_H,uiColor);
            if(ps.freq>0.f)canvas.fillRect(x+1,PG_Y+1,PG_BW-2,PG_H-2,uiColor);
            else if(ps.tie)canvas.fillRect(x+1,PG_Y+PG_H/2,PG_BW-2,PG_H/2-1,uiColor);
        }

        // ---- Selected entry's own Transpose/Repeat, since the timeline
        // blocks are too small to show these numbers directly. Kept
        // short (well under 240px/40 chars) — a longer version here
        // previously overflowed the screen width and wrapped onto the
        // line below it.
        SongEntry &ce=songEntries[songCursorEntry];
        canvas.setCursor(4,98);
        canvas.printf("E%d/%d %c%d  T:%+d  x%d",
            songCursorEntry+1,songLen,'A'+ce.bank,ce.slot+1,ce.transpose,ce.repeat);
        canvas.setCursor(4,109);
        canvas.printf("I:Inherit T/S %-3s  O:Loop %-3s",songInheritTempoSwing?"ON":"off",songLoopAtEnd?"ON":"off");
    }

    canvas.setCursor(4,124);
    canvas.print("Space:Play/Stop  H:Help  Tab:Back");

    canvas.endWrite();
    canvas.pushSprite(0,0);
}

void drawPatchScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillRect(0,0,240,11,BLACK);
        canvas.drawRect(0,0,240,11,uiColor);
        canvas.fillRect(1,1,238,9,WHITE);
        canvas.setTextColor(BLACK,WHITE);
        const char *title=(patchMode==PatchMode::SAVE)?"PATCH BANK - SAVE":"PATCH BANK - LOAD";
        canvas.setCursor((240-(int)strlen(title)*6)/2,2);
        canvas.print(title);
        canvas.setTextColor(uiColor,BLACK);
    }
    canvas.fillRect(0,12,240,123,BLACK);

    if(patchUiState==PatchUiState::NAME_ENTRY){
        const char *label=patchRenaming?"Rename to:":(patchDuplicating?"Duplicate as:":"New patch name:");
        canvas.setCursor(6,28);canvas.print(label);
        canvas.drawRect(6,40,228,16,uiColor);
        canvas.setCursor(10,44);
        canvas.printf("%s_",patchNameBuffer.c_str());
        const char *nav="Type name   Enter:OK   Tab:Cancel";
        canvas.setCursor((240-(int)strlen(nav)*6)/2,110);
        canvas.print(nav);
    } else if(patchUiState==PatchUiState::CONFIRM_DELETE){
        canvas.setCursor(6,40);
        canvas.printf("Delete '%s' ?",patchNames[patchActionIndex].c_str());
        const char *nav="/ or Enter:Yes   Tab:No";
        canvas.setCursor((240-(int)strlen(nav)*6)/2,60);
        canvas.print(nav);
    } else if(patchUiState==PatchUiState::CONFIRM_OVERWRITE){
        canvas.setCursor(6,40);
        canvas.printf("Overwrite '%s' ?",patchNames[patchActionIndex].c_str());
        const char *nav="/ or Enter:Yes   Tab:No";
        canvas.setCursor((240-(int)strlen(nav)*6)/2,60);
        canvas.print(nav);
    } else {
        int count=patchListCount();
        if(count==0){
            canvas.setCursor(6,40);
            canvas.print("No patches saved yet.");
        } else {
            constexpr int ROW=13,startY=16,maxRows=7;
            int top=constrain(selectedPatchIndex-maxRows/2,0,max(0,count-maxRows));
            for(int i=0;i<maxRows&&(top+i)<count;i++){
                int row=top+i;
                canvas.setCursor(6,startY+i*ROW);
                String label=patchIsNewRow(row)?"<New Patch>":patchNames[patchRowToNameIndex(row)];
                if(row==selectedPatchIndex)canvas.printf(">%s",label.c_str());
                else                       canvas.printf(" %s",label.c_str());
            }
        }
        const char *nav1=";/.:Select  /:OK  r:Rename";
        const char *nav2="c:Duplicate  ,:Delete  Tab:Back";
        canvas.setCursor((240-(int)strlen(nav1)*6)/2,111);
        canvas.print(nav1);
        canvas.setCursor((240-(int)strlen(nav2)*6)/2,122);
        canvas.print(nav2);
    }
    canvas.endWrite();
    canvas.pushSprite(0,0);
}

// ==========================================================
// MAIN screen
// ==========================================================
const char *getNoteName(float freq){
    if(freq<=0)return "---";
    static const char *n[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int nn=(int)roundf(12*log2f(freq/440)+69);
    int nm=nn%12;if(nm<0)nm+=12;
    static char buf[8];snprintf(buf,sizeof(buf),"%s%d",n[nm],nn/12-1);
    return buf;
}

void drawBendMeter(LovyanGFX &gfx,float bc,float mc,int yOff=0,int xOff=0){
    const int MX=80+xOff; constexpr int MW=12,MH_TOTAL=55; // total footprint matches the other side blocks (57-112)
    const int MY=57+yOff;
    constexpr int LBL_H=8; // vertical space reserved for the UP/DWN labels, top and bottom
    const int GY=MY+LBL_H, GH=MH_TOTAL-2*LBL_H, GCY=GY+GH/2; // the gauge itself, inset between the labels
    const int GCX=MX+MW/2; // gauge's horizontal center, used to center the UP/DWN labels on it
    // Clear the meter's full footprint incl. labels (DWN is the widest at
    // 18px, centered on GCX) — starts at x=76, clearing the left info
    // box's border at x=73 by 3px so the two boxes' borders stay intact.
    gfx.fillRect(76+xOff,MY,20,MH_TOTAL,BLACK);
    gfx.drawRect(MX,GY,MW,GH,uiColor);
    gfx.drawFastHLine(MX,GCY,MW,uiColor);
    if(fabsf(bc)>0.5f){
        float r=constrain(bc/mc,-1.f,1.f);
        int bh=(int)(fabsf(r)*(GH/2-1));
        gfx.fillRect(MX+1,r>0?GCY-bh:GCY,MW-2,bh,uiColor);
    }
    gfx.setCursor(GCX-6,MY);gfx.print("UP");         // "UP" = 2 chars = 12px, centered
    gfx.setCursor(GCX-9,MY+MH_TOTAL-7);gfx.print("DWN"); // "DWN" = 3 chars = 18px, centered
}

float dispAccelX=0.f, dispAccelY=0.f; // display-only smoothing, doesn't affect audio

void drawImuPad(LovyanGFX &gfx,int yOff=0,int xOff=0){
    const int PX=98+xOff; constexpr int PAD_SIZE=44;
    const int PY=57+yOff,cx=PX+PAD_SIZE/2,cy=PY+PAD_SIZE/2;
    // NOTE: the pad's own fillRect below already clears its full interior
    // (including where the "Y" label sits) every redraw, so no external
    // clear-rect is needed. A prior version cleared a rect starting at
    // PY-9 (i.e. reaching up to y=48), which intruded into the waveform
    // plot's row range (y=12-54) directly above this pad and periodically
    // erased part of the waveform curve there — moving the label fully
    // inside the pad's box removes that overlap entirely.
    gfx.fillRect(PX,PY,PAD_SIZE,PAD_SIZE,BLACK);
    gfx.drawRect(PX,PY,PAD_SIZE,PAD_SIZE,uiColor);
    uint16_t dim=gfx.color565(0,64,0);
    gfx.drawFastHLine(PX,cy,PAD_SIZE,dim);
    gfx.drawFastVLine(cx,PY,PAD_SIZE,dim);
    int dX,dY;
    if(isCardputerAdv){
        dispAccelX+=(lastAccelX-dispAccelX)*0.3f;
        dispAccelY+=(lastAccelY-dispAccelY)*0.3f;
        // Same angle conversion as updateImu(), including each axis's
        // calibration offset, so the dot recenters after Calibrate is used.
        float angleXDeg=asinf(constrain(dispAccelX,-1.f,1.f))*180.f/PI-imuAxisX.calOffsetDeg;
        float angleYDeg=asinf(constrain(dispAccelY,-1.f,1.f))*180.f/PI-imuAxisY.calOffsetDeg;
        float nX=constrain(angleXDeg/TILT_MAX_DEGREES,-1.f,1.f);
        float nY=constrain(angleYDeg/TILT_MAX_DEGREES,-1.f,1.f);
        dX=constrain(cx+(int)(nX*(PAD_SIZE/2-3)),PX+2,PX+PAD_SIZE-3);
        dY=constrain(cy+(int)(nY*(PAD_SIZE/2-3)),PY+2,PY+PAD_SIZE-3);
    } else {
        // Original Cardputer: show the key-driven virtual PAD position
        // directly (already -1..1, no calibration/tilt-angle concept here).
        dX=constrain(cx+(int)(padVirtualX*(PAD_SIZE/2-3)),PX+2,PX+PAD_SIZE-3);
        dY=constrain(cy+(int)(padVirtualY*(PAD_SIZE/2-3)),PY+2,PY+PAD_SIZE-3);
    }
    // Hollow circle once Calibrate has been used, filled dot otherwise —
    // an at-a-glance reminder that the zero point has been moved.
    if(imuCalibrated)gfx.drawCircle(dX,dY,3,uiColor);
    else             gfx.fillCircle(dX,dY,3,uiColor);
    gfx.setCursor(cx-3,PY+PAD_SIZE+2);gfx.print("Y");
    gfx.setCursor(PX+PAD_SIZE+2,cy-4);gfx.print("X");
}

float getImuNorm(ImuTarget t){
    switch(t){
        case ImuTarget::TIMBRE:        return params.timbreMorph/(float)max(1,morphChainLen-1);
        case ImuTarget::VIBRATO_DEPTH: return constrain(params.vibratoDepth+params.vibratoDepthOffset,0.f,1.f);
        case ImuTarget::VIBRATO_RATE:  return (constrain(params.vibratoRateHz+params.vibratoRateOffset,1.f,10.f)-1)/9.f;
        case ImuTarget::TREMOLO:       return constrain(params.tremoloDepth+params.tremoloDepthOffset,0.f,1.f);
        case ImuTarget::VOLUME:        return 1.0f-params.volumeScale;
        case ImuTarget::PITCH_BEND:    return (params.pitchBendCents+keyBendMaxCents)/(keyBendMaxCents*2);
        case ImuTarget::BEND_UP:       return constrain(params.pitchBendCents/keyBendMaxCents,0.f,1.f);
        case ImuTarget::BEND_DOWN:     return constrain(-params.pitchBendCents/keyBendMaxCents,0.f,1.f);
        case ImuTarget::BITCRUSH:      return params.bitcrush;
        case ImuTarget::FILTER_CUTOFF: return params.filterCutoffOffset;
        case ImuTarget::SHAPE:           return constrain(params.oscShape+params.oscShapeOffset,0.f,1.f);
        case ImuTarget::DETUNE:        return (params.detuneOffset+50.f)/100.f;
        case ImuTarget::NOISE:         return (params.noiseOffset+1.f)/2.f;
        case ImuTarget::SUB_LEVEL:     return (params.subLevelOffset+1.f)/2.f;
        case ImuTarget::RESONANCE:     return (params.resonanceOffset+3.f)/6.f;
        case ImuTarget::LFO_RATE:      return (lfoRateOffset+LFO_RATE_MAX)/(LFO_RATE_MAX*2.f);
        case ImuTarget::LFO_DEPTH:     return (lfoDepthOffset+1.f)/2.f;
        case ImuTarget::FX_RING_RATE:    return (params.ringModRateOffset+990.f)/1980.f;
        case ImuTarget::FX_RING_MIX:     return (params.ringModMixOffset+1.f)/2.f;
        case ImuTarget::FX_LIMIT_DRIVE:  return (params.limiterDriveOffset+2.f)/4.f;
        case ImuTarget::FX_CHORUS_DEPTH: return (params.chorusDepthOffset+10.f)/20.f;
        case ImuTarget::FX_CHORUS_MIX:   return (params.chorusMixOffset+1.f)/2.f;
        case ImuTarget::FX_DELAY_FB:     return (params.delayFeedbackOffset+0.45f)/0.9f;
        case ImuTarget::FX_DELAY_MIX:    return (params.delayMixOffset+1.f)/2.f;
        case ImuTarget::FX_REVERB_ROOM:  return params.reverbRoomOffset+0.5f;
        case ImuTarget::FX_REVERB_MIX:   return (params.reverbMixOffset+1.f)/2.f;
        case ImuTarget::OSC_MIX:         return 1.f-constrain(params.osc2Level+params.osc2LevelOffset,0.f,1.f);
        case ImuTarget::OSC2_SHAPE:      return constrain(params.osc2Shape+params.osc2ShapeOffset,0.f,1.f);
        case ImuTarget::ARP_TEMPO:     return ((lastMainMode==AppMode::SEQ?seqTempoOffset:arpTempoOffset)+100.f)/200.f;
        case ImuTarget::ARP_SWING:   return ((lastMainMode==AppMode::SEQ?seqSwingOffset:arpSwingOffset)+50.f)/100.f;
        default: return 0;
    }
}

// "+" for a positive additive offset, nothing for zero or negative — the
// numeric formatting below already supplies the "-" (v0.9993x). Every
// target this touches is an OFFSET added on top of the patch's own base
// setting, not an absolute value, and a bare positive number reads as
// though it WERE the absolute value — the exact confusion reported for
// ARP Tempo ("+15bpm" is 15 over whatever tempo is set, not 15bpm).
const char *imuSign(float v){return v>0.001f?"+":"";}

String getImuValStr(ImuTarget t){
    switch(t){
        case ImuTarget::TIMBRE:{int slot=constrain((int)params.timbreMorph,0,max(0,morphChainLen-1));return String(oscWaveformAbbrev(morphChain[slot]))+"("+String(params.timbreMorph,1)+")";}
        case ImuTarget::VIBRATO_DEPTH: return String((int)(constrain(params.vibratoDepth+params.vibratoDepthOffset,0.f,1.f)*100))+"%";
        case ImuTarget::VIBRATO_RATE:  return String(constrain(params.vibratoRateHz+params.vibratoRateOffset,1.f,10.f),1)+"Hz";
        case ImuTarget::TREMOLO:       return String((int)(constrain(params.tremoloDepth+params.tremoloDepthOffset,0.f,1.f)*100))+"%";
        case ImuTarget::VOLUME:        return String((int)(params.volumeScale*100))+"%";
        case ImuTarget::PITCH_BEND:    return String(imuSign(params.pitchBendCents))+String((int)params.pitchBendCents)+"c";
        case ImuTarget::BEND_UP:       return "+"+String((int)params.pitchBendCents)+"c";
        case ImuTarget::BEND_DOWN:     return String((int)params.pitchBendCents)+"c";
        case ImuTarget::BITCRUSH:      return String((int)(params.bitcrush*100))+"%";
        case ImuTarget::FILTER_CUTOFF:{float c=filterParams.cutoffHz*(1-params.filterCutoffOffset*0.9f);return String((int)constrain(c,FILTER_CUTOFF_MIN,FILTER_CUTOFF_MAX))+"Hz";}
        // The EFFECTIVE shape, not the raw offset: the offset alone reads
        // as a signed number with no obvious relation to what you hear
        // (v0.9943).
        case ImuTarget::SHAPE:           return String((int)(constrain(params.oscShape+params.oscShapeOffset,0.f,1.f)*100))+"%";
        case ImuTarget::DETUNE:        return String(imuSign(params.detuneOffset))+String((int)params.detuneOffset)+"c";
        case ImuTarget::NOISE:         return String(imuSign(params.noiseOffset))+String((int)(params.noiseOffset*100))+"%";
        case ImuTarget::SUB_LEVEL:     return String(imuSign(params.subLevelOffset))+String((int)(params.subLevelOffset*100))+"%";
        case ImuTarget::RESONANCE:     return String(imuSign(params.resonanceOffset))+String(params.resonanceOffset,1);
        case ImuTarget::LFO_RATE:      return String(imuSign(lfoRateOffset))+String(lfoRateOffset,1)+"Hz";
        case ImuTarget::LFO_DEPTH:     return String(imuSign(lfoDepthOffset))+String((int)(lfoDepthOffset*100))+"%";
        case ImuTarget::ARP_TEMPO:{
            float v=lastMainMode==AppMode::SEQ?seqTempoOffset:arpTempoOffset;
            return String(imuSign(v))+String(v,0)+"bpm";
        }
        case ImuTarget::ARP_SWING:{
            float v=lastMainMode==AppMode::SEQ?seqSwingOffset:arpSwingOffset;
            return String(imuSign(v))+String(v,0)+"%";
        }
        case ImuTarget::FX_RING_RATE:    return String((int)fxEffRingRateHz)+"Hz";
        case ImuTarget::FX_RING_MIX:     return String((int)(fxEffRingMix*100))+"%";
        case ImuTarget::FX_LIMIT_DRIVE:  return String(fxEffLimiterDrive,1)+"x";
        case ImuTarget::FX_CHORUS_DEPTH: return String(fxEffChorusDepthMs,1)+"ms";
        case ImuTarget::FX_CHORUS_MIX:   return String((int)(fxEffChorusMix*100))+"%";
        case ImuTarget::FX_DELAY_FB:     return String((int)(fxEffDelayFeedback*100))+"%";
        case ImuTarget::FX_DELAY_MIX:    return String((int)(fxEffDelayMix*100))+"%";
        case ImuTarget::FX_REVERB_ROOM:  return String((int)(constrain(params.reverbRoomSize+params.reverbRoomOffset,0.f,1.f)*100))+"%";
        case ImuTarget::FX_REVERB_MIX:   return String((int)(fxEffReverbMix*100))+"%";
        case ImuTarget::OSC_MIX:{
            float m=constrain(params.osc2Level+params.osc2LevelOffset,0.f,1.f);
            if(m<0.001f)return "1 only";
            if(m>0.999f)return "2 only";
            return String((int)((1.f-m)*100))+"/"+String((int)(m*100));
        }
        case ImuTarget::OSC2_SHAPE:      return String((int)(constrain(params.osc2Shape+params.osc2ShapeOffset,0.f,1.f)*100))+"%";
        default: return "---";
    }
}

// HELP overlay: shown while H is held
// Drawn over the existing screen content without fillScreen (no flicker)
void drawHelpOverlay(LovyanGFX &gfx){
    gfx.setTextColor(uiColor,BLACK);
    // Box height fits each screen's own content (v0.9996x). PLAY's
    // content ends at y=108 (+8 text height = 116); 109 keeps the same
    // ~6px bottom margin SEQ/SONG already have at their own height (99,
    // content ending at 106). 13+99=112 / 13+109=122, both well inside
    // the 135px display.
    int helpBoxH=(appMode==AppMode::SEQ||appMode==AppMode::SONG)?99:109;
    // The MAXIMUM footprint (109, PLAY's own height) is cleared to black
    // first, unconditionally, every draw — not just helpBoxH's own
    // smaller area (v0.9996x, fixing a real glitch the owner found:
    // switching from PLAY's taller box to SEQ/SONG's shorter one while H
    // was held left the old box's bottom strip un-erased, showing through
    // under the new, shorter one). The visible bordered box itself is
    // still drawn at the mode-appropriate helpBoxH for the tighter look
    // that was actually asked for; only the black clear underneath it
    // always covers the largest case.
    constexpr int HELP_BOX_MAX_H=109;
    gfx.fillRect(2,13,236,HELP_BOX_MAX_H,BLACK);
    gfx.drawRect(2,13,236,helpBoxH,uiColor);
    if(appMode==AppMode::SEQ){
        gfx.setCursor(6,17); gfx.print("=== SEQ HELP (hold H) ===");
        gfx.setCursor(6,26); gfx.print(",// :Cursor  Notekey:Set+Preview+Adv");
        gfx.setCursor(6,35); gfx.print("Bksp:Clear  Shift+Bksp:Clear all");
        gfx.setCursor(6,44); gfx.print("Space:Play/Stop  f:Focus toggle");
        gfx.setCursor(6,53); gfx.print(";/.:Adjust/Toggle  g:Cycle target");
        gfx.setCursor(6,62);
        if(isCardputerAdv)gfx.print("Shift+;/./,//:Octave/Transpose");
        else              gfx.print("J/N :Octave    B/M:Transpose");
        gfx.setCursor(6,71); gfx.print("k/l :Volume    Z/X:Bend");
        gfx.setCursor(6,80); gfx.print("V:Mark  Sh+C:Copy Sh+X:Cut Ent:Paste");
        // Tap Tempo (Shift+Enter) works in SEQ too, matching PLAY, but
        // had no mention here (v0.9996). G0:PLAY dropped to make room —
        // it is the same key that got here in the first place, the
        // natural inverse of Space/G0's toggle a player already knows.
        gfx.setCursor(6,89); gfx.print("Tab:Nx Sh+Tab:Pv  Sh+Ent:TapTempo");
        gfx.setCursor(6,98); gfx.print("release H to close");
        return;
    }
    if(appMode==AppMode::SONG){
        gfx.setCursor(6,17); gfx.print("=== SONG HELP (hold H) ===");
        gfx.setCursor(6,26); gfx.print(",// :Entry cursor  f:Focus Entry/Song");
        gfx.setCursor(6,35); gfx.print("g:Cycle field  ;/.:Adjust field");
        gfx.setCursor(6,44); gfx.print("Enter:Insert entry (copies Bank/Slot)");
        gfx.setCursor(6,53); gfx.print("Bksp:Delete entry  k/l:Volume");
        gfx.setCursor(6,62); gfx.print("Space:Play/Stop Song");
        gfx.setCursor(6,71); gfx.print("Shift+S:Save  Shift+L:Load Song");
        gfx.setCursor(6,80); gfx.print("I:Inherit T/S  O:Loop-at-end");
        // Sh+H:Latch added (v0.9996x) to match the new latch support
        // above; "to PLAY/SEQ" trimmed off "Tab:Back" for room — Tab's
        // own behaviour is already consistent across every other screen.
        gfx.setCursor(6,89); gfx.print("H:Help  Sh+H:Latch  Tab:Back");
        gfx.setCursor(6,98); gfx.print("release H to close");
        return;
    }
    gfx.setCursor(6,17); gfx.print("=== HELP (hold H) ===");
    gfx.setCursor(6,26);
    if(playMode==PlayMode::PRO)gfx.print("1-0-=+Bksp/q..[]\\:Notes(scale)");
    else                       gfx.print("1-0-=+Bksp/q..[]\\:Notes(Major)");
    if(isCardputerAdv){
        // Re-laid-out v0.9996 to fit three features that had no mention
        // anywhere in HELP: Morph slot select (Shift+1..0), Tap Tempo
        // (Shift+Enter, added v0.9994), and holding G0 for SONG mode —
        // all real, working shortcuts a player would otherwise have no
        // way to discover except by reading the firmware itself. Trimmed
        // "H/S+H:Help hold/latch" down to just the latch half (Sh+H,
        // folded into the Tab line below) — the hold half is redundant
        // with self-evidently already being on this screen.
        gfx.setCursor(6,35); gfx.print(";/. :Octave    ,//:Transpose");
        gfx.setCursor(6,44); gfx.print("k/l :Volume    Z/X:Bend");
        // A/S is IMU axis HOLD (freeze the current value); Shift+A/Shift+S
        // is the axis ENABLE toggle — two different actions the previous
        // single-line version conflated, corrected on request (v0.9996x).
        // The box grew by one row (see fillRect above) specifically to
        // fit both here rather than compress them into one line again.
        // Named "XHold"/"YHold" here and "NoteHold" below for D, since
        // both are called "Hold" for genuinely different things and
        // sitting three lines apart made that easy to conflate.
        gfx.setCursor(6,53); gfx.print("C:Porta  A:XHold  S:YHold");
        gfx.setCursor(6,62);
        gfx.printf("Sh+A:XEn(%s) Sh+S:YEn(%s)",
            imuXEnabled?"on":"off", imuYEnabled?"on":"off");
        // ARP before Latch (v0.9996x): Latch presupposes ARP is already
        // running, so listing it first read backwards — the owner's
        // observation, once pointed out.
        gfx.setCursor(6,71); gfx.print("D:NoteHold  Sh+V:Arp  V:Latch");
        gfx.setCursor(6,80); gfx.print("Sh+1-0:Morph  Sh+Ent:TapTempo");
        gfx.setCursor(6,89); gfx.print("Space:Seq  G0:SEQ Hold-G0:Song");
    } else {
        gfx.setCursor(6,35); gfx.print("J/N :Octave    B/M:Transpose");
        gfx.setCursor(6,44); gfx.print("k/l :Volume    Z/X:Bend");
        gfx.setCursor(6,53); gfx.print("C   :Porta     ;/.:PAD Y-axis");
        gfx.setCursor(6,62); gfx.print(",// :PAD X-axis A/S:PAD hold");
        gfx.setCursor(6,71); gfx.print("Sh+1-0:Morph  Sh+Ent:TapTempo");
        gfx.setCursor(6,80); gfx.print("Space:Seq  G0:SEQ Hold-G0:Song");
    }
    // Fixed position for both branches (v0.9996x): the ADV branch above
    // now uses one more row than the non-ADV one, so these — shared by
    // both — sit low enough to clear ADV's last line (89+8=97). Non-ADV
    // just gets a slightly bigger gap above them; nothing overlaps.
    gfx.setCursor(6,98); gfx.print("Tab:Next Sh+Tab:Prev  Sh+H:Latch");
    gfx.setCursor(6,108); gfx.print("release H to close");
}

void drawPlayScreen(bool full){
    // HELP overlay: keep using the original full-size canvas — this is a
    // rare, toggle-triggered case, not worth splitting.
    if(helpVisible){
        canvas.startWrite();
        if(full){
            canvas.fillScreen(BLACK);
            canvas.setTextColor(uiColor,BLACK);
        }
        drawHelpOverlay(canvas);
        canvas.endWrite();
        canvas.pushSprite(0,0);
        return;
    }
    bool forceFullBoth=full; // caller already folds in "help just closed" via modeChanged||forceFullRedraw

    // ---- TOP (y=0-54): tab bar + waveform. Only pushed when the tab bar
    // needs a full redraw or the waveform shape actually changed — most
    // of the time nothing is modulating Timbre/PWM, so this skips a
    // meaningful chunk (26KB) of the old single 63KB transfer entirely.
    // The display value is smoothed so that modulation (IMU tilt, an LFO
    // on Timbre or Shape) animates rather than jumping frame to frame.
    // That is right while modulation is moving the value, but wrong when
    // the user simply edited Timbre in VCO and came back: the preview then
    // crawled from the OLD waveform to the new one, showing a shape that
    // is not what the synth is set to. Editing is a discrete change, so
    // snap to it (v0.9903) and keep the smoothing for continuous motion.
    // 0.05 is far larger than modulation moves between two frames but far
    // smaller than any menu step, which is what separates the two cases.
    static float dispTimbreMorph=lastModMorph, dispOscShape=lastModShape;
    if(forceFullBoth||fabsf(lastModMorph-dispTimbreMorph)>0.05f)dispTimbreMorph=lastModMorph;
    else dispTimbreMorph+=(lastModMorph-dispTimbreMorph)*0.15f;
    if(forceFullBoth||fabsf(lastModShape-dispOscShape)>0.05f)dispOscShape=lastModShape;
    else dispOscShape+=(lastModShape-dispOscShape)*0.15f;
    static float lastDrawnMorph=-999.f, lastDrawnShape=-999.f;
    // Arpeggiator state was only visible in the help overlay, so you had to
    // press a key to find out whether it was on (v0.9921). It shows in the
    // corner of the waveform area now. Included in the dirty test so a
    // change redraws immediately rather than waiting for the waveform to
    // move — with a static patch that could otherwise be a long wait.
    static bool lastDrawnArp=false,lastDrawnLatch=false;
    bool topDirty=forceFullBoth||fabsf(dispTimbreMorph-lastDrawnMorph)>0.004f||fabsf(dispOscShape-lastDrawnShape)>0.002f
                  ||arpEnabled!=lastDrawnArp||arpLatchEnabled!=lastDrawnLatch;
    if(topDirty){
        canvasTop.startWrite();
        if(forceFullBoth){
            canvasTop.fillScreen(BLACK);
            canvasTop.setTextColor(uiColor,BLACK);
            drawTabBar(canvasTop,AppMode::PLAY);
        }
        drawWaveform(canvasTop,dispTimbreMorph,dispOscShape);
        // Drawn after the waveform so it is never overpainted. Top-right of
        // the graph area, which the trace rarely reaches and where it does
        // not compete with anything else on this screen.
        if(arpEnabled){
            const char *tag=arpLatchEnabled?"ARP L":"ARP";
            int w=(int)strlen(tag)*6;
            canvasTop.fillRect(240-w-6,14,w+4,10,BLACK);
            canvasTop.setTextColor(uiColor,BLACK);
            canvasTop.setCursor(240-w-4,15);
            canvasTop.print(tag);
        }
        lastDrawnArp=arpEnabled;lastDrawnLatch=arpLatchEnabled;
        lastDrawnMorph=dispTimbreMorph;lastDrawnShape=dispOscShape;
        canvasTop.endWrite();
        canvasTop.pushSprite(0,0);
    }

    // ---- NAME (x=0-73, y=55-112): note info, O/T, P/H. Always pushed
    // when this function runs — this is literally what changes on every
    // note keypress, so there's no meaningful dirty-check to skip it with.
    canvasName.startWrite();
    if(forceFullBoth){
        canvasName.fillScreen(BLACK);
        canvasName.setTextColor(uiColor,BLACK);
        canvasName.drawFastHLine(0,55-BOTTOM_Y_OFFSET,74,uiColor); // this canvas's own segment of the waveform/info divider
    }
    canvasName.fillRect(0,56-BOTTOM_Y_OFFSET,73,57,BLACK);
    canvasName.drawRect(0,56-BOTTOM_Y_OFFSET,73,57,uiColor);
    if(arpEnabled&&arpHeldCount>0){
        // Arp is running: list every held note (press order), small text,
        // highlighting whichever one is currently sounding.
        canvasName.setTextSize(1);
        int x=4,y=60-BOTTOM_Y_OFFSET;
        for(int i=0;i<arpHeldCount;i++){
            char nm[8];
            snprintf(nm,sizeof(nm),"%s",getNoteName(arpHeldFreqs[i]));
            bool isCurrent=fabsf(arpHeldFreqs[i]-arpLastTriggeredFreq)<0.5f;
            int w=(int)strlen(nm)*6+3;
            if(x+w>70){x=4;y+=9;}
            // Capped at two rows rather than three (v0.99936), to
            // guarantee a free row below the note list for Swing to live
            // on its own line — a large chord or a heavily latched ARP
            // could otherwise grow into a third row and collide with it,
            // the same way the fixed Octave/Transpose line already did
            // once. Two rows covers ordinary playing comfortably; a very
            // large held chord simply stops listing further notes rather
            // than risking that collision again.
            if(y>71-BOTTOM_Y_OFFSET)break; // out of room in this box
            if(isCurrent){
                canvasName.fillRect(x-1,y-1,w,9,uiColor);
                canvasName.setTextColor(BLACK,uiColor);
            } else {
                canvasName.setTextColor(uiColor,BLACK);
            }
            canvasName.setCursor(x,y);
            canvasName.print(nm);
            x+=w;
        }
        canvasName.setTextColor(uiColor,BLACK);
        // Tempo and rate on the line the frequency uses when the arp is
        // off (v0.99891). With the arp running that line is unused, and
        // tempo is the thing you most want to see while it is — the note
        // list above already says WHAT is playing, so the useful missing
        // information is how fast. Also shows an external clock's tempo
        // when one is driving, which is the only place that value is
        // visible outside the MIDI menu.
        bool ext=(midiClockEnabled&&midiClockLocked);
        float shownBpm=constrain((ext?midiClockBpm:arpTempoBpm)+arpTempoOffset,40.f,240.f);
        // y=23/32 (v0.99938, corrected from 78/84): every other row in
        // this box — the note list, O:+0/T:+0, P:off/H:off — is spaced
        // 9px apart, and these two were only 6px apart, close enough for
        // descenders to visibly touch on the real screen even though nothing
        // was technically out of bounds. The note list is capped to two
        // rows (v0.99936) specifically so y=23 is free; y=32 follows it at
        // the same 9px used everywhere else, landing exactly on the O:+0
        // line at y=40 with no gap wasted.
        // This box's convention is "absolute pixel row minus
        // BOTTOM_Y_OFFSET" everywhere else (e.g. O:+0 at 95, P:off at
        // 104) — kept the same here rather than writing a raw local
        // number, so this line's position stays correct if
        // BOTTOM_Y_OFFSET is ever changed. 78 -> local 23, immediately
        // after the note list's two guaranteed rows.
        canvasName.setCursor(4,78-BOTTOM_Y_OFFSET);
        // The '~' external-clock marker only prints when it applies now,
        // rather than always reserving its column as a literal space —
        // that space was pushing this line one character right of every
        // other row in the box, which is what looked like poor left
        // alignment in the photo.
        if(ext)canvasName.print('~');
        canvasName.printf("%.0f %-4s",shownBpm,ARP_RATES[arpRateIndex].label);
        float shownSwing=constrain(arpSwing+arpSwingOffset,-100.f,100.f);
        // 87 -> local 32, exactly 9px below Tempo/Rate — the same spacing
        // as every other row in this box, landing flush against O:+0 at
        // local 40 (95-BOTTOM_Y_OFFSET) with no gap wasted.
        canvasName.setCursor(4,87-BOTTOM_Y_OFFSET);
        canvasName.printf("Sw%+.0f%%",shownSwing);
    } else {
        float df=playingFreq>0?playingFreq:currentFreq;
        canvasName.setTextSize(2);
        canvasName.setCursor(4,60-BOTTOM_Y_OFFSET);
        canvasName.printf("%-4s",getNoteName(df));
        canvasName.setTextSize(1);
        canvasName.setCursor(4,84-BOTTOM_Y_OFFSET);
        if(df>0)canvasName.printf("%-9s",(String(df,1)+"Hz").c_str());
        else    canvasName.print("---      ");
    }
    canvasName.setTextSize(1);
    canvasName.setCursor(4,95-BOTTOM_Y_OFFSET);
    canvasName.printf("O:%+d T:%+d %c",params.octaveShift,transposeSemitones,playMode==PlayMode::PRO?'P':'E');
    canvasName.setCursor(4,104-BOTTOM_Y_OFFSET);
    canvasName.printf("P:%-3s H:%-3s",portaEnabled?"ON":"off",noteHeld?"ON":"off");
    canvasName.drawFastHLine(0,112-BOTTOM_Y_OFFSET,73,uiColor); // note block's own bottom border
    canvasName.endWrite();
    canvasName.pushSprite(0,BOTTOM_Y_OFFSET);

    // ---- IMU (x=73-240, y=55-112): bend meter, IMU pad+readout, volume.
    // Only pushed when something in it actually changed — this is the
    // main win: playing notes with IMU=None (and Bend/Volume untouched)
    // skips this ~27KB region entirely.
    static float lastPushedBend=-9999.f, lastPushedVol=-9999.f;
    static float lastPushedImuX=-9999.f, lastPushedImuY=-9999.f;
    static float lastPushedAccelX=-9999.f, lastPushedAccelY=-9999.f;
    float curBend=params.pitchBendCents+keyBendCurrent;
    float curVol=params.keyVolume;
    float curImuX=(imuAxisX.target!=ImuTarget::NONE)?getImuNorm(imuAxisX.target):0.f;
    float curImuY=(imuAxisY.target!=ImuTarget::NONE)?getImuNorm(imuAxisY.target):0.f;
    // The pad DOT's position tracks raw tilt (or the virtual PAD axes on
    // original Cardputer) directly, independent of whether any IMU target
    // is even assigned — curImuX/Y above don't capture this at all (they
    // stay 0 with target=None), so without also checking the raw input,
    // the dot only ever moved when something ELSE (bend/volume/a target
    // value) also happened to change, freezing otherwise.
    float curAccelX=isCardputerAdv?lastAccelX:padVirtualX;
    float curAccelY=isCardputerAdv?lastAccelY:padVirtualY;
    bool imuDirty=forceFullBoth
        ||fabsf(curBend-lastPushedBend)>1.0f
        ||fabsf(curVol-lastPushedVol)>0.001f
        ||fabsf(curImuX-lastPushedImuX)>0.01f
        ||fabsf(curImuY-lastPushedImuY)>0.01f
        ||fabsf(curAccelX-lastPushedAccelX)>0.01f
        ||fabsf(curAccelY-lastPushedAccelY)>0.01f;
    if(imuDirty){
        canvasImu.startWrite();
        if(forceFullBoth){
            canvasImu.fillScreen(BLACK);
            canvasImu.setTextColor(uiColor,BLACK);
            canvasImu.drawFastHLine(0,55-BOTTOM_Y_OFFSET,167,uiColor); // this canvas's own segment of the waveform/info divider
        }
        drawBendMeter(canvasImu,curBend,keyBendMaxCents,-BOTTOM_Y_OFFSET,-IMU_X_OFFSET);
        drawImuPad(canvasImu,-BOTTOM_Y_OFFSET,-IMU_X_OFFSET);

        constexpr int TX=152-IMU_X_OFFSET,TW=88;
        canvasImu.fillRect(TX,56-BOTTOM_Y_OFFSET,TW,57,BLACK);
        canvasImu.setCursor(TX,57-BOTTOM_Y_OFFSET);
        canvasImu.printf("X:%-10s",imuTargetName(imuAxisX.target));
        canvasImu.setCursor(TX,66-BOTTOM_Y_OFFSET);
        {
            String xVal=getImuValStr(imuAxisX.target);
            if(!imuXEnabled)xVal="-- OFF --"; else if(imuXHeld)xVal+="(HOLD)";
            canvasImu.printf("%-12s",xVal.c_str());
        }
        // An axis that is off shows an empty track (v0.9934). getImuNorm()
        // returns each target's CURRENT value, and where zero sits differs
        // per target — Shape's neutral is the middle of its range, the
        // Bit-crusher's is the bottom — so a disabled axis was drawing a
        // half-full bar for one target and an empty one for another. That
        // was never a decision, just what the maths happened to give.
        if(imuXEnabled&&imuAxisX.target!=ImuTarget::NONE){
            canvasImu.fillRect(TX,75-BOTTOM_Y_OFFSET,TW-2,4,canvasImu.color565(0,64,0));
            if((int)(curImuX*(TW-4))>0)canvasImu.fillRect(TX,75-BOTTOM_Y_OFFSET,(int)(curImuX*(TW-4)),4,uiColor);
        }
        canvasImu.setCursor(TX,81-BOTTOM_Y_OFFSET);
        canvasImu.printf("Y:%-10s",imuTargetName(imuAxisY.target));
        canvasImu.setCursor(TX,90-BOTTOM_Y_OFFSET);
        {
            String yVal=getImuValStr(imuAxisY.target);
            if(!imuYEnabled)yVal="-- OFF --"; else if(imuYHeld)yVal+="(HOLD)";
            canvasImu.printf("%-12s",yVal.c_str());
        }
        if(imuYEnabled&&imuAxisY.target!=ImuTarget::NONE){
            canvasImu.fillRect(TX,99-BOTTOM_Y_OFFSET,TW-2,4,canvasImu.color565(0,64,0));
            if((int)(curImuY*(TW-4))>0)canvasImu.fillRect(TX,99-BOTTOM_Y_OFFSET,(int)(curImuY*(TW-4)),4,uiColor);
        }
        canvasImu.setCursor(TX,105-BOTTOM_Y_OFFSET);
        canvasImu.printf("VOL:%d%% BND:%dst  ",
            (int)(params.keyVolume*100),(int)(keyBendMaxCents/100));

        canvasImu.endWrite();
        canvasImu.pushSprite(IMU_X_OFFSET,BOTTOM_Y_OFFSET);
        lastPushedBend=curBend;lastPushedVol=curVol;lastPushedImuX=curImuX;lastPushedImuY=curImuY;
        lastPushedAccelX=curAccelX;lastPushedAccelY=curAccelY;
    }

    // ---- NAV (x=0-240, y=113-134): full-width nav divider, scale name,
    // nav text. Only redrawn/pushed on forceFullBoth — this rarely changes.
    if(forceFullBoth){
        canvasNav.startWrite();
        canvasNav.fillScreen(BLACK);
        canvasNav.setTextColor(uiColor,BLACK);
        canvasNav.drawFastHLine(0,0,240,uiColor); // full-width nav divider (absolute y=113)
        if(playMode==PlayMode::PRO){
            // Chromatic is the sole member of the Chromatic category, so
            // the usual "Category: Name" read out as "Chromatic:
            // Chromatic" (v0.9924). Any category whose name matches the
            // scale's is printed once.
            const char *cat=SCALE_CATEGORY_NAMES[SCALES[currentScaleIndex].category];
            const char *nm =SCALES[currentScaleIndex].name;
            char scaleBuf[40];
            if(strcmp(cat,nm)==0)snprintf(scaleBuf,sizeof(scaleBuf),"%s",nm);
            else                 snprintf(scaleBuf,sizeof(scaleBuf),"%s: %s",cat,nm);
            canvasNav.setCursor((240-(int)strlen(scaleBuf)*6)/2,115-NAV_Y_OFFSET);
            canvasNav.print(scaleBuf);
        }
        const char *nav="Tab:MENU  H:HELP  G0:SEQ mode";
        canvasNav.setCursor((240-(int)strlen(nav)*6)/2,124-NAV_Y_OFFSET);
        canvasNav.print(nav);
        canvasNav.endWrite();
        canvasNav.pushSprite(0,NAV_Y_OFFSET);
    }
}

void drawSeqScreen(bool full){
    canvas.startWrite();
    if(full){
        canvas.fillScreen(BLACK);
        canvas.setTextColor(uiColor,BLACK);
        drawTabBar(canvas,AppMode::SEQ);
        if(playMode==PlayMode::PRO){
            // Chromatic is the sole member of the Chromatic category, so
            // the usual "Category: Name" read out as "Chromatic:
            // Chromatic" (v0.9924). Any category whose name matches the
            // scale's is printed once.
            const char *cat=SCALE_CATEGORY_NAMES[SCALES[currentScaleIndex].category];
            const char *nm =SCALES[currentScaleIndex].name;
            char scaleBuf[40];
            if(strcmp(cat,nm)==0)snprintf(scaleBuf,sizeof(scaleBuf),"%s",nm);
            else                 snprintf(scaleBuf,sizeof(scaleBuf),"%s: %s",cat,nm);
            canvas.setCursor((240-(int)strlen(scaleBuf)*6)/2,115);
            canvas.print(scaleBuf);
        }
    }

    if(helpVisible){
        drawHelpOverlay(canvas);
        canvas.endWrite();
        canvas.pushSprite(0,0);
        return;
    }

    // Step grid, where PLAY would show the waveform (SEQ has no waveform).
    // Velocity is shown as a bottom-aligned bar (taller = louder). Tied
    // runs of steps merge into one shape (thick outer border, no internal
    // vertical border at the join) so it reads as "one long note", while
    // each step's own bar segment stays visible (drawn using the run's
    // starting note's velocity) so the playhead/cursor can still pick out
    // individual steps within the run. Accent = pointed/triangle top
    // instead of the usual flat top. Slide = small diagonal notch at the
    // bottom-left corner, showing the pitch "sliding in" from the left.
    canvas.fillRect(0,12,240,43,BLACK);
    constexpr int GY=15,GH=36,STEP_W=15,BOX_W=13;

    // Pass 1: figure out which steps are part of a "sounding" run (a
    // note-on step followed by zero or more ties extending it), what
    // velocity each should display (the run's own starting velocity),
    // and whether the run is accented (also from its starting step).
    int runStart[SEQ_NUM_STEPS];
    uint8_t effVel[SEQ_NUM_STEPS];
    bool effAccent[SEQ_NUM_STEPS];
    {
        int curStart=-1; uint8_t curVel=100; bool curAccent=false;
        for(int i=0;i<SEQ_NUM_STEPS;i++){
            SeqStep &s=seqSteps[i];
            if(s.freq>0.f){ curStart=i; curVel=s.velocity; curAccent=s.accent; runStart[i]=i; effVel[i]=curVel; effAccent[i]=curAccent; }
            else if(s.tie&&curStart>=0){ runStart[i]=curStart; effVel[i]=curVel; effAccent[i]=curAccent; }
            else { runStart[i]=-1; effVel[i]=0; effAccent[i]=false; curStart=-1; }
        }
    }

    // Built once per draw rather than per step; color565() is a call, and
    // a function-local static initialised inside the loop would be worse.
    const uint16_t seqBeatColors[4]={
        // Primary red / orange / green / blue, from a palette the user
        // picked (v0.9928). Four clearly separated hues rather than the
        // 808's warm gradient, which suits a small backlit panel better —
        // the yellow that sat here before was too close to the near-white
        // fill to read across it.
        canvas.color565(233,  7,  7),   // beat 1 — red
        canvas.color565(255,125,  0),   // beat 2 — orange
        canvas.color565( 49,212, 28),   // beat 3 — green
        // Lightened from the source palette's 1005EB. That blue is very
        // dark, and a dark outline on a black background is close to
        // invisible on this display — the hue is kept, the luminance is
        // raised until it actually reads.
        canvas.color565( 60, 80,255),   // beat 4 — blue
    };
    for(int i=0;i<SEQ_NUM_STEPS;i++){
        int x=i*STEP_W;
        SeqStep &gs=seqSteps[i];
        bool isCursor=(i==seqCursorStep);
        bool isPlayhead=(seqPlaying&&i==seqPlayStep);
        bool inRun=(runStart[i]>=0);
        bool joinLeft=inRun&&i>0&&runStart[i-1]==runStart[i];
        bool joinRight=inRun&&i<SEQ_NUM_STEPS-1&&runStart[i+1]==runStart[i];
        int thick=inRun?2:1;
        // The cursor outline stays white even though the fill is now
        // near-white: the fill is inset by `thick`, so there is always a
        // black gap between the two and the outline still reads (v0.9927).
        // TR-808 bar colouring (v0.9924). The 808 tinted its sixteen step
        // buttons in groups of four — red, orange, yellow, cream — so you
        // could see at a glance which beat of the bar you were on. The 909
        // dropped it, but it is genuinely useful and costs nothing here.
        // Applied to the step OUTLINE only: the fill still shows velocity
        // and accent, and the cursor and playhead still take priority, so
        // nothing that already carried meaning is displaced.
        //
        // Note these are 4 groups of 4 sixteenth notes, which is one beat
        // each rather than one bar — matching the 808's own layout, where
        // the sixteen buttons are a single bar.
        uint16_t beatColor=seqBeatColors[(i/4)&3];
        uint16_t lineColor=isCursor?WHITE:beatColor;
        // Dimmed fill (v0.9925). The velocity bar used the same bright
        // orange as the beat outlines, so once steps had notes in them the
        // outlines vanished into the fill — the colours were simply too
        // close in both hue and brightness. Nothing is lost by darkening
        // the fill: bar HEIGHT carries velocity and the accent bar stays a
        // different hue, so both still read, and the beat colours now sit
        // clearly on top. The playhead stays full white so it still wins
        // over everything.
        uint16_t barColor=isPlayhead?seqBarPlayhead:(effAccent[i]?seqBarAccent:seqBarNormal);
        int bx=x+1,by=GY,bw=BOX_W,bh=GH;

        canvas.fillRect(bx,by,bw,bh,BLACK);
        if(inRun){
            // Inset by the outline thickness (v0.9925) so the fill never
            // touches the beat-coloured border. A one-pixel black gap does
            // as much for legibility as the dimming does, and it keeps the
            // outline a clean unbroken rectangle.
            int ix=bx+thick,iw=bw-thick*2;
            int ih=bh-thick*2;
            if(iw>0&&ih>0){
                int barH=max(1,(int)(ih*(effVel[i]/100.0f)));
                canvas.fillRect(ix,by+thick+ih-barH,iw,barH,barColor);
            }
        }

        for(int t=0;t<thick;t++){
            canvas.drawFastHLine(bx,by+t,bw,lineColor);
            canvas.drawFastHLine(bx,by+bh-1-t,bw,lineColor);
        }
        if(!joinLeft) for(int t=0;t<thick;t++)canvas.drawFastVLine(bx+t,by,bh,lineColor);
        if(!joinRight)for(int t=0;t<thick;t++)canvas.drawFastVLine(bx+bw-1-t,by,bh,lineColor);

        if(gs.slide){
            canvas.drawLine(bx,by+bh-1,bx+5,by+bh-6,WHITE);
            canvas.drawLine(bx,by+bh-2,bx+5,by+bh-7,WHITE);
        }
        // Selection: a small yellow strip above the box, for steps within
        // the current Copy/Cut range (marking or confirmed alike).
        if(seqSelStart>=0&&i>=seqSelStart&&i<=seqSelEnd){
            canvas.fillRect(bx,by-3,bw,2,canvas.color565(255,255,0));
        }
        // Cursor: small inset outline, drawn last so it stays visible even
        // on a step whose own left/right border was skipped (tie-joined).
        if(isCursor)canvas.drawRect(bx+1,by+1,bw-2,bh-2,WHITE);
    }
    canvas.drawFastHLine(0,55,240,uiColor);

    // Left block (matches PLAY's note-info box position/size): everything
    // is always visible — step/note, Velocity+flags, Tempo+Swing,
    // Octave+Transpose, Portamento+Hold (matching PLAY's own "O:/T:" and
    // "P:/H:" lines exactly, since both are active in SEQ too), and a
    // single "Ed:" indicator showing which value ,/. currently adjusts,
    // plus Play state.
    canvas.fillRect(0,56,73,57,BLACK);
    canvas.drawRect(0,56,73,57,uiColor);
    SeqStep &st=seqSteps[seqCursorStep];
    canvas.setCursor(4,58);
    canvas.printf("S%2d %-4s",seqCursorStep+1,getNoteName(st.freq));
    canvas.setCursor(4,67);
    canvas.printf("V%3d%% %s%s%s",st.velocity,
        st.tie?"T":" ",st.slide?"S":" ",st.accent?"A":" ");
    canvas.setCursor(4,76);
    canvas.printf("B%3.0f S%+3.0f%%",seqTempoBpm,seqSwing);
    canvas.setCursor(4,85);
    canvas.printf("O:%+d T:%+d",params.octaveShift,transposeSemitones);
    canvas.setCursor(4,94);
    canvas.printf("P:%-3s H:%-3s",portaEnabled?"ON":"off",noteHeld?"ON":"off");
    canvas.setCursor(4,103);
    const char *editLabel=(seqFocus==SeqFocus::STEP)
        ?(seqStepTarget==SeqStepTarget::VELOCITY?"Vel":seqStepTarget==SeqStepTarget::TIE?"Tie":seqStepTarget==SeqStepTarget::SLIDE?"Sld":"Acc")
        :(seqPatternTarget==SeqPatternTarget::TEMPO?"Bpm":"Swg");
    canvas.printf("Ed:%-3s %s",editLabel,seqPlaying?"PLAY":"STOP");

    // Right block: IMU pad, reused directly from PLAY — same targets,
    // same physical tilt/PAD input, works identically here.
    drawImuPad(canvas);
    drawBendMeter(canvas,params.pitchBendCents+keyBendCurrent,keyBendMaxCents);
    constexpr int TX=152,TW=88;
    canvas.fillRect(TX,56,TW,57,BLACK);
    canvas.setCursor(TX,57);
    canvas.printf("X:%-10s",imuTargetName(imuAxisX.target));
    canvas.setCursor(TX,66);
    {
        String xVal=getImuValStr(imuAxisX.target);
        if(!imuXEnabled)xVal="-- OFF --"; else if(imuXHeld)xVal+="(HOLD)";
        canvas.printf("%-12s",xVal.c_str());
    }
    if(imuXEnabled&&imuAxisX.target!=ImuTarget::NONE){   // v0.9934: empty when off
        static float seqDispNormX=0.f;
        float n=constrain(getImuNorm(imuAxisX.target),0.f,1.f);
        seqDispNormX+=(n-seqDispNormX)*0.3f;
        canvas.fillRect(TX,75,TW-2,4,canvas.color565(0,64,0));
        if((int)(seqDispNormX*(TW-4))>0)canvas.fillRect(TX,75,(int)(seqDispNormX*(TW-4)),4,uiColor);
    }
    canvas.setCursor(TX,81);
    canvas.printf("Y:%-10s",imuTargetName(imuAxisY.target));
    canvas.setCursor(TX,90);
    {
        String yVal=getImuValStr(imuAxisY.target);
        if(!imuYEnabled)yVal="-- OFF --"; else if(imuYHeld)yVal+="(HOLD)";
        canvas.printf("%-12s",yVal.c_str());
    }
    if(imuYEnabled&&imuAxisY.target!=ImuTarget::NONE){   // v0.9934: empty when off
        static float seqDispNormY=0.f;
        float n=constrain(getImuNorm(imuAxisY.target),0.f,1.f);
        seqDispNormY+=(n-seqDispNormY)*0.3f;
        canvas.fillRect(TX,99,TW-2,4,canvas.color565(0,64,0));
        if((int)(seqDispNormY*(TW-4))>0)canvas.fillRect(TX,99,(int)(seqDispNormY*(TW-4)),4,uiColor);
    }
    canvas.setCursor(TX,105);
    canvas.printf("VOL:%d%%  ",(int)(params.keyVolume*100));

    canvas.drawFastHLine(0,112,73,uiColor);
    canvas.drawFastHLine(0,113,240,uiColor);
    const char *nav="Tab:MENU  H:HELP  G0:PLAY mode";
    canvas.setCursor((240-(int)strlen(nav)*6)/2,124);
    canvas.print(nav);

    canvas.endWrite();
    canvas.pushSprite(0,0);
}

// ==========================================================
// setup / loop
// ==========================================================
void setup(){
#if CPS_USB_MIDI
    // Before Serial.begin(): with TinyUSB the CDC interface and the MIDI
    // interface are enumerated together as one composite device, so the
    // MIDI interface has to exist before the host is told what this board
    // is (v0.9961). Naming it here is what makes it show up as "C.P.S."
    // in a host's MIDI device list rather than a generic descriptor.
    usbMidi.setStringDescriptor("C.P.S.");
    usbMidi.begin();
#endif
    Serial.begin(921600);
    randomSeed(esp_random());
    auto cfg=M5.config();
    M5Cardputer.begin(cfg,true);
    isCardputerAdv = (M5.getBoard() == m5::board_t::board_M5CardputerADV);
    refreshSettingItems();
    Serial.printf("[Board] %s\n", isCardputerAdv?"CardputerADV":"Cardputer (original)");
    M5Cardputer.Display.setRotation(1);
    // The three mode accents come from the theme now (v0.9936); this call
    // replaces the individual assignments that used to sit here.
    applyUiTheme();
    // Brightness applied here too, not only from the settings parser: with
    // no card present, or a settings file predating this key, nothing
    // would otherwise push the default to the panel (v0.9937).
    applyUiBrightness();
    // Serial MIDI is cheap enough to start unconditionally: one UART, no
    // allocation worth the name, and nothing happens if no unit is plugged
    // in (v0.998).
    midiSerialBegin();
    seqAccentNoteColor = M5Cardputer.Display.color565(220,30,30); // red — accented steps' velocity bar
    // Darkened versions used only for the step velocity bars (v0.9925), so
    // the TR-808 beat outlines stay legible over a filled step. Text,
    // borders and every other use of the accent colour are untouched —
    // dimming the whole SEQ theme would have made the readouts harder to
    // read to fix a problem that only exists inside the step boxes.
    // Darkening the orange straight down took it through brown — 120,66,0
    // is literally a dark brown, and on the panel it read as muddy rather
    // than dim (v0.9926). Pulling the hue toward amber/gold and keeping
    // saturation high instead gives the same drop in brightness against
    // the beat outlines while still looking like a colour rather than
    // dirt. The accent bar moves to a deep pink for the same reason: dark
    // red went red-brown, and pink stays clearly a different hue from the
    // red beat-1 outline above it.
    // The bar is near-white now (v0.9927). Every attempt at a dimmed
    // ORANGE fill stayed in the same hue family as the beat outlines —
    // beat 1 and beat 2 in particular — so the border kept sinking into
    // it. A neutral has no hue to collide with, so it separates from all
    // four beat colours equally, and it is the brightest thing available,
    // which suits the element that should read first.
    //
    // That forces the other two fill colours to move, since white was
    // already taken. Accent goes violet: it is the one direction no beat
    // colour occupies (they run red -> orange -> lime -> cyan), so it can
    // never be confused with an outline. The playhead goes green for the
    // same reason, and it is still the most conspicuous thing on screen
    // because it is the only part that moves.
    // Back to white for the ordinary fill (v0.993), on preference: white
    // bars simply look better on the panel than grey ones.
    //
    // The cost is real and worth stating, since this may want reverting.
    // White was doing the work of marking the playhead, and brightness is
    // the strongest signal there is — a white bar moving through grey ones
    // is unmissable. With the fill white, the playhead has to be marked by
    // HUE instead, which is weaker, and weakest of all on a low-velocity
    // step where the bar is only a few pixels tall. Purple is the pick
    // because the beat outlines leave exactly two usable gaps in the hue
    // circle and teal already has the other one.
    //
    // The cursor outline stays white and still reads over a white fill,
    // because the fill is inset by the outline thickness and there is
    // always a black gap between them (established in v0.9927).
    seqBarNormal   = M5Cardputer.Display.color565(240,240,240);  // white
    // Teal, replacing the magenta (v0.9929). The choice is more
    // constrained than it looks: the four beat outlines occupy roughly
    // 0 deg (red), 30 (orange), 110 (green) and 235 (blue), which leaves
    // exactly two gaps wide enough to be unambiguous — around 175 (cyan /
    // teal) and around 300 (purple / magenta). Teal is the better of the
    // two: it sits ~60 deg from both of its neighbours, where purple is
    // only ~50 from blue and would be the fill sitting INSIDE a blue
    // outline. Fully saturated rather than lightened, so it reads as a
    // strong colour rather than a pastel, and its hue separates it from
    // the white playhead even though both are bright.
    seqBarAccent   = M5Cardputer.Display.color565(  0,190,205);  // teal
    seqBarPlayhead = M5Cardputer.Display.color565(170, 40,255);  // purple
    // Bank A-H timeline colors: a spread across the color wheel so each
    // bank reads as visually distinct at a glance.
    songBankColors[0]=M5Cardputer.Display.color565(220,60,60);   // A red
    songBankColors[1]=M5Cardputer.Display.color565(230,140,40);  // B orange
    songBankColors[2]=M5Cardputer.Display.color565(220,210,50);  // C yellow
    songBankColors[3]=M5Cardputer.Display.color565(90,210,90);   // D green
    songBankColors[4]=M5Cardputer.Display.color565(60,190,190);  // E teal
    songBankColors[5]=M5Cardputer.Display.color565(80,130,230);  // F blue
    songBankColors[6]=M5Cardputer.Display.color565(160,90,220);  // G purple
    songBankColors[7]=M5Cardputer.Display.color565(220,90,170);  // H pink
    M5Cardputer.Display.setTextColor(uiColor,BLACK);
    M5Cardputer.Display.setTextSize(1);
    drawSplashBootText();

    canvas.setColorDepth(16);
    // PSRAM: tried internal SRAM as an experiment (v0.9363) to rule out
    // PSRAM-bus contention as the cause of an audible crackle, but it
    // made no difference — the crackle correlated with redraw FREQUENCY
    // during rapid key input, not where the canvas lives (see the
    // MIN_REDRAW_MS throttle below, added to address the real cause).
    // Back to PSRAM so internal SRAM isn't needlessly spent on this.
    canvas.setPsram(true);
    if(!canvas.createSprite(240,135)){
        Serial.println("[Canvas] createSprite FAILED — PLAY/SEQ will show a blank screen until this is fixed");
    }
    canvas.setTextSize(1);

    canvasTop.setColorDepth(16);
    canvasTop.setPsram(true);
    if(!canvasTop.createSprite(240,55)){
        Serial.println("[CanvasTop] createSprite FAILED — PLAY's top region will be blank until this is fixed");
    }
    canvasTop.setTextSize(1);

    canvasName.setColorDepth(16);
    canvasName.setPsram(true);
    if(!canvasName.createSprite(74,58)){ // x=0-73, y=55-112
        Serial.println("[CanvasName] createSprite FAILED — PLAY's note-name region will be blank until this is fixed");
    }
    canvasName.setTextSize(1);

    canvasImu.setColorDepth(16);
    canvasImu.setPsram(true);
    if(!canvasImu.createSprite(167,58)){ // x=73-240, y=55-112
        Serial.println("[CanvasImu] createSprite FAILED — PLAY's IMU/bend region will be blank until this is fixed");
    }
    canvasImu.setTextSize(1);

    canvasNav.setColorDepth(16);
    canvasNav.setPsram(true);
    if(!canvasNav.createSprite(240,22)){ // x=0-240, y=113-134
        Serial.println("[CanvasNav] createSprite FAILED — PLAY's nav/scale text will be blank until this is fixed");
    }
    canvasNav.setTextSize(1);

    buildWaveTables();
    updateSquareDuty(constrain(params.oscShape+params.oscShapeOffset,0.f,1.f));  // v0.9891
    refreshMorphTablePtrs();  // v0.991: valid before the first buffer or preview draw
    updateSquareDuty2(params.osc2Shape);  // v0.9935
    updateFilterCoefficients();

    // Allocate before the card mount, and report the heap either side of
    // it. Internal DRAM is what SD.begin() draws on, and a mount failure
    // here presents as every setting being back at its default — worth
    // being able to see the number rather than inferring it (v0.9953).
    allocMorphSlots();
    // No PSRAM figure: this hardware has none and never did (see
    // platformio.ini). What matters is internal DRAM, and how much of it
    // mounting the card costs — measured at ~27KB, from ~59KB down to
    // ~31KB, which is the real ceiling on anything wanting a big buffer.
    Serial.printf("[Mem] free heap %u, largest block %u\n",
        (unsigned)ESP.getFreeHeap(),(unsigned)ESP.getMaxAllocHeap());
    bool sdOk=initSDCard();
    Serial.printf("[Mem] after SD.begin: free heap %u\n",(unsigned)ESP.getFreeHeap());
    // Moved here (v0.99981) — right after the mount itself, before the
    // heavier ensureCpsFolder()/loadSettings()/morphLoadAllSlots() block
    // below. The previous position (after all of that) still failed to
    // allocate on real hardware; this board has no PSRAM at all (see the
    // comment on the free-heap print above), so every sprite here draws
    // from the same small internal-DRAM pool SD.begin() itself competes
    // for, and morphLoadAllSlots() reading up to 10 patch files off the
    // card was the next-likeliest thing to fragment or eat further into
    // whatever the mount left free. Creating the (now smaller) sprite as
    // early as possible after the mount gives it first claim on whatever
    // memory is available before anything else gets a chance to use it.
    drawSplashLogoAnimation();
    if(sdOk){
        ensureCpsFolder();ensurePatchFolder();loadSettings();
        // Slot snapshots are built once, at boot, so that pressing a slot
        // mid-performance never touches the card (v0.995).
        //
        // morphLoadAllSlots() does NOT need scanPatches() first — it reads
        // morphSlotPatch[i] (a stored NAME string) and checks SD.exists()
        // directly, never patchNames[]. A scanPatches() call was added
        // here anyway on the assumption it was needed, and it broke every
        // later patch load: the ESP32 SD library does not always reset a
        // directory handle's internal state cleanly on a second open of
        // the same path, so this boot-time scan silently emptied the list
        // the NEXT scanPatches() — the one that runs when Load is opened —
        // was supposed to populate. Removed (v0.9952). patchNames[] is
        // still scanned exactly when it always was: on entering the
        // Load/Save browser.
        morphLoadAllSlots();
        Serial.println("[SD] OK");
    }
    else Serial.println("[SD] not found");

    // The sensor is NOT probed at boot any more (v0.99909).
    //
    // Probing means reconfiguring an I2C peripheral, and one of the two is
    // M5's, carrying the keyboard and the IMU. Doing that unasked on every
    // startup meant a board with no sensor attached still had its bus
    // disturbed — reported as parameters drifting, the waveform changing
    // and notes sounding with nothing present. Nothing that touches shared
    // hardware should happen unless the player asked for it.
    //
    // Turning Theremin on, or pressing Rescan, probes. Restoring the
    // saved setting therefore probes too, but only for someone who had it
    // switched on when they last saved.
    if(thereminEnabled)thereminBegin();

    recomputeKeyNotes(); // build the note key frequency tables from the loaded (or default) play mode/scale

    // lastModMorph/lastModShape only get updated during active note playback
    // (see audioTask), so without this they'd sit at their compile-time
    // defaults — showing a default Sine waveform on the MAIN screen —
    // until the first note was played after boot/load.
    lastModMorph=params.timbreMorph;
    lastModShape=constrain(params.oscShape+params.oscShapeOffset,0.f,1.f);

    bool imuOk=M5.Imu.begin();
    Serial.println(imuOk?"[IMU] OK":"[IMU] not found");

    auto sc=M5Cardputer.Speaker.config();
    sc.sample_rate=SAMPLE_RATE;sc.dma_buf_count=8; // was 4 — more queued headroom so
    // occasional delays in the Speaker's own I2S-feed task (pinned to the
    // same core as the display's SPI-DMA activity, see below) don't force
    // audioTask's playRaw() call to wait as long for a free buffer slot,
    // which showed up as "over budget" buffers correlated with screen
    // redraws (e.g. a key press) — an audible click. Costs a bit more
    // fixed audio latency in exchange for resilience; worth revisiting if
    // the latency becomes noticeable.
    sc.dma_buf_len=512;sc.task_pinned_core=APP_CPU_NUM;
    M5Cardputer.Speaker.config(sc);
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(255);

    // Audio synthesis runs on Core 0 (PRO_CPU). Core 1 (APP_CPU) is left for
    // loop() (keyboard scan + display) and the Speaker's own DMA-feeding task
    // (task_pinned_core above). Both cores were previously shared between
    // loop() and this task, which could starve the watchdog under heavy
    // load (e.g. LFO active + rapid retriggering) and freeze the device.
    xTaskCreatePinnedToCore(audioTask,"audioTask",4096,nullptr,5,nullptr,PRO_CPU_NUM);

    // The one real application of the saved brightness (v0.99986) — see
    // the deferral comment on applyUiBrightness()/bootBrightnessDeferred
    // above. Everything up to this point (the splash's own fade, canvas
    // creation, wavetables, the SD mount and settings load, the speaker
    // just above) is done; this is the last line of setup(), so clearing
    // the flag here and applying now lands right as PLAY is about to
    // appear, not sometime earlier mid-boot.
    bootBrightnessDeferred=false;
    applyUiBrightness();
}

unsigned long lastDisplayMs=0;
AppMode lastDrawnMode=AppMode::SETTINGS;
bool lastImuPickerOpen=false;
bool lastImuCalibrateConfirmOpen=false;
bool lastResetConfirmOpen=false;
bool lastScalePickerOpen=false;
int  lastScalePickerLevel=0;
bool lastHelpVisible=false;

// Deferred Row-1 retrigger state (v0.99955), checked every loop() pass
// with no delay() anywhere — see the note above where this is set, in
// the keyChanged block, for what this replaced and why.
bool  pendingRow1Retrigger=false;
unsigned long pendingRow1SetMs=0;
float pendingRow1Nf=0.f;
bool  pendingRow1MidiActive=false;

// Volume, step 5%->1%, hold-to-repeat (v0.9996x) — same request as ARP/
// SEQ/SONG's Tempo/Swing, applied here too since keyVolume is read from
// the SAME two keys ('l'/'k') regardless of appMode: one fix covers
// PLAY, SEQ, and SONG together rather than needing three separate ones.
// Runs unconditionally every loop() pass, not gated by keyChanged, for
// the same reason updateSeqEditing()/updateSongEditor() do — a function
// only invoked when the key SET changes never gets called again while a
// key is simply held steady, so menuKeyFire()'s own timing logic would
// never get polled again either. This used to live inside
// updateOctaveAndVolume(), which is keyChanged-gated for everything else
// it does and stayed that way; only volume needed pulling out.
// Shift+L is "Load Song" in SONG mode, so plain 'l' has to be excluded
// there — matching the same exclusion the old inline code already had.
void updateVolumeRepeat(){
    auto s=M5Cardputer.Keyboard.keysState();
    bool vU=false,vD=false;
    for(char c:s.word){
        if(c=='l'&&!(appMode==AppMode::SONG&&s.shift))vU=true;
        if(c=='k')vD=true;
    }
    if(menuKeyFire(vU,prevVolumeUpPressed,volUpHeldMs,volUpLastMs))
        params.keyVolume=min(params.keyVolume+0.01f,1.f);
    if(menuKeyFire(vD,prevVolumeDownPressed,volDownHeldMs,volDownLastMs))
        params.keyVolume=max(params.keyVolume-0.01f,0.f);
    prevVolumeUpPressed=vU;prevVolumeDownPressed=vD;
}

// Keyboard-scan watchdog (v0.9997x) — a real, documented hardware quirk
// of the Cardputer ADV's TCA8418 I2C keypad controller: under certain I2C
// bus conditions, most reliably triggered by fast key presses, the chip
// can get stuck reporting stale data until its own RESET line is pulled,
// which M5Cardputer's library does not expose a way to do from software.
// This is not a C.P.S. bug; it is documented behaviour of the same chip
// reported independently by engineers working with it directly. Without
// a way to reset the chip itself, the best available recovery is a full
// restart of the whole board — trading "stuck until the player notices
// and manually resets" for "recovers on its own within the timeout".
//
// The condition deliberately requires the reported key state to be
// NON-EMPTY and unchanging: an idle keyboard (nothing pressed) staying
// that way for a long time is completely normal and must never trigger
// this. What it cannot distinguish is a genuinely stuck TCA8418 from a
// player deliberately holding one long, unchanging note or chord for the
// same span of real time — those look identical from here. The long
// threshold below is chosen specifically to make that collision rare,
// not to make it impossible; see the README/manual note this is meant to
// pair with.
constexpr unsigned long KB_WATCHDOG_TIMEOUT_MS=30000;
char lastKbWatchdogWord[24]={0};
unsigned long lastKbWatchdogChangeMs=0;
void updateKeyboardWatchdog(){
    static unsigned long lastCheckMs=0;
    unsigned long nowMs=millis();
    if(nowMs-lastCheckMs<1000)return;
    lastCheckMs=nowMs;
    auto s=M5Cardputer.Keyboard.keysState();
    char wbuf[24]={0}; int wi=0;
    for(char c:s.word){if(wi<20)wbuf[wi++]=c;}
    // Sorted before comparing (v0.9997x fix) — a report showed the
    // 30-second restart never firing despite arpHeldCount staying frozen
    // at the same value the whole time, which only makes sense if s.word
    // itself was being read as "changed" every single check. The
    // suspected cause: s.word's character ORDER is not guaranteed stable
    // between reads even for the exact same held key set — if one check
    // reads "3i" and the next reads "i3", a raw strcmp sees those as
    // different and keeps resetting the timer, never accumulating enough
    // consecutive unchanged time to reach the threshold. Sorting first
    // makes the comparison depend only on WHICH keys are held, not what
    // order the library happened to report them in.
    for(int i=1;i<wi;i++){char key=wbuf[i];int j=i-1;while(j>=0&&wbuf[j]>key){wbuf[j+1]=wbuf[j];j--;}wbuf[j+1]=key;}
    // The per-change and every-5s confirmation logging that used to sit
    // here (v0.9997x) is retired (UI/UX diagnostic pass) — it did its
    // job: confirmed the sort fix above actually works (a real recovery
    // was observed firing at 30017ms). Left running, it would print on
    // nearly every keypress and every 5s of any held note, which is too
    // noisy for ordinary use. Only the actual restart notice below stays
    // — genuinely rare, and worth knowing happened.
    if(strcmp(wbuf,lastKbWatchdogWord)!=0){
        strcpy(lastKbWatchdogWord,wbuf);
        lastKbWatchdogChangeMs=nowMs;
        return;
    }
    if(wbuf[0]=='\0')return;   // nothing held — an unchanging empty state is normal, not stuck
    if(nowMs-lastKbWatchdogChangeMs>=KB_WATCHDOG_TIMEOUT_MS){
        Serial.printf("[watchdog] keyboard word \"%s\" unchanged for %lums - restarting\n",
            wbuf,nowMs-lastKbWatchdogChangeMs);
        delay(50);   // give the Serial write above a moment to actually go out before reset
        ESP.restart();
    }
}

void loop(){
    // Set at the very top of every pass, before anything that could hang
    // (v0.9997x) — a hang partway through this exact iteration should
    // still leave the timestamp from the START of that iteration, proof
    // loop() reached here before whatever came next didn't finish.
    loopHeartbeatMs=millis();
    M5Cardputer.update();

    // Morph advances here rather than in audioTask: loop() runs far
    // faster than the ear needs and this keeps the audio path untouched.
    thereminUpdate();   // v0.999
    midiPoll();   // v0.9961 (USB) / v0.998 (serial)
    // IMU -> CC out (v0.99871). Driven from loop() rather than from inside
    // the IMU update, which sits above this code in the file; the throttle
    // means the call site's rate does not matter. Sent from the same
    // values the synth is using, so what leaves the wire matches what is
    // heard, and gated on the axis being live so a disabled or held axis
    // stops transmitting rather than freezing a receiver at its last
    // value.
    midiNoteOutUpdate();   // v0.99872
    midiBendOutUpdate();   // v0.99875
    midiClockOutUpdate();  // v0.99895
    midiCcOutUpdate(imuXLastNorm,imuYLastNorm,
        imuXEnabled&&!imuXHeld,imuYEnabled&&!imuYHeld);
    // MIDI's route into the arpeggiator's chord used to be a flag polled
    // here (v0.9986). Removed (v0.99931): the four places that change
    // MIDI's held notes now call rebuildArpChord() directly, which already
    // carries its own eligibility check, so there is nothing left for
    // loop() to do on their behalf.
    midiDiagTick();
    morphTick();
    // Independent of audioTask entirely, deliberately — if audioTask has
    // truly stopped (a deadlock), it can never set diagPrintPending
    // again, so piggybacking on that flag would never fire either. This
    // uses only loop()'s own clock against the heartbeat's last value
    // (v0.99944).
    {
        static unsigned long lastHeartbeatCheckMs=0;
        unsigned long nowMs=millis();
        if(nowMs-lastHeartbeatCheckMs>=1000){
            lastHeartbeatCheckMs=nowMs;
            unsigned long silentUs=micros()-audioTaskHeartbeatUs;
            // A normal buffer is on the order of tens of ms; several
            // hundred ms with no new buffer is already far outside that,
            // so 1s is a comfortable margin against false alarms from an
            // ordinarily slow buffer rather than a genuinely stopped task.
            if(silentUs>1000000UL){
                Serial.printf("[audioTask] no heartbeat for %lums - task may be stuck\n",
                    silentUs/1000);
            } else if(currentFreq>0.f&&envPhase==EnvPhase::IDLE){
                // audioTask IS alive (heartbeat is current) but the
                // envelope disagrees with what should be sounding — a
                // different class of fault than a stuck task, and this
                // distinguishes it on the next occurrence rather than
                // requiring a guess between them (v0.99944).
                Serial.printf("[audioTask] alive but envPhase=IDLE while currentFreq=%.1f\n",currentFreq);
            }
        }
    }
    if(diagPrintPending){
#if CPS_LOG_AUDIO
        Serial.printf("[audioTask] %lu buffers, avg %lu us, max %lu us, %lu over budget (budget 23220us)\n",
            diagPrintCount,diagPrintCount?diagPrintSumUs/diagPrintCount:0,diagPrintMaxUs,diagPrintOverCount);
#endif
        diagPrintPending=false;
    }
    // G0 button (BtnA): short press toggles PLAY<->SEQ (unchanged
    // behavior, just now resolved on release instead of press-down, so
    // it can be distinguished from a long press); long press (500ms)
    // enters/exits SONG mode instead. Physically separate from the
    // keyboard matrix, so it can't be accidentally triggered while
    // playing/typing.
    constexpr uint32_t G0_LONG_PRESS_MS=500;
    static bool g0LongFired=false;
    if(M5Cardputer.BtnA.isPressed()&&!g0LongFired&&M5Cardputer.BtnA.pressedFor(G0_LONG_PRESS_MS)){
        g0LongFired=true;
        if(appMode==AppMode::SONG){
            appMode=lastMainMode; // back to whichever was home
        } else {
            appMode=AppMode::SONG;
        }
    }
    if(M5Cardputer.BtnA.wasReleased()){
        if(!g0LongFired){
            // Short press: existing PLAY<->SEQ toggle, unchanged.
            lastMainMode=(lastMainMode==AppMode::PLAY)?AppMode::SEQ:AppMode::PLAY;
            refreshSettingItems();
            if(appMode==AppMode::SETTINGS||appMode==AppMode::SEQ)saveSettings();
            appMode=lastMainMode;
            currentFreq=0;
            // PLAY and SEQ are distinct modes — switching between them should
            // silence whatever was sounding rather than carrying it over.
            if(seqPlaying){seqPlaying=false;songPlaying=false;seqSliding=false;seqAccentCutoffBoostTarget=0.f;seqAccentResoBoostTarget=0.f;seqVelocityMult=1.0f;}
            arpHeldCount=0;
            arpLatchedCount=0; // Latch mode rebuilds arpHeldCount from this every update, so it must be cleared too
            midiLatchedCount=0;   // v0.9984
            noteHeld=false;heldFreq=0.f;midiPedalTookHold=false;
        }
        g0LongFired=false;
    }

    updateImu();
    updateMenuNavigation();
    updateVolumeRepeat();
    updateKeyboardWatchdog();
    if(appMode==AppMode::PATCH)updatePatchBrowser();
    if(appMode==AppMode::PATTERN)updatePatternBank();
    if(appMode==AppMode::SONG)updateSongEditor();
    if(appMode==AppMode::TIMBRE)updateTimbreScreen();
    if(appMode==AppMode::CATEGORY&&morphSlotScreenOpen)updateMorphSlotScreen();
    if(appMode==AppMode::CATEGORY&&themePickerOpen)updateThemePicker();
    if(appMode==AppMode::CATEGORY&&imuPickerOpen)updateImuPicker();
    if(appMode==AppMode::CATEGORY&&imuCalibrateConfirmOpen)updateImuCalibrateConfirm();
    if(appMode==AppMode::CATEGORY&&resetConfirmOpen)updateResetConfirm();
    if(appMode==AppMode::CATEGORY&&scalePickerOpen)updateScalePicker();
    if(appMode==AppMode::SEQ)updateSeqEditing();

    bool keyChanged=M5Cardputer.Keyboard.isChange();
    // Notes can be triggered on every screen except the Patch Bank and
    // SEQ itself (which has its own dedicated note-entry key handling).
    // Every other screen (VCO/VCF/VCA/LFO/SETTINGS/CATEGORY, including
    // all of CATEGORY's own sub-screens/overlays) only uses ;/./,// for
    // its own navigation, so there's no key conflict — and it means
    // tone/filter/LFO changes can be heard live while editing, not just
    // on PLAY.
    bool notesAllowed=(appMode!=AppMode::PATCH&&appMode!=AppMode::SEQ&&appMode!=AppMode::PATTERN&&appMode!=AppMode::SONG&&appMode!=AppMode::TIMBRE);
    if(keyChanged){
        updateOctaveAndVolume();
        if(seqPlaying){
            // Sequencer has exclusive control of currentFreq while
            // playing (see updateSeqTiming(), called unconditionally
            // below so the pattern keeps looping even on other screens)
            // — don't let normal note-triggering or Arp fight it.
        } else if(notesAllowed&&arpEnabled){
            // Arp mode: track the held chord here; updateArpTiming() below
            // (which runs every loop iteration, not just on keyChanged)
            // drives currentFreq/envelope retriggering from it. Latch's
            // own edge-detection is a separate, genuinely keyChanged-only
            // concern (v0.99931) — see updateArpLatchEdges().
            updateArpLatchEdges();
            rebuildArpChord();
        } else if(notesAllowed){
            float nf=resolveFreqFromKeys();
            // v0.99951's cross-event debounce is REVERTED here (v0.99952)
            // — it broke every ordinary keypress. keyChanged is edge-
            // triggered: it fires once when the pressed-key SET changes,
            // not repeatedly while a key is simply held. Requiring a
            // SECOND keyChanged carrying the same nf therefore had no
            // natural way to ever arrive for a normal single press — there
            // is no further state change to report while a finger just
            // sits on one key — so nfConfirmedForRetrigger was false for
            // essentially every real note, not just the Shift+digit
            // glitch it was meant to catch. Silent from boot onward was
            // the result. The real fix is below, scoped to the one
            // situation that actually needs it instead of every keypress.
            // Don't let the built-in keyboard clear a note that MIDI is
            // holding (v0.9983). resolveFreqFromKeys() returns 0 when no
            // local key is down, and this used to assign that
            // unconditionally — so any keyChanged event killed a MIDI note
            // instantly. keyChanged fires for octave, volume, Tab and
            // every other key too, which is why the symptom was notes
            // cutting off or not sounding at all, seemingly at random,
            // rather than never working.
            //
            // The local keys still win while one is actually held: whoever
            // pressed last is playing, and that matches the last-note
            // priority both inputs already use individually.
            if(nf<=0&&midiNoteActive){
                // MIDI owns the note; leave currentFreq alone.
            } else {
                // Retrigger when a local key takes over from a MIDI note
                // as well as from silence (v0.9984). Testing currentFreq==0
                // alone meant that pressing a local key while MIDI was
                // sounding changed the pitch but left the envelope mid-
                // note, which reads as the built-in keyboard being ignored.
                // Row 1 (the number row, ROW1_KEYS) doubles as Shift+1..0
                // for Morph slot select, and that specific combo has a
                // confirmed scan-timing race (v0.99950/51): the Shift edge
                // and the digit's own edge don't land in perfectly the
                // same instant, so one scan can briefly report the bare
                // digit with shift read as false.
                //
                // v0.99952's fix used delay(5) here to re-check a moment
                // later, which caused a hard crash (v0.99955): delay()
                // yields to the scheduler, and doing that synchronously
                // inside loop()'s keyChanged handling opened a scheduling
                // window that let some other, previously-latent race
                // actually fire — the crash log showed several of these
                // retriggers firing in quick succession right before a
                // Guru Meditation StoreProhibited fault. No delay() call
                // is used anywhere in this replacement.
                //
                // Instead, a Row-1 retrigger is deferred rather than
                // decided immediately: the note info is stashed, and a
                // SEPARATE check — outside this keyChanged-gated block,
                // reached on every ordinary loop() pass regardless of
                // further key events — finalises it once ~5ms have
                // genuinely elapsed, with a fresh, non-blocking keysState()
                // read at that point. loop() runs continuously many times
                // per millisecond on its own, so this needs no explicit
                // wait at all.
                bool viaRow1=false;
                {
                    // A plain keysState() read, unlike delay(), does not
                    // yield to the scheduler — only the delay() call this
                    // replaced did that.
                    auto sChk=M5Cardputer.Keyboard.keysState();
                    for(char c:sChk.word)
                        for(int i=0;i<12;i++)
                            if(ROW1_KEYS[i]==c){viaRow1=true;break;}
                }
                if(viaRow1){
                    pendingRow1Nf=nf;
                    pendingRow1MidiActive=midiNoteActive;
                    pendingRow1SetMs=millis();
                    pendingRow1Retrigger=true;
                } else if(nf>0&&(currentFreq==0||midiNoteActive)){
                    if(envPhase==EnvPhase::IDLE)envLevel=0;
                    envPhase=EnvPhase::ATTACK;
                    filterEnvPhase=EnvPhase::ATTACK;  // trigger filter envelope
                    if(portaEnabled&&portaFreq==0)portaFreq=nf;
                }
                currentFreq=nf;
            }
        } else if(appMode==AppMode::SEQ){
            // Handled by updateSeqEditing() above (note preview + auto-
            // advance) — don't reset currentFreq here, or the preview
            // note gets zeroed the instant it's set.
        } else {
            currentFreq=0;
        }
    }
    // Self-healing safety net (v0.99958, narrowed v0.9997x): currentFreq
    // nonzero with envPhase stuck IDLE means SOMETHING set a pitch
    // without the envelope ever following — originally found after rapid
    // Row-1 key presses (fast Morph-slot A/B testing), where the single-
    // slot pending mechanism below can have a NEWER press overwrite an
    // OLDER one's still-pending retrigger before its 5ms window
    // finalises, silently dropping it.
    // Excluded while arpEnabled now — a real report of one note stuck
    // sounding continuously, ARP on, no Latch, traced back to this: ARP's
    // own triggerArpStep() sets currentFreq and envPhase=ATTACK together,
    // atomically, so there is no gap there, but a gate shorter than 100%
    // deliberately leaves a silent gap BETWEEN steps — envPhase legitimately
    // reaches IDLE via its own release while currentFreq still holds the
    // note that JUST finished, since nothing zeroes it for that gap on
    // purpose. This check does not know that gap is intentional, so it
    // re-attacked the stale note immediately, every single loop() pass,
    // fighting ARP's own timing outright and getting stuck on whichever
    // note happened to be playing when this first fired. ARP already
    // owns currentFreq/envPhase completely and correctly while it runs;
    // this safety net was never meant to compete with it, only to catch
    // ordinary keyboard play's Row-1 pending-drop case, which does not
    // involve ARP at all.
    // seqPlaying excluded too, same reasoning as arpEnabled — SEQ has its
    // own "exclusive control of currentFreq while playing" (see the note
    // near its own note-triggering code), including deliberate rest gaps
    // between steps, which this check has no way to distinguish from a
    // genuine stuck note.
    if(currentFreq>0.f&&envPhase==EnvPhase::IDLE&&!arpEnabled&&!seqPlaying){
        envLevel=0;
        envPhase=EnvPhase::ATTACK;
        filterEnvPhase=EnvPhase::ATTACK;
    }
    // Finalises a deferred Row-1 retrigger (v0.99955) — runs on every
    // ordinary loop() pass, unconditionally, not gated by keyChanged, so
    // it needs no explicit wait: loop() naturally runs many times within
    // 5ms on its own. No delay() call anywhere in this path.
    if(pendingRow1Retrigger&&millis()-pendingRow1SetMs>=5){
        pendingRow1Retrigger=false;
        bool shiftNow=M5Cardputer.Keyboard.keysState().shift;
        if(!shiftNow&&pendingRow1Nf>0&&(currentFreq==0||pendingRow1MidiActive)){
            // The [retrigger] diagnostic that used to sit here (v0.99950)
            // is retired (v0.9997x, UI/UX diagnostic pass) — it did its
            // job: found the Shift+digit keyboard scan race that caused
            // it, fixed since v0.99952. Row 1 is this instrument's main
            // octave, so this fired on nearly every note played; no
            // ongoing reason to keep that up now that it is confirmed
            // fixed.
            if(envPhase==EnvPhase::IDLE)envLevel=0;
            envPhase=EnvPhase::ATTACK;
            filterEnvPhase=EnvPhase::ATTACK;
            if(portaEnabled&&portaFreq==0)portaFreq=pendingRow1Nf;
        }
    }
    if(!seqPlaying&&notesAllowed&&arpEnabled)updateArpTiming();
    // The [arp] diagnostic that used to sit here (v0.9997x) is retired
    // (UI/UX diagnostic pass) — it did its job: proved arpHeldCount and
    // the raw keyboard word both stayed frozen identically during a
    // freeze, which is what led to the actual cause (the TCA8418
    // keyboard chip, not this code) and the watchdog that now mitigates
    // it. No ongoing reason to print this every second ARP is on.
    if(seqPlaying)updateSeqTiming();

    // Computed here (after all of this frame's mode-changing input —
    // G0, Tab-cycle, SONG/PATCH/PATTERN's own Tab handling — has already
    // been processed above), not at the top of loop(), so a full redraw
    // triggered by a mode change this same frame already sees the RIGHT
    // color instead of yesterday's.
    uiColor=(appMode==AppMode::SONG)?songAccentColor:((lastMainMode==AppMode::SEQ)?seqAccentColor:playAccentColor);

    bool modeChanged=(appMode!=lastDrawnMode);
    lastDrawnMode=appMode;

    // Non-PLAY screens: only redraw on menu keys or mode change
    bool menuKey=false;
    if(appMode!=AppMode::PLAY&&keyChanged){
        auto st=M5Cardputer.Keyboard.keysState();
        for(char c:st.word)if(c==';'||c=='.'||c==','||c=='/')menuKey=true;
        if(st.tab)menuKey=true;
    }

    // Minimum interval between forced (key-triggered) pushSprite() calls —
    // rapid key repeats/menu navigation could otherwise trigger a full
    // 240x135 canvas push on nearly every loop() iteration, which showed
    // up as an audible "crackle" (occasional audioTask buffers running
    // over budget, correlated with key presses). modeChanged always
    // bypasses this so screen transitions still feel instant.
    constexpr unsigned long MIN_REDRAW_MS=30;
    unsigned long nowMs0=millis();
    bool canForceRedraw=modeChanged||(nowMs0-lastDisplayMs)>=MIN_REDRAW_MS;

    // delay(5) removed from the end of every branch below (v0.99957).
    // It was a longstanding, pre-existing pattern — present on every
    // non-PLAY screen — not something added this session, which is why
    // it took this long to implicate: two crashes (Guru Meditation,
    // StoreProhibited, EXCVADDR=0x4) both happened on the very first
    // transition away from PLAY, on a completely fresh boot, reproduced
    // via both Tab (into VCO) and G0 (into SEQ) — the one thing every
    // one of these branches has in common is this delay(5) at the end.
    // delay() yields to the FreeRTOS scheduler; something added earlier
    // this session (most plausibly the morphChain/filterParams locking
    // work) most likely introduced a race that only manifests when a
    // scheduler yield happens to land in the wrong window, and this
    // delay was simply the first place reliable enough to hit it. The
    // underlying race itself is not yet found — this removes the yield
    // point these branches share rather than the race, the same
    // mitigation that worked for the earlier Row-1 retrigger crash.
    // The actual per-screen redraw throttling (MIN_REDRAW_MS,
    // canForceRedraw, the 100ms checks) is untouched and still limits
    // how often the canvas itself gets redrawn — only the artificial
    // pause on every loop() iteration is gone.
    if(appMode==AppMode::VCO){
        unsigned long now=millis();
        if((menuKey&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawVcoScreen(modeChanged);}
        return;
    }
    if(appMode==AppMode::VCF){
        unsigned long now=millis();
        if((menuKey&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawVcfScreen(modeChanged);}
        return;
    }
    if(appMode==AppMode::VCA){
        unsigned long now=millis();
        if((menuKey&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawVcaScreen(modeChanged);}
        return;
    }
    if(appMode==AppMode::LFO){
        unsigned long now=millis();
        if((menuKey&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawLfoScreen(modeChanged);}
        return;
    }
    if(appMode==AppMode::FX){
        if(modeChanged)fxViewMode=FxViewMode::PAD_SELECTOR;
        unsigned long now=millis();
        if((menuKey&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawFxScreen(modeChanged);}
        return;
    }
    if(appMode==AppMode::SETTINGS){
        unsigned long now=millis();
        // Same as the category screen: a theme applied elsewhere has to
        // repaint this screen's tab bar when we come back to it (v0.9938).
        bool full=modeChanged||uiThemeDirty;
        if((menuKey&&canForceRedraw)||full||(now-lastDisplayMs)>=100){
            lastDisplayMs=now;
            if(full)uiThemeDirty=false;
            drawSettingsScreen(full);
        }
        return;
    }
    if(appMode==AppMode::CATEGORY){
        bool pickerChanged=(imuPickerOpen!=lastImuPickerOpen);
        lastImuPickerOpen=imuPickerOpen;
        bool calChanged=(imuCalibrateConfirmOpen!=lastImuCalibrateConfirmOpen);
        lastImuCalibrateConfirmOpen=imuCalibrateConfirmOpen;
        bool resetChanged=(resetConfirmOpen!=lastResetConfirmOpen);
        lastResetConfirmOpen=resetConfirmOpen;
        bool scaleChanged=(scalePickerOpen!=lastScalePickerOpen)||(scalePickerLevel!=lastScalePickerLevel);
        lastScalePickerOpen=scalePickerOpen;lastScalePickerLevel=scalePickerLevel;
        static bool lastMorphScreenOpen=false;
        bool morphScrChanged=(morphSlotScreenOpen!=lastMorphScreenOpen);
        lastMorphScreenOpen=morphSlotScreenOpen;
        static bool lastThemePickerOpen=false;
        bool themeChanged=(themePickerOpen!=lastThemePickerOpen);
        lastThemePickerOpen=themePickerOpen;
        // uiThemeDirty joins the full-redraw condition here too (v0.9938).
        // Applying a theme happens on this screen, and the tab bar is only
        // painted on a full redraw — so previously the accent changed
        // everywhere except the tab bar, which kept the old colour until
        // something else forced a repaint.
        bool full=modeChanged||pickerChanged||calChanged||resetChanged||scaleChanged||themeChanged||morphScrChanged||uiThemeDirty;
        unsigned long now=millis();
        bool doRedraw=full||((menuKey||(now-lastDisplayMs)>=100)&&canForceRedraw);
        if(doRedraw){
            lastDisplayMs=now;
            if(full)uiThemeDirty=false;   // consumed by this repaint
            if(morphSlotScreenOpen)drawMorphSlotScreen(full);
            else if(themePickerOpen)drawThemePickerScreen(full);
            else if(imuPickerOpen)drawImuPickerScreen(full);
            else if(imuCalibrateConfirmOpen)drawImuCalibrateConfirmScreen(full);
            else if(resetConfirmOpen)drawResetConfirmScreen(full);
            else if(scalePickerOpen)drawScalePickerScreen(full);
            else drawCategoryScreen(full);
        }
        return;
    }
    if(appMode==AppMode::PATCH){
        unsigned long now=millis();
        if((keyChanged&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawPatchScreen(modeChanged);}
        return;
    }
    if(appMode==AppMode::PATTERN){
        unsigned long now=millis();
        if((keyChanged&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawPatternBankScreen(modeChanged);}
        return;
    }
    if(appMode==AppMode::SONG){
        unsigned long now=millis();
        if((keyChanged&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawSongScreen(modeChanged);}
        return;
    }
    if(appMode==AppMode::TIMBRE){
        unsigned long now=millis();
        if((keyChanged&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){lastDisplayMs=now;drawTimbreScreen(modeChanged);}
        return;
    }
    bool helpChanged=(helpVisible!=lastHelpVisible);
    lastHelpVisible=helpVisible;
    // The overlay covers the whole screen, but PLAY and SEQ redraw through
    // several partial sprites — so closing it MUST be followed by a full
    // redraw or leftover overlay pixels stay wherever no dirty region
    // happens to cover.
    //
    // That request used to be computed fresh each frame and consumed in
    // the same frame. If the redraw was throttled that frame
    // (canForceRedraw false, MIN_REDRAW_MS not yet elapsed) it was simply
    // dropped — and lastHelpVisible had already been updated, so the next
    // frame no longer knew a full redraw was owed. The 100ms fallback then
    // redrew only the dirty regions, leaving help text sitting in the
    // waveform area or the nav line blanked. This is exactly why the
    // corruption came and went at random: it depended on where in the
    // throttle window the key release landed (v0.9922).
    //
    // Latched instead, and cleared only once a full redraw has actually
    // been issued.
    static bool pendingFullRedraw=false;
    if(helpChanged&&!helpVisible)pendingFullRedraw=true;
    // A mode change owes one for the same reason: PLAY and SEQ use
    // different accent colours and slightly different layouts, so swapping
    // between them without a full clear leaves the previous mode's colour
    // in any area the new mode's dirty regions do not touch. modeChanged
    // is only true for a single frame, so if that frame's redraw is
    // throttled the guarantee is lost — latch it here too (v0.9922).
    if(modeChanged)pendingFullRedraw=true;
    // A theme change repaints everything for the same reason (v0.9936).
    if(uiThemeDirty){pendingFullRedraw=true;uiThemeDirty=false;}
    bool forceFullRedraw=pendingFullRedraw;

    if(appMode==AppMode::SEQ){
        unsigned long now=millis();
        // pendingFullRedraw also forces the frame to happen, so a throttled
        // frame cannot swallow it a second time.
        if(((keyChanged||helpChanged||pendingFullRedraw)&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){
            lastDisplayMs=now;
            bool fullNow=modeChanged||forceFullRedraw;
            drawSeqScreen(fullNow);
            if(fullNow)pendingFullRedraw=false;
        }
        return;
    }

    // PLAY screen
    unsigned long now=millis();
    if(((keyChanged||helpChanged||pendingFullRedraw)&&canForceRedraw)||modeChanged||(now-lastDisplayMs)>=100){
        lastDisplayMs=now;
        bool fullNow=modeChanged||forceFullRedraw;
        drawPlayScreen(fullNow);
        if(fullNow)pendingFullRedraw=false;
    }
    // Removed for the same reason as every other branch above (v0.99957)
    // — PLAY has not shown this crash yet, but that is not the same as
    // being provably safe from the same underlying race; consistency
    // with the rest of loop() costs nothing here.
}
