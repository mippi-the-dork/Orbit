## 0.2.2 - Iteration 013 - Character landing, facing, live Details and transforms

- Reworked the near-floor assist to use UE 5.8.3 native `K2_FindFloor` walkable sweep results instead of incorrectly feeding shortened floor-sweep hits into the actual-landing `IsValidLandingSpot` validation. Added native floor gap and walkability diagnostics. Manual sphere/low-g validation required; do not treat it as a verified fix yet.
- Added editor-only **Mirror Editor Changes to PIE** on Gravity Source (default true), copying only the changed editable Orbit property to matching PIE-world source actor(s) by GUID or exact actor/level identity. Exposed `Last Editor PIE Mirror Count` diagnostic. Retained game-world Blueprint setters.
- Added **Face Movement Direction** and smoothing/threshold controls to Orbit Character Controls. Actor facing follows latest gravity-tangent movement input independently of the quaternion Spring Arm camera.
- Added two Blueprint Library convenience wrapper nodes taking a Character target (Self in Character Blueprints) to resolve component target type errors without removing the original component API.
- Added four gravity-frame/transform Blueprint utilities (explicit gravity Up or Actor query, relative-to-world and world-to-relative transforms) compatible with Unreal's standard transform math nodes.
- Two regression tests added for transform round trips and gravity-tangent facing. Three maintained reference guides updated plus `Docs/ITERATION_013.md` and test plan.
- Optional OrbitMover companion version synchronized; Mover behavior and Chaos/rollback support unchanged.

**Verification:** Local source review, script checks, ZIP integrity checks. **Not compiled or PIE-tested against UE 5.8.3.**

## 0.2.0 - Iteration 011 - Optional Kinematic Mover Integration

- Added separately enabled `OrbitMover` companion plugin with the Blueprint-spawnable `Orbit Mover Gravity` component. The core `Orbit` plugin remains independent of Epic Mover.
- Applied the existing Orbit field's gravity acceleration and local Up through Mover's public overrides at the synchronous kinematic pre-simulation callback. Respect Orbit Receiver Gravity Channels and the source's Affect Characters setting.
- Explicit zero-gravity fields preserve a stable Up; source exit and disabled adapters relinquish Orbit overrides to native Mover.
- Added two optional Blueprint functions and runtime diagnostic properties, plus three pure regression tests for gravity/Up policy.
- Chaos Mover and async backends explicitly excluded until gravity can be injected before the backend's simulation input snapshot. Network rollback and source-state replay not implemented.
- Added and linked `Docs/MOVER_INTEGRATION.md`; updated existing component/API and Gravity Source references to cover the optional integration.

**Verification:** Complete source archive checked. Unreal Engine 5.8.3 compile, PIE and network tests are pending. No engine source changed.

## 0.1.9 - Documentation update - Gravity Source Actor and Blueprint API references

- Added `Docs/GRAVITY_SOURCE_ACTOR.md`, documenting every placeable Gravity Source property, all five source shapes, vortex physics behavior, gravity channels, combining, bounds, editor previews, live editing, limitations and practical examples.
- Kept the current `Docs/COMPONENTS_AND_BLUEPRINT_API.md` (updated to v0.1.9) in the same folder, cross-linked both references and updated the README index.
- Established a source-verification requirement for maintaining both reference documents in future Orbit iterations.
- Documented a known Additive-only influence-reporting limitation found during the source review; no runtime code changed in this documentation-only pass.

**Verification:** Both references were cross-checked against the Orbit v0.1.9 C++ headers and implementation. No Unreal compile or runtime validation was performed; plugin binaries and runtime source are unchanged.

## 0.1.9 - Iteration 010 - Character gravity and orientation foundation

- Extended the existing Orbit Character Gravity Component to apply gravity **magnitude** through native CharacterMovement.GravityScale as well as gravity direction. Gravity magnitude tracks live source Strength/falloff. Explicit zero-g fields set the scale to zero while retaining a stable orientation.
- Capture and restore native Character Movement gravity direction and scale as the character enters and exits matching Orbit fields. Channel selection and the existing "Use Default Gravity Outside Fields" property are respected.
- Added opt-in actor/capsule alignment to local Up with configurable quaternion smoothing and an option to return to the original Up when leaving a field. Off by default to avoid fighting an existing controller or camera.
- Added pure Blueprint nodes `Get Gravity Tangent Basis` and `Make Gravity Aligned Rotation`, backed by a shared numerical gravity-frame helper with defined behavior for invalid/collinear vectors.
- Added three `Orbit.Character.*` regression tests for tangent bases, orientation alignment and gravity magnitude conversion (18 total tests, 15 field + 3 character). Added dedicated Character test instructions and refreshed Blueprint/component documentation.
- Preserved the existing source actors, three Gravity Channels, physics path, visualizer, streamlines and the complete supplied Orbit icon set.

