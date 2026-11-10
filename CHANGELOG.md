## v0.2.36 — Iteration 047

- Clarify Volume direction precedence in Actor and Component Details by disabling Field Direction for nonlegacy Volume modes without resetting it.
- Rename the cylinder-axis dropdown labels to Local Z Axis (internal enum values unchanged).
- Add two native regression tests for editor metadata and all-shape direction precedence.
- Preserve Standard/Experimental behavior and legacy defaults.

## v0.2.35 — Iteration 046

- Dual corner box authoring with automatic legacy half-extents migration, preserved serialized values and editor gizmos.
- Shared Standard/Experimental volume direction modes (center, cylinder axis, local Z) with legacy directional default.
- Limit-box and max-distance wireframe color/thickness customization.
- Two native gravity field regression tests; C++/UE runtime validation pending.

## 0.2.34 — Iteration 045 (partial particle lifecycle safety)

- Guard experimental gravity ownership against Chaos particle identity replacement during physics-state recreation.
- Avoid restoring an old particle's gravity group onto the new particle.
- Add live tests for simulate-physics toggle and physics-state recreation / reacquisition.
- Correct non-simulating fixture mobility to avoid the SetStaticMesh PIE warning.
- Automatic runtime spawn interception and skeletal multi-particle Chaos ownership remain outstanding.

## 0.2.32 — Iteration 043 (explicit runtime gravity ownership)

- Added `PrepareSpawnedPhysicsBody` on the world subsystem for opt-in synchronous ownership of configured runtime-spawned static-mesh physics bodies.
- Shared ownership eligibility code between pre-BeginPlay and runtime paths; commits new Chaos receiver channel snapshots immediately.
- Two new live Chaos regression tests (UE validation pending). No default settings changed; skeletal and automatic first-step ownership remain open.

## 0.2.31 — Iteration 042 (receiver channel and Vortex Chaos parity)

- **No change to Standard, existing opt-in defaults, public Blueprint node signatures or gravity authoring.** Two independent **default-off** experiments: `Experimental: Receiver Channel Compatibility` and `Experimental: Vortex Compatibility`. Earlier modes retain selection behavior when these are disabled.
- Receiver channel parity: the existing Chaos group-7 pre-integrate callback receives a copied map of **Chaos particle UniqueIdx -> original Orbit receiver mask** (channels 0/1/2 and combinations). Each body evaluates the same `CombineForBody` physics field selection as Standard. Unknown bodies get **zero custom acceleration**, never another body's channel. Mask changes and group release update the registry; no UObject access on the physics thread. Channel mode takes precedence only when explicitly opted in.
- Vortex parity: Point/Axis/Spline Vortex Inward/Outward use the existing evaluator. Continuous Force uses field acceleration; inward Spiral Capture receives actual Chaos substep particle velocity for target-flow steering. Both modes are selected only when explicitly opted in. Mixed vortex/nonvortex fields require valid expanded eligibility; mixed channels + vortex require **both** options.
- Supports existing field Additive/Override/Priority/Strongest/Blended rules, source rotation/bounds, gravity receiver component, and normal source lifecycle via the same data path. No new callback object, extra gravity group or manual sleep/damping adjustment.
- Five new native math/policy tests and **five** new live-world tests for independent receivers, union masks, live receiver switching, Vortex Continuous, Vortex Spiral Capture and runtime-spawned receiver discovery. Existing 29+9 suites preserved. Expected **34 GravityIntegration / 14 IntegrationWorld** tests. All 10 new tests are pending UE 5.8.3 compilation and execution; no claim of physics validation until reported.
- Retained the existing 127-column Motion History format. Channel/Vortex usage reports through the existing `experimental_chaos_expanded_field_gravity` diagnostics, not separate per-mode columns. A future diagnostics extension can add per-mode labels if desired without silently changing existing exported CSV schemas.
- Remaining parity gaps: first-physics-step ownership of dynamically spawned bodies, skeletal/articulated or other compound particles, >32 physics sources, end-to-end APIs/character/Mover/regression, real performance and packaged-game revalidation. See [`Docs/COMPATIBILITY_PARITY.md`](Docs/COMPATIBILITY_PARITY.md) and [`Docs/ITERATION_042.md`](Docs/ITERATION_042.md).

## 0.2.30 — Iteration 041 Hotfix 2 (directional sleep/wake test setup)

- Corrected the editor-only `DirectionalNaturalSleepAndWake` test to tilt the Directional source by 90° **pitch** instead of rotating 90° **yaw**. Orbit gravity follows the negative source-Up axis; a yaw-only rotation does not change an upright source's gravity direction.
- Added explicit source-field and subsystem-snapshot assertions before evaluating natural wake and movement along the changed gravity. This distinguishes an invalid test configuration from a true physics wake failure.
- User validated Iteration 041 Hotfix 1: **8/9** live-world tests and **29/29** field/policy tests. The sole failed live-world test was configured with an unchanged gravity vector. Retest of this hotfix remains pending UE 5.8.3 execution.
- **Test-only change.** Runtime modules, Standard/Experimental behavior, default-off opt-ins, Blueprint APIs, and diagnostics are unchanged.

## 0.2.30 — Iteration 041 Hotfix 1 (automation test compile fix)

- Fixed `TestNotNull()` argument type for `GravitySpline` in `OrbitChaosWorldIntegrationTests.cpp` by passing `.Get()` from `TObjectPtr<USplineComponent>`.
- No gravity runtime changes; Standard remains the default and Experimental stays opt-in.
- Pending UE 5.8.3 compilation and execution of the nine live-world tests.

## 0.2.30 — Iteration 041 (Native live-world Chaos automation)

