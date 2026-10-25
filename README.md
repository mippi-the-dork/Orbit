# Orbit

**Orbit** is a C++ gravity field plugin for Unreal Engine 5.8.3. Designers can place gravity sources in a level to author spherical, cylindrical, directional, bounded-volume, and spline-shaped gravity. Gameplay code can query the same field, while an initial runtime subsystem automatically applies custom acceleration to eligible physics-simulating components.

**Current version:** 0.1.3 (Iteration 004, experimental)  
**Target:** Unreal Engine 5.8.3, Win64 (not yet build-verified)  
**Engine modifications:** None  
**Status:** Source package prepared; compile, PIE and packaged-game validation are pending.

Orbit is a *gravity framework*, not a Character Movement plugin. The world owns the gravity field. Physics, movement and future navigation/EQS integrations query the same definition.

## What is included

| Capability | Status |
| --- | --- |
| Point (planet, spherical interior) | Implemented in field evaluator; pending UE test |
| Axis (cylindrical or Halo-style radial gravity) | Implemented; pending UE test |
| Directional (uniform field, reversed gravity) | Implemented; pending UE test |
| Volume (rotated Box, Sphere, Cylinder, Capsule) | Implemented; pending UE test |
| Editable spline (curved and closed tubular worlds) | Implemented, sampled as polyline; pending UE test |
| Inward and outward fields | Implemented; pending UE test |
| Inverse-square, linear and constant strength | Implemented; pending UE test |
| Bounded influence and rotated box masks | Implemented; pending UE test |
| Priority overrides and additive combinations | Implemented; pending UE test |
| Zero-gravity field and null-direction detection | Implemented; pending UE test |
| Runtime world subsystem and Blueprint queries | Implemented; pending UE test |
| Automatic ordinary simulated rigid-body gravity | **Experimental** game-thread `AddForce` bridge |
| Gravity-enabled-state preservation and opt-out tags | Implemented; pending UE test |
| Optional Character Movement gravity-direction bridge | **Experimental**, not a complete locomotion/camera solution |
| Editor volumetric field arrows and wireframe volume bounds | Implemented; pending UE test |
| Actor viewport billboard from supplied Orbit artwork | Implemented with Editor-only transient texture; pending UE visual test |
| Orbit editor icons | Registered for plugin browser, gravity-source actor, character gravity component and class thumbnail; pending UE visual test |
| Field math automated tests | 8 tests authored; not yet run in Unreal |
| Chaos solver callbacks and per-body substep gravity | **Not yet implemented** |
| Gravity-aware NavMesh, EQS, AI MoveTo | **Not yet implemented** |
| Ragdoll/cloth/vehicles/water/Niagara/PCG | **Not yet integrated** |
| Multiplayer gravity source replication and prediction | **Not yet supported** |

## Install

1. Extract the ZIP so the folder is `YourProject/Plugins/Orbit/`, with `Orbit.uplugin` inside it.
2. Regenerate Visual Studio project files (if necessary) and build your UE **5.8.3** Editor target.
3. Launch the Editor, open **Edit > Plugins**, and confirm Orbit is enabled.
4. Project-wide options are under **Project Settings > Plugins > Orbit**.
5. Place an `Orbit Gravity Source` actor from the Place Actors panel or search for `OrbitGravitySource`.

No Epic Engine files should be changed. This is an unverified source package, not a precompiled binary.

## Editor icons (introduced in 0.1.1)

Orbit now includes an **Editor-only** `OrbitEditor` module for Slate class-icon registration. The existing gameplay module remains free of editor/Slate dependencies.

| Resource | Current use |
| --- | --- |
| `Resources/Icon128.png` | Plugin Browser thumbnail (128x128, supplied artwork) |
| `Resources/Icons/OrbitActor_16.svg` | `AOrbitGravitySource` class icon in the World Outliner, class pickers and similar editor views |
| `Resources/Icons/Orbit-Icon.svg` | `UOrbitCharacterGravityComponent` class icon in component pickers; registered at 16/20/32px for future menus/toolbars |
| `Resources/Icons/OrbitAsset_64.svg` | `AOrbitGravitySource` class thumbnail, including applicable class/Blueprint browser views |

