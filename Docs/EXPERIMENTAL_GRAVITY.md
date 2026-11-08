# Current compatibility milestone: Iteration 040 (Orbit 0.2.29)

**New separate opt-in:** Project Settings > Plugins > Orbit > Physics > Gravity Integration > **Experimental: Expanded Field Compatibility** (default **Off**). Requires main Experimental Chaos mode. The established Single Directional/Point, Multiple Point, and Mixed Point + Directional selections still take priority. The new fallback-only selection supports ordinary Axis, Spline (including Spline Up/Down), Directional with finite range, rotated/box-limited sources, Volume Box/Sphere/Cylinder/Capsule and combinations of these with Points, up to 32 active Channel-0 physics sources. It reuses the authoritative Standard field evaluator in Chaos's per-particle step; it is not a second Blueprint API. Vortex, nondefault channel combinations, skeletal/compound bodies and malformed geometry stay Standard.

**Diagnostics:** New `bExperimentalChaosExpandedFieldGravity` receiver diagnostic and 127th append-only CSV column `experimental_chaos_expanded_field_gravity`. On a solver-owned body, exactly one of five method flags should be `1`. Existing CSV columns are not reordered. The Python analyzer understands the new flag. **New runtime behaviors are not yet UE 5.8.3 verified.** See [`COMPATIBILITY_PARITY.md`](COMPATIBILITY_PARITY.md) for an ongoing feature-by-feature inventory and [`ITERATION_040.md`](ITERATION_040.md) for test order.

**Latest user-verified baseline:** Iteration 039 passed 22/22 native Gravity Integration tests and user-reported packaged-game success with expected Orbit behavior. Those results predate the Iteration 040 expansion. Standard remains the default.

---

# Orbit | Experimental Chaos Gravity Integration

## Iteration 039: regression and validation tooling (no gravity behavior change)

**Current version: Orbit 0.2.28.** This pass deliberately does **not** alter Chaos gravity delivery, per-body field math, source selection, startup ownership, sleep behavior, or any existing default-off setting. All four solver flags retain their previous meaning and the Motion History CSV remains at **126 fields**.

**Verified after Iteration 038:** 18/18 Gravity Integration automation tests passed in UE 5.8.3. In user-recorded stationary mixed-source tests, 6/6 spheres naturally slept with Experimental Mixed, compared with 0/6 using Standard in the tested baseline. Both Strongest Influence and Blended Influence allowed all six spheres to sleep in their stationary captures. Moving-source tests showed accelerations and long-lived rolling, but isolation to one active sphere gave decreasing late-window mean linear speeds in both Point-only and Mixed captures. A velocity increase while approaching a gravity source is not proof of new energy being injected. Neither eventual sleeping nor total mechanical energy dissipation can be established from linear speed alone. The current evidence does **not** justify a damping or waking workaround.

**New regression gates:** four native Automation Framework field-math/eligibility tests (expect **22** with earlier 18), and offline `Scripts/AnalyzeGravityMotion.py` for repeatable `.csv` / `.zip` analysis including per-body missing-capture detection and solver-path flags. See [`ITERATION_039.md`](ITERATION_039.md) and [`PERFORMANCE_VALIDATION.md`](PERFORMANCE_VALIDATION.md). These gates do **not** exercise live Chaos contact/wake/restore or prove production performance. **Actual Win64 Development/Shipping game packaging remains unverified.**

## Iteration 038: Mixed Point + Directional (new, separate opt-in)

**Current implementation:** Orbit 0.2.27 adds `Experimental: Mixed Point + Directional Sources` under Project Settings > Plugins > Orbit > Physics > Gravity Integration. It is **Off by default** and requires `Rigid Body Gravity Application = Experimental Chaos Gravity (Directional + Point)`. Enabling it does **not** require enabling `Experimental: Multiple Point Sources`, and enabling either one does not implicitly enable the other. Standard is still the main default. Early ownership is still independently default-Off.

**Eligible field configurations:** two to 32 enabled physics sources, at least one Point and at least one Directional. Points must be ordinary Inward/Outward, with optional finite range and Constant, Linear or Inverse Square falloff. Directional sources must be normal Inward/Outward, have a **nonzero finite direction vector**, Constant falloff, and unlimited range (`Max Distance=0`). All active physics sources must be eligible, with source channel mask **Channel 0 only**; no box masks, vortex, Axis, Spline, or Volume. Only simulated Static Mesh components with original native gravity enabled, and receivers on Channel 0 **only**, are assigned. Mixed-field eligibility is world-wide; receiver group assignment is per body. Unsupported setups use Standard with no new force path.