- Added **nine** editor-only Unreal Automation Framework tests in `Orbit.Physics.IntegrationWorld`, using UE's `FTestWorldWrapper` to spawn/tick isolated temporary game worlds instead of editing or requiring a test map.
- Coverage: live Axis motion, Spline Up and Down motion, inside/outside behavior for all four Volume shapes, mixed priority motion, actual Vortex Standard fallback, moving source response, native sleeping/waking on a collision floor, nondefault-channel Standard fallback, and source-destruction gravity restoration.
- Each test temporarily opts into Experimental/Expanded with Early Ownership and both rest interventions off, restores all changed `UOrbitSettings` values on exit, and never writes project config or level assets. The user's Project Settings may remain at Standard.
- No production gravity simulation, settings defaults, public API, gravity source schema or 127-column CSV changes from Iteration 040 Hotfix 1. Experimental remains opt-in; Standard remains the default.
- Updated maintained `Docs/INTEGRATION_TESTING.md`, `Docs/EXPERIMENTAL_GRAVITY.md`, `Docs/COMPATIBILITY_PARITY.md`, and `Docs/TEST_PLAN.md`.
- **Validation:** The existing **29/29** Gravity Integration unit/policy tests were reported passing on UE 5.8.3. The nine new live-world tests are newly authored and **have not been compiled or run in UE**; results must not be inferred from static checks.

### 0.2.29 — Iteration 040 Hotfix 1 (automation-only)

- Correct `ExpandedMixedFieldParity` to match Standard's established priority semantics: an active Override suppresses Additive fields of equal or lower priority; a higher-priority Additive contributes alongside the Override.
- Verify both cases in the native Automation Framework test and compare the original fields with the copied solver snapshot. No runtime physics, settings, Blueprint API, metadata, or diagnostics changed.
- Add [`Docs/ITERATION_040_TEST_RUNBOOK.md`](Docs/ITERATION_040_TEST_RUNBOOK.md) with exact opt-in settings, commands and a reduced manual test matrix. The 29 tests cover field math/policy, not actual Chaos sleeping or collision.

## 0.2.29 — Iteration 040

- Started a maintained full [Standard / Experimental compatibility inventory](Docs/COMPATIBILITY_PARITY.md) covering source shapes, receivers, Blueprint helpers, components, body classes, Mover, ownership, diagnostics, packaging and testing gaps.
- New default-Off **Experimental: Expanded Field Compatibility** setting. Routes eligible Axis, Spline, Volume, finite Directional and rotated/bounded position-based gravity fields (including mixed combinations, up to 32 channel-0 sources) through the existing solver callback and shared field math. Prior experimental modes take precedence; Standard remains the default.
- Vortex, unsupported receiver channels and compound/skeletal physics bodies continue safely falling back to Standard. No manual sleep/damping/friction changes.
- Appended `bExperimentalChaosExpandedFieldGravity` diagnostics and column 127 to Motion History, retaining earlier columns. Analyzer recognizes new mode.
- Seven new native Automation Framework policy/math tests (29 total authored); Windows UE 5.8.3 compile, PIE and package validation pending.

## Orbit 0.2.28 | Iteration 039 | Compatibility, profiling procedure and shipping gates

- **No runtime gravity behavior changes.** Standard remains the default and all existing experimental mode options retain their previous defaults, selection behavior and CSV columns (126 total). No new sleeping intervention, damping, friction, force injection or callback was added.
- Add four **native Unreal Automation Framework** Gravity Integration policy/field tests (22 total): deterministic moving-source restoration, Mixed → Multi-Point → Single → Standard transitions, finite Point range/linear falloff, and channel receiver isolation. These tests do not replace PIE physics validation.
- Add a standard-library-only offline Python analysis tool for Motion History `.csv`/`.zip` recordings, exposing sample completeness, individual body speed by PIE-time window, final awake state and active solver flags. Include three tool smoke tests. **It is not a Chaos profiler or a contact-energy measurement.**
- Extend the existing Windows game packaging helper to accept `-Configuration Development` or `Shipping`, maintaining Shipping as the default, plus separate-archive and input checks. This does not run or certify a Shipping build.
- Maintain `Docs/EXPERIMENTAL_GRAVITY.md` with Iteration 038's user-verified stationary and controlled moving-source observations; add `Docs/PERFORMANCE_VALIDATION.md`, `Docs/ITERATION_039.md`, and refresh Shipping validation.
- **Validation pending:** UE5.8.3 compile, 22 automation tests, live PIE regression, actual Unreal Insights profiling and clean packaged Win64 Development/Shipping runtime.

# Orbit 0.2.27 | Iteration 038 | Opt-in Mixed Point + Directional, regression and profiling

