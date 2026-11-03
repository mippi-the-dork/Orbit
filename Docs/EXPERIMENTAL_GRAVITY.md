# Orbit | Experimental Chaos Gravity Integration

**Maintained reference | v0.2.25, Iteration 036 | Unreal Engine 5.8.x | Editor/Win64 test target**

> **Experimental, explicit opt-in. Not production-validated.** One Orbit plugin and one codebase. Updating the plugin does **not** enable this integration: **Standard (Existing AddForce)** is the default for both new projects and existing projects without a saved override.

This is the canonical reference for Orbit's experimental gravity path. Update this document whenever solver eligibility, method selection, body ownership, fallback behavior, observable diagnostics, automation tests, or known limitations change. Keep `README.md`, `CHANGELOG.md`, `Docs/TEST_PLAN.md`, and applicable API guides consistent. Historical iteration notes are not substitutes for this living reference.

## Purpose and verified history

The original Standard implementation applies sampled field acceleration by `UPrimitiveComponent::AddForce(Acceleration, NAME_None, true)` each game tick. In controlled flat-world experiments at 980 cm/s², cubes stayed awake even when essentially stationary. Native Chaos gravity allowed all ten cubes to sleep in each of three comparison recordings.

Iteration 034 introduced an opt-in solver gravity-group implementation for a single uniform Directional source. **User-verified in UE 5.8.3:** three native automation tests passed; 30 of 30 cubes across three recordings slept naturally with both optional Orbit resting interventions off. Rotating the Directional source woke bodies; restoring downward gravity let them settle and sleep again (visually confirmed). These observations validate **the single-uniform mode only**.

Iteration 035's single Point-source per-body path was subsequently user-tested in UE 5.8.3. In a six-cube radial planet test, all six naturally slept under the experimental solver while zero of six slept under Standard AddForce. Moving the Point source woke sleeping bodies; they settled and slept again. Spheres also settled under ordinary Point gravity. Multiple-source fallback to Standard was demonstrated, and single-source finite-range Linear falloff agreed with the intended strength formula in recorded captures. These tests validate limited scenarios, **not** the complete plugin in production.

**Iteration 036 introduces a separate, default-OFF startup experiment:** `Experimental: Claim Gravity Before BeginPlay`. This preclaims pre-existing eligible rigid bodies in `UWorldSubsystem::OnWorldBeginPlay`, instead of waiting for the first Orbit subsystem tick. The change only runs when both the main Experimental method and the startup option are explicitly enabled. The experimental path **without this new option** remains unchanged. PIE confirmation is still required.

## Enable / disable

1. Open **Project Settings > Plugins > Orbit > Physics > Gravity Integration**.
2. Set **Rigid Body Gravity Application**:
   - **Standard (Existing AddForce)**: default, established behavior. No Chaos gravity-group callback is registered.
   - **Experimental Chaos Gravity (Directional + Point)**: may use the solver for the eligible configurations below. The serialized enum value remains `ExperimentalChaosUniform` to preserve old project settings.
3. **Optional new startup experiment (Iteration 036):** Set **Experimental: Claim Gravity Before BeginPlay** to On if testing the suspected first physics-step world-gravity impulse. It is **Off by default**, including in projects that already saved the experimental gravity method. This only covers eligible, existing, simulated Static Mesh bodies with an active matching field during world initialization. Bodies spawned later still use the normal tick discovery. This is not a guarantee for every physics world startup order.
4. For controlled gravity integration comparisons, turn **Stabilize Resting Rigid Bodies** and **Damp Supported Rigid Body Motion** **Off**. Leave **Compare Native World Gravity on Matching Fields** **Off**.
5. To revert, select **Standard**. Orbit releases its solver callback, restores group ownership, and returns managed bodies to the original force method; PIE restart is recommended after first experimental use in any important project.

The setting is **project-wide**. There is no second Orbit plugin, manual asset migration, or new gravity-source class.

## Eligibility and fallback (Iteration 035)

| Scenario | Solver implementation | Other behavior |
|---|---|---|
| Exactly one unrestricted constant-strength **Directional** physics source | **Uniform gravity group 7**, as in Iteration 034 | Same proven solver path |
| Exactly one ordinary **Point** physics source, inward or outward | **Per-particle Point gravity** in the physics-step callback | Acceleration evaluated at each particle's physics position before integration |
| Point with constant, linear, or inverse-square falloff; finite or unlimited range | **Per-particle Point gravity** | Standard field evaluator supplies bound/falloff math; runtime PIE pending |
| Point Vortex Inward/Outward or box-masked Point | Standard `AddForce` | No approximation of vortex or custom bounds |
| Multiple active physics sources | Standard `AddForce` for all managed bodies | Includes non-overlapping, additive and priority override combinations |
| Axis, Spline, Volume, bounded/non-constant Directional | Standard `AddForce` | No partial unsupported solver integration |
| Simulated skeletal, compound, other non-StaticMesh body | Standard `AddForce` | Single-particle ownership restrictions retained |
| Body initially configured with native gravity disabled | Standard / existing eligibility policy | Never silently changes authored zero-gravity policy |
| Chaos group 7 already owned by another simulated body | Experimental registration refused, Standard fallback | Does not overwrite another system's group 7 |
| A body already assigned to an alternate nonzero group | Standard for that body | Does not steal group ownership |
| No eligible gravity source, Physics Integration disabled, or method Standard | No Orbit Chaos solver callback | Legacy ownership restoration applies |

