# Orbit - Master Development Roadmap

**Document version:** 0.1 (initial planning baseline)  
**Target engine:** Unreal Engine 5.8.3  
**Target platform for initial validation:** Windows 64-bit  
**Implementation:** C++ Unreal Engine plugin, stock engine, no engine-source modifications  
**Plugin name:** Orbit  
**Release goal:** Comprehensive, dependable v1.0.0  
**Iteration budget:** 165 to 264 developer/test feedback cycles; planning midpoint approximately 200  
**Status:** Phase 0, Iteration 001 source implementation prepared. Not yet validated by an Unreal Engine 5.8.3 compile or runtime test. Refer to Docs/ITERATION_001.md and Docs/COMPATIBILITY.md for observed support.

## 1. Product vision

A designer can install the plugin, place and configure gravity sources, and build playable spherical planets, ringworlds, toroidal habitats, bent tunnels, local gravity zones, and zero-gravity spaces. Eligible physics bodies work automatically. Other engine systems use well-integrated plugin adapters where automatic support is impossible. Developers retain Unreal's familiar workflows wherever practical.

**Core principle:** Gravity belongs to the world. Physics, movement, navigation, EQS, AI, worldbuilding, and optional simulations sample the same authoritative gravity field rather than implementing inconsistent gravity calculations.

**Nonnegotiable constraints:**

- No changes to Epic Engine source or binaries. No engine forks or private-header patches.
- The Core module must not depend on Character Movement, Chaos, AI, EQS, or editor-only modules.
- No mandatory receiver component on every ordinary eligible simulated rigid body.
- Gravity sources include Point, Axis, Directional, Volume, and Spline from the earliest core implementation.
- Both inward and outward attraction/repulsion are supported where meaningful.
- Gravity direction and local spatial orientation are distinct concepts.
- Navigation and EQS are major product capabilities, not indefinitely deferred nice-to-haves.
- Stock-engine packaged builds, not only Editor PIE, are part of validation.
- Unsupported integrations are explicitly labeled; never claim universal automatic engine compatibility.

## 2. What constitutes completion

v1.0.0 is eligible to ship when all **Release Required** capabilities below have passing regression cases in Editor and packaged builds, with written limitations for any qualified support.

### Release Required (R)

- Gravity fields: Point, Axis, Directional, bounded Volume, and Spline, including closed and curved splines.
- Field composition: Priority, Override, Additive, Strongest, and defined blending; default behavior outside sources.
- Thread-safe, deterministic-within-snapshot evaluation and invalid/zero-gravity fallback behavior.
- Automatic gravity application to supported Chaos rigid bodies, including spawned bodies, opt-outs, and avoidance of double-gravity.
- Player locomotion: gravity-relative walking, falling, jumping, floor detection, and camera/input orientation using supported player movement integration.
- Gravity-aware projectiles using plugin adapters for non-simulated projectile movement.
- Custom navigable surface representation capable of routes around a complete planet and ring/tube, including AI path following and end-to-end MoveTo integration. It need not be based on Recast; the public navigation framework is preferred.
- Custom EQS generators and tests that use the same gravity field and the same navigable representation as AI movement.
- Common gravity-relative AI arrival, path progress, and near-agent avoidance for grounded agents.
- Usable source-editing tools, field visualization, gravity probe, diagnostics, Blueprint and C++ query APIs.
- Explicit multiplayer and streaming compatibility policy, with passing tests for the supported subset.
- Test scenes, automated regression suite, packaging validation, documentation, known limitations, and performance baselines.

### Targeted Integrations (T)

Ragdolls, basic Chaos Vehicles, Geometry Collections, animation/root motion/IK, Niagara CPU and GPU effects, PCG, object placement, foliage, Mass/crowd workflows, optional Mover integration, World Partition navigation streaming. Each must pass an independently documented support level before being described as supported. Prioritize by common gameplay relevance.

### Specialized / Conditional (S)

Full ocean/water replacement, physically accurate global fluid simulations, all cloth solver modes, arbitrary third-party movement plugins, fully deformable dynamic spline navigation at large scale, all experimental engine systems, and realistic rotating-reference-frame physics. These are separate qualified extensions, not silent v1.0.0 guarantees.

