# TMC2209 StealthChop Calibration — Operation, Feature Interaction, and a Plan

This document explains how the TMC2209 driver actually works in this firmware, how
its relevant features interact, what "calibration" means from the programming-model
perspective, and how to store and reuse that calibration so the runtime move
(`testRun`) is no longer needed on every boot.

It also records the problems found in the current implementation and a plan to fix
them. No code — this is a reference to read against the datasheet
(`tmc2209_datasheet_rev1.09.pdf`) while you improve the implementation yourself.

Datasheet sections referenced throughout: §6 (StealthChop), §11 (StallGuard4),
§5.5.1 (CHOPCONF), §5.5.2 (PWMCONF), and the register map in §5.

---

## 1. The big picture: what this device is doing electrically

The mechanical idea is: drive the curtain with a deliberately **low motor current**
(low torque). If the curtain meets an obstacle, the motor cannot produce enough
torque to keep turning, so it **stalls** (stops rotating) instead of forcing through.
The firmware detects that stall and stops driving.

Two independent TMC2209 subsystems make this work, and it is important to see that
they are **separate concerns that happen to depend on the same operating mode**:

1. **Current control (the chopper)** — decides *how much current* flows in the coils
   for a given velocity. Here it runs in **StealthChop2** voltage-PWM mode.
2. **Load/stall sensing** — **StallGuard4** measures mechanical load and raises the
   **DIAG** pin when load crosses a threshold.

The link between them: **StallGuard4 is designed to work in StealthChop mode**, and
its readings are only stable when the chopper's current regulation is well tuned.
So "calibrating StealthChop" is not cosmetic — it is what makes stall detection
repeatable.

---

## 2. Operating modes and why StealthChop is the right (and required) choice here

The TMC2209 chopper can run in two modes:

- **StealthChop2** — a *voltage-mode* PWM chopper. Quiet, smooth, and it supports an
  automatic tuning procedure that adapts the drive voltage to the motor. Stall
  sensing here is **StallGuard4**.
- **SpreadCycle** — a *current-mode* cycle-by-cycle chopper. Stall sensing here is
  **StallGuard2** (the older variant).

Selection is via `GCONF.en_SpreadCycle`:

- `en_SpreadCycle = 0` → StealthChop (this is the power-up default and what this
  firmware relies on — the code never sets this bit, which is correct).
- `en_SpreadCycle = 1` → SpreadCycle.

**Consequence for this project:** StallGuard4 (the `SGTHRS` / `SG_RESULT` / DIAG stall
path the firmware uses) is only meaningful in StealthChop. If you ever switched to
SpreadCycle you would be on StallGuard2 with different tuning. Keep
`en_SpreadCycle = 0`.

---

## 3. Current scaling: how "low torque" is actually set

Current is set by a chain of factors, not a single number. Understanding this chain
is essential because the calibration is defined *relative to the run current*.

- **VREF (analog) vs. internal scaling** — `GCONF.I_scale_analog`. The firmware sets
  this to `1`, meaning the **external VREF voltage** sets the full-scale current.
  This is where the physically low current ("low torque") is dialed in — in hardware,
  via VREF.
- **`vsense` (CHOPCONF)** — selects the sense-resistor voltage range. The firmware
  sets `vsense = 1` (high sensitivity / low sense voltage), i.e. the low-current
  range. Consistent with the low-torque intent.
- **IRUN / IHOLD (IHOLD_IRUN register)** — a 0…31 scale *on top of* the VREF/vsense
  full scale. `IRUN` is the current while moving; `IHOLD` is the reduced current at
  standstill. The firmware sets `IRUN = 31` (100% of the VREF-defined scale while
  moving) and `IHOLD = 16` (~50% at rest). So "low torque" comes from VREF, and IRUN
  = 31 means "use all of that (low) current while moving."
- **IHOLDDELAY / standstill reduction** — because `IHOLD < IRUN`, the driver ramps
  current *down* to IHOLD a short time after motion stops (see §5 on why this matters
  for calibration).