- Added **Experimental: Mixed Point + Directional Sources**, **Off by default**, independent of the earlier **Multiple Point Sources** option. Neither Standard projects nor existing Experimental multi-Point projects change eligibility merely by updating.
- Eligibility is deliberately limited to **2–32 sources** containing at least one ordinary inward/outward Point and at least one **unlimited constant-strength Directional** source. All physics sources and participating receivers must use only Channel 0. No vortex, box-masked fields, Axis, Spline, Volume, finite Directional or Directional falloff support in this pass. Unsupported configurations continue using the original Standard path.
- Reused the existing per-world Chaos callback, per-particle evaluation, and `FOrbitFieldEvaluator::CombineForBody` for Additive, Override and priority parity. No new group, sleeping adjustment or engine modification. Existing single-source and multi-Point runtime logic is preserved.
- Added a fourth mutually exclusive runtime diagnostic and appended **CSV field 126**, `experimental_chaos_mixed_point_directional_gravity`. The original 125 columns are preserved unchanged. Both initial and awake/sleeping diagnostics identify the mixed mode.
- Added **five native Automation Framework tests** (18 total in `Orbit.Physics.GravityIntegration`) for opt-in/mode switching, eligibility and fallback, field math, moved source math and evaluator-cost reporting. Field evaluator benchmark logs work at 2, 8, and 32 sources with 64/256 bodies; it **does not** claim to measure the complete Chaos simulation. Added a native physics-thread stat scope `OrbitExperimentalGravityPreIntegrate` for actual profiling.
- Added [`Docs/SHIPPING_VALIDATION.md`](Docs/SHIPPING_VALIDATION.md) and an optional PowerShell `Scripts/RunShippingSmokeTest.ps1` helper to run a real Windows game Shipping package on the user's UE installation. **No Shipping build was run in this environment.** A packaged plugin Editor build is not a game Shipping build.
- Updated the maintained [`Docs/EXPERIMENTAL_GRAVITY.md`](Docs/EXPERIMENTAL_GRAVITY.md), new iteration notes, test plan and source compatibility documentation.
- **Validation pending:** Windows UE 5.8.3 compile, 18 automation tests, mixed-field PIE simulation, profiling comparisons and clean C++ and Blueprint-only Win64 Shipping build. Prior single-Point/multi-Point manual successes do not prove the new mixed-field path.

## Orbit 0.2.26 | Iteration 037 | Separately opt-in multiple Point solver gravity

- Add **Experimental: Multiple Point Sources**, **Off by default**, in Project Settings > Orbit > Physics > Gravity Integration. Standard remains default and existing single-source Experimental projects retain their prior behavior until independently opted in.
- Support 2–32 ordinary Channel-0-only Point physics sources per world through the **existing** Chaos group-7 callback. On each physics step, evaluate the full per-body field combination using Orbit's existing Additive, Override, priority, influence resolution, range and falloff rules, without submitting simultaneous game-thread `AddForce` for solver-owned bodies.
- Constrain eligibility: mixed shapes, vortex/box limits, nondefault source channels, >32 eligible physics sources and incompatible group ownership retain Standard. Nondefault/multichannel receiver bodies remain Standard even in otherwise eligible multi-Point worlds.
- Preserve existing single Directional/Point paths, dynamic source snapshots, early-ownership opt-in, field-change wake handling, body group restoration and Standard fallback. No manual sleep or force-based workaround added.
- Add Receiver diagnostics and append-only **125th Motion History CSV column** `experimental_chaos_multi_point_gravity` without reordering previous columns. Add **five** native UE Automation Framework tests (13 GravityIntegration tests in total).
- Update the maintained [`Docs/EXPERIMENTAL_GRAVITY.md`](Docs/EXPERIMENTAL_GRAVITY.md), `Docs/ITERATION_037.md`, motion history and test plan. Iteration 036's early-ownership 8/8 test results and initial 2/6 versus 6/6 sleeping result were user-verified and are reflected in the maintained guide.
- **Not yet UE 5.8.3 built, PIE-tested or Shipping-packaged.** Multi-point sleeping, moving-source behavior and performance require direct validation.

## Orbit 0.2.25 | Iteration 036 | Experimental pre-BeginPlay gravity ownership

- Added an **additional opt-in**, default-OFF `Experimental: Claim Gravity Before BeginPlay` setting. Standard and existing Experimental gravity projects are unchanged unless this second setting is enabled.
- For eligible single Directional or Point solver fields, discover and assign pre-existing simulated Static Mesh bodies in `OnWorldBeginPlay`, before actor `BeginPlay` and the first regular subsystem tick. Reuses the existing group-7 callback and does not run early AddForce or modify World Gravity Z.
- Unsupported fields, body types, nonmatching channels, out-of-range bodies and initially gravity-disabled/ignored bodies retain the prior path. Runtime-spawned bodies retain existing tick discovery.
- Added two native Unreal Automation Framework policy tests (8 GravityIntegration tests total) and maintained `Docs/EXPERIMENTAL_GRAVITY.md` with user-confirmed iteration 035 results, first-step verification steps and the Chaos weak-gravity sleep limitation.
- This startup experiment is **not yet UE 5.8.3 compiled or PIE-proven**; no claim of complete first-step ownership in all game modes.

## Orbit 0.2.24 | Iteration 035 | Opt-in per-particle Point solver gravity

- Expand the **existing** Experimental Chaos method to support exactly one ordinary inward/outward Point gravity source, evaluating unique acceleration for each eligible rigid body on each Chaos physics substep. The proven single uniform Directional path remains.
- Preserve Standard as the default and its existing AddForce implementation unchanged. Keep the serialized experimental enum value stable for existing settings. Axis/Spline/Volume, vortex, box-masked Point, multi-source and unsupported bodies remain Standard.
- Avoid introducing any solver callback in Standard mode. Keep group-7 exclusive ownership and restoration, with distinct receiver diagnostics for uniform versus Point gravity.
- Append CSV column 124 `experimental_chaos_point_gravity` without changing previous columns. Add three native Automation Framework policy/math tests (six gravity-integration tests total).
- **New maintained reference:** `Docs/EXPERIMENTAL_GRAVITY.md` documents settings, supported/unsupported combinations, fallback, diagnostics, tests, risks, and ongoing validation. See also `Docs/ITERATION_035.md`.
- **Not yet compiled or PIE-validated in UE 5.8.3.** Natural sleep on curved worlds remains the principal hypothesis to test. Existing project behavior unchanged unless experimental mode is explicitly selected.

## Orbit 0.2.23 | Iteration 034 | Opt-in directional Chaos gravity-group experiment

