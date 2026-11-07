> **Iteration 040 parity:** Expanded gravity sources reuse the same `FOrbitFieldEvaluator::CombineForBody` and existing Chaos physics callback, rather than a duplicate field system. The expanded mode has an independent default-Off setting and lowest selection priority. See [Compatibility Parity](COMPATIBILITY_PARITY.md).

# Orbit Architecture | v0.2.26 (Iteration 037)

## Objective

World-defined local gravity shared by physics, characters, AI and authoring tools, without Unreal Engine modifications. Runtime behavior should remain viable in a normal stock UE 5.8.3 installed-engine project.

## Current dependency direction

```
AOrbitGravitySource (authoring actor, UObject)
      |
      | serializes a per-frame field definition
      v
FOrbitFieldDefinition (UObject-free vectors and plain data)
      |
      v
FOrbitFieldEvaluator (shared direction + strength math)
      |
      v
UOrbitWorldSubsystem (runtime world owner and snapshot)
      |                         |
      |                         +--> Blueprint queries
      |                         +--> Character adapter, optional
      |
      +--> Standard rigid-body force integration (game thread, default)
      |
      +--> Opt-in Chaos gravity bridge (physics thread, group 7)
                  |--> Uniform Directional
                  |--> Single Point
                  +--> Multiple Point (separate opt-in)
```

The authoring actor is *not* called from the field evaluator. Its editable spline is sampled to `TArray<FVector>` polyline and spline-Up vectors in an immutable, UObject-free definition. Standard mode samples the definitions on the game thread. Eligible opt-in Directional and Point solver modes transfer **copies** of field definitions through Chaos's simulation callback input mechanism. No Actor or Component is dereferenced on the physics thread. Mixed/unimplemented solver shapes retain Standard.

## Evaluation rules

- Point: position relative to source actor location.
- Axis: perpendicular component of position relative to source actor-local +Z axis.
- Directional: actor-local -Z or +Z, with optional distance/box influence.
- Volume: box-limited directional field aligned to actor transform.
- Spline: nearest sampled polyline segment. Inward/outward follows the vector to/from this segment; Spline Up uses interpolated authored spline Up vector.
- Constant: configured acceleration magnitude throughout the influence.
- Linear: fades magnitude from source to Max Distance; Max Distance 0 leaves constant strength.
- Inverse-square: magnitude scales by `(ReferenceDistance / max(Distance, ReferenceDistance))²`.
- Max Distance: 0 is unlimited. Distance is measured relative to the source geometry.
- Singularity: a location exactly on the source axis/curve/point has no radial direction. Returns influenced but zero acceleration and invalid local Up.
- Composition: the highest-priority active override is selected. Additive sources with priority above that override can contribute. If no override applies, the sum of additive fields is returned.
- Channel filters: General, Physics, Character. A source can be enabled for one consumer category and disabled for another.
- Default fallback: world gravity along Z outside the influence of custom sources, if enabled in project settings.

## Current runtime loop

1. Source registers itself with its play-world subsystem via BeginPlay.
2. On each world-subsystem tick, Orbit rebuilds field definitions from enabled registered sources.
3. At a configurable interval, Orbit discovers eligible `UPrimitiveComponent` instances in this world, remembering their original native gravity setting.
4. In **Standard** mode (the default), Orbit disables the managed component's native gravity, samples the Physics field per body, and calls `AddForce(Acceleration, NAME_None, true)` each game tick.
5. In explicitly enabled **Experimental Chaos** mode with eligible fields, Orbit assigns eligible static-mesh rigid bodies to gravity group 7 instead. One uniform Directional source supplies a shared group acceleration; one ordinary Point source uses per-particle PreIntegrate; multiple eligible Point sources use the same per-particle path only with the **separate multi-source opt-in**. Multi-Point uses `FOrbitFieldEvaluator::CombineForBody()` for priority and additive/override rules. Noneligible fields and receivers use Standard.
6. The separately opt-in early-ownership setting can preclaim eligible pre-existing bodies in `OnWorldBeginPlay` to avoid the measured first-step world-gravity impulse. Runtime-spawned/streamed bodies still use tick discovery.
7. On release, method switch, EndPlay or subsystem deinitialization, Orbit restores the remembered component gravity state and original Chaos gravity group and unregisters the callback when no longer needed.

The solver gravity implementation is **experimental**; the user has validated select Directional, single-Point and early-ownership PIE scenarios. Multi-Point runtime behavior has not yet been validated or Shipping-packaged. See [`EXPERIMENTAL_GRAVITY.md`](EXPERIMENTAL_GRAVITY.md).

## Editor style boundary (introduced 0.1.1)

`OrbitEditor` is an Editor-only module that owns `OrbitEditorStyle`. It loads the supplied icon resources from the plugin's `/Resources` directory, registers `ClassIcon`/`ClassThumbnail` entries for existing Orbit types on startup, and unregisters them on shutdown. It has no gameplay logic. The runtime `Orbit` module does not depend on Slate, SlateCore, Projects, or OrbitEditor. The 64px class thumbnail is not a custom Content Browser asset type.