**Mental model:** `effective current ≈ VREF_fullscale × vsense_range × (CS/32)`,
where `CS` is IRUN while moving and ramps toward IHOLD at standstill.

---

## 4. StealthChop2 automatic tuning — the "calibration" (datasheet §6.1)

StealthChop2 needs to know two motor-specific numbers to produce the target current
across the velocity range. It **measures them itself** through a two-phase automatic
tuning (AT) procedure. These two numbers *are* the calibration.

### The two calibrated values

- **`PWM_OFS_AUTO`** — the PWM offset needed to reach the target current **at
  standstill / very low velocity**. Found in phase **AT#1**.
- **`PWM_GRAD_AUTO`** — the *gradient*: how much extra PWM voltage is needed per unit
  of velocity, to compensate the motor's back-EMF. Found in phase **AT#2**.

Both are readable from the **`PWM_AUTO` (0x72)** register and are continuously
refined by the chip during subsequent motion and standstill.

### Phase AT#1 — calibrate `PWM_OFS_AUTO`

Preconditions (all must hold):

- Motor in **standstill**.
- Actual current scale **equals IRUN** (not the reduced IHOLD).
- VS (supply) and VREF at operating level.
- Duration: at least ~130 ms of standstill (up to ~2^20 + 2·2^18 clocks) at IRUN.

Critical subtlety: if standstill current reduction is enabled (which it is here,
because `IHOLD < IRUN`), the chip will have dropped to IHOLD, so AT#1 would calibrate
at the *wrong* current. The datasheet's remedy: **"an initial step pulse switches the
drive back to run current."** So AT#1 must be performed while the current is genuinely
at IRUN.

### Phase AT#2 — calibrate `PWM_GRAD_AUTO`

Preconditions:

- Motor must **move** at a velocity that generates significant back-EMF and where
  full run current is still reachable (datasheet hint: a typical band is 60–300 RPM;
  verify for this motor and mechanics).
- The regulation must sit in the window
  `1.5 · PWM_OFS_AUTO · (IRUN+1)/32  <  PWM_SCALE_SUM  <  4 · PWM_OFS_AUTO · (IRUN+1)/32`
  and `PWM_SCALE_SUM < 255`.
- Duration: ~8 full steps per unit change of `PWM_GRAD_AUTO`; from the OTP default
  this can take **up to ~400 full steps** to converge.

Note that AT#2 depends on AT#1: its preconditions are written in terms of
`PWM_OFS_AUTO`. **A bad AT#1 produces a bad AT#2.**

### How to know tuning actually converged

Read **`PWM_SCALE` (0x71)** during AT#2:

- `PWM_SCALE_AUTO` (bits 24…16, signed) should settle **toward 0** — that means the
  feed-forward model (`PWM_OFS`/`PWM_GRAD`) now supplies the right voltage without the
  regulator having to add much.
- `PWM_SCALE_SUM` (bits 7…0) should be inside the window above and `< 255`.