### Support classifications

- **Automatic:** Eligible native objects respond correctly without per-object gravity-specific setup.
- **Integrated:** Works when the developer selects a supplied component, subclass, project setup, query node, or module.
- **Authoring Support:** Plugin provides field data and tools; the project must author gravity-aware content.
- **Experimental:** Works only under explicitly documented constraints, without production guarantee.
- **Unsupported:** Known limitation or no reliable stock-engine integration identified.

## 3. Architectural rules

### 3.1 World field and local frame

- Author sources as actors/components or data assets; the runtime maintains a registry and immutable, versioned sampling snapshots for physics and worker threads.
- The field query returns world-space acceleration, magnitude, contributing-source metadata, field validity, and optional reference-frame information.
- Sample positions use Large World Coordinate-friendly precision and stable transforms. Convert precision only at integration boundaries.
- The local frame service resolves up, tangent plane, projected heading, and rotation continuity; it does not assume that a near-zero net gravity vector has a unique up direction.
- Source-specific authored notions such as radius or height remain distinct from universal field queries. There is no globally valid scalar height for all arbitrary fields.
- Static and dynamic sources use clearly defined update timing and snapshot handoff. Physics callbacks never read live UObjects unsafely.
- No hidden dependence on World Z except the configured default-gravity fallback.

### 3.2 Source model

| Source | Field behavior | Required tests |
|---|---|---|
| Point | Toward/away from center | Center degeneracy, falloff, large distances |
| Axis | Radial relative to finite/infinite axis | Axis degeneracy, finite endpoints, rotations |
| Directional | Fixed vector, optionally bounded | Zero field, override, default compatibility |
| Volume | Bounded local field / override | Overlap, boundary blend, streaming |
| Spline | Toward/away from closest curve or direction from spline frame | Closed seam, tight bends, self-intersection, endpoints, per-point data |

**Spline-specific rules:** Start with circular cross-sections, reference radius, influence radius, sampled falloff, local roll/oriented gravity, and stable closest-branch behavior. Optimize via cached curve segments and acceleration structure; do not run expensive game-thread spline calls inside each Chaos substep. Curves define gravity; they do not necessarily generate geometry. Toroidal attraction toward a ring centerline and outward gravity toward the walls of a tubular interior must both be possible.

### 3.3 Logical modules (final boundaries can change after the feasibility spike)

- `GravityCore` - runtime field sources, composition, snapshots, spatial frames, C++/Blueprint API.
- `GravityPhysics` - Chaos integration and rigid-body participation/exemptions.
- `GravityMovement` - Character/Pawn integrations, projectile adapter, camera and input helpers.
- `GravityNavigation` - custom navigable surfaces, agent clearance, paths, rebuild/invalidation, link handling.
- `GravityAI` - AI path following, EQS generators/tests, avoidance and high-level AI adapters.
- `GravityEditor` - editor visualization, gravity probe, presets, settings, validation UI.
- `GravityTests` - automation and test fixtures (may be developer-only test module).
- Optional modules as justified - Vehicles, Niagara, PCG, Mass, Mover, Cloth, Water, or World Partition specific integrations.

Do not create a separate module simply to satisfy a diagram. Split modules when optional dependencies, load phases, runtime/editor separation, or maintenance make it worthwhile.

## 4. Risk-first sequencing

The implementation is sequenced to answer the highest-risk engine compatibility questions early. Phase 0 includes throwaway spikes of physics, navigation, path following, and EQS. These are not production implementations. Navigation production work occurs in Phase 4.

