# Orbit

**Orbit** is a C++ gravity field plugin for Unreal Engine 5.8.3. Designers can place gravity sources in a level to author spherical, cylindrical, directional, bounded-volume, and spline-shaped gravity. Gameplay code can query the same field, while an initial runtime subsystem automatically applies custom acceleration to eligible physics-simulating components.

**Current version:** 0.1.8 (Iteration 009, experimental)  
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
| Inward and outward fields, plus signed radial vortex modes | Implemented for Point, Axis and Spline; pending UE test |
| Velocity-aware inward vortex spiral capture for rigid bodies | Implemented as configurable alternative to continuous tangential force; pending UE test |
| Inverse-square, linear and constant strength | Implemented; pending UE test |
| Bounded influence and rotated box masks | Implemented; pending UE test |
| Priority overrides and additive combinations | Implemented; pending UE test |
| Zero-gravity field and null-direction detection | Implemented; pending UE test |
| Runtime world subsystem and Blueprint queries | Implemented; pending UE test |
| Automatic ordinary simulated rigid-body gravity | **Experimental** game-thread `AddForce` bridge |
| Gravity-enabled-state preservation and opt-out tags | Implemented; pending UE test |
| Optional Character Movement gravity-direction bridge | **Experimental**, not a complete locomotion/camera solution |
| Editor volumetric field arrows and purple streamlines, relative density (1–10), influence wireframes | Implemented; pending UE test |
| Actor viewport billboard from supplied Orbit artwork | Implemented with Editor-only transient texture; pending UE visual test |
| Orbit editor icons | Registered for plugin browser, gravity-source actor, character gravity component and class thumbnail; pending UE visual test |
| Field math automated tests | 15 tests authored, including streamline tracing and vortex capture; pending UE 5.8.3 verification |
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
| Direction | Inward, Outward, Vortex Inward/Outward (Point/Axis/Spline), Spline Up or Spline Down (last two are spline-only) |
| Vortex Spin | Signed direction / magnitude for the vortex (positive clockwise, negative counterclockwise). In Spiral Capture, scales angular speed. 0 = radial, without velocity capture. |
| Vortex Physics Mode | For simulated rigid bodies under Vortex Inward: Spiral Capture (default) or Continuous Force (legacy, may eject bodies) |
| Vortex Convergence Rate | Fractional rate at which the desired spiral approaches its axis/centerline (default 0.5 per second) |
| Vortex Angular Rate | Desired angular speed in degrees per second at abs(Spin)=1 (default 63) |
| Vortex Response Rate | How rapidly a simulated body steers toward the desired flow (default 6 per second) |
| Vortex Max Flow Speed | Maximum target speed for Spiral Capture (default 1500 cm/s) |
| Vortex Max Acceleration Factor | Steering acceleration limit relative to local field Strength after falloff (default 3x) |
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
| Show Gravity Preview | 3D sampling of the authored field in editor and PIE (cyan instantaneous acceleration arrows) |
| Show Streamlines | Separate purple flow lines tracing the acceleration field; can be displayed alone or with arrows |
| Streamline Density | Relative 1–10 seed distribution and line count over the same displayed region; higher values reveal local field variation |
| Streamline Max Lines / Steps | Default 32 lines and 64 RK2 tracing steps per line; editor preview cost limits |
| Streamline Length | Maximum traced distance relative to the displayed source extent (default 2x) |
| Streamline Max Segments | Total per-source line-segment budget (default 2048) independent of vector arrow budget |
| Streamline Direction Markers / Thickness | Optional small arrows along each traced line and adjustable line weight |
| Fit Preview to Max Distance | Enabled by default; expands arrows through the finite range instead of stopping at Preview Extent |
| Show Max Distance Boundary | Blue wireframe: sphere for Point/Directional/Volume, radial cylinder for Axis, sampled tube for Spline |
| Max Distance 0 (unlimited) | Shows only the finite Preview Extent rather than an infinite visualization |
| Show Volume Bounds | Draw a green wireframe of the volume boundary even when gravity arrows are disabled |
| Preview Density | Relative visualization density 1–10, independent of the field size; 3–12 positions per axis, or corresponding spline tube samples |
| Preview Max Arrows | Optional cap on emitted debug arrows; default 2048. High density can be costly in editor. |
| Preview Extent | Visualization size for unbounded fields and optional preview cap; also Axis boundary drawing length (the field itself stays infinite along the axis) |
| Preview Arrow Length / Scale by Strength | Set arrow length; optionally scale relative to local acceleration magnitude |

### Live editing during gameplay

Gravity Source field properties are `EditAnywhere, BlueprintReadWrite` and the runtime world subsystem rebuilds source snapshots every tick. Changing a source in a running PIE world, or modifying its exposed properties from Blueprint/C++, updates the field on the following tick. Moving, rotating and changing spline point data is also supported, but spline snapshots currently rebuild each frame and can be costly. Project Settings are configuration settings and are not an in-game tuning UI. Editor preview drawing is compiled only in editor builds. Orbit sources are not replicated and changes will not automatically synchronize in multiplayer.

### Spiral Capture and field-line previews