If `PWM_SCALE_AUTO` does not approach 0, the move velocity is wrong (too slow → not
enough back-EMF; too fast → current can't reach full scale), or AT#1 was invalid.

### Persistence of the calibration

`PWM_OFS_AUTO` / `PWM_GRAD_AUTO` live in registers and **survive as long as the chip
is powered**. `disable()` (ENN high) only gates the output stage; it does **not**
reset registers. That is exactly why the firmware can run one tuning move at boot and
then rely on the values for the rest of the power cycle. They are lost on power-off,
which is the whole reason to store them in flash.

**Validity caveat:** the values are specific to *this motor + this VREF + this supply
voltage*. The datasheet explicitly states that changing VREF or VS invalidates the
tuning. Treat a stored calibration as valid only while those are unchanged; otherwise
recalibrate.

---

## 5. How the current implementation maps to the AT procedure

Reading the code as a state machine (`initialize()` then `testRun()` in `doJob`):

**`DriverImpl::initialize()`** (`tmc2209.cpp`):

1. `enable()` then `sleep_ms(5000)` — a 5 s standstill, but **before any registers are
   configured**, so the chip is at OTP power-up defaults (wrong IRUN, VREF scaling not
   yet enabled). This window does nothing useful for the final tuning.
2. `set_gconf()`, `set_IRunIHold()`, `set_SGTHRS()`, `set_TCOOLTHRS()` — configuration.
3. `sleep_ms(5000)` — a second standstill; this is the only one that could serve as
   AT#1, because it happens after configuration.
4. `disable()`.

**`testRun()`** (`blink.cpp`): `enable()` → short delay → `move(0)` (starts the step
square wave) → 5 s of motion (this is AT#2) → `stop()` → `disable()`.

So the intended structure is "standstill for AT#1, then a move for AT#2," which is the
right shape. But there are concrete problems:

### Problems

1. **Configuration happens after the first standstill.** The first `sleep_ms(5000)`
   is wasted (OTP defaults). Config must come first.

2. **AT#1 almost certainly runs at IHOLD, not IRUN.** With `IHOLD = 16 < IRUN = 31`,
   standstill reduction is active. Nothing issues the "initial step pulse" the
   datasheet requires, so `PWM_OFS_AUTO` is likely calibrated at ~half current — the
   wrong reference. This is the most important defect, and because AT#2 depends on
   AT#1, it also degrades `PWM_GRAD_AUTO`.

3. **AT#1 and AT#2 are split across two functions with a `disable()` between them.**
   This only works because registers persist while powered — it is functionally OK but
   fragile, and it obscures the fact that the AT#1 result feeding AT#2 may be invalid.

4. **No convergence check.** Nothing reads `PWM_SCALE` / `PWM_SCALE_AUTO`, so the code
   cannot tell a good calibration from a bad one — it would happily store garbage. The
   existing `debug()` only dumps `TSTEP` and `SG_RESULT`.

5. **`move()` hard-codes a 156 µs step period and ignores its `rpm` argument.** AT#2
   needs a defined "medium velocity." Whether 156 µs lands in the right band depends on
   the MS1/MS2 microstep setting and the mechanics; this should be validated via the
   `PWM_SCALE_AUTO → 0` check.

### Verdict

The concept is right, but the procedure is **not reliably producing a correct
calibration**, chiefly due to (1) ordering and (2) AT#1 running at the wrong current.
Fix those before trusting any stored values.

---

## 6. Reading the calibration back (to save to flash)

Read register **`PWM_AUTO` (0x72)** — read-only — after a *good* AT#1 + AT#2 run, while
the chip is still powered:

| Field | Bits | Meaning |
|---|---|---|
| `PWM_OFS_AUTO` | 7…0 | Auto-determined offset (0…255) |
| `PWM_GRAD_AUTO` | 23…16 | Auto-determined gradient (0…255) |

That is the entire calibration: two bytes. Store them in flash (they fit naturally
alongside the existing persisted config). Recommended to also store a **"calibrated"
flag** (and ideally a version/marker for the VREF+supply assumptions) so the boot code
can decide whether a stored calibration is trustworthy or a fresh calibration run is
needed.

Where to read them from in the flow: right after AT#2 (motor has moved and converged),
before power-off. `disable()` in between is fine — registers persist.

---

## 7. Applying a stored calibration (so `testRun` is no longer needed)

You seed the stored values into the **`PWMCONF` (0x70)** register, which the current
firmware never writes. Relevant layout (reset default `0xC10D0024`):

| Field | Bits | Notes |
|---|---|---|
| PWM_LIM | 31…28 | Scale limit on mode switch (leave default 12) |
| PWM_REG | 27…24 | Regulator gradient (only matters when autoscale=1) |
| freewheel | 21…20 | Standstill mode when IHOLD=0 |
| **pwm_autograd** | 19 | 1 = auto-tune gradient; 0 = fixed `PWM_GRAD` |
| **pwm_autoscale** | 18 | 1 = current-feedback tuning; 0 = feed-forward |
| pwm_freq | 17…16 | PWM frequency divider |
| **PWM_GRAD** | 15…8 | Seed / fixed gradient ← write stored `PWM_GRAD_AUTO` |
| **PWM_OFS** | 7…0 | Seed / fixed offset ← write stored `PWM_OFS_AUTO` (default 36) |

There are two ways to use the stored numbers, with a genuine trade-off:

### Option A — fully deterministic, zero tuning at boot (recommended here)

Write `PWM_OFS = stored PWM_OFS_AUTO`, `PWM_GRAD = stored PWM_GRAD_AUTO`, and turn
tuning **off**: `pwm_autoscale = 0`, `pwm_autograd = 0`. This is the datasheet's
feed-forward velocity-controlled mode (§6.2): the chip drives
`PWM_OFS·(CS+1)/32 + PWM_GRAD·256/TSTEP` directly.

- No AT#1 standstill, no AT#2 move — **instant and repeatable at every boot**.
- The datasheet endorses this once PWM_OFS/PWM_GRAD "have been determined in automatic
  tuning mode initially," and notes it gives the most stable amplitude (hint: "use
  pre-determined PWM_GRAD and set pwm_autograd = 0" to reduce amplitude jitter).
- Stable amplitude is actually good for StallGuard consistency.
- Downside: it does **not** compensate for supply-voltage or temperature drift. Fine
  for a fixed, mains-powered install with a stable VREF; recalibrate if any of those
  change.

This is the option that matches the stated goal ("no `testRun` needed").

### Option B — keep adaptivity, skip only the long phase

Leave `pwm_autoscale = 1`, `pwm_autograd = 1`, but **seed** `PWM_OFS` and `PWM_GRAD`
with the stored values. Per §5.5.2, seeding `PWM_GRAD` "speeds up the automatic tuning
process," and autoscale "starts with `PWM_SCALE_AUTO = PWM_OFS`."

- Eliminates the expensive AT#2 sweep (the up-to-~400-full-step convergence from the
  OTP default), but you still need a brief AT#1 standstill (~130 ms at IRUN) after
  enabling.
- Not literally zero-move, but collapses the 5 s move to a fraction of a second, and
  stays robust to drift.

### Sequencing for either option

Write `PWMCONF` as part of the initial register setup, **before the first motion**, in
the same block as GCONF / IHOLD_IRUN. Keep `en_SpreadCycle = 0` so StallGuard4 keeps
working. If you use Option A, no calibration move is ever issued on a boot that has a
valid stored calibration.

---

## 8. Feature-interaction summary (the mental model to keep)

- **Mode gates everything:** StealthChop (`en_SpreadCycle=0`) is required for
  StallGuard4 *and* is what has the AT tuning. Do not change it.
- **VREF + vsense + IRUN/IHOLD** set the physical (low) current. IRUN is the reference
  the AT calibration is defined against — so AT#1 must run at IRUN.
- **Standstill reduction (IHOLD<IRUN)** is what silently breaks AT#1 unless you force
  run current (step pulse, or temporarily set IHOLD=IRUN) during calibration.
- **AT#1 → PWM_OFS_AUTO** (standstill at IRUN); **AT#2 → PWM_GRAD_AUTO** (moving); AT#2
  depends on AT#1.
- **`PWM_SCALE_AUTO → 0`** is the signal that tuning converged; **`PWM_AUTO` (0x72)**
  is what you read out and store; **`PWMCONF` (0x70)** is where you write it back.
- **Registers persist while powered, not across power-off** — hence flash storage.
- **StallGuard4 (`SGTHRS`/`SG_RESULT`/DIAG)** is the stall-detection layer; it benefits
  from a good, stable current calibration, and its DIAG pulse is only active in
  StealthChop when `TCOOLTHRS ≥ TSTEP > TPWMTHRS` (i.e. above the configured minimum
  velocity). This is the mechanism the obstacle-detection relies on.

---

## 9. Plan / checklist to improve the implementation

Ordered by priority. (Implementation left to you.)

1. **Fix `initialize()` ordering:** configure all registers (GCONF, CHOPCONF/vsense,
   IHOLD_IRUN, SGTHRS, TCOOLTHRS, PWMCONF) *first*, then any standstill. Drop the
   wasted pre-config 5 s standstill.

2. **Make AT#1 run at IRUN:** either temporarily set `IHOLD = IRUN` for the duration of
   calibration, or issue a single step pulse to restore run current, then hold
   standstill > ~130 ms. Restore the normal IHOLD afterward.

3. **Unify calibration into one routine** that does AT#1 (standstill at IRUN) → AT#2
   (defined-velocity move) without a semantically load-bearing `disable()` in the
   middle.

4. **Add a convergence check:** read `PWM_SCALE` during AT#2 and confirm
   `PWM_SCALE_AUTO → 0` and `PWM_SCALE_SUM` inside the window and `< 255`. Only store a
   calibration that passed. Use this same check to validate the AT#2 move velocity
   (adjust the step period if needed).

5. **Add `PWM_AUTO` (0x72) read-back** and persist `PWM_OFS_AUTO` + `PWM_GRAD_AUTO`
   (plus a "calibrated" flag / validity marker) to flash.

6. **On boot, branch on stored calibration:** if a valid calibration exists, write it
   into `PWMCONF` (Option A: autoscale=0, autograd=0) during register setup and skip
   the calibration move entirely; otherwise run the full calibration routine, store the
   result, and continue.

7. **Recalibration policy:** invalidate/refresh the stored calibration if VREF, supply
   voltage, or the motor changes (the datasheet says these invalidate the tuning).

---

## 10. Prior art and related projects (inspiration, not oracle)

A survey of how other TMC2209 projects handle stall detection and StealthChop
calibration. **Treat these as inspiration, not authority** — several contain outright
mistakes or advice for a different chip generation (see the misinformation note below).
Their most useful contribution is showing which *patterns* are common in practice versus
what this project is doing.

### The headline finding: almost nobody stores the AT calibration

The mainstream pattern (Marlin, TMCStepper-based projects, janelia's library) is to
**let the chip re-run its runtime StealthChop autotuning every power cycle** and only
configure the *stall threshold*. Storing `PWM_OFS_AUTO` / `PWM_GRAD_AUTO` in flash and
replaying them (the plan in this document) is **off the beaten path**. That is not a
reason to abandon it — it is a legitimate optimization for a device that boots, does one
short move, and sleeps — but be aware there is little existing code to copy from, and the
"just let the chip tune each boot" approach is the well-trodden, low-risk default worth
keeping as a fallback.

### Two philosophies for obtaining PWM_OFS / PWM_GRAD

- **(a) Motion-based automatic tuning** — run the motor, let the chip measure, read
  `PWM_AUTO`. This is the datasheet procedure and what this firmware does.
- **(b) Calculate the values from the motor's datasheet constants** — resistance,
  inductance, holding torque, rated current, steps/rev — and write `PWM_OFS`/`PWM_GRAD`
  (and chopper timing) analytically, no tuning move at all. This is what
  **`andrewmcgr/klipper_tmc_autotune`** does. It is a genuine alternative to consider:
  if the motor's R/L are known, the offset/gradient can be seeded directly, which would
  let this project skip the calibration move *without* ever having run one. Trade-off:
  it depends on trusting datasheet motor constants and the model, rather than a
  measurement of the actual motor.

### Stall-detection patterns observed

- **Static threshold, StallGuard active at all speeds** — the canonical
  `teemuatlut/TMCStepper` StallGuard example sets `TCOOLTHRS = 0xFFFFF` (max, so
  StallGuard is enabled across the whole velocity range), a fixed `SGTHRS`, and reads
  `SG_RESULT` in a loop for tuning. No calibration move. Simple, and close to what this
  firmware does (fixed `SGTHRS = 10`, `TCOOLTHRS = 130`). Note: `TCOOLTHRS = max` also
  enables StallGuard at very low velocities where the datasheet (§11.5) warns the reading
  is unreliable — this firmware's smaller `TCOOLTHRS` is arguably more correct.
- **Dynamic, velocity-dependent threshold** — `AndreaFavero71/stepper_sensorless_homing`
  (RP2040 + MicroPython, the closest platform match to this project) does *not* use a
  fixed threshold. It computes an expected StallGuard value as a linear function of speed
  (`min_expected_SG ≈ 0.15 × speed`), reads `SG_RESULT` over UART **and** watches the
  DIAG edge, and **ignores StallGuard for the first ~100 ms** after the motor starts to
  avoid false triggers during startup. Reports ~±0.01 mm 3σ repeatability. Directly
  relevant inspirations for this project: (1) a startup guard window before trusting a
  stall, and (2) optionally reading `SG_RESULT` over UART during the move as a
  cross-check / for logging, rather than relying on the DIAG edge alone.
- **Interactive threshold tuning is the norm** — every serious source tunes `SGTHRS`
  empirically per motor and velocity by watching `SG_RESULT` under increasing load (the
  datasheet §11.2 procedure). None pick a magic number blind. This firmware's hard-coded
  `SGTHRS = 10` should be validated the same way, which reinforces the "add
  observability" items in the plan (§9.4).

### Common misinformation to guard against

A lot of forum and tutorial advice says *"StallGuard only works in SpreadCycle — disable
StealthChop for sensorless homing."* **That is true for the older TMC2130 (StallGuard2),
and wrong for the TMC2209 (StallGuard4).** The TMC2209 datasheet (§11.2) is explicit:
the DIAG stall output is *"only enabled in StealthChop mode,"* and StallGuard4 is
"developed for operation in conjunction with StealthChop." So this project's
StealthChop-based approach is correct; ignore advice premised on the earlier chip. This
is the clearest example of why these projects are inspiration, not oracle.

### Where this project already matches good practice

- Hand-rolled UART with CRC and IFCNT (transmit-counter) write verification — the same
  robustness the mature libraries implement internally
  (`teemuatlut/TMCStepper`, `janelia-arduino/TMC2209`, `terjeio/Trinamic-library`).
- DIAG-edge interrupt for stall — matches the common approach; AndreaFavero's project
  suggests *also* reading `SG_RESULT` over UART as a cross-check, which is the main
  low-cost upgrade available.
- Leaving `en_SpreadCycle = 0` (StealthChop) — correct for StallGuard4, despite
  widespread advice to the contrary.

### Reference projects

- `teemuatlut/TMCStepper` — the canonical Arduino TMC library; StallGuard example uses a
  static threshold and no calibration persistence.
- `janelia-arduino/TMC2209` — clean C++ register-level driver; exposes
  `setPwmOffset`/`setPwmGradient`/`enableAutomaticCurrentScaling`/
  `enableAutomaticGradientAdaptation` but leaves StealthChop autotuning to the chip.
- `AndreaFavero71/stepper_sensorless_homing` — RP2040/MicroPython sensorless homing with
  a velocity-dependent StallGuard threshold, DIAG + UART, startup guard, and a
  repeatability study. Closest platform/approach match.
- `andrewmcgr/klipper_tmc_autotune` — calculates chopper/PWM/StallGuard settings from
  motor datasheet constants instead of running a tuning move; the main example of
  philosophy (b) above.
- `MarlinFirmware/Marlin` — large real-world user; relies on the chip's runtime
  autotuning each power cycle and configures homing sensitivity only (does not persist
  PWM_OFS/PWM_GRAD).
- `KushagraK7/TMC2209_sensorless_homing_test`, `edwardocano/Esp32-TMC2209` —
  TMCStepper + AccelStepper worked examples.
- `terjeio/Trinamic-library` — register-level C driver used by grblHAL.