There is **no standalone Orbit Content Browser asset class** in this iteration. The 64px artwork is used for the existing source actor class thumbnail and is rasterized to `Resources/Icons/OrbitBillboard.png` for an Editor-only, selectable billboard component. The billboard uses a session-cached transient texture; no Content Browser asset is created. No toolbar is registered yet.

**Verification:** Restart the Editor after building so the new Slate style is loaded. Check **Edit > Plugins** for the plugin icon, place an `Orbit Gravity Source` and check its World Outliner icon, inspect the source class thumbnail in class selection views, then open a Character Blueprint's **Add Component** picker for `Orbit Character Gravity Component`. Rendering remains untested against the user's actual UE 5.8.3 installation.

## First physics test

1. Make a new test level. Use a static-mesh cube with **Simulate Physics** enabled and normal **Enable Gravity** checked.
2. Add an **Orbit Gravity Source** at a location away from the cube.
3. Set **Shape: Point**, **Direction: Inward**, **Strength: 980**, **Falloff: Constant** and **Max Distance: 0** (unlimited).
4. Press Play. The cube should accelerate toward the source position, not toward world -Z. Existing native body gravity is disabled while Orbit owns the component.
5. Stop PIE. In a normal PIE workflow, the editor copy of the actor remains unchanged.
6. Reverse the field direction and repeat. Switch to **Axis / Outward** for cylindrical or ringworld behavior.
7. For a spline habitat, use **Spline**, modify `GravitySpline` control points and enable `bShowGravityPreview` on the actor to visualize sample arrows.

**Note:** Orbit does not create ground or collision geometry. The designer must provide a suitably curved mesh and collision. A gravity source is not a planet mesh.

## Gravity source settings

| Setting | Behavior |
| --- | --- |
| Shape | Point, Axis, Directional, Volume, Spline |
| Direction | Inward, Outward, Spline Up or Spline Down (last two are spline-only) |
| Strength | Magnitude in cm/s², default 980 |
| Falloff | Constant, Linear within Max Distance, or Inverse Square |
| Max Distance | Zero means unlimited. For Axis, distance is radial to the axis; for Spline, distance to its closest curve section |
| Inverse Square Reference Distance | Strength is specified at this distance, and capped closer to center |
| Combine | Override or Additive |
| Priority | Higher-priority override wins. Additive sources above its priority still apply |
| Enabled | Toggle the source at runtime or in-editor |
| Affect Physics / Affect Characters | Channel flags evaluated by the common gravity field |
| Limit To Box | Optional extra rotated box mask; distinct from Volume Shape. Volume always uses its own shape boundary |
| Box Half Extents | Per-axis half extents of the Box Volume and/or extra box mask |
| Volume Shape | Box, Sphere, Cylinder, Capsule. Actual influence bounds, not visuals only |
| Volume Radius | Sphere radius, Cylinder radius, Capsule radius |
| Volume Half Height | Cylinder height from center to cap; Capsule total half-height including rounded caps |
| Spline Sample Spacing | Approximate spline-to-polyline sampling interval in cm |
| Show Gravity Preview | 3D sampling of the authored field in editor and PIE (cyan arrows) |
| Show Volume Bounds | Draw a green wireframe of the volume boundary even when gravity arrows are disabled |
| Preview Density | Samples per axis, 2–7; controls density/cost of the 3D preview |
| Preview Extent | Preview size for unbounded fields and spline sampling |
| Preview Arrow Length / Scale by Strength | Set arrow length; optionally scale relative to local acceleration magnitude |

**Directions:** For Point, Axis and Spline, Inward means toward the center/axis/curve, Outward means away. For Directional and Volume, Inward means the actor's local **-Z**, and Outward means its local **+Z**. For Spline Up, gravity follows the spline's authored Up vector; Spline Down is the opposite of authored Up. Spline point roll changes the oriented gravity direction.

**Axis:** The axis runs along the source actor's **local Z direction**. Rotate the *Gravity Source actor* 90 degrees about its local Y axis to turn a vertical axis into a horizontal one. Gravity must remain perpendicular to the **rotated** axis. It currently extends infinitely in both axial directions, subject to an optional rotated box mask.

**Spline:** The curve is sampled into line segments. The closest point on these segments defines the field. Closed loop splines work by connecting the ends; heavy, self-intersecting or dynamically moving curves are not yet optimized.