**Why group 7?** Chaos exposes eight gravity groups, indexed 0–7. Orbit uses group 7 as an **ownership marker**, never as a shared radial direction. For Point gravity, its group acceleration is set to zero and the physics-step callback evaluates a *different* acceleration at each marked dynamic particle. For uniform Directional gravity, group 7 still has the shared solver vector. Ordinary world gravity stays on group 0. Do not combine this experiment with other systems that assign group 7.

**Thread boundary:** The game thread constructs a UObject-free `FOrbitFieldDefinition` snapshot. The Chaos callback consumes that snapshot on the physics thread, never reads a Gravity Source Actor directly, and evaluates Point gravity during `OnPreIntegrate_Internal()` each physics substep. Sleep-state bodies are not explicitly awakened every step. When the field changes substantially, Orbit's pre-existing game-thread field-change wake policy remains responsible for waking eligible sleeping bodies.

**Known precision distinction:** The Point evaluator on the solver thread uses the particle's physics position (`Particle.GetX()`), while the Standard game-thread sampler queries `UPrimitiveComponent::GetCenterOfMass()`. Bodies with substantially offset center-of-mass origins need extra regression testing before equivalence is claimed.

## Initialization and early ownership (Iteration 036)

**Why it exists:** In controlled sphere/planet captures with World Gravity Z = -980, all spheres had approximately one initial world-down step before their intended Point gravity was applied. Setting World Gravity Z to zero removed that initial impulse and changed settling. Changing World Gravity Z globally is a diagnostic, **not** a production fix. This new option attempts to establish Orbit gravity ownership before actors receive `BeginPlay`, using UE's `UWorldSubsystem::OnWorldBeginPlay` lifecycle callback.

The preclaim is also disabled when the separate `Compare Native World Gravity on Matching Fields` A/B diagnostic is enabled, to avoid assigning group 7 to a body that would immediately return to native world gravity during the normal tick.

The preclaim performs a one-time scan of pre-existing, registered gravity source Actors/Components (their normal registration takes place in `BeginPlay`), confirms a *single eligible* solver field, starts the existing experimental Chaos callback, then assigns group 7 to eligible simulated Static Mesh components **without** calling `AddForce()`. Their native `Enable Gravity` state remains enabled, but gravity is routed through the owned Chaos group. This option does not scan for or take over initially gravity-disabled, `Orbit.Ignore`, out-of-range, wrong-channel, skeletal or other unsupported components. Ineligible/multi-source worlds use the existing discovery/fallback implementation. Group-7 conflicts retain the existing fail-safe. There is no engine patch, second plugin, or new solver callback mode.

The path logs `Experimental early gravity ownership: N existing static mesh bodies preclaimed before Actor BeginPlay (Point/Directional).` when a compatible callback starts. This is **not proof that no physics step happened before the callback**; confirm via Motion History samples. Exact timing and compatibility remain unverified until UE 5.8.3 PIE.

**Known boundaries:** actor/components added by runtime spawning or streaming after initial world begin play use legacy tick discovery and can still experience a discovery delay. Early snapshot precedes BeginPlay script changes to gravity-source settings; later tick refreshes them. Solver callback input processing relative to the very first physics substep must be verified in PIE. Disabling this extra option fully restores prior startup behavior even when the main experimental method is selected (restart PIE after setting changes).

## Weak gravity and native Chaos sleeping

A user-controlled inverse-square Point experiment with ~7.41 cm/s² acceleration showed six spheres entering sleep approximately 125 cm above the planet. Removing the startup world-gravity impulse **did not fix** this: under World Gravity Z = 0, all six fell asleep while unsupported. A separate, native Chaos test with no Orbit source and World Gravity Z near -7.41 also appeared to stop while suspended, although its native sleep-state flag was not recorded.

UE 5.8's `Chaos/Island/IslandManager.cpp` explicitly evaluates *isolated particles* using `VSmooth`, `WSmooth`, and a sleep counter without requiring collision support; default isolated-particle sleep multipliers can be controlled by the engine's global sleep CVars. Those global CVars or physics-material sleep thresholds are **investigation tools only**; changing them affects unrelated physics and is **not performed by Orbit**. For now, preserve native sleep policy: no forced wake loop, support sweep, threshold override, or material changes have been added. Strong normal-gravity success does not guarantee correct behavior under very weak acceleration.

