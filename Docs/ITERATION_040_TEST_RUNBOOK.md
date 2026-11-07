# Orbit 0.2.29 | Iteration 040 Hotfix 1 test runbook

**Purpose:** Verify the new Expanded Field Compatibility physics path in UE 5.8.3 without modifying the existing nine-source demo map. Standard remains the default and is the correct baseline. These instructions are limited to this iteration's newly expanded physics source shapes.

## 1. Automated test pass (no Project Settings changes needed)

Use the Unreal Editor Output Log console (not Windows PowerShell):

```text
Automation RunTests Orbit.Physics.GravityIntegration.ExpandedMixedFieldParity
Automation RunTests Orbit.Physics.GravityIntegration
```

Run these **one at a time**, after the preceding test completes. Expect the individual test to pass and **29/29** Gravity Integration tests to pass. They create their own field definitions and do not use the live Project Settings mode. An `ExpandedMixedFieldParity` failure in original Iteration 040 was a **test expectation error**, not evidence of a runtime gravity bug.

## 2. Build an isolated comparison level

Create an Empty Level named `L_Orbit_ExpandedGravity_Test`, with one `Orbit Gravity Source Actor` (at world origin) and two simulated `/Engine/BasicShapes/Cube` Static Mesh Actors. Use **Simulate Physics: On**, **Enable Gravity: On**, **Collision: PhysicsActor**, scale `(0.5,0.5,0.5)`. Remove every other Orbit source from this new level. Do **not** alter the nine-source demo map.

For controlled shape comparisons, use a simple Static Mesh floor at Z=-100 with BlockAll collision. When testing Axis/Spline cases where the acceleration is sideways or otherwise non-downward, make sure the cubes are clear of the floor so it cannot stop their motion prematurely. Keep identical cube physics settings in Standard and Experimental.

### Exact Project Settings for Experimental capture

Project Settings > Plugins > Orbit:

| Property | Value |
|---|---|
| Enable Physics Integration | **On** |
| Automatically Apply to Physics Bodies | **On** |
| Rigid Body Gravity Application | **Experimental Chaos Gravity (Directional + Point)** |
| Experimental: Claim Gravity Before BeginPlay | **On** |
| Experimental: Multiple Point Sources | **Off** |
| Experimental: Mixed Point + Directional Sources | **Off** |
| Experimental: Expanded Field Compatibility | **On** |
| Stabilize Resting Rigid Bodies | **Off** |
| Damp Supported Rigid Body Motion | **Off** |
| Compare Native World Gravity on Matching Fields | **Off** |
| Include Initially Gravity Disabled Bodies | **Off** |
| Use World Gravity Outside Fields | **Off** for the boundary test (avoids confusing native -Z with source gravity) |

For the **Standard comparison**, change only `Rigid Body Gravity Application` to **Standard (Existing AddForce)**, and restart PIE. The other opt-ins have no effect while Standard is selected. Restore Experimental before the next Expanded capture.

### Common source settings

Unless specified otherwise: source **Enabled On**, **Affect Physics On**, **Affect Characters unchanged**, gravity channels **Channel 0 On, Channel 1 Off, Channel 2 Off**, **Strength 980**, **Falloff Constant**, **Max Distance 0**, **Direction Inward**, **Combine Override**, **Priority 0**, **Limit To Box Off**. All cubes remain Channel 0 only.

### Capture console commands

Enter these three commands in the **Unreal Editor Output Log console before starting PIE**:

```text
Orbit.Motion.AutoStartAll 1
Orbit.Motion.AutoDuration 20
Orbit.Motion.AutoRate 60
```

Start PIE. In the **PIE/game console**, enter after approximately 20 seconds:

```text
Orbit.Motion.Status
Orbit.Motion.Export Axis_Expanded_01
```

Replace the export label for subsequent captures. Files are written to `Project/Saved/Orbit/MotionHistory/`. Before finishing, back in the editor console, turn off auto-capture:

```text
Orbit.Motion.AutoStartAll 0
```

### Three priority scenarios

1. **Axis:** Set Shape = `Axis (Cylindrical)`, Center `(0,0,0)`, world rotation `(0,0,0)` (local axis +Z). Start one cube near `(500,0,300)` unobstructed. Expected inward acceleration approximately toward **-X**, not world -Z. Experimental CSV `experimental_chaos_expanded_field_gravity = 1`.
2. **Spline:** Set Shape = `Spline (Curve / Torus)`, Direction = **Spline Down (Oriented)**. Author an approximately straight spline from `(0,0,0)` to `(1000,0,0)` and leave its up orientation unchanged. Place a cube near `(500,100,300)`. Expected gravity follows spline Down/up orientation (nominally -Z for unrolled spline). Confirm new Expanded flag = 1. Test a second position, or the same source with `Spline Up (Oriented)` to verify reverse direction. Preserve spline point rotations/up vectors when comparing Standard and Experimental.
3. **Volume boundaries:** Set Shape = `Volume (Directional)`, Direction Inward, Strength 980, source centered at `(0,0,0)`, rotation zero. For Box use Half Extents `(500,500,500)`; Sphere Radius = 500; Cylinder Radius = 500 and Half Height = 500; Capsule Radius = 500 and Half Height = 700. For each shape, place one cube clearly **inside** (e.g. `(0,0,250)`) and one **outside** (e.g. `(800,0,250)`). With `Use World Gravity Outside Fields` Off, the inside cube should accelerate along source -Z while the outside cube should receive no source acceleration. Rotate the source around Y by 90 degrees for a separate bounded-orientation check. Confirm Expanded flag = 1 for eligible managed samples and that no solver/force duplication occurs.

For each of these, make one Standard capture and one Experimental capture **using identical authored geometry**, restarting PIE between runs. Favor initial acceleration direction and path over sleeping totals: the body's expected support conditions differ among these test geometries.

### Fallback test (one capture is enough)

Set the source's Direction to `Vortex Inward` with an ordinary Point/Axis source (instead of Axis/Spline/Volume). Experimental mode and Expanded opt-in remain On. Expected: `experimental_chaos_expanded_field_gravity = 0` and all other solver flags zero. Gravity should continue through Standard. Restore original source Direction afterward.

## 3. Checks automated vs still manual

**Already automated:** source shape eligibility, Axis and Spline vector math, rotated volume boundary math, mixed-source snapshot equivalence, additive/override priority rules, finite-range boundary math and basic fallback selection. Run the 29-test suite to verify these; there is no reason to re-prove every field arithmetic rule manually.

**Still requires live PIE testing:** actual Chaos integration, wake/sleep, impact/contact stability, runtime source edits, correct initial ownership, transitions to/from fallback, and actual thread performance. A pure field-math test does not prove these behaviors.

**Next automation target:** add a native Unreal Automation Framework *physics-world integration suite* that creates its own small test world/actors, executes fixed-time solver steps, moves and destroys sources, switches eligibility, and reads body awake state and solver flags. This requires UE5.8 Editor-backed runtime testing and must not be marked validated from the existing policy/math tests. No Probe integration.