| Phase | Development cycles | Primary deliverable | Exit gate |
|---|---:|---|---|
| 0. Feasibility and architecture | 8 to 12 | Stock UE 5.8.3 compile/PIE/package technical spikes | G0: plugin-only viability demonstrated |
| 1. Gravity core and editor | 12 to 20 | Five field types, snapshots, source authoring, probe | G1: deterministic fields and spline stability |
| 2. Chaos physics | 14 to 22 | Automatic rigid-body gravity, spawned bodies, opt-outs | G2: correct solver behavior, no double gravity |
| 3. Movement and cameras | 16 to 26 | Supported player locomotion, projectiles, controls | G3: full walk and jump traversal on curved worlds |
| 4. Navigation and pathfinding | 35 to 55 | Curved-surface nav representation and path following | G4: AI traverses sphere and irregular tube |
| 5. EQS, AI, avoidance | 18 to 30 | EQS, MoveTo workflows, AI spatial tests, avoidance | G5: AI decisions reliably lead to reachable movement |
| 6. Additional engine integrations | 22 to 34 | Priority adapters and interoperability | G6: documented support matrix and regression tests |
| 7. Networking and large worlds | 15 to 25 | Networked field sync, replication, partition/precision | G7: supported network and streaming tests pass |
| 8. QA, optimization, release | 25 to 40 | Release candidate, docs, packages, regression suite | G8: v1.0.0 acceptance matrix signed off |

**Total:** 165 to 264 iterations, planning midpoint near 200. Work may overlap but gate prerequisites must hold. Reforecast after G0, G3, and G4.

## 5. Detailed action plan and milestones

### Phase 0 - Technical proof and feasibility (8 to 12 cycles)

**Objective:** Establish that stock UE 5.8.3 plugin extension points suffice before investing in broad implementation.

- [ ] Create a clean C++ test project and minimal runtime/editor plugin skeleton.
- [ ] Verify Windows Win64 Editor, Development packaged, and Shipping packaged builds.
- [ ] Implement a tiny world-field sample for one point and one closed spline.
- [ ] Prototype Chaos physics callbacks influencing ordinary eligible simulated bodies without receiver setup; validate spawn-after-start and engine-gravity cancellation policy.
- [ ] Prototype custom `ANavigationData` registration/query returning a short path along a non-horizontal surface. Validate public headers, symbol exports, agent selection, and packaging.
- [ ] Prototype actual AI travel along a curved path using standard `MoveTo` when possible, or a plugin-provided path-following adapter if required.
- [ ] Prototype an EQS gravity-relative generator/test and verify it runs during PIE and packaged play.
- [ ] Document required overrides, areas relying on default engine internals, unsupported calls, and fallback implementation choices.
- [ ] Compare local gravity-aligned nav patches against an orientation-independent surface graph. Choose a candidate with evidence; do not assume rotating Recast solves arbitrary 3D navigation.

**G0 PASS:** Rigid bodies follow a test field, a non-Z surface has a functioning nav query, a test AI reaches a curved-surface target, and a gravity-aware EQS query succeeds. All run using stock-engine plugin code, without private-symbol hacks, including packaged validation.

**G0 FAIL / DECISION:** If any path is blocked by unexported or inaccessible APIs, investigate plugin-only replacement layers. If a core requirement would demand engine modification, stop and revise scope before Phase 1. Never quietly relax the no-engine-modification constraint.

### Phase 1 - Gravity foundation and editor (12 to 20 cycles)

- [ ] Specify public gravity sample, field snapshot, source handles, and local spatial frame API.
- [ ] Build five source types: Point, Axis, Directional, Volume, Spline.
- [ ] Implement inward/outward modes, stable field blending, priority, strongest, additive, and override.
- [ ] Define falloff modes, source radius/extent, zero-gravity zones, and default-gravity fallback.
- [ ] Resolve zero magnitude, opposing fields, spline self-intersections, source disable/delete, and no-valid-frame scenarios.
- [ ] Create thread-safe source snapshot publishing, dynamic updates, spatial query batching, and large-coordinate support.
- [ ] Add spline per-point attributes, seams, endpoints, radius/falloff, and cached acceleration structure.
- [ ] Expose a small Blueprint and C++ API for field acceleration, field sample, local up, tangent plane, and projection.
- [ ] Deliver editor gizmos, field vector visualization, overlap view, gravity probe, and basic source validation.
- [ ] Add unit tests for algebra, continuity, limits, precision, and malformed source data.

**G1 PASS:** All field types and blend policies produce finite, repeatable results; closed splines have no unintended gravity seam; editor changes propagate predictably; physics-thread sampling has no UObject access.

### Phase 2 - Automatic Chaos integration (14 to 22 cycles)