- One Orbit package and codebase. Added `Rigid Body Gravity Application`: Standard (default, unchanged) or Experimental Chaos Uniform Gravity (explicit opt-in).
- For exactly one unlimited, constant, Directional physics source, eligible gravity-enabled static meshes use Chaos gravity group 7 and a per-world solver callback to set its vector instead of per-frame `AddForce`. All other cases use the existing Standard path. Rest/damping, characters, OrbitMover, receiver fields and source authoring are unchanged by default.
- Restore each eligible body's original gravity group and flag on release or mode switch; refuse detected group-7 conflicts and preexisting nonzero groups. Experimental mode is not compatible with integrations independently using group 7.
- Expose the active solver path through Receiver diagnostics and append-only CSV column `experimental_chaos_uniform_gravity` (123 columns, old 122 indices unchanged). Add three native Automation Framework policy tests.
- **Not yet UE-compiled or PIE-proven.** Do not enable experimental mode in production. Detailed requirements and a first-play-test matrix are in `Docs/ITERATION_034.md`.

## 0.2.22 | Iteration 033 | Opt-in first-person camera-facing

- Add **Facing Mode** on Orbit Character Controls: Movement Direction (default, compatible with all existing assets), Camera Direction (opt-in gravity-relative first-person yaw), Manual (external facing owner).
- Preserve legacy `bFaceMovementDirection` and movement-facing response defaults. Camera Direction instead follows the gravity-tangent camera heading even at zero velocity; capsule remains upright relative to gravity and ignores camera pitch. Preserve heading when looking nearly vertically.
- Add independent Camera Facing Response Rate (default 0, snap). Reuse existing post-movement character rotation path and restore native yaw flags when ownership ends. Camera mode requires locally controlled Character and valid Controller.
- Add three native UE Automation Framework tests and strengthen legacy-default assertions. No physics/gravity/Chaos, Character standing-load or Mover behavior changes. Existing first-person controller-rotation fallback limitations at extreme gravity axes remain documented; live PIE validation required.

## 0.2.21 | Iteration 032 | Character pressure isolation and controlled gravity comparison

- Address reported Character Movement behavior when standing/jumping onto Orbit-simulated cubes. Added a selective Character Gravity component option to temporarily reduce the **additional native Character Movement standing force** only on a currently Orbit-managed simulated movement base, with author-value restoration on exit. Default extra standing force on Orbit bases is 0; ordinary impact/push/repulsion impulses are not disabled. This is an isolated mitigation requiring PIE validation, not proof of root cause.
- Added optional **native world-gravity equivalence A/B** setting, default OFF and restricted to bodies originally using native gravity and effective field vectors matching `(0,0,WorldGravityZ)` within 1 cm/s². The native and Orbit AddForce paths are mutually exclusive. This is a flat-scene diagnostic, not spherical-gravity support or a Chaos callback backend.
- Added `IsOrbitManagedPhysicsBody` C++ read-only helper and testable policy gates. Two new native Automation Framework tests (33 targeted across Physics Rest, Motion History, Mover, and Integration/Character Interaction). CSV remains 122 columns. New `Docs/ITERATION_032.md` documents the flat 2x2 comparison and planet/character tests.
- Retains current substep-aware frame `AddForce` path. No unverified low-level Chaos simulation callback, no modified engine, no Probe. User validated Iteration 031 with 31/31 native tests and a full planet PIE capture; Iteration 032 requires UE 5.8.3 rebuild, automation, PIE, and packaging verification.

## 0.2.20 | Iteration 031 | Recoverable resting and support-query cache

- Recoverable manual-sleep suppression: after repeated immediate wakes, require sustained stable support and elapsed retry cooldown; grant a single probationary manual-sleep request, then return to suppression on failure. Native Chaos sleeping, field-change wakes, impulses and collisions remain unchanged.
- Distinguish simple authored sphere collision from boxes/convex/compound shapes for rest position displacement: sphere envelope defaults to 1.0 cm, generic remains 0.25 cm. Both still require slow tangent/angular motion, stationary contact and bounded solver-normal velocity.
- Short-lived confirmed side-obstruction cache (default 0.12 s) reduces redundant repeated support sweeps, but expires and invalidates on changed geometry/gravity and falls back to unfiltered queries if support disappears. This affects only the support query, not real physics collisions.
- Append five motion-history CSV fields (122 total). Add four native Automation Framework tests for stable recovery, sphere geometry rest threshold, cache invalidation and deterministic multi-frame policy, 31 targeted tests expected.
- Iteration 030 (27/27 targeted) and two 87-body PIE captures were verified by the user. Iteration 031 source/ZIP checks performed here; still requires UE 5.8.3 compile, Automation, PIE and BuildPlugin verification. No Probe dependency.

## v0.2.19 | Iteration 030 | Native wake evidence and rest-episode control

- Bound manual-sleep retries after repeated immediate wakeups. Following two failures per stable-contact episode, Orbit leaves sleep decisions to Chaos until sustained genuine movement/loss of support or an explicit field-change wake starts a new episode. The existing two-second cooldown remains.
- Require a stationary position envelope while permitting small solver-normal velocities, with separate low tangent/angular speed and verified stable support requirements; does not cancel any gravity forces or alter native contact impulses.
- Add at most two optional short support-query retries which exclude only rejected sideways blockers, preserving physical collision/stacking and the 0.70 support normal threshold. Default one retry, no extra query when the first sweep finds support.
- Bind/unbind native component wake/sleep observers without changing the component's Generate Wake Events setting. Log/capture notifications when available, without claiming a notification identifies the actual wake cause. Event counters supplement per-frame awake-state comparisons.
- Append 12 diagnostics to motion-history CSV (117 total); the first 105 columns are unchanged. Add three pure Automation Framework tests (`RepeatedWakeSuppression`, `StationarySolverVelocity`, `SideObstructionQualification`). 27 targeted tests expected; UE 5.8.3 rebuild, PIE outcomes and standalone BuildPlugin not yet verified for this pass.