**Integrator:** The exact same Chaos group 7 callback, immutable field snapshots, and `FOrbitFieldEvaluator::CombineForBody` as multi-Point. Directional + Point contributions combine using the authored `Additive`, `Override`, source priority, and selected `Override Resolution`, without additional `AddForce` on group-owned bodies. Sources update from game-thread snapshots; group changes and wake handling remain subject to the existing Orbit world subsystem tick cadence. There is no new sleeping or damping workaround.

**Diagnostics:** The Receiver exposes `Experimental Chaos Mixed Point Directional Gravity`. The **126th**, append-only CSV column is `experimental_chaos_mixed_point_directional_gravity`. A solver-owned body should show **exactly one** of Uniform, Point, Multi Point, or Mixed Point Directional = 1. Earlier columns (including the first 125) and their meanings are retained unchanged. All four zero can indicate Standard, native comparison, or no solver ownership; inspect native gravity and the authored setting. The solver group flag identifies selection; it is not evidence of correct sleeping or trajectories by itself.

**Performance:** A new native automation test, `Orbit.Physics.GravityIntegration.EvaluationCostBaseline`, prints evaluator-only timings at 2, 8, and 32 Point sources for 64 and 256 synthetic bodies. It is a diagnostic baseline, not a performance requirement and does not measure Chaos contacts, per-particle iteration, snapshot copying, or Editor rendering. `OrbitExperimentalGravityPreIntegrate` is now a native CPU stat scope in the active Point/multi-source solver callback. Compare Development-game PIE captures using the same map and fixed render workload, plus Unreal Insights or native stats. Source×body×substep scaling remains a risk at high counts.

**New mixed test:** use a duplicate of your dedicated 2-planet test level, not the nine-source demo. Keep two ordinary Point sources, then add one unlimited Constant Directional source (e.g. down Z) to create an actual mixed field; choose `Additive`, modest Strength (e.g. 100), and Channel 0. First leave Mixed Off, verify all solver flags 0 (Standard). Then enable Mixed On, restart PIE, verify only the new Mixed flag is 1 for eligible spheres/cubes. Verify trajectories against source math, contact, wake and natural resleep. Change to Override and different priorities. Disable the Directional again: it should select the original multi-Point solver only if the independent Multiple Point setting is On; otherwise Standard. Add Axis, Vortex, finite Directional, or Channel 1 to confirm fallback. Revert both opt-ins and restart PIE.

**Status:** Only source checks were performed for Iteration 038. It requires UE 5.8.3 compilation and PIE verification. Weak-field unsupported sleep, preclaim limitations with runtime-spawned bodies, exclusive group 7 and unsupported multi-channel bodies remain known limits. See `Docs/ITERATION_038.md` and `Docs/SHIPPING_VALIDATION.md`.


**Maintained reference | v0.2.27, Iteration 038 | Unreal Engine 5.8.x | Editor/Win64 test target**

> **Experimental, explicit opt-in. Not production-validated.** One Orbit plugin and one codebase. Updating the plugin does **not** enable this integration: **Standard (Existing AddForce)** is the default for both new projects and existing projects without a saved override.

This is the canonical reference for Orbit's experimental gravity path. Update this document whenever solver eligibility, method selection, body ownership, fallback behavior, observable diagnostics, automation tests, or known limitations change. Keep `README.md`, `CHANGELOG.md`, `Docs/TEST_PLAN.md`, and applicable API guides consistent. Historical iteration notes are not substitutes for this living reference.

## Purpose and verified history

The original Standard implementation applies sampled field acceleration by `UPrimitiveComponent::AddForce(Acceleration, NAME_None, true)` each game tick. In controlled flat-world experiments at 980 cm/s², cubes stayed awake even when essentially stationary. Native Chaos gravity allowed all ten cubes to sleep in each of three comparison recordings.

Iteration 034 introduced an opt-in solver gravity-group implementation for a single uniform Directional source. **User-verified in UE 5.8.3:** three native automation tests passed; 30 of 30 cubes across three recordings slept naturally with both optional Orbit resting interventions off. Rotating the Directional source woke bodies; restoring downward gravity let them settle and sleep again (visually confirmed). These observations validate **the single-uniform mode only**.