- [ ] Evaluate field in the appropriate Chaos simulation callback/simulation step, consistent with solver thread rules.
- [ ] Correctly handle native world gravity, mass-independent acceleration, ordinary rigid bodies, and late spawning.
- [ ] Define opt-out / custom override semantics that do not require adding components to every physics body.
- [ ] Test sleeping and waking, collision, bouncing, rolling, kinematic/simulated transitions, and substeps.
- [ ] Test multiple physics scenes/solvers if the stock engine environment uses them.
- [ ] Cover skeletal ragdoll basic cases and establish Geometry Collection integration classification.
- [ ] Record timing, body count, gravity-field query count, and source count metrics.

**G2 PASS:** Unmodified eligible simulated physics bodies respond to configured fields automatically; no unintended double gravity; failures and unsupported physics modes are reported rather than hidden.

### Phase 3 - Player, pawn, projectile, and camera movement (16 to 26 cycles)

- [ ] Integrate with `UCharacterMovementComponent::SetGravityDirection` and correct magnitude handling where supported.
- [ ] Handle walking, fall, jump, crouch, slope, step, based movement, ledges, root-motion common cases, and airborne transitions.
- [ ] Implement stable tangent-plane movement and camera policy options (camera-follows-up, partial follow, camera-independent).
- [ ] Preserve heading across rotations, loops, gravity inversions, and zero-G boundaries.
- [ ] Add adapter for non-simulated `UProjectileMovementComponent`-style ballistics and test predicted trajectories.
- [ ] Provide helper components/APIs for custom Pawns; avoid implying every arbitrary PawnMovement is automatic.
- [ ] Establish early client/server movement tests, even if complete network hardening comes later.

**G3 PASS:** A supported character walks and jumps fully around an exterior sphere, inner cylinder, inner torus tube, and curved spline corridor without orientation flips or incorrect world-Z behavior; common projectile and input behavior follows field rules.

### Phase 4 - Curved-surface navigation (35 to 55 cycles)

- [ ] Choose and implement orientation-independent navigable-surface representation using public navigation APIs where viable.
- [ ] Build geometry acquisition, walkability (surface normal vs local Up), agent clearance, and connected surface graph.
- [ ] Support projection, queries, path costs, complete path around curved surfaces, and path smoothing constrained to real geometry.
- [ ] Handle multi-surface intersections, disconnected components, changes in local orientation, and agent size.
- [ ] Integrate navigation data registration and selection with Unreal NavigationSystem.
- [ ] Implement gravity-relative path following, path-progress tests, reach/arrival checks, and edge transitions.
- [ ] Support initial custom links (jump, drop, gravity transition) and dynamic obstacle invalidation.
- [ ] Implement navigation debug rendering and regeneration diagnostics.
- [ ] Establish performance baseline against surface polygon count, dynamic obstacles, agent count, and query rate.
- [ ] Validate packaged builds and concurrency rules.

**G4 PASS:** AI routes around a complete planet and a looping, irregular spline-shaped tube, including a route that is not valid in world XY; it follows a continuous surface path without tunneling through geometry or incorrectly accepting unreachable destinations.

**Important distinction:** The result may be custom `ANavigationData` instead of a conventional Recast NavMesh. The user-facing requirement is correct navigable surfaces and familiar movement workflows, not a specific internal polygon format.

### Phase 5 - EQS, AI, and avoidance (18 to 30 cycles)

- [ ] Implement gravity-tangent ring and grid generators, field-based projection, slope, gravity-relative height, and path length/reachability tests.
- [ ] Ensure EQS and MoveTo query the exact same navigation topology and agent filters.
- [ ] Support common Behavior Tree, StateTree, AI Controller, and Smart Object movement workflows through adapters when needed.
- [ ] Add local tangent-plane avoidance and gravity-relative agent separation. Evaluate full 3D avoidance requirements for small planets and zero-G.
- [ ] Verify perception assumptions, line of sight, cover, approach poses, and interaction facing without rewriting systems that already work in 3D.
- [ ] Test competing EQS candidate points for reachable vs unreachable choices, moving objects, and gravity changes.

**G5 PASS:** AI can patrol, pursue, choose a reachable EQS location, avoid peers and obstacles, and complete interactions around curved surfaces without special-case gameplay logic for every world shape.

### Phase 6 - Broader engine integrations (22 to 34 cycles)