## Orbit 0.2.18 | Iteration 029 | Rest stability and moving support selection

- Added a retry cooldown after an immediate wake following Orbit's manual rigid-body sleep request. Field-driven wakes remain permitted. The exact non-Orbit wake source is **not** identified by this game-thread code.
- Manual sleep now requires fresh, continuous contact with the same support, low body speeds, and an effectively stationary supporting primitive. Brief query grace remains useful for damping but does not accumulate a new manual-sleep timer.
- The existing single support sweep now prefers a stationary qualifying candidate over a moving candidate when their gravity-relative support-normal alignments are within 0.10; a clearly better-aligned contact may still win. No additional collision sweeps or custom impulses.
- A fast-moving or fast-spinning simulated support no longer qualifies for Orbit's temporary native supported damping or manual sleep; physical Chaos contact, collision, and gravity continue normally. Static supports and slow stacked bodies retain eligibility. Settings: `RestWakeRetryCooldown` (2 s), `RestMaxMovingSupportSpeed` (5 cm/s), and `RestMaxMovingSupportAngularSpeed` (0.08 rad/s).
- Added three editor console variables for capture beginning at first eligible body discovery during PIE: `Orbit.Motion.AutoStartAll`, `.AutoDuration`, `.AutoRate`. Capture continues to use the bounded 256-body / 250,000-row budget and existing auto-export safeguards.
- Added ten appended CSV fields (105 total), covering moving support velocities, manual sleep requests, immediate subsequent wakes, field-change wakes, cooldown and continuous rest timer. Previous 95 fields retain their order.
- Five new native Unreal Automation Framework math/eligibility tests (12 Rest, 8 Motion History, 4 Mover test cases in the targeted suites). Tests are source-authored; Unreal 5.8.3 compilation, standalone BuildPlugin packaging, runtime transitions and PIE capture remain unverified here. OrbitMover unchanged from Packaging Hotfix 1.

## 0.2.17 - Iteration 028 - Packaging Hotfix 1

- Fix a UE 5.8 Win64 `BuildPlugin` compile error in `Source/OrbitMover/Private/Tests/OrbitMoverTests.cpp`, `Orbit.Mover.MissingMoverSafe`.
- Construct the test component with `NewObject<UOrbitMoverGravityComponent>()`, using the `NewObject` default transient outer. The prior explicit `GetTransientPackage()` required a complete `UPackage` type to convert to `UObject*` in this translation unit.
- Include `UObject/UObjectGlobals.h` directly in the test translation unit instead of depending on transitive includes.
- No runtime gravity, Mover behavior, reflected APIs, or physics diagnostic behavior changed. The release remains v0.2.17 / Iteration 028.

**Validation:** Source and archive checks performed; UE 5.8.3 `BuildPlugin` packaging must be verified in a Windows UE installation.

## 0.2.17 - Iteration 028 - Curved-surface support evidence and recorder accuracy

- Correct stale sleeping-body motion diagnostics: refresh speed and awake flag from current Chaos velocity when sampled, and refresh reflected read-only receiver values on the resting path.
- Annotate the existing support sweep with blocking-contact count, rejected-normal count, best rejected surface/geometry, and reasons for skipping or failing support. This adds **no extra sweeps** and changes no accepted contact, gravity, damping or sleep decisions.
- Append 42 optional analysis fields to the previous 53-column interleaved CSV (95 total), retaining the earlier indices. Capture initial PIE time/body pose/motion/awake state, current settings/effective damping, per-body first sampled fresh support/sleep transitions and support state changes.
- Three new pure automation tests (eight Motion History tests; previously five). Update Motion History and all three maintained API guides.

**Validation:** Static source and CSV format-count checks only. Requires Unreal Engine 5.8.3 compilation, Automation Framework regression and one PIE export smoke test. Source-based support evidence is **not** a Chaos solver manifold or definitive collision-contact truth.

## 0.2.16 - Iteration 027 - Record all managed physics bodies

- Add `Orbit.Motion.StartAll` to record a frozen snapshot of all Orbit-managed simulated primitives; preserve single-body Start command and the original 49 CSV columns.
- Capture independent body rate/velocity reversal histories; add stable roster identity, Actor labels, mesh paths and a shared 256-body/250k-row cap. Stream batched CSV writes.
- Add two automated tests, update the three maintained plugin guides, and provide a test recipe for an 87-body planet scene. No physics-force, damping, sleeping, or Mover functional changes.

**Validation:** Source/static checks only. UE 5.8.3 compilation and PIE CSV export not verified in this environment.

## Orbit 0.2.15 | Iteration 026 | Physics Motion History

- Added explicit single-body motion history via PIE/game-world console commands `Orbit.Motion.List`, `.Start`, `.Status`, `.Stop`, `.Export`, using a bounded in-memory buffer and UTF-8 CSV export in `Saved/Orbit/MotionHistory`.
- Records game-thread motion, rigid-body center of mass, quaternion, linear/angular velocities, sampled gravity, dominant source center/ID, support-sweep state/normal/impact, source radial alignment, native gravity/damping/awake state, simple collision geometry counts, and significant tangential direction reversals.
- Auto-export unexported captures on PIE world shutdown where possible. Filename labels are sanitized and exports are uniquely named. No capture by default; one targeted body per world; no gravity/force or solver changes.
- Added three pure regression tests (`Orbit.Physics.MotionHistory.*`) and new `PHYSICS_MOTION_HISTORY.md` reference / controlled comparison procedure. Updated all three standing Orbit component/source/Mover references to accurately scope this feature.
- Test count: 54 authored core plus 4 Mover tests. Static inspection and archive validation only; no local UE 5.8.3 compilation or PIE verification.