Iteration 035's single Point-source per-body path was subsequently user-tested in UE 5.8.3. In a six-cube radial planet test, all six naturally slept under the experimental solver while zero of six slept under Standard AddForce. Moving the Point source woke sleeping bodies; they settled and slept again. Spheres also settled under ordinary Point gravity. Multiple-source fallback to Standard was demonstrated, and single-source finite-range Linear falloff agreed with the intended strength formula in recorded captures. These tests validate limited scenarios, **not** the complete plugin in production.

**Iteration 036 added a separate, default-OFF startup experiment:** `Experimental: Claim Gravity Before BeginPlay`. The user verified eight native tests and a controlled six-sphere comparison: with early ownership Off, 2/6 spheres slept at 20 seconds; with it On, 6/6 slept. In the latter run, the initial velocities followed radial gravity without the previous common world-down impulse. The user also confirmed source-movement wake/response and two-source Standard fallback. These observations validate the **controlled single-Point startup scenario**, not all world types or runtime spawning.

**Iteration 037 adds another separately opted-in experiment:** `Experimental: Multiple Point Sources`. Two to 32 ordinary Channel-0 Point gravity sources can use Chaos per-body gravity, with existing Additive, Override, priority, influence-resolution, distance and falloff rules. This mode is **Off by default**, including for projects already using Experimental and Early Ownership. The user verified multi-source runtime behavior in the dedicated two-planet scene: all six spheres slept naturally in the experimental 20-second capture versus one of six in Standard, and moving both sources caused bodies to react and eventually settle. These tests do **not** certify all fields, users, or Shipping builds.

## Enable / disable

1. Open **Project Settings > Plugins > Orbit > Physics > Gravity Integration**.
2. Set **Rigid Body Gravity Application**:
   - **Standard (Existing AddForce)**: default, established behavior. No Chaos gravity-group callback is registered.
   - **Experimental Chaos Gravity (Directional + Point)**: may use the solver for the eligible configurations below. The serialized enum value remains `ExperimentalChaosUniform` to preserve old project settings.
3. **Optional new startup experiment (Iteration 036):** Set **Experimental: Claim Gravity Before BeginPlay** to On if testing the suspected first physics-step world-gravity impulse. It is **Off by default**, including in projects that already saved the experimental gravity method. This only covers eligible, existing, simulated Static Mesh bodies with an active matching field during world initialization. Bodies spawned later still use the normal tick discovery. This is not a guarantee for every physics world startup order.
4. **Optional multi-source experiment (Iteration 037):** Enable **Experimental: Multiple Point Sources** if and only if you want to test two or more eligible Point sources. It remains **Off by default**, even when the main Experimental mode is active. Its independent setting supports existing single-source projects without behavior changes. Eligibility is described below.
5. **Optional mixed-source experiment (Iteration 038):** Enable **Experimental: Mixed Point + Directional Sources** for at least one ordinary Point plus one unlimited Constant Directional source. This is **Off by default**, independent of Multiple Point, and does not alter the legacy mode selection.
6. For controlled gravity integration comparisons, turn **Stabilize Resting Rigid Bodies** and **Damp Supported Rigid Body Motion** **Off**. Leave **Compare Native World Gravity on Matching Fields** **Off**.
7. To revert, switch the new mixed experiment Off, switch the earlier multi-Point experiment Off, or select **Standard**. Orbit releases its solver callback, restores group ownership, and returns managed bodies to the original force method; PIE restart is recommended after first experimental use in any important project.

The setting is **project-wide**. There is no second Orbit plugin, manual asset migration, or new gravity-source class.

## Eligibility and fallback (Iterations 037–038)