**Validation:** UE 5.8.3 documentation was consulted for the Character Movement direction and math interfaces, and the source/ZIP were statically checked. No UE installation is available in this environment, so this iteration has not been compiled or tested in PIE. Full controller, camera, animation, multiplayer and navigation integrations remain future work.

## 0.1.8 - Iteration 009 - Inward vortex spiral capture and live-edit clarification

- Added a **Spiral Capture** physics behavior (default) for Vortex Inward on Point, Axis and Spline gravity. It steers simulated bodies toward a converging rotational flow and brakes excess tangential speed to avoid the runaway ejection observed with continuously applied tangential acceleration.
- Preserved **Continuous Force (Legacy)** as a selectable physics mode. Vortex Outward remains continuous-force; a zero Vortex Spin keeps ordinary radial behavior.
- Added Blueprint-editable per-source controls for Convergence Rate, Angular Rate, Response Rate, Max Flow Speed and Max Acceleration Factor. Changing them at runtime takes effect as the world subsystem updates its snapshots each tick.
- Axis and Spline capture preserve the velocity component along the source's axis/tangent rather than arbitrarily braking movement down a tunnel.
- Preserved field sampling/streamlines and existing integration APIs. Their arrows visualize the spatial field, not momentum- and velocity-dependent steering.
- Added one new automation test (15 total) covering spin direction, over-speed braking, zero-spin and legacy compatibility, influence/channel isolation and eight-second numerical convergence.
- No modifications to other plugins or Unreal Engine source.

**Verification:** Source-level checks only; UE 5.8.3 build and physics/PIE confirmation are still required. The physics bridge remains game-thread AddForce, so results with collisions and fast-moving bodies may still differ from substep-level simulation.

## 0.1.7 - Iteration 008 - UE 5.8.3 compile fix

- Fixed `C4458` in `OrbitGravitySource.cpp` by renaming the streamline seed generator's local `Layers` variable to `RadialLayerCount`. `AActor` already declares `Layers`, and the project treats shadowing as an error.
- Updated its loop bound and radius calculation to use the new name. No gravity simulation, preview density, streamline behavior, or other feature logic was changed.
- Orbit only. Errors reported in Omni and Strand and the aggregate `MSB3073` from their build failure are outside this change.

**Verification:** Confirmed the changed references and complete plugin archive locally. A UE 5.8.3 build was not run in this environment.

## 0.1.6 - Iteration 007 - Gravity field streamlines

- Added an independently selectable **Show Streamlines** gravity preview. Purple curves trace the local **acceleration field** through space, with pale direction markers to indicate flow. These are *field lines*, not ballistic trajectories: existing momentum and collisions are intentionally excluded.
- Streamlines work with Point, Axis, Directional, Volume, and Spline fields, including the inward/outward signed vortex modes. Preview seeds are generated across the full relative 3D sample region or a tube around a spline.
- Added **Streamline Density** (1–10, relative to preview size), **Max Lines** (default 32), **Steps** (default 64), **Length** (default 2x displayed extent), **Max Segments** (default 2048), **Direction Markers** and **Thickness**. The existing cyan vector arrow preview remains optional and independent.
- Traces use midpoint/RK2 spatial integration through the same pure field evaluator used by physics and Blueprint queries. Trace termination rejects nulls, invalid values, field boundaries, and radial singularity reversal.
- Streamline geometry is cached and refreshed at most eight times per second while unchanged; every editor frame redraws cached geometry. Property edits or actor motion force a refresh. The cache is Editor-only and excluded from cooked builds.
- Added `FOrbitFieldEvaluator::TraceStreamline` as a reusable pure numerical helper and a fourteenth field automation test that checks straight directional lines, vortex curvature, boundary termination, and singularities.
- No changes to the gravity acceleration, physics, channel filtering, or existing visualization modes.

**Verification:** Static code and archive checks only. This package has not been compiled or run against UE 5.8.3; viewport appearance and preview performance require editor testing.