- [ ] Triage each integration by actual gameplay value, public API feasibility, test effort, and expected user adoption.
- [ ] Ragdoll/physical animation and root-motion/IK finishing work.
- [ ] Chaos Vehicle and Geometry Collection compatibility.
- [ ] Niagara gravity sampling and modules for CPU/GPU pathways where feasible.
- [ ] PCG point orientation, slope, local-height data, placement, and foliage integration.
- [ ] Mass AI, crowds, navigation, and avoidance assessment/adapters.
- [ ] Optional Mover support behind a separate dependency; treat UE 5.8 Mover as Experimental.
- [ ] Evaluate Chaos Cloth and buoyancy without promising that solver-level rigid-body gravity applies to them.
- [ ] Build extension API and short authoring recipes for external third-party tools.
- [ ] Record explicit automatic/integrated/authoring/experimental/unsupported classification for every subsystem.

**G6 PASS:** Priority integration cases have recorded tests; incomplete or specialized integrations are accurately marked. No optional engine plugin is accidentally a hard dependency of the gravity core.

### Phase 7 - Multiplayer, streaming, large worlds (15 to 25 cycles)

- [ ] Define source replication and deterministic field versioning, snapshot timing, authority, and ownership.
- [ ] Validate CharacterMovement corrections, client prediction, physics resimulation policies, and moving fields.
- [ ] Ensure persistent source definitions survive visual/source actor streaming when influence remains relevant.
- [ ] Validate World Partition and large-coordinate field evaluation, navigation streaming, and source handoffs.
- [ ] Test join-in-progress, teleport, source enable/disable, streaming boundary, and late-spawn replication.

**G7 PASS:** Supported networked use cases exhibit stable authoritative gravity and documented correction behavior; sources and navigation remain valid across supported streaming transitions.

### Phase 8 - QA, optimization, packaging, release (25 to 40 cycles)

- [ ] Complete automated and manual regression matrix across all mandatory scenes and modules.
- [ ] Fix critical/crash/data-loss, high-impact movement, and navigation regressions.
- [ ] Stress-test high agent counts, high rigid-body counts, multiple fields, many spline segments, and deforming/moving sources within supported limits.
- [ ] Profile field evaluation, Chaos callback, navigation build/query, AI/EQS, render/debug overhead, memory, and streaming.
- [ ] Audit editor startup/shutdown, source undo/redo, duplicate, PIE, multi-world Editor, save/load, and plugin disable/remove handling.
- [ ] Test clean project installation and Win64 packaged Development/Shipping builds.
- [ ] Freeze supported feature set and public API; finalize README, setup guides, API reference, sample scenes, compatibility matrix, known limitations, changelog, and release notes.
- [ ] Test no Unreal Engine modifications and no runtime editor dependency.

**G8 PASS:** All Release Required scenarios pass; release support levels are honest; no release-blocking known issue remains; package builds and installation tests succeed.

## 6. Permanent test environments

| Map/test scene | Purpose | Critical pass condition |
|---|---|---|
| T00_FlatBaseline | Ordinary Unreal behavior comparison | Defaults remain correct when no field is active |
| T01_PointPlanet | Exterior spherical gravity | Player, bodies, and AI traverse full circumference |
| T02_InnerSphere | Outward point gravity | Walkable inside sphere; no upside-down camera issues |
| T03_AxisRing | Outward axial gravity | Continuous interior ring movement and AI routes |
| T04_SplineTorus | Closed toroidal tube | Stable spline seam and paths around the tube |
| T05_TwistedSpline | Nonplanar curved tunnel, tight bends | Stable closest-branch behavior and local frames |
| T06_Transitions | Multiple fields, override, additive, zero-G | No NaNs, violent unexpected flips, or double gravity |
| T07_DynamicFields | Moving/disabled/re-enabled sources | Snapshot consistency, regeneration, no stale sources |
| T08_CrowdAI | Many AI agents with EQS and avoidance | No pathological crowd oscillation or unreachable targets |
| T09_Network | Listen/dedicated server, multiple clients | Supported authority/prediction behavior works |
| T10_StreamingLWC | World Partition, large positions, late loads | Field and nav continuity through load/unload |
| T11_PhysicsStress | Many simulating bodies, ragdolls, colliders | Stable force application and bounded runtime cost |