## 0.2.14 - Iteration 025 - Native contact damping and runtime rigid-body diagnostics

- Replaced game-thread supported-body corrective AddForce/AddTorque with native Chaos linear and angular damping. Original component damping is restored when contact is lost or Orbit stops managing it.
- Maintains supported-contact eligibility and short grace; stops modifying damping every frame unless the desired value changes. **Supported Linear Damping** (the former `SupportedTangentialDamping` setting) now maps to supported native linear damping, not tangential-only acceleration; clearly documented change in semantics.
- Exposes read-only runtime `FOrbitPhysicsBodyDiagnostics` via Orbit Gravity Receiver and `GetPhysicsBodyDiagnostics` on the world subsystem. In PIE the Receiver component on a simulated prop now displays radial alignment, support normal, source-to-support-center offset, native gravity/damping state, support status, and speeds; also queryable per primitive from Blueprint.
- Makes viewport text less transient, while acknowledging it can be invisible in certain PIE setups. Debug line colors unchanged.
- Two authored pure tests: native damping selection and diagnostic defaults. **51 core + 4 Mover** tests authored. Not a Chaos solver/substep rewrite; equator drift is not yet confirmed resolved.
- Single plugin, all guides maintained. Static source checks only; UE 5.8.3 build and physics PIE tests required.

## 0.2.13 - Iteration 024 - Spherical rigid-body support diagnosis

- Physics gravity now samples simulating rigid bodies at their actual physics center of mass, eliminating component-pivot offsets from automatic field evaluation and force application.
- Improved near-contact support query with a bounded, conservative 0.08-second grace after a genuine hit. Adds support normal and support component diagnostics; does not invent contact or damp free flight.
- Added opt-in four-color support debug (cyan verified / blue grace / amber unsupported / red normal mismatch), plus Point radial alignment, field ID, native gravity state and source-to-support-bounds-center distance. These are for distinguishing misaligned source/geometry from Chaos contact instability.
- Added `Rest Support Contact Grace Seconds` setting and two physics math tests (`Orbit.Physics.Rest.*`, **49 core + 4 internal Mover** authored tests).
- The report was with Tangential Damping **3**, not 30. No higher damping defaults or world-axis bias introduced. This is a diagnostic/contact-stability pass, **not yet a confirmed cure for equator settling**.
- Maintains one Orbit plugin; all required guides and a new Iteration 024 guide updated. Static package checks only; UE 5.8.3 compile and PIE tests pending.

## 0.2.12 - Iteration 023 - Supported rigid-body damping and contact detection

- Diagnosed two resting problems with the existing automatic physics gravity: cubes slowly spinning on a sphere, and small spheres continuing to roll around its surface. The default sleep gate required very low linear AND angular velocities before it could activate.
- Added independent, contact-gated damping acceleration opposing gravity-tangent movement and angular spin. Default rates are 1.5 /s and 2.0 /s; configurable under Orbit Project Settings > Physics > Rest. Free flight and fast surface impacts are excluded; no artificial world-Z damping.
- Replaced support center-ray/bounding-sphere approximation with `UWorld::ComponentSweepMulti` using the simulated component's collision geometry, short sweep distance and support-normal check.
- Added opt-in cyan/amber support debug lines. Preserved the existing auto sleep, source physics behavior, `Orbit.NoRest` opt-out and normal/automatic gravity restoration.
- Added three authored pure math tests (`Orbit.Physics.Rest.*`, total 47 core tests plus 4 internal Mover tests), Iteration 023 regression plan and reference documentation.
- Character motion, AnimBP, field evaluation, Editor authoring and internal Mover behavior are unchanged. This remains a game-thread gravity bridge rather than a Chaos substep callback.

**Verification:** Static source and ZIP checks only. UE 5.8.3 compile/PIE runtime validation pending.

## Orbit 0.2.10 | Iteration 021 | Hotfix 1: Automation test compile correction

- Fixed `Orbit.Character.ControlsAutoActivationDefault` in `OrbitFieldTests.cpp`: UE 5.8.3 `UActorComponent` has no `GetAutoActivate()` accessor. The test now reads the reflected `bAutoActivate` bool on the component class default object using `FBoolProperty`.
- No changes to runtime gravity, character facing, Volume boundaries, Editor tools or optional Mover integration. All previous Iteration 021 behavior and documentation remain in effect.
- Build/run verification: the supplied compilation errors identify this specific Orbit test failure; the corrected source and ZIP have been statically validated, but UE 5.8.3 compilation has not been run here.

## 0.2.11 - Iteration 022 - Verified Locomotion and Gravity-relative Jump Animation

- Documented the **user-confirmed** fix for semi-strafing from `ABP_Unarmed_Orbit`: its `Vector Length XY` GroundSpeed and world-horizontal `Calculate Direction` fed inaccurate Speed and Direction inputs into `BS_Idle_Walk_Run` on a large curved ring. Four animation values now use `Get Orbit Gravity Animation State` instead; user confirmed correct forward-running gait.
- Added signed `GravityRelativeVerticalSpeed` and `bIsAscending` / `bIsDescending` to the animation state, independent of world Z. New `Calculate Orbit Gravity Vertical Speed` pure Blueprint function and math primitive also exposed for custom jump transitions and non-Character graph data.
- Added optional native `UOrbitGravityAnimInstance` Animation Blueprint base class, caching Orbit animation state during game-thread `NativeUpdateAnimation`. Existing AnimBP graphs are never rewritten automatically; keep the verified pure Blueprint node integration if preferred.
- Added a complete node-by-node example of the user's graph, including replacing original `Velocity.Z > 100` jump transition with gravity-relative vertical speed; did not bundle or manufacture a broken/untested `.uasset`.
- Added three authored tests: `Orbit.Character.Animation.GravityVerticalSpeedAllAxes`, `.GravityVerticalSpeedInvalidInput`, `.NativeInstanceDefaults`. Total **44 core + 4 Mover** tests authored.
- No source gravity evaluator, character rotation, camera, low-g landing, or OrbitMover behavior change. The new release requires UE 5.8.3 compilation/PIE validation.