## Diagnostics and CSV

For an eligible Orbit Gravity Receiver component, inspect its physics body diagnostics:

- `Experimental Chaos Uniform Gravity` = `true` only for the Directional group-7 implementation.
- `Experimental Chaos Point Gravity` = `true` only for the new per-body Point solver implementation.
- Both `false` = Standard / native comparison / no solver ownership. (Native gravity may still be active independently.)

Motion History CSV keeps the existing 123 fields and appends field **124**, `experimental_chaos_point_gravity`. Field 123, `experimental_chaos_uniform_gravity`, retains its original meaning. Either `1` identifies which solver route was active at the sampled game-thread instant. No existing CSV column is renamed or reordered.

To record: `Orbit.Motion.StartAll` then `Orbit.Motion.Export` after a controlled 20-second PIE run. For the initial radial trial verify Point=1 for all test cubes, Uniform=0, stabilization and damping both disabled. If Point remains 0, inspect the field restrictions and source ownership before interpreting sleeping results.

## Validated test history and Iteration 036 test checklist

1. **Compile and run native automation:** `Automation RunTests Orbit.Physics.GravityIntegration`. Six tests expected: the original three compatibility tests and three new Point eligibility/vector/fallback tests. These are not a substitute for solver integration tests.
2. **Uniform regression:** With a single unlimited Directional source, 10 cubes on a flat surface, verify all wake → naturally sleep → wake on rotation → naturally sleep again. Confirm Uniform=1, Point=0. The proven Iteration 034 uniform behavior must remain intact.
3. **Single-planet Point test (primary new test):** One Point gravity source centered on a spherical planet, cubes placed around its surface. Enable experimental gravity, disable rest stabilization and supported damping. Verify **Point=1** for the cubes. Record at least 20 seconds; compare awake state, motion, and resting with Standard at the same settings. Record whether cubes naturally sleep, not just whether they appear still.
4. **Source movement:** Move the Point center during PIE; verify previously sleeping bodies wake when their local field changes enough, move toward the new center, and can subsequently sleep if support stabilizes. Avoid starting with high-speed movement.
5. **Falloff and finite range:** Test constant, linear and inverse-square; enter and leave a bounded source. Ensure no unintended world-Z gravity, zero gravity, or force duplication. Note that changes may reach the solver with a game-thread-to-physics-thread input delay.
6. **Fallback:** Add a second source; expect Point=0 and old Standard behavior. Try Axis/Spline/Volume, vortex and box-masked Point. Disable experimental and restore source. Ensure no force duplication or residual group 7 after switching.
7. **Real-world regression:** Existing Orbit projects, skeletal physics, multiplayer, stacked bodies, character interactions and frame-time cost remain **unverified** until intentionally tested.
8. **Packaging:** Complete Win64 UE 5.8.x BuildPlugin and PIE validation. No engine source modifications or Probe dependency.

## Iteration 036 A/B procedure (do not change World Gravity)

1. In the **dedicated** six-sphere planet test level, use one Point source, inward Constant 980, unlimited range, normal World Gravity Z = -980, both rest interventions Off, Native World Gravity Comparison Off.
2. Keep main **Experimental Chaos Gravity** mode On. Set **Experimental: Claim Gravity Before BeginPlay** **Off**, restart PIE, capture from startup using `Orbit.Motion.AutoStartAll 1`, 20 seconds, and export the CSV.
3. Set only the new startup option **On**, restart PIE, capture again with the same settings. Look for the preclaim count in Output Log.
4. Compare the **first recorded velocities** of equatorial spheres. They should not receive a common downward world-Z velocity before their local radial field. Compare tangential movement and natural sleeping at 20 seconds. Do not use a sleeping count alone to infer that gravity is correct.
5. Test six cubes and then single Directional, changing source during PIE, reset to Standard, and try a two-source scene. With two sources, early ownership should not claim bodies (Standard fallback). Confirm no duplicate gravity or lost native gravity.
6. Turn the startup option Off again and restart PIE for a baseline. Test runtime-spawned and streaming actors separately: they are explicitly **out of scope** for this startup fix.

All runtime validation is pending. The new suite has **eight** policy/math tests: `Automation RunTests Orbit.Physics.GravityIntegration`. These policy tests do not assert actual solver first-frame timing or native sleep state.

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
- **Early preclaim (036):** source-implemented with an additional default-off setting and native Automation Framework policy tests, **not UE 5.8.3 build- or PIE-validated**. Confirm initial per-body gravity vector and startup trajectory with option Off vs On.
- **Standard:** default; intentionally unchanged. Existing experimental configurations with early ownership Off also retain their previous startup behavior.
