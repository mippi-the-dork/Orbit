# Orbit | Physics Motion History

**Since:** v0.2.15 (Iteration 026). **All-body capture:** v0.2.16 (Iteration 027). **Support evidence and live speeds:** v0.2.17 (Iteration 028).  
**Scope:** Development diagnostics for simulated primitives managed by Orbit's automatic rigid-body gravity. Not a physics solver/substep recorder.

The recorder can monitor **one selected body or every currently managed body** in the PIE world. All-body recording is useful for distinguishing localized collision problems from system-wide gravity, damping, support and sleep behavior.

## Commands (PIE or non-Shipping game worlds)

| Command | Behavior |
| --- | --- |
| `Orbit.Motion.List` | List eligible managed primitives with internal path, friendly actor label (Editor), and Static Mesh asset. |
| `Orbit.Motion.Start <internal-name> [seconds=20] [Hz=60]` | Capture one uniquely matched Actor/Component/path; original behavior preserved. |
| `Orbit.Motion.StartAll [seconds=20] [Hz=60]` | Capture **all** currently Orbit-managed simulated primitives (up to 256), without needing any names. |
| `Orbit.Motion.Status` | Report mode, roster size, number of bodies sampled, currently unavailable bodies, total rows and capacity, reversal count, requested duration/rate, and stop reason. |
| `Orbit.Motion.Stop` | End recording without discarding samples. |
| `Orbit.Motion.Export [label]` | Export one UTF-8 CSV with all retained bodies to `Project/Saved/Orbit/MotionHistory/`. |

Durations are clamped to **0.25–120 seconds**; rates to **1–120 samples/sec**, with a maximum of one sample per body per game tick. Bulk recording has a **hard cap of 256 bodies and 250,000 total rows**; it refuses the recording if the captured roster exceeds 256 instead of silently ignoring actors. The row cap may end a large/long session before the requested duration. Single-body mode keeps its original 15,002-row upper bound. Both modes are observation-only: no new force, damping, torque, or sleep request.

`StartAll` freezes the **set of monitored bodies at command time**. Bodies entering the field or becoming Orbit-managed *later* do not automatically join, while bodies leaving Orbit management stop receiving new samples; remaining captured bodies continue. The roster uses deterministic component-path ordering at capture start. Start recording after the scene has finished spawning/discovering bodies, or run `Orbit.Motion.List` first to confirm the expected count.

Starting another recording automatically attempts to export any unexported prior data as `OrbitMotion_AutoReplaced...`; if saving fails the old capture is retained. Stopping PIE with unexported rows attempts an `OrbitMotion_AutoPIEStop...` export. Neither automatic export is guaranteed after a crash.

## Multi-body CSV schema

One **interleaved CSV** covers every body; sample rows from the same game tick are near one another. The original 49 fields (from `body` to `tangential_reversal`) remain in place, followed by the four Iteration 027 identity fields:

| Field | Purpose |
| --- | --- |
| `body_id` | Zero-based, fixed roster index during this recording; distinguish each primitive even when owner names repeat. |
| `actor_name` | UE's internal owner name. |
| `actor_label` | World Outliner-friendly Actor label when recorded in Editor, internal name otherwise. |
| `mesh_asset` | Full Static Mesh asset path, when applicable. |

Use `body` (full component object path) as the primary identity key and `body_id` as the compact index. `body_geometry` is retained for per-body collision description. Support component names/geometry are captured per sampled row. For multi-component actors, each primitive is a distinct body.

The CSV is written in fixed-size UTF-8 batches instead of building a second enormous CSV in memory. The in-memory sample buffer is still bounded, and recording 87 bodies will consume more CPU/memory than recording one. If a 20-second capture starts with 87 bodies and achieves a steady 60 game ticks/sec, expect approximately **104,400 sample rows**, not exactly that many. Actual rate is limited by game-thread tick rate and body availability.

Data includes position, orientation, center of mass, linear and angular velocity, sampled Orbit acceleration, dominant source and source center, support sweep state/normal/impact, approximate collision-geometry description, native gravity and damping status, awake state, tangential speed, and tangential reversal flags. Reversals are calculated **independently per body**; interleaved rows from different bodies are never compared. These are game-thread measurements of existing Orbit support queries, **not Chaos collision manifolds or physics substep recordings**. Sampling every managed body may affect runtime performance, so compare test runs with comparable frame rates.

## Iteration 028: additional fields and interpretation

**Backward compatibility:** the **first 53 fields** have not been reordered, renamed or removed. Iteration 028 appends 42 columns, bringing the CSV to **95 columns**. Old recordings remain readable. The values of `tangential_speed` and `angular_speed` now reflect **current** physics velocities even while sleeping, not a previous awake frame. This is an intentional accuracy correction to existing column values.