| Scenario | Solver implementation | Other behavior |
|---|---|---|
| Exactly one unrestricted constant-strength **Directional** physics source | **Uniform gravity group 7**, as in Iteration 034 | Same proven solver path |
| Exactly one ordinary **Point** physics source, inward or outward | **Per-particle Point gravity** in the physics-step callback | Acceleration evaluated at each particle's physics position before integration |
| Point with constant, linear, or inverse-square falloff; finite or unlimited range | **Per-particle Point gravity** | Standard field evaluator supplies bound/falloff math; runtime PIE pending |
| Point Vortex Inward/Outward or box-masked Point | Standard `AddForce` | No approximation of vortex or custom bounds |
| Two to 32 ordinary inward/outward **Point** physics sources, all on Channel 0 only | **Per-particle multi-Point gravity**, **only** if `Experimental: Multiple Point Sources` is On | Supports Additive, Override, Priority Override, Strongest Influence, Blended Influence, finite ranges and falloff. Verified in a dedicated two-planet manual experiment; not production-qualified. |
| Multiple sources with new multi-Point option Off | Standard `AddForce` for multi-Point fields | Maintains existing Experimental project behavior |
| **Mixed Point + Directional** (2–32 total, channel 0, normal Points and unlimited Constant Directional) | **Per-particle mixed solver**, only when **Mixed Point + Directional Sources** option On | Separate new experiment; unchecked or unsupported source configurations keep Standard |
| Mixed Point + Directional option Off | Standard `AddForce` | The older multi-Point option does not enable this new shape combination |
| Unsupported mixed shapes (Axis/Spline/Volume), nondefault source channels or more than 32 sources | Standard `AddForce` | Conservative global fallback; no partial solver approximation |
| Receiver using Channel 1, Channel 2, or multiple channels in an otherwise eligible multi-source world | Standard `AddForce` for that body | Only Channel-0-only receivers may use multi-Point or mixed solver |
| Axis, Spline, Volume, bounded/non-constant Directional | Standard `AddForce` | No partial unsupported solver integration |
| Simulated skeletal, compound, other non-StaticMesh body | Standard `AddForce` | Single-particle ownership restrictions retained |
| Body initially configured with native gravity disabled | Standard / existing eligibility policy | Never silently changes authored zero-gravity policy |
| Chaos group 7 already owned by another simulated body | Experimental registration refused, Standard fallback | Does not overwrite another system's group 7 |
| A body already assigned to an alternate nonzero group | Standard for that body | Does not steal group ownership |
| No eligible gravity source, Physics Integration disabled, or method Standard | No Orbit Chaos solver callback | Legacy ownership restoration applies |

**Why group 7?** Chaos exposes eight gravity groups, indexed 0–7. Orbit uses group 7 as an **ownership marker**, never as a shared radial direction. For Point gravity, its group acceleration is set to zero and the physics-step callback evaluates a *different* acceleration at each marked dynamic particle. For uniform Directional gravity, group 7 still has the shared solver vector. Ordinary world gravity stays on group 0. Do not combine this experiment with other systems that assign group 7.

**Thread boundary:** The game thread constructs UObject-free `FOrbitFieldDefinition` snapshots (one for single-Point or an array for multi-Point), plus the selected Override Resolution setting. The Chaos callback consumes its own copied snapshots on the physics thread, never reads a Gravity Source Actor directly, and evaluates Point gravity during `OnPreIntegrate_Internal()` each physics substep. For multi-Point, it calls the same `FOrbitFieldEvaluator::CombineForBody` routine as Standard, but with per-particle positions and only the supported Channel-0-only receiver scope. Sleep-state bodies are not explicitly awakened every step. When the field changes substantially, Orbit's pre-existing game-thread field-change wake policy remains responsible for waking eligible sleeping bodies.

**Known precision distinction:** The Point evaluator on the solver thread uses the particle's physics position (`Particle.GetX()`), while the Standard game-thread sampler queries `UPrimitiveComponent::GetCenterOfMass()`. Bodies with substantially offset center-of-mass origins need extra regression testing before equivalence is claimed.

## Multiple Point sources (Iteration 037, limited PIE verification)

Turn **Experimental: Multiple Point Sources** On in the Gravity Integration settings **in addition** to the existing main Experimental method. The new option is independent of **Experimental: Claim Gravity Before BeginPlay** (that option may be On or Off). This is still one Orbit plugin, and the legacy single-source solver modes retain their previous eligibility and behavior.

The multi-source extension requires 2–32 ordinary inward/outward Point physics sources, with all active physics sources using *only Channel 0*. Physics source configurations with Axis/Spline/Volume, Directional mixed with Point, vortex, box limits, or other channel masks cause a **whole-world Standard fallback**. An eligible source set can include multiple Additive and Override fields, priorities, finite distances, Linear and Inverse Square attenuation. The evaluator applies the project's `Override Resolution` (`Priority Override`, `Strongest Influence`, or `Blended Influence`) on the **physics thread** at each owned particle's current position.