Each map has at least one programmatic pass/fail test where practical. Run smoke tests after every integration pass; run full matrix at gates and releases. Probe may be used as a test orchestrator, but the plugin's acceptance must not depend on Probe being installed in a user's project.

## 7. Engineering validation and quality bar

**Functional:** For fixed source configuration and query position, repeated field queries return equivalent results; direction/magnitude math is finite; zero-G and exact-center cases have an explicit noncrashing policy. Navigation paths stay on connected walkable geometry and respect agent clearance.

**Physics:** A standard simulated object responds correctly in supported gravity fields, spawned bodies are included, opt-outs work, substepping does not double-apply acceleration, and forces are mass-independent unless a special mode explicitly says otherwise.

**Movement:** No uncontrolled up-vector discontinuities, the supported character can complete 360-degree curved traversal, and jump/landing behavior remains consistent across gravity-relative orientations.

**AI/EQS:** Reachable candidates are actually traversable by the selected agent. Path costs represent route distance/cost rather than naive straight-line distance through planetary geometry.

**Performance:** Baseline measured before optimization. Track field samples/second, cost per sample, physics step overhead, path query latency, navigation rebuild time, AI count, memory, and debug-render cost. Set numeric budgets once scene complexity and target hardware are established; do not invent unmeasured global millisecond targets.

**Compatibility:** No engine-source patch, no unexported private method call, no editor-only runtime dependency, no required third-party plugins for the core. Pin testing to UE 5.8.3 and Win64 before widening support.

**Release policy:** P0 crashes/corruption, physics instability, failed main gameplay traversal, invalid paths presented as reachable, and packaged-build failures block release. Lower severity issues may remain only if documented and not misleadingly classified as fully supported.

## 8. Integration tracking matrix

Create and maintain one row per engine subsystem and scenario with these fields:

`Subsystem | Scenario | Priority (R/T/S) | Support Type | Status | UE Version | Automated Test | Manual Test | Known Limitation | Evidence/Build | Next Action`

Statuses: `Not Started`, `Feasibility`, `Implementing`, `Works in Editor`, `Packaged Verified`, `Regression Passing`, `Qualified`, `Blocked`.

No row becomes `Regression Passing` based only on source code review or a successful compile. A successful runtime test is required.

## 9. Top risks and responses

| Risk | Severity | Early action | Fallback / decision |
|---|---|---|---|
| Navigation extension points inaccessible or insufficient in packaged UE 5.8.3 | Critical | Phase 0 nav + MoveTo + packaging spike | Implement plugin-owned navigation/path-following adapter; reassess automatic compatibility |
| Physics-thread safety / unsupported Chaos callback integration | Critical | Phase 0 solver test | Redesign snapshot handoff; avoid unsafe game-thread access |
| Hardcoded world-Z inside existing engine subsystems | High | Record assumptions during every adapter | Subclass, project configuration, explicit replacement, or documented unsupported |
| Spline intersections and abrupt local frame changes | High | Unit tests from Phase 1 | Stable branch selection, blending, continuity policy |
| Player prediction/replication correction | High | Initial tests Phase 3, final Phase 7 | Restrict supported network modes until tests pass |
| Dynamic navigation rebuilding is too expensive | High | Instrument during Phase 4 | Static/nav-segment caching, localized invalidation, explicit supported motion limits |
| Massive world field/streaming precision loss | High | LWC-aware core design, Phase 0 prototype | Persistent source definitions, double-precision sample math |
| Too many optional engine dependencies | Medium | Separate extension modules | Keep Core independent; defer nonessential adapters |
| Compatibility percentages mistaken for promises | Medium | Classify and test every integration | Publish evidence-backed matrix instead of blanket claims |

## 10. Work agreement for each development iteration

**Inputs:** Latest complete plugin ZIP or repository commit, UE 5.8.3 build result, last test report/log/screenshots where applicable, and the specific acceptance criteria for the next pass.

**Assistant deliverable for code iterations:**

