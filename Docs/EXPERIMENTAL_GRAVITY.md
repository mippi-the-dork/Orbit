# Orbit | Experimental Chaos Gravity Integration

**Maintained reference | v0.2.24, Iteration 035 | Unreal Engine 5.8.x | Editor/Win64 test target**

> **Experimental, explicit opt-in. Not production-validated.** One Orbit plugin and one codebase. Updating the plugin does **not** enable this integration: **Standard (Existing AddForce)** is the default for both new projects and existing projects without a saved override.

This is the canonical reference for Orbit's experimental gravity path. Update this document whenever solver eligibility, method selection, body ownership, fallback behavior, observable diagnostics, automation tests, or known limitations change. Keep `README.md`, `CHANGELOG.md`, `Docs/TEST_PLAN.md`, and applicable API guides consistent. Historical iteration notes are not substitutes for this living reference.

## Purpose and verified history

The original Standard implementation applies sampled field acceleration by `UPrimitiveComponent::AddForce(Acceleration, NAME_None, true)` each game tick. In controlled flat-world experiments at 980 cm/s², cubes stayed awake even when essentially stationary. Native Chaos gravity allowed all ten cubes to sleep in each of three comparison recordings.

Iteration 034 introduced an opt-in solver gravity-group implementation for a single uniform Directional source. **User-verified in UE 5.8.3:** three native automation tests passed; 30 of 30 cubes across three recordings slept naturally with both optional Orbit resting interventions off. Rotating the Directional source woke bodies; restoring downward gravity let them settle and sleep again (visually confirmed). These observations validate **the single-uniform mode only**.

Iteration 035 adds a *new, unvalidated* **single Point-source per-body path**. This is not yet evidence that radial bodies naturally sleep on curved surfaces. That result requires a PIE test and measured capture.

## Enable / disable

1. Open **Project Settings > Plugins > Orbit > Physics > Gravity Integration**.
2. Set **Rigid Body Gravity Application**:
   - **Standard (Existing AddForce)**: default, established behavior. No Chaos gravity-group callback is registered.
   - **Experimental Chaos Gravity (Directional + Point)**: may use the solver for the eligible configurations below. The serialized enum value remains `ExperimentalChaosUniform` to preserve old project settings.
3. For controlled gravity integration comparisons, turn **Stabilize Resting Rigid Bodies** and **Damp Supported Rigid Body Motion** **Off**. Leave **Compare Native World Gravity on Matching Fields** **Off**.
4. To revert, select **Standard**. Orbit releases its solver callback, restores group ownership, and returns managed bodies to the original force method; PIE restart is recommended after first experimental use in any important project.

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

## Diagnostics and CSV

For an eligible Orbit Gravity Receiver component, inspect its physics body diagnostics:

- `Experimental Chaos Uniform Gravity` = `true` only for the Directional group-7 implementation.
- `Experimental Chaos Point Gravity` = `true` only for the new per-body Point solver implementation.
- Both `false` = Standard / native comparison / no solver ownership. (Native gravity may still be active independently.)

Motion History CSV keeps the existing 123 fields and appends field **124**, `experimental_chaos_point_gravity`. Field 123, `experimental_chaos_uniform_gravity`, retains its original meaning. Either `1` identifies which solver route was active at the sampled game-thread instant. No existing CSV column is renamed or reordered.

To record: `Orbit.Motion.StartAll` then `Orbit.Motion.Export` after a controlled 20-second PIE run. For the initial radial trial verify Point=1 for all test cubes, Uniform=0, stabilization and damping both disabled. If Point remains 0, inspect the field restrictions and source ownership before interpreting sleeping results.

## Iteration 035 manual validation checklist

1. **Compile and run native automation:** `Automation RunTests Orbit.Physics.GravityIntegration`. Six tests expected: the original three compatibility tests and three new Point eligibility/vector/fallback tests. These are not a substitute for solver integration tests.
2. **Uniform regression:** With a single unlimited Directional source, 10 cubes on a flat surface, verify all wake → naturally sleep → wake on rotation → naturally sleep again. Confirm Uniform=1, Point=0. The proven Iteration 034 uniform behavior must remain intact.
3. **Single-planet Point test (primary new test):** One Point gravity source centered on a spherical planet, cubes placed around its surface. Enable experimental gravity, disable rest stabilization and supported damping. Verify **Point=1** for the cubes. Record at least 20 seconds; compare awake state, motion, and resting with Standard at the same settings. Record whether cubes naturally sleep, not just whether they appear still.
4. **Source movement:** Move the Point center during PIE; verify previously sleeping bodies wake when their local field changes enough, move toward the new center, and can subsequently sleep if support stabilizes. Avoid starting with high-speed movement.
5. **Falloff and finite range:** Test constant, linear and inverse-square; enter and leave a bounded source. Ensure no unintended world-Z gravity, zero gravity, or force duplication. Note that changes may reach the solver with a game-thread-to-physics-thread input delay.
6. **Fallback:** Add a second source; expect Point=0 and old Standard behavior. Try Axis/Spline/Volume, vortex and box-masked Point. Disable experimental and restore source. Ensure no force duplication or residual group 7 after switching.
7. **Real-world regression:** Existing Orbit projects, skeletal physics, multiplayer, stacked bodies, character interactions and frame-time cost remain **unverified** until intentionally tested.
8. **Packaging:** Complete Win64 UE 5.8.x BuildPlugin and PIE validation. No engine source modifications or Probe dependency.

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

- **Uniform Directional, Iteration 034:** user-confirmed natural sleeping and wake/resleep in simple PIE configurations. Other combinations are not automatically validated.
- **Single Point, Iteration 035:** source-authored, engine-source-reviewed, not yet compiled/tested in user's UE 5.8.3 environment. Does **not** yet establish natural sleeping in radial worlds.
- **Standard:** default; no intentional behavior change in this iteration.