Receiver masks are a separate gate: a body with a mask other than exactly Channel 0 stays on Standard, even if its world has an eligible multi-Point source set. Chaos gravity group 7 is not a gravity channel. This avoids assigning a non-matching gravity field to a particle whose channel mask isn't represented in the callback. Limitations include game-thread snapshots of moving source transforms, weak-field sleeping, and linear performance cost in the number of fields times particles times physics steps. Do not use this new path for gameplay or packaged production until verified.

For Motion History CSVs, the **125th** column `experimental_chaos_multi_point_gravity` records the Iteration 037 mode (Iteration 038 appends a separate **126th** mixed-mode column). With a solver-owned multi-Point body it should be `1` while the earlier uniform/point flags are `0`. With multi-source opt-in disabled, the same two-Point scene should show all four flags as `0` (Standard). The CSV's `gravity_source_id` and `source_center` are the **dominant** source only; the gravity vector is the combined result and there is no per-contributor source list. The `gravity_*` fields are sampled on the game thread at COM, while the solver uses particle physics positions and its current field snapshot.

**Suggested regression:** Use a clean two-planet/two-Point test scene, sources at `(0,0,0)` and `(-2500,0,0)` with `Max Distance=2000`, and spheres around the original planet including one at X=-1150. Capture with the new option Off/On, compare the acceleration vectors and sleep/wake outcomes, then move a source, change priorities, test Additive/Override and repeat with a third unsupported source shape. See [`ITERATION_037.md`](ITERATION_037.md) for the full test order.

## Initialization and early ownership (Iteration 036)

**Why it exists:** In controlled sphere/planet captures with World Gravity Z = -980, all spheres had approximately one initial world-down step before their intended Point gravity was applied. Setting World Gravity Z to zero removed that initial impulse and changed settling. Changing World Gravity Z globally is a diagnostic, **not** a production fix. This new option attempts to establish Orbit gravity ownership before actors receive `BeginPlay`, using UE's `UWorldSubsystem::OnWorldBeginPlay` lifecycle callback.

The preclaim is also disabled when the separate `Compare Native World Gravity on Matching Fields` A/B diagnostic is enabled, to avoid assigning group 7 to a body that would immediately return to native world gravity during the normal tick.

The preclaim performs a one-time scan of pre-existing, registered gravity source Actors/Components (their normal registration takes place in `BeginPlay`), confirms an eligible solver field configuration (single Point/Directional, or multi-Point when separately opted in), starts the existing experimental Chaos callback, then assigns group 7 to eligible simulated Static Mesh components **without** calling `AddForce()`. Their native `Enable Gravity` state remains enabled, but gravity is routed through the owned Chaos group. This option does not scan for or take over initially gravity-disabled, `Orbit.Ignore`, out-of-range, wrong-channel, skeletal or other unsupported components. Ineligible worlds and multi-source worlds with the additional multi-Point setting Off use the existing discovery/fallback implementation. Group-7 conflicts retain the existing fail-safe. There is no engine patch, second plugin, or new solver callback mode.

The path logs `Experimental early gravity ownership: N existing static mesh bodies preclaimed before Actor BeginPlay (Point/Directional/Multiple Point).` when a compatible callback starts. This is **not proof that no physics step happened before the callback**; confirm via Motion History samples. Single-Point early ownership was user-verified in a controlled PIE comparison; other world startup scenarios and multi-Point early ownership remain unverified.

**Known boundaries:** actor/components added by runtime spawning or streaming after initial world begin play use legacy tick discovery and can still experience a discovery delay. Early snapshot precedes BeginPlay script changes to gravity-source settings; later tick refreshes them. Solver callback input processing relative to the very first physics substep must be verified in PIE. Disabling this extra option fully restores prior startup behavior even when the main experimental method is selected (restart PIE after setting changes).

## Weak gravity and native Chaos sleeping

A user-controlled inverse-square Point experiment with ~7.41 cm/s² acceleration showed six spheres entering sleep approximately 125 cm above the planet. Removing the startup world-gravity impulse **did not fix** this: under World Gravity Z = 0, all six fell asleep while unsupported. A separate, native Chaos test with no Orbit source and World Gravity Z near -7.41 also appeared to stop while suspended, although its native sleep-state flag was not recorded.