1. Complete plugin source ZIP with exactly one top-level plugin folder, named after the final plugin (not a version-suffixed root folder).
2. All changed source and assets included. Exclude generated `Binaries/`, `Intermediate/`, `.vs/`, and other local build artifacts.
3. A short change summary, explicit verification status, and test instructions.
4. Updated `CHANGELOG.md` / `KNOWN_ISSUES.md` / roadmap checklist when a feature or gate changes.
5. No assertion of a successful compile or test unless it was actually performed.

**User test pass:** Place the full plugin in the project, compile, execute the specified smoke scene(s), report compile failures or runtime discrepancies, and provide engine headers/source excerpts when stock UE APIs cannot be confirmed.

**Regression rule:** Fix newly introduced breakage before widening scope. One major subsystem at a time when practical; intentional cross-subsystem changes get explicit integration tests.

**Release workflow:** Experimental phases use incrementing development versions; a release candidate is created only once required gates pass. Major changes to the API or core semantics require an architecture review note.

## 11. Immediate next action: Phase 0, Iteration 001

**Aim:** Build the technical test harness and first small vertical slice. Do not start large editor UI, spline mesh generation, or a production nav generator yet.

**Deliverables:**

- Initial plugin skeleton, separate Runtime and Editor modules, source-only packaged as `PluginName/` inside ZIP.
- Barebones field interface and a Point source plus a simple closed Spline source.
- One physics actor test: an ordinary dynamically spawned cube accelerates according to a plugin-defined source without a receiver component.
- One custom-nav test actor and a registered experimental nav-data class, initially allowed to return a deliberately simple surface-constrained path.
- One AI MoveTo test that consumes that path, with the fallback adapter called out if needed.
- Test map or reproducible setup, compile instructions, known limitations, and a compact Phase 0 results sheet.

**Iteration 001 exit criteria:** Plugin compiles in UE 5.8.3 and we have evidence for the two most important extension points: ordinary rigid-body gravity and custom navigation registration. End-to-end movement and EQS are Phase 0 completion requirements, not necessarily Iteration 001 requirements.

## 12. Public API and UX checkpoints

- One obvious Gravity Source placement workflow with shape-dependent Details options, not five unrelated UX patterns unless needed.
- Human-readable property names, sensible gravity defaults, direct visual feedback, and safe changes at edit time.
- Clear `Automatic`, `Integrated`, and `Unsupported` documentation. Do not mislead users into thinking arbitrary code with fixed `FVector(0,0,-1)` will be rewritten.
- Gravity probe gives source, acceleration, magnitude, up-policy, frame continuity state, and nav availability.
- Override and opt-out mechanisms are discoverable in project settings or per-actor controls.
- Source types support Blueprint spawn/setup and C++ extension where relevant.
- Default project gravity remains familiar until a user explicitly enables/replaces it.

## 13. References checked when creating this roadmap

- Epic Games: `UCharacterMovementComponent::SetGravityDirection`, UE 5.8: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UCharacterMovementComponent/SetGravityDirection
- Epic Games: `ANavigationData`, UE 5.8: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/NavigationSystem/ANavigationData
- Epic Games: `UPathFollowingComponent`, UE 5.8: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/AIModule/UPathFollowingComponent
- Epic Games: `UEnvQueryGenerator`, UE 5.8: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/AIModule/UEnvQueryGenerator
- Epic Games: `UEnvQueryTest`, UE 5.8: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/AIModule/UEnvQueryTest
- Epic Games: `USplineComponent`, UE 5.8: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USplineComponent
- Epic Games: Chaos callbacks custom-gravity tutorial: https://dev.epicgames.com/community/learning/tutorials/lydy/unreal-engine-using-chaos-callbacks-for-a-custom-gravity-system-working-with-round-worlds
- Epic Games: Mover in Unreal Engine 5.8 (Experimental): https://dev.epicgames.com/documentation/unreal-engine/mover-in-unreal-engine
- Epic Games: World Partitioned Navigation Mesh 5.8 (Experimental): https://dev.epicgames.com/documentation/unreal-engine/world-partitioned-navigation-mesh

---

**Decision for next pass:** Execute Phase 0, Iteration 001 against stock UE 5.8.3, then re-evaluate architecture based on actual compile, runtime, and packaged test evidence. The plan is a living engineering baseline, not proof that any untested interface will work.