**Corkscrew corridor:** Set Shape = Spline, draw the spline along the corridor's centerline, Direction = **Spline Down (Oriented)**, and author the spline point **Roll** values to keep its Up direction pointing toward the local ceiling. `Max Distance` can confine influence around the corridor, but the current bound is radial, not an actual corridor-shaped collision volume. Orbit's existing character component only sets gravity direction; rotation of the capsule, camera, and controller is still an unfinished movement-integration milestone.

**Volume:** Box uses Box Half Extents. Sphere uses Volume Radius. Cylinder uses Volume Radius and Volume Half Height and is aligned to local Z. Capsule uses Volume Radius and total Volume Half Height (including rounded caps). Select Show Volume Bounds for a viewport wireframe even with gravity arrows off. No Orbit volume shape generates collision geometry. All volumes provide an actor-relative **directional** field, not radial attraction to their boundary.

## Blueprint API

From a world context, use **Sample Orbit Gravity**. The `FOrbitGravitySample` result contains:

- **Acceleration:** World-space acceleration (cm/s²).
- **Down / Up:** Unit vectors from the result; Up defaults to world +Z if no valid direction exists.
- **Magnitude:** Acceleration length in cm/s².
- **Has Influence:** Whether a custom Orbit field contributed; can be true for a zero-gravity/null direction.
- **Has Valid Direction:** Whether acceleration is nonzero and a local Up direction is meaningful.
- **Dominant Source ID:** Diagnostic source ID for the current runtime instance, not a replicated identifier.

Use **Project Onto Gravity Plane** to remove movement along local Up, and **Gravity Up From Acceleration** to safely handle null directions. The `Orbit World Subsystem` can also be queried directly.

## Optional Character Movement bridge

Add an **Orbit Character Gravity Component** to a Blueprint based on `ACharacter`.

It samples the Character channel and updates `UCharacterMovementComponent::SetGravityDirection()` every tick. It intentionally **does not** rotate the pawn/capsule, camera, controller, or animation setup. Full gravity-relative character locomotion requires further validation and is a later milestone.

Do not assume this adapter supports network prediction or every movement mode yet.

## Physics participation

Eligible, newly discovered `UPrimitiveComponent` instances with **Simulate Physics** and **Enable Gravity** on are managed automatically when at least one active physics-affecting Orbit source exists.

- **`Orbit.Ignore`** component tag: Excludes the component from Orbit gravity.
- **`Orbit.Force`** component tag: Includes a component whose original Enable Gravity was off.
- **Project Settings: Include Initially Gravity Disabled Bodies**: Alternative to setting `Orbit.Force` individually.
- Orbit records each managed component's original Enable Gravity value, disables native gravity to avoid double application, and restores the original value when Orbit stops managing it.
- Newly spawned components are picked up during the next scan, default every 0.2 seconds. Change **Physics Discovery Interval** to `0` for per-frame discovery at a higher cost.
- If no active Orbit field affects a point, the default setting uses that world's WorldSettings gravity. This fallback can be disabled for zero-gravity regions outside sources.

### Physics limitations in 0.1.0

This uses game-thread `UPrimitiveComponent::AddForce(..., bAccelChange=true)` once per game tick, **not** a Chaos physics-thread callback. It is suitable for early experiments with ordinary rigid bodies. It does not guarantee accurate substep forces, correct skeletal multi-body physics, sleeping-body behavior, or advanced vehicle/cloth/destruction behavior. The same source evaluator is built to support a future solver-level integration.

## Tests and documentation

- `Docs/ITERATION_001.md`: original gravity foundation deliverables and decision log.
- `Docs/ITERATION_002.md`: icon integration implementation, scope and verification checklist.
- `Docs/COMPATIBILITY.md`: implementation and validation status, without inflated support claims.
- `Docs/TEST_PLAN.md`: step-by-step Editor, physics, field and packaging smoke tests.
- `Docs/ARCHITECTURE.md`: module design, extension contracts and future roadmap.
- `Docs/Orbit_Master_Roadmap.md`: living project-wide roadmap.
- `Source/Orbit/Private/Tests/OrbitFieldTests.cpp`: eight Unreal automation tests under `Orbit.Field.*`.

## Release status

This is an **experimental source-first developer iteration**. The host environment does not contain the Unreal 5.8.3 SDK or UnrealBuildTool, so no actual Unreal compilation or runtime test has been performed here. Please provide the full Unreal build log if you encounter a compile error, and we can resolve it against the actual 5.8.3 engine headers.