UE 5.8's `Chaos/Island/IslandManager.cpp` explicitly evaluates *isolated particles* using `VSmooth`, `WSmooth`, and a sleep counter without requiring collision support; default isolated-particle sleep multipliers can be controlled by the engine's global sleep CVars. Those global CVars or physics-material sleep thresholds are **investigation tools only**; changing them affects unrelated physics and is **not performed by Orbit**. For now, preserve native sleep policy: no forced wake loop, support sweep, threshold override, or material changes have been added. Strong normal-gravity success does not guarantee correct behavior under very weak acceleration.

## Diagnostics and CSV

For an eligible Orbit Gravity Receiver component, inspect its physics body diagnostics:

- `Experimental Chaos Uniform Gravity` = `true` only for the Directional group-7 implementation.
- `Experimental Chaos Point Gravity` = `true` only for the existing **single Point** solver implementation.
- `Experimental Chaos Multi Point Gravity` = `true` only for the separately opted-in **multiple Point** solver implementation.
- `Experimental Chaos Mixed Point Directional Gravity` = `true` only for separately opted-in **mixed Point + Directional** source configurations.
- All four `false` = Standard / native comparison / no solver ownership. (Native gravity may still be active independently.)

Motion History CSV contains **126 fields**. Field 123 `experimental_chaos_uniform_gravity`, field 124 `experimental_chaos_point_gravity`, and field 125 `experimental_chaos_multi_point_gravity` retain their previous meanings. Field 126 is `experimental_chaos_mixed_point_directional_gravity`. Exactly one of these four flags should be `1` when the sampled body is solver-owned. No existing CSV column is renamed or reordered.

To record: `Orbit.Motion.StartAll` then `Orbit.Motion.Export` after a controlled 20-second PIE run. For the initial radial trial verify Point=1 for all test cubes, Uniform=0, stabilization and damping both disabled. If Point remains 0, inspect the field restrictions and source ownership before interpreting sleeping results.

## Regression checklists: single-source and startup

1. **Compile and run native automation:** `Automation RunTests Orbit.Physics.GravityIntegration`. **22 tests expected in Iteration 039**: the 18 user-validated Iteration 038 tests and four new field-transition/compatibility tests. These do not replace solver integration tests.
2. **Uniform regression:** With a single unlimited Directional source, 10 cubes on a flat surface, verify all wake → naturally sleep → wake on rotation → naturally sleep again. Confirm Uniform=1, Point=0. The proven Iteration 034 uniform behavior must remain intact.
3. **Single-planet Point test (primary new test):** One Point gravity source centered on a spherical planet, cubes placed around its surface. Enable experimental gravity, disable rest stabilization and supported damping. Verify **Point=1** for the cubes. Record at least 20 seconds; compare awake state, motion, and resting with Standard at the same settings. Record whether cubes naturally sleep, not just whether they appear still.
4. **Source movement:** Move the Point center during PIE; verify previously sleeping bodies wake when their local field changes enough, move toward the new center, and can subsequently sleep if support stabilizes. Avoid starting with high-speed movement.
5. **Falloff and finite range:** Test constant, linear and inverse-square; enter and leave a bounded source. Ensure no unintended world-Z gravity, zero gravity, or force duplication. Note that changes may reach the solver with a game-thread-to-physics-thread input delay.
6. **Fallback:** Add a second source with the new `Experimental: Multiple Point Sources` setting **Off**; expect all experimental solver flags = 0 and old Standard behavior. Try Axis/Spline/Volume, vortex and box-masked Point. Disable experimental and restore source. Ensure no force duplication or residual group 7 after switching.
7. **Real-world regression:** Existing Orbit projects, skeletal physics, multiplayer, stacked bodies, character interactions and frame-time cost remain **unverified** until intentionally tested.
8. **Packaging:** Complete Win64 UE 5.8.x BuildPlugin and PIE validation. No engine source modifications or Probe dependency.

## Iteration 036 A/B procedure (do not change World Gravity)