The original Vortex Inward continuously applied tangential acceleration, which could pump velocity into a body until it escaped from the central region. `Vortex Physics Mode: Spiral Capture` is now the default for Vortex Inward on simulated rigid bodies. It uses the body's current velocity to guide it toward a converging rotational flow, including braking excessive tangential speed as the orbit tightens. `Continuous Force` preserves the original behavior for intentionally chaotic effects. Vortex Outward retains the original force-based approach; non-physics gravity sampling and Character Movement direction queries still return the instantaneous acceleration field. Cyan arrows and purple streamlines illustrate that field, not the velocity-dependent rigid-body capture force nor actual collision trajectories. No change to the Chaos substep integration status.

**Streamlines vs trajectories:** Streamlines show the direction of the gravity field at successive positions, not the future trajectory of an object with existing velocity. They never simulate inertia, restitution, collisions or physics time steps. For vortex debugging, enable streamlines to see how the vector field spirals; enable cyan arrows as well to inspect local acceleration. For unlimited sources, Preview Extent limits the seed region and maximum trace length; finite fields terminate at their actual influence boundaries. Streamline previews display this source by itself, not the combined result of overlapping sources.

**Vortex directions:** For Point, Axis, and Spline, **Vortex Inward / Vortex Outward** combine inward/outward acceleration with an orbiting tangent. `Vortex Spin` is a signed *tangent-to-radial acceleration ratio*, **not** angular velocity: `0` = purely radial, `+1` = 45° clockwise, `-1` = 45° counterclockwise. At large absolute values the acceleration is mostly tangential, so bodies may orbit instead of entering/exiting the source quickly. Clockwise means viewed from positive source local Z looking toward negative Z; for splines, viewed from positive spline tangent looking backwards along the spline. Point vortex uses the actor's local +Z as its spin axis. Changing source rotation (or reversing a spline's point order) changes the spin reference. The magnitude always remains determined by Strength and falloff. These modes are not supported as actual swirl fields on Directional/Volume shapes; those retain ordinary directional behavior. They do not automatically rotate a character's camera or capsule.

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
- If no matching Orbit field affects a point, the default setting uses that world's WorldSettings gravity. When fallback is disabled, eligible bodies with at least one channel-compatible source enter zero gravity outside its influence; bodies with no compatible channel retain native gravity.

### Gravity Channels (introduced in 0.1.4)

Orbit has three source/receiver **Gravity Channels**, similar to Unreal's Lighting Channels and independent of physical collision channels:

- Both **Gravity Sources** and ordinary actors default to **Channel 0 only**.
- To change an actor, add **Orbit Gravity Receiver Component** and check channels 0/1/2 in its Details panel.
- A source influences a receiver when **at least one enabled channel matches**. A source or receiver with all channels disabled matches nothing.
- The same rule applies to Physics, Characters, and actor-based Blueprint gravity queries.
- A mismatched source will not claim an ordinary physics body or disable that body's native gravity.
- Existing `Physics / Character / General` query type is a separate integration selector, not one of the three Gravity Channels.

Three channels are stored in a three-bit mask. Channel matching is a cheap bitwise comparison; the cost is dominated by physics/body discovery and field evaluations, not the number of channel bits.

### Resting physics (experimental in 0.1.4)

In **Project Settings > Plugins > Orbit > Physics > Rest**, `Stabilize Resting Rigid Bodies` is on by default. Orbit avoids reapplying gravity to sleeping bodies unless the sampled field changes significantly. For supported low-speed, single-body static meshes in stable contact along local gravity, an optional rest timer puts the body to sleep. Use the component tag `Orbit.NoRest` to opt out. Rest stabilization does not adjust the physics material's bounce or change physical damping, so some bounciness may remain with game-thread forces until Chaos substep integration.

### Physics limitations in 0.1.4

This uses game-thread `UPrimitiveComponent::AddForce(..., bAccelChange=true)` once per game tick, **not** a Chaos physics-thread callback. It is suitable for early experiments with ordinary rigid bodies. It does not guarantee accurate substep forces, correct skeletal multi-body physics, sleeping-body behavior, or advanced vehicle/cloth/destruction behavior. The same source evaluator is built to support a future solver-level integration.

## Tests and documentation

- `Docs/ITERATION_001.md`: original gravity foundation deliverables and decision log.
- `Docs/ITERATION_002.md`: icon integration implementation, scope and verification checklist.
- `Docs/COMPATIBILITY.md`: implementation and validation status, without inflated support claims.
- `Docs/TEST_PLAN.md`: step-by-step Editor, physics, field and packaging smoke tests.
- `Docs/ARCHITECTURE.md`: module design, extension contracts and future roadmap.
- `Docs/Orbit_Master_Roadmap.md`: living project-wide roadmap.
- `Source/Orbit/Private/Tests/OrbitFieldTests.cpp`: ten Unreal automation tests under `Orbit.Field.*`.

## Release status

This is an **experimental source-first developer iteration**. The host environment does not contain the Unreal 5.8.3 SDK or UnrealBuildTool, so no actual Unreal compilation or runtime test has been performed here. Please provide the full Unreal build log if you encounter a compile error, and we can resolve it against the actual 5.8.3 engine headers.