## 0.2.10 - Iteration 021 - Controls activation and Volume Max Distance shape correction

- Fixed the actual v0.2.9 PIE failure identified by user diagnostics: Orbit Character Controls was ticking/receiving move inputs but `IsActive()` was false, so facing never attempted. Enable Auto Activate in the native constructor and repair inactive saved Blueprint instances during BeginPlay when facing is enabled; explicit runtime deactivation remains possible.
- Add `Facing Begin Play Activation Count` for PIE diagnosis. This does not automatically change Manny/Quinn's Animation Blueprint warping logic.
- Volume Max Distance now limits the selected rotated local Box/Sphere/Cylinder/Capsule dimensions. Eliminated the accidental second Euclidean/spherical cutoff and made Volume Linear falloff normalized to the selected shape. Added a single shared effective-volume-bounds helper, used by gameplay evaluation and editor debug/preview.
- Updated the blue finite-distance boundary visualization, gravity preview seeds and streamline seeds; green wireframe still shows authored volume dimensions. Other gravity source shapes and their Max Distance behavior are unchanged.
- Added `Orbit.Character.ControlsAutoActivationDefault` and `Orbit.Field.VolumeMaxDistanceRespectsShape` authored automation tests (41 core + 4 Mover, uncompiled).
- Maintained Source Actor, Components/Blueprint API, Animation, and Mover documentation, plus test plan and Iteration 021 guide.

**Validation:** Source/static and ZIP checks only. Requires real UE 5.8.3 compilation and PIE smoke tests before treating as verified.

## 0.2.9 - Iteration 020 - Post-Movement Character Facing

- Apply character-facing actor rotation exactly once per eligible PostUpdateWork tick after native movement rather than on native movement callbacks. Native callbacks are now diagnostics only.
- Support local or authoritative character-facing ownership while excluding remote simulated proxies, and reassert native rotation disablement every tick while owned.
- Add cyan desired-heading debug arrow and 11 read-only runtime diagnostics for chosen heading, rotation success, residual error and rotation being overwritten by another system.
- Add gravity-relative desired-facing quaternion helper and three math regression tests (39 core tests + 4 Mover tests authored); update full documentation.
- No changes to core gravity selection, landing assist, Mover integration or Animation Blueprint assets. UE compilation and runtime confirmation still required.

## 0.2.8 - Iteration 019 - Character Facing and Debug Visibility Regression Fix

- Fix debug arrows being hidden when the facing-ownership predicate is false. Drawing now depends on the explicit `Draw Locomotion Debug` setting and a ticking Controls component, rather than on successful facing ownership.
- Add PIE component diagnostics: `Debug Tick Count`, `Native Movement Callback Count`, `Tick Facing Fallback Count`, `Orbit Move Input Count`, and `Last Facing Debug Status`. This identifies inactive components, locally-uncontrolled pawns, missing input wiring, and absent native callbacks.
- Add an orientation-update fallback on the Controls post-update tick **only when** the native Character Movement callback did not arrive. Avoid applying the same rotation twice when the native callback did arrive.
- Allow velocity-based facing to use actual gravity-tangent Character Movement velocity even when no recent Orbit-specific input event was recorded. This also helps characters using the ordinary AddMovementInput path, though Orbit's gravity-relative input functions remain preferred for accurate camera-relative controls.
- Add 2 unit tests for velocity-driven facing without Orbit input and requested-heading fallback/idle behavior (36 base + 4 internal Mover authored tests).
- Preserve one final actor rotation owner, embedded source components, multi-planet competition, Mover adapter, and existing Blueprint APIs.

**Validation:** Source/static checks only. Compile and PIE tests in UE 5.8.3 still required; animation asset wiring is unchanged.

## 0.2.7 - Iteration 018 - Unified Character Orientation and Animation Bridge

- Give Orbit Character Controls sole ownership of the final locally controlled character rotation when movement-facing is enabled. Orbit Character Gravity continues applying gravity before native movement, but no longer applies its own competing actor rotation in that configuration. Characters without active Controls facing retain the original Gravity alignment path.
- Parallel-transport heading over curved gravity surfaces, including at idle when there is no recent move input; combine local Up and facing into one quaternion rotation.
- Optionally suspend native `OrientRotationToMovement`, `UseControllerDesiredRotation` and `UseControllerRotationYaw` while Orbit Controls owns facing; capture and restore their original values.
- Add gravity-local movement diagnostics and optional requested input / velocity / facing / Up debug arrows in PIE.
- Add `Get Orbit Gravity Animation State` Blueprint-pure node with gravity-local Ground Speed, Forward/Right speed, signed direction, tangent acceleration, Should Move, Falling and world-XY diagnostic speed.
- Add `Docs/ANIMATION_INTEGRATION.md` with direct Manny/Quinn AnimBP hookup instructions and explicit warning that plugin code alone cannot repair existing asset graphs or orientation-warp nodes.
- Add 4 authored orientation/animation math automation tests; 34 core tests + 4 optional internal Mover tests, uncompiled.
- Preserve internal optional OrbitMover module without functional changes; all prior field and physics logic retained.

**Validation:** Static ZIP checks only. UE 5.8.3 compile, PIE, animation graph and Mover smoke tests remain necessary.

## 0.2.6 - Iteration 017 - Single Plugin Consolidation