1. In the **dedicated** six-sphere planet test level, use one Point source, inward Constant 980, unlimited range, normal World Gravity Z = -980, both rest interventions Off, Native World Gravity Comparison Off.
2. Keep main **Experimental Chaos Gravity** mode On. Set **Experimental: Claim Gravity Before BeginPlay** **Off**, restart PIE, capture from startup using `Orbit.Motion.AutoStartAll 1`, 20 seconds, and export the CSV.
3. Set only the new startup option **On**, restart PIE, capture again with the same settings. Look for the preclaim count in Output Log.
4. Compare the **first recorded velocities** of equatorial spheres. They should not receive a common downward world-Z velocity before their local radial field. Compare tangential movement and natural sleeping at 20 seconds. Do not use a sleeping count alone to infer that gravity is correct.
5. Test six cubes and then single Directional, changing source during PIE, reset to Standard, and try a two-source scene with the new **multi-Point setting Off**. With two sources and that setting Off, early ownership should not claim bodies (Standard fallback). Confirm no duplicate gravity or lost native gravity.
6. Turn the startup option Off again and restart PIE for a baseline. Test runtime-spawned and streaming actors separately: they are explicitly **out of scope** for this startup fix.

The **single-Point startup** A/B and eight original automation tests were user-verified in Iteration 036. Their successful 2/6 vs 6/6 comparison does not validate all startup scenarios. Iteration 037 adds five multi-Point policy tests (13 total) and a separately opted-in physics solver path. Its two-planet 6/6 sphere sleeping test and moving-source wake/resleep were later user-verified, but broader performance and Shipping remain pending. Iteration 038 raised the test count to **18** and introduced an opt-in mixed-source mode; its 18 tests and several stationary/moving-source PIE scenarios were later user-validated. Iteration 039 adds four math/policy tests and offline diagnostics, with Windows compile/Shipping still pending.

## Failure criteria and return path

- Bodies do not sleep naturally, wake unreliably, visibly change trajectory, penetrate geometry, lose gravity, or exhibit unacceptable performance.
- An owned body is found in a group used by a different integration.
- A callback registration error or project-specific crash occurs.

**Immediate workaround:** switch Project Settings back to **Standard**, restart PIE, and preserve a Motion History capture/log for diagnosis. Do not enable additional manual sleep or damping as a hidden fix. Fixes must be tested behind the same opt-in setting before expanding eligibility.

## Current implementation locations

- `Source/Orbit/Public/OrbitSettings.h`: method selector and default.
- `Source/Orbit/Private/OrbitChaosGravityBridge.{h,cpp}`: field eligibility, group ownership, Chaos solver callback, per-particle Point integration.
- `Source/Orbit/Private/OrbitWorldSubsystem.cpp`: field snapshots, body assignment/release, game-thread wake decisions, diagnostics.
- `Source/Orbit/Public/OrbitGravityTypes.h`: receiver physics diagnostics and field math API.
- `Source/Orbit/Private/OrbitMotionHistory.cpp`: append-only CSV diagnostics.
- `Source/Orbit/Private/Tests/OrbitChaosGravityTests.cpp`: native Unreal Automation Framework policy tests.

## Status

- **Uniform Directional (034):** 30/30 natural cube sleeps across 3 runs, wake and resleep after gravity rotation observed.
- **Single Point (035):** six of six cubes sleeping on a spherical planet in one recorded run; spheres and wake/resleep observed under ordinary fields. Two-source Standard fallback and finite-range linear falloff verified in targeted recordings. Other configurations remain experimental.
- **Weak inverse-square (035):** unsupported sleeping reproduced, and a similar motionless native Chaos case observed; not solved.
- **Early preclaim (036):** user-verified in UE 5.8.3: 8/8 automation tests; early ownership Off = 2/6 spheres asleep, On = 6/6 asleep in a matched 20-second Point-gravity planet test, with initial tangential speed corrected (13.48 cm/s to 0). Source movement wake/response and two-source fallback also tested. These observations do not guarantee all startup pathways or late-spawned bodies.
- **Multiple Point (037):** user-verified with a two-planet six-sphere A/B capture: Experimental 6/6 naturally asleep by 20s vs Standard 1/6; multi-Point diagnostic active in the Experimental run, and moving both sources allowed spheres to react and eventually settle. The user reported the other manual behavior was as expected. Formal 037 automation results and full performance/Shipping validation were not provided.
- **Mixed Point + Directional (038):** user-verified in UE 5.8.3: 18/18 automation tests, stationary mixed-source 6/6 natural sleeping vs Standard 0/6 in one recording, and correctly selected fallback. In dynamic source tests, spheres gained speed locally and slowed over longer intervals; one-sphere isolation showed no proven energy injection. Full frame-time profiling and Shipping are pending.
- **Compatibility pass (039):** source physics unchanged; 4 new native field/policy tests (22 total) and an offline Motion History analyzer supplied for targeted regression and speed-window analysis. Actual Windows compile and game package pending.
- **Standard:** default; intentionally unchanged. Existing experimental configurations with early ownership Off also retain their previous startup behavior.