## 0.1.5 - Iteration 006 - Relative preview density and signed vortex gravity

- Expanded **Preview Density** to a 1–10 relative resolution over the entire currently visualized region. 1 samples 3 positions per axis, 10 samples 12. Widening Max Distance with Auto Fit expands the sample spacing, not the total number of samples. Spline previews scale their longitudinal, angular, and radial sampling together.
- Added **Preview Max Arrows** (default 2048) as an optional debug drawing safety cap. High preview densities are deliberately opt-in and can increase editor overhead; this setting does not affect runtime gravity.
- Added **Vortex Inward** and **Vortex Outward** direction modes (appended to preserve serialized existing modes) for Point, Axis, and Spline fields, with **Vortex Spin**: signed tangent-to-radial acceleration ratio. Positive values produce clockwise rotation viewed from the positive source axis toward its negative direction; negative values produce counterclockwise. Zero becomes ordinary inward/outward radial gravity.
- Point vortex spins around the actor's local +Z axis (with a smooth, zero-spin pole); Axis vortex around the full actor local +Z line; Spline vortex around each sampled curve segment's forward tangent. Reversing spline point order reverses the perceived sense of rotation.
- Vortex spin changes the acceleration **direction** without changing total acceleration magnitude. Default +1 yields a 45-degree spiral away from singularities. The source's existing Strength, falloff, Max Distance, priorities and Gravity Channels are still applied by the common evaluator.
- Directional / Volume gravity do not have radial vortex geometry and retain their linear directional behavior; vortex direction enum values in those shapes act only as inward/outward polarity. Spline Up and Spline Down remain distinct from vortex modes.
- Added three vortex-focused automation tests (13 `Orbit.Field.*` tests total). Fixed an existing automation guard so MaxDistanceShapes is compiled only when developer automation tests are enabled (Shipping build safety).
- No new movement, Chaos solver, or navigation integrations. Existing physics and Blueprint field queries automatically use the vortex acceleration because both consume the same evaluator.

**Verification:** Source/ZIP structure and static checks only. The plugin still needs compiling and runtime testing against Unreal Engine 5.8.3.

## 0.1.4 - Iteration 005 - Gravity channels, settling improvements, and max-distance preview

- Added three independent Gravity Channels (0/1/2), matching Unreal Lighting Channel defaults. All sources and actors default to channel 0 only. An optional `Orbit Gravity Receiver Component` configures actor participation in channels 1 and 2; ordinary physics cubes require no added component.
- Implemented source/receiver bitmask matching before prioritization and additive blending, including Blueprint actor/channel gravity queries. Unmatched fields cannot override matching ones.
- When outside a matching field or outside active influence, physics objects with world fallback enabled retain or recover normal native gravity. Channel-mismatched physics objects are never taken over, even with fallback disabled.
- Added experimental rest stabilization for eligible single-body static meshes: wake only for changed fields, and optionally sleep slow supported objects. No global restitution or damping changes; opt out using the `Orbit.NoRest` component tag. Actual bounce/settling still needs validation against Chaos substep integration.
- Updated previews so **Fit Preview to Max Distance** defaults on; raising a finite Max Distance expands 3D arrow samples, with fixed arrow-count bounds.
- Added independent visible **Show Max Distance Boundary** geometry (blue): spherical Point/Directional/Volume distance, radial Axis cylinder (finite display along an infinite axis), and a sampled Spline tube. Existing green Volume-shape bounds and optional yellow Box mask remain separate.
- Retained `Preview Extent` for unlimited fields, intentional preview clamping, and the visualized length of an infinite Axis source.
- Added Max Distance geometry regression test; total field tests: ten (pending engine validation).
- Updated source shape display label to avoid calling every Volume a Box.

**Verification:** Source archive, code references and ZIP structure checked. This environment cannot compile against the user's installed UE 5.8.3 engine. Physics rest tuning and all visualization changes require PIE/editor validation.

## 0.1.3 - Iteration 004 - Volume shapes, 3D previews, and source billboard