- Consolidated the separate `OrbitMover` plugin source into the main `Orbit` plugin as its third internal module. The release now has only `Orbit/Orbit.uplugin` and installs as one folder.
- Removed OrbitMover's compile-time dependency on Epic Mover, using validated Blueprint-reflected `SetGravityOverride`, `SetUpDirectionOverride`, `GetUpDirection`, and `IsBackendAsync` calls at runtime instead.
- Mover gravity is now updated in a pre-physics game-thread tick with a best-effort Mover tick prerequisite; this differs from the prior typed pre-simulation delegate. Documented the resulting timing limitations for network/prediction/independent simulation. Chaos and async remain unsupported.
- Kept the Orbit Mover Gravity component, its two authored Blueprint nodes and existing three pure Mover policy tests; added a missing-Mover regression test.
- Updated the maintained Gravity Source, Components/Blueprint, and Mover-specific docs, plus architecture, compatibility, file tree and integration test plan. No engine source modifications.

## 0.2.5 - Iteration 016 - Influence-based gravity arbitration and reusable gravity components

- Implemented Strongest Influence (new project default), Blended Influence and Priority Override (legacy) for equal-priority overriding sources, preserving explicit Priority and channel filters.
- Added Character Gravity's optional source hysteresis (configurable threshold) and optional smoothed native gravity direction; magnitude stays current.
- Added the Orbit Gravity Source Scene Component for source fields embedded directly on arbitrary Actors, including spline/vortex settings and runtime subsystem registration.
- Added Orbit Gravity Orientation component with six signed local axes, optional specific source Actor/Component, smoothing, and Orient Now.
- Added Get Orbit Gravity Up for Actor, Make Orbit Axis Aligned Rotation, Orient Actor to Orbit Gravity Blueprint nodes; existing editor right-click orient action now considers embedded sources.
- Added four automation tests; synchronized the three maintained Markdown guides and created Docs/ITERATION_016.md. Optional OrbitMover's source code unchanged.

**Verification:** Static inspection/archive only; UE 5.8.3 compile and runtime tests pending.

## 0.2.4 - Iteration 015 - Planet-to-planet source arbitration and gravity orientation authoring

- Equal-priority Point/Override gravity fields now select whichever point center is nearer to the queried position. Previously the first registered source controlled the entire overlapping region, regardless of distance. Higher explicit Priority always wins; non-Point equal-priority behavior is unchanged.
- Added **Orbit > Orient to Gravity** on the Level Editor actor right-click context menu, with multi-selection, Undo, per-actor gravity/Receiver channels, preservation of location/scale and projected forward heading. Excludes Gravity Source actors to avoid accidentally changing their field axes.
- Added `Orbit.Field.EqualPriorityPlanetTransfer` numerical regression test.
- Bumped the optional OrbitMover plugin metadata to the matching package version, with no OrbitMover source changes. Added a detailed two-planet jump and editor-platform authoring guide in `Docs/ITERATION_015.md` and updated all three maintained feature references.
- Does **not** add automatic planet jumps, momentum-aware handoff, source hysteresis, field blending or camera transition smoothing. These remain later reliability tasks.

**Verification:** Archive and static checks only. UE 5.8.3 compilation, Outliner/viewport menu placement, Undo behavior and PIE transfer not yet tested.

## 0.2.3 - Iteration 014 - Native initial landing and gravity-relative animation diagnostics

- Responded to the reported difference between initial low-gravity spawning and successful jump re-landing. Switched near-floor observation to `ACharacter::OnCharacterMovementUpdated`, after the Character Movement scope, while continuing to inject gravity direction and magnitude before movement. Grounded characters keep native landing behavior.
- Raised the default landing-assist approach limit from 100 to 600 cm/s to cover typical first drops. Still requires an actual, sweep-validated, walkable native `K2_FindFloor` result within the configured distance; no fabricated collision, teleport, or arbitrary Walking mode switch. Native Walking still performs its own floor/base validation. Existing saved user overrides remain their values.
- Added persistent `Last Landing Assist Result`, descent/lateral speed, `Has Been Grounded` and `Pre First Landing Checks` diagnostics to identify why a spawn drop differs from jumping. New floor results are evaluated after native movement.
- Added **Get Gravity Relative Locomotion** Blueprint node and `FOrbitGravityRelativeLocomotion` structure: tangent-plane Ground Speed, Forward/Right Speed, signed Direction Degrees, tangent velocity and local Up. Unlike generic world-XY animation math, it works at planet equators and poles; **users still need to rewire their own Animation Blueprint**.
- Added optional `Face Velocity When Moving` (default true, threshold 30 cm/s) to native Orbit Character Controls so actor facing can match physical gravity-tangent velocity rather than camera-relative input alone. Manual animation logic and root motion remain independent.
- Added two new pure math regression tests for cardinal gravity-frame locomotion and source-strength initial drop eligibility, for **25 authored base Orbit tests** plus three unchanged companion OrbitMover tests. Updated the maintained component/Blueprint guide, Gravity Source guide and Orbit-specific Mover guide; added `Docs/ITERATION_014.md` and current test instructions.
- No changes to the gravity source field evaluator, physical rigid-body gravity, Mover integration behavior, network rollback, or Unreal Engine source.

**Verification:** Archive integrity and static source inspections only. This release is not UE 5.8.3 build-tested or PIE-validated. Animation Blueprint assets, including the project's `ABP_Manny`/`ABP_Quinn`, are not part of this source ZIP.

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

## 0.2.33 — Iteration 044 (partial compatibility pass)

- Add explicit `PrepareSpawnedPhysicsActor` Blueprint API for multiple independent simulated Static Mesh components on the same Actor.
- Add two live Chaos integration tests for multi-body preparation and eligibility.
- Standard remains the default; skeletal/articulated and automatic first-step ownership remain outstanding.