## Iteration 041 — real Chaos automation (new, not yet UE-validated)

The Iteration 040 user-verified **29/29** policy/math suite remains unchanged. Iteration 041 adds nine separate **editor-only live-world** tests under `Orbit.Physics.IntegrationWorld`. They spawn temporary physics bodies and fields, tick Chaos, and verify actual acceleration, solver selection, source edits/removal, and natural sleeping/waking. These tests temporarily enable experimental expanded gravity and early ownership, then restore the user's project settings without saving them. **They must pass in UE 5.8.3 before we replace the remaining manual expanded-field validation.** No public runtime behavior or opt-in defaults changed. See [Integration Testing](INTEGRATION_TESTING.md) and [Iteration 041](ITERATION_041.md).

## Iteration 041 automated live-world validation (Hotfix 2)

A UE 5.8.3 test run passed 8/9 physics-world integration tests and 29/29 gravity field/policy tests. The remaining Directional sleep/wake test was a fixture error: rotating an upright source by yaw does not change its `-Up` gravity vector. Hotfix 2 tilts the source by pitch and asserts the updated gravity vector before testing waking and acceleration. The corrected test must still be run in UE; the observed failure is **not evidence of an Orbit runtime wake regression**. Standard remains default and all experimental capabilities remain opt-in.

## Iteration 042 — Receiver channels and Vortex in the Chaos solver (pending UE tests)

In **Project Settings > Plugins > Orbit > Physics > Gravity Integration**, select `Rigid Body Gravity Application = Experimental Chaos Gravity` to access these independent, **default-Off** settings:

| Option | Default | Enables | When Off |
|---|---|---|---|
| `Experimental: Receiver Channel Compatibility` | Off | Independent Channels 0/1/2 and combined receiver masks on supported position fields, using a physics-thread particle-ID/mask snapshot | Existing experimental selections and channel fallback remain unchanged |
| `Experimental: Vortex Compatibility` | Off | Point, Axis, Spline Vortex Inward/Outward, Continuous Force, and inward Spiral Capture using actual Chaos particle velocity | Vortex remains Standard AddForce fallback |

Keep `Claim Gravity Before BeginPlay` optional and independent. `Expanded Field Compatibility`, `Multiple Point Sources`, and `Mixed Point + Directional` remain separate historical opt-ins; enabling the new **Receiver Channel Compatibility** selects the channel-aware evaluator first **only when explicitly enabled and field-eligible**, including a single uniform Directional field. This is necessary to prevent two channel receivers sharing Chaos group 7 from receiving each other's fields.

**Implementation boundaries:** up to 32 finite or unlimited valid source fields, static-mesh single-body rigid components; never steal custom Chaos gravity groups. A body whose particle identity has not arrived in the physics-thread snapshot receives zero Orbit acceleration until registered, not another receiver's channel. Runtime-spawned actors still enter via normal discovery and are **not guaranteed** correct custom gravity from their very first substep. Skeletal/compound bodies stay on Standard. Vortex Spiral Capture is intentionally velocity-steered and may sustain motion; do not expect it to sleep like static Directional gravity.

**To validate:** leave the *saved* project settings on Standard and run the native test suite, whose isolated world fixtures configure the new options and restore them. The original 29+9 tests passed in UE 5.8.3; Iteration 042's added tests remain unverified until the user runs them.

```text
Automation RunTests Orbit.Physics.GravityIntegration
Automation RunTests Orbit.Physics.IntegrationWorld
```

Expect **34 policy + 14 live-world tests**. For more detail, see [Iteration 042](ITERATION_042.md). Old 127-column Motion History exports remain readable, with channel/vortex grouped under `experimental_chaos_expanded_field_gravity`.