| Field(s) | How to read them |
| --- | --- |
| `capture_start_pie_sec`, `pie_time_sec` | World's game time when Start/StartAll ran, and when each sample was taken. Compare `pie_time_sec - capture_start_pie_sec` against `t_sec`. This is game time, not wall-clock time or a physics substep timestamp. |
| `initial_x/y/z`, `initial_awake`, `initial_linear_speed`, `initial_angular_speed` | Per-body pose and native rigid-body state at recorder start, repeated on each row. Use these to assess whether two runs really started from equivalent conditions. |
| `first_verified_support_pie_sec`, `first_sleep_transition_pie_sec` | First **sampled** fresh support and first observed **awake to asleep** transition for the body. `-1` until detected. An initially sleeping body is not counted as a new sleep transition. These values are cumulative within each capture, not retroactively written to earlier rows. |
| `first_verified_support_event`, `first_sleep_transition_event`, `support_state_changed`, `awake_state_changed` | Flags on sampled rows for notable changes. A fast transition can be missed between game ticks/samples. `support_state_changed` includes changes to descriptive states such as `Sleeping (previous support sample)`. |
| `probe_outcome` | Explains `Ineligible`, `Normal impact too fast`, `No qualifying blocking hit`, `Blocking hit rejected by normal`, `Query miss (grace retained)`, `Verified`, `Verified (normal mismatch)` and `Sleeping (no new probe)`. A body put to sleep after the current tick's verified query retains its `Verified` outcome. |
| `rest_candidate`, `gentle_contact`, `probe_attempted`, `fresh_support`, `grace_support` | The actual per-tick gating decisions and support type. Grace is never a new contact, just a brief persistence of previously verified support. |
| `probe_blocking_hits`, `probe_rejected_normals`, `best_rejected_alignment` | Counts from the **same** `ComponentSweepMulti` used by Orbit's rest system. An alignment below **0.70** is rejected. `best_rejected_alignment = -2` means no rejected candidate. An accepted contact with alignment below **0.90** is still supported but reported as a normal mismatch. |
| `best_rejected_normal_x/y/z`, `best_rejected_impact_x/y/z`, `best_rejected_start_penetrating` | Best (highest-alignment) rejected blocking candidate normal, hit impact point and starting-penetration flag. Zero vectors and false indicate no rejected candidate. |
| `best_rejected_component`, `best_rejected_geometry` | World component path and simple collision setup of the best rejected hit. Useful for detecting contacts with adjacent simulated props instead of the planet. Empty/None when no rejected candidate. |
| `normal_speed_cm_s`, `grace_remaining_sec` | Signed velocity along gravity Down (+ toward support) and current per-body support grace timer. Normal-speed gating uses the existing project setting. |
| `effective_linear_damping`, `effective_angular_damping` | Current damping on the actual primitive at sample time (can include pre-existing component damping). |
| `config_supported_damping_enabled`, `config_rest_sleep_enabled`, `config_supported_linear_damping`, `config_supported_angular_damping`, `config_probe_distance_cm`, `config_max_contact_speed_cm_s` | Orbit's settings at sample time, so an enabled feature is not confused with damping actually being applied to an individual body. |

**Limitations:** A rejected candidate is a response from a game-thread overlap/sweep, not evidence of a native Chaos contact impulse. No extra sweeps are performed. Collision shape, friction, solver iterations, compound bodies and live source changes may affect what the existing query returns. Sleeping rows do not perform a fresh sweep, so a previously stored support normal or collision component may be historical; rely on `probe_attempted` and `probe_outcome` rather than interpreting all `support_name` values as fresh contacts. The recorder samples at most once per game tick/body at a requested cadence, so timestamps describe observed transitions, not their exact solver-substep time.

### Investigating the October 30 planet captures

Previously observed cases such as `Ball3` (many unsupported samples on an apparent multi-convex support) and `Ball53` (frequent normal mismatches) can be re-recorded with **damping on at defaults**, then filtered by `actor_label` or `body_id`. Examine `probe_outcome` first:

- `No qualifying blocking hit` during gentle contact points to the existing query finding no usable collision geometry in the sweep interval.
- `Blocking hit rejected by normal` points to an unfavorable collision/sweep normal relative to gravity Up; compare rejected component/normal and accepted `support_name` with the planet's physics collision geometry.
- `Verified (normal mismatch)` means a hit met the unchanged 0.70 support acceptance threshold but its normal is below the 0.90 debug-alignment threshold.
- `Sleeping (no new probe)` is a deliberate optimization, not evidence that collision support was lost.

Large `normal_speed_cm_s` may cause a probe to be skipped even when a body looks near the ground. A high `probe_rejected_normals` count does **not** prove a bug, since the sweep can overlap walls and neighboring props as well as the supporting planet.

## Suggested planet experiment

In a planet scene with many physics bodies, run the same level in PIE with the same initial body transforms and gravity setup for each trial. Change **only** Orbit's **Damp Supported Rigid Body Motion** setting between trial A (off) and trial B (on, default rates 1.5/2.0 per second). Keep the rest/sleep system settings unchanged between trials.

**Trial A** (Damping Off):

```text
Orbit.Motion.List
Orbit.Motion.StartAll 20 60
Orbit.Motion.Status
Orbit.Motion.Export Planet_All_DampingOff
```

**Trial B** (Damping On):

```text
Orbit.Motion.StartAll 20 60
Orbit.Motion.Status
Orbit.Motion.Export Planet_All_DampingOn
```

Run export after recording has stopped automatically (20 seconds of PIE time). Perform a third stronger-damping trial only if the first two datasets fail to distinguish the behavior. Keep initial conditions reproducible, avoid interacting with bodies while recording, and send the two CSV files along with any observation of the affected actors.

## Automated tests

```text
Automation RunTests Orbit.Physics.MotionHistory
Automation RunTests Orbit.Physics.Rest
```

Eight Motion History tests now cover tangential reversal classification, CSV escaping/filename rules, bounded single-body configuration, bounded all-body budgets, independent per-body sampling cadence, current/sleeping velocity truth, support query outcome classification, and first observed contact/sleep transitions. These math/configuration tests do **not** replace actual PIE testing of multi-body collection, streamed CSV output, auto-export, or level-specific physics behavior. Iteration 028 was statically inspected but has not been compiled or run in UE 5.8.3 in the authoring environment.