## Intended future module boundaries

- `Orbit`: pure gravity definitions, evaluation, field source authoring, Blueprint access and world subsystem.
- `OrbitPhysics`: Chaos solver callback, robust rigid bodies, multi-body skeletal gravity, advanced physics tests.
- `OrbitMovement`: Character / Mover adapters, projectiles, vehicles where compatible, camera helpers.
- `OrbitNavigation`: new gravity-relative surface navigation representation, pathfinding and streaming.
- `OrbitAI`: EQS generators/tests, path following, local-tangent avoidance.
- `OrbitEditor`: **currently exists** for class icons and style registration; future work includes field inspector, context tools, diagnostics and test fixtures.
- Optional, separately enabled integrations: Niagara, PCG, Water, Mass, Chaos Vehicles, cloth, animation.

At the present stage, the lightweight adapters share a single `Orbit` module to reduce build friction. Modules should be split only after the correct engine extension points and dependency boundaries are confirmed.

## Navigation proof requirements

1. A plugin-only `ANavigationData` subclass compiles and registers correctly.
2. A 3D navigable surface can be represented on the full outer shell of a sphere and interior of a torus, without world-XY projection collapse.
3. A path query can return a connected route around curved surfaces.
4. AI path-following correctly traverses that route, using a gravity-relative arrival check.
5. EQS's projected/queried locations match the actual navigation representation and agent capabilities.
6. Packaged Win64 build links without needing inaccessible engine internals.

Before writing this, we should inspect the installed 5.8.3 headers for NavigationData, Recast, NavigationSystem, path following, and link generation, rather than guessing engine internals.

## Thread safety plan

The editor/runtime actor representation and the simulation representation must be separate. Field snapshots should be built on the game thread and published atomically/at a safe simulation boundary to the physics-thread consumer. Avoid storing raw actor pointers or directly calling `USplineComponent` inside physics callbacks. Stable IDs and deterministic updates need to replace runtime `GetUniqueID()` diagnostics when networking is added.

## API compatibility rule

Avoid private engine includes, memory patching, engine forks and copying engine implementation where possible. When a subsystem has no extension point, document that limitation or replace functionality through public plugin APIs rather than claiming transparent interception.


## Internal optional OrbitMover module (Iteration 017)

The distribution is a **single Orbit plugin** with three modules: `Orbit` (gravity/runtime), `OrbitEditor` (editor tools), and `OrbitMover` (optional-in-use Mover adapter). The `OrbitMover` module has **no** compile-time dependency on Epic's Experimental Mover. It locates Mover components at runtime by class ancestry, validates the public reflected methods, and passes world-field gravity/Up using `ProcessEvent`. If Mover is disabled, Orbit still loads and other gravity consumers work normally. Kinematic overrides are updated during game-thread `TG_PrePhysics` and can be ordered before normal Mover ticks via a tick prerequisite; unlike the old typed callback, this cannot guarantee ordering in independent simulations. Chaos Mover, async simulation, rollback/prediction remain unsupported. See `Docs/MOVER_INTEGRATION.md`.


## Iteration 015: Point source ownership and editor orientation

The shared `FOrbitFieldEvaluator::Combine`/`CombineForBody` now disambiguates **same-priority Point Override** fields with distance-squared to center, so source registration order cannot cause one planet to own the other side of an overlap. Priority still takes precedence; non-Point ties use prior selection semantics. Discontinuity at the bisector is currently intentional pending a designed smooth transfer/hysteresis model.

The `OrbitEditor` module extends `LevelEditor.ActorContextMenu` via `ToolMenus`. It enumerates enabled source definitions in the actor's editor world, applies receiver channel matching and the same pure field evaluator, then uses `FOrbitGravityFrame::MakeAlignedRotation` in an undoable actor transaction. Editor authoring does not require a gameplay World Subsystem, which exists only in game worlds.

### Experimental solver gravity scope (v0.2.26)

The per-world `FOrbitChaosGravityBridge` uses Chaos group 7 only after explicit selection of the Experimental method and field eligibility. Uniform Directional uses the group-shared native acceleration; single Point and optional multiple Point sources set the shared acceleration to zero and inject per-particle gravity during Chaos PreIntegrate. Multi-Point supports 2–32 channel-0-only ordinary Point physics sources and only channel-0-only static-mesh receivers, and requires the independent default-off `Experimental: Multiple Point Sources` setting. Remaining source combinations or receivers stay Standard. The pre-BeginPlay ownership option is also separately default-off. No engine edits or separate plugin variants. See [`EXPERIMENTAL_GRAVITY.md`](EXPERIMENTAL_GRAVITY.md) for maintained eligibility and validation status.