- Added real Box, Sphere, Cylinder and Capsule volume influence boundaries in the evaluator and viewport wireframes, respecting the source actor rotation.
- Fixed an existing direction error where Volume Inward and Outward both produced local +Z; Inward now correctly uses actor local -Z, Outward local +Z.
- Upgraded gravity arrows to 3D sampling, configurable density, preview extent, arrow length and acceleration-based scaling. Spline previews now sample tubular cross-sections.
- Added an Editor-only `OrbitBillboard` component and PNG rendition of the supplied `OrbitAsset_64.svg`. Texture is imported once per editor session into a cached transient texture, not a new content asset.
- Added `Spline Down (Oriented)` to support floor-directed gravity from authored spline Up/roll. Existing Spline Up behavior is preserved.
- Added two automated tests for volume boundaries/rotation and the opposite oriented spline directions (8 total field tests).
- Documented Axis actor rotation and corkscrew hallway setup; recorded that the user deferred spline/physics-tag tests.
- No changes to the experimental game-thread physics force integration, full character orientation, or navigation roadmap.

**Verification:** Source/ZIP checks completed. Unreal Engine 5.8.3 compilation, visual tests and PIE simulation remain pending.

## 0.1.2 - Iteration 003 - UE 5.8 finite-vector compatibility fix

- Replaced the unavailable `FVector::IsFinite()` method in `OrbitBlueprintLibrary.cpp` and `OrbitGravityTypes.cpp` with UE 5.8's `FVector::ContainsNaN()` check, inverted as appropriate.
- Preserved the existing behavior: reject vectors with NaN or infinity in any component, including invalid source positions, sample positions, and resulting acceleration.
- No changes to gravity calculations, Slate icons, runtime behavior, dependencies, or other plugins.

**Verification:** The reported Orbit compile errors have been addressed at the source level, and the archive was checked. An Unreal 5.8.3 build has not been run in this environment. The separate Unmark build error is outside Orbit and untouched.

## 0.1.1 - Iteration 002 - Editor icon integration

- Added the supplied 128px plugin icon as `Resources/Icon128.png` for Unreal's Plugin Browser.
- Added a separate, Editor-only `OrbitEditor` module that registers and unregisters a Slate style set.
- Registered the supplied 16px actor icon as `ClassIcon.OrbitGravitySource` for the World Outliner and class UI.
- Registered the general Orbit SVG for the existing character-gravity component's Add Component/class icon.
- Registered the 64px Orbit artwork as the gravity-source **class thumbnail**; no custom Content Browser asset type was added.
- Registered reusable general Orbit icon brushes for future editor menus and headers.
- Left all runtime gravity implementations and public APIs unchanged.

**Verification:** Resource and packaging checks completed locally. UE 5.8.3 compile and visual verification remain pending.

# Changelog

## 0.2.1 - Iteration 012 - Character Movement reliability

- Added same-world runtime Source **Set Gravity Strength** and World Subsystem **Refresh Active Sources** Blueprint nodes; PIE-world Details changes also refresh active source snapshots immediately.
- Added PIE diagnostics for actual applied native gravity scale, dominant field ID and near-floor walkable sweep distance.
- Added opt-in **near-floor landing assist** gated by native UE 5.8.3 gravity-relative capsule sweep, landing validity, Falling movement mode, and slow descending speed. No teleport or fake floor hits.
- Added optional **Orbit Character Controls** component with gravity-tangent movement and quaternion/parallel-transport camera, preferentially rotating the existing Spring Arm directly to avoid world pitch clamp. Input bindings must be rewired deliberately in the Third Person character Blueprint.
- Added three pure regression tests, documentation, sphere repro instructions, and updated all three maintained guides. OrbitMover code unchanged.

**Verification:** Source authored and ZIP consistency checked. Engine compilation, automated test execution and PIE landing/camera validation not available in this environment.



## 0.1.0 - Iteration 001 - Experimental

- Established the Orbit runtime plugin and UE 5.8.3 source layout.
- Added Point, Axis, Directional, Volume and editable Spline gravity fields.
- Added inward/outward gravity, spline-local Up, three falloff models, box masks, influence radius, and source combination policies.
- Added a world subsystem exposing unified gravity queries to Blueprint and C++.
- Added experimental automatic physics gravity for eligible simulated components with original-gravity restoration and `Orbit.Ignore` / `Orbit.Force` tags.
- Added optional Character Movement gravity-direction adapter, without claiming complete arbitrary-gravity locomotion.
- Added editor field previews, project settings, six field tests, a compatibility matrix and a technical roadmap.

**Build note:** Unreal Engine 5.8.3 compilation and gameplay tests have not yet been performed. This is a developer source iteration rather than a tested public release.
