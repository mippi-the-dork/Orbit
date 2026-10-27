# Orbit Compatibility and Validation Matrix

**Plugin:** Orbit 0.2.4, Iteration 015  
**Engine target:** Unreal Engine 5.8.3  
**Verification:** Source and documentation checks only; UE compilation, live simulation, and packaged game tests not yet performed.

## Support vocabulary

- **Implemented, unverified**: Plugin code exists, but no matching UE build/runtime result has been supplied.
- **Experimental**: Integration is structurally usable but has limitations and has not passed production-quality validation.
- **Designed / not implemented**: Included in roadmap and architecture only.
- **Unsupported**: Not currently supported by this plugin version.

| Engine area | Integration type | v0.1.9 status | Validation needed |
| --- | --- | --- | --- |
| Orbit editor icons | Editor-only Slate style set, ClassIcon/ClassThumbnail | Implemented, unverified | Editor visual validation |
| Point gravity | Native Orbit evaluator | Implemented, unverified | Automation + PIE |
| Axis gravity | Native Orbit evaluator | Implemented, unverified | Automation + PIE |
| Directional gravity | Native Orbit evaluator | Implemented, unverified | Automation + PIE |
| Box / Sphere / Cylinder / Capsule Volume gravity | Native Orbit evaluator | Implemented, unverified | Automation + PIE |
| Volumetric debug preview and billboard | Runtime/editor visualization | Implemented, unverified | Viewport selection, PIE |
| Finite Max Distance wireframes and auto-fitted previews | Point sphere, Axis radial cylinder, Spline tubular approximation, additional Volume cutoff | Implemented, unverified | Shape-by-shape viewport validation |
| Streamlines | Editor-only RK2 field integration with signed vortex visualization, cached path rendering and segment budget | Implemented, unverified | Field curves, direction markers, boundaries and dense-spline viewport performance |
| Gravity Channels 0/1/2 | Source/receiver masks; no receiver component means channel 0 | Implemented, unverified | Physics, actor and character query validation |
| Rest stabilization | Single-body static mesh contact sleep test, no restitution changes | Experimental | Physics settling, collisions and waking tests |
| Closed spline / oriented spline down | Polyline approximation and authored spline roll | Implemented, unverified | PIE, seam, twisting hallway tests |
| Strength falloff | Constant / linear / inverse square | Implemented, unverified | Automation |
| Source precedence | Additive / priority override | Implemented, unverified | Automation |
| Rigid bodies | Game-thread AddForce, mass-independent acceleration | Experimental | PIE and substep tests |
| Runtime spawned bodies | Periodic discovery | Experimental | PIE spawn/despawn tests |
| Skeletal physics / ragdolls | May only affect root rigid body | Unsupported as complete integration | Multi-body tests |
| Character Movement | Opt-in SetGravityDirection, gravity magnitude, native restoration and actor alignment | Experimental | PIE magnitude, transitions, poles, camera/controller interaction |
| Camera / input | User project controls | Not implemented | Movement milestone |
| Projectiles | No adapter | Not implemented | Movement milestone |
| Chaos solver callbacks | Architecture placeholder | Not implemented | UE header/code audit |
| Custom NavMesh / navigation graph | No code | Not implemented | Navigation feasibility gate |
| AI MoveTo / path following | No integration | Not implemented | Navigation feasibility gate |
| EQS | No integration | Not implemented | AI milestone |
| Crowd / Mass | No integration | Not implemented | AI milestone |
| Niagara / GPU particles | No integration | Not implemented | Ecosystem milestone |
| Chaos Vehicles / cloth / geometry collections | No dedicated integration | Not implemented | Ecosystem milestone |
| PCG / foliage | Gravity query exposed, no native nodes | Authoring support only | Ecosystem milestone |
| Networking | No source replication or prediction | Unsupported | Networking milestone |
| World Partition | No custom persistence model | Not implemented | Large-world milestone |
| LWC | Field values use FVector / double engine types | Partial architecture | Large-world stress test |

## Known behavior boundaries

- There is no plugin-independent way to rewrite all arbitrary world-Z assumptions embedded in project and third-party gameplay code.
- Simulated components with their native gravity already disabled are ignored unless a project setting or component tag opts them into Orbit management.
- The current physical acceleration is game-thread force application, not guaranteed per-substep or physics-thread execution. Sleeping-body stabilization is a stopgap, not a replacement for solver callbacks.
- Field combinations and spline nearest-segment evaluation happen on regular game thread snapshots. The plain field data can later be used by a Chaos solver callback, but thread-safe integration is not yet implemented.
- Component gravity-state ownership is currently a simple original-value capture and restore. Multiple systems independently changing Enable Gravity require an ownership policy.
- Character adapter applies optional actor/capsule rotation but does not align the controller/camera, adapt animation or input, or provide network prediction.
- World gravity fallback and zero-gravity source have different meanings. A source with zero acceleration can intentionally suppress the fallback.

## Channel and boundary semantics

- No Receiver component means Channel 0. Source defaults to Channel 0. Any matching bit (0/1/2) grants access; Physics/Character flags are separate.
- Unmatched physics bodies are never taken over, even if `Use World Gravity Outside Fields` is disabled. A channel-matched body outside all active influence can enter zero gravity if that setting is disabled.
- Max Distance uses Euclidean distance from Point/Directional/Volume source center, radial distance from the Axis, and closest polyline distance from a Spline.
- Volume shapes and optional Box masks further restrict the field, so blue Max Distance boundary alone is not necessarily the exact effective region. A spline boundary tube is a sampled visual approximation.
- Editor debug render geometry is not collision geometry; Preview Extent is never a simulation boundary.


## Epic Mover integration (Iteration 011)

An internal `OrbitMover` module (inside the **one Orbit plugin**) exposes a Mover gravity/Up adapter with **no compile-time dependency on Epic Mover**. It dynamically finds Mover and uses its reflected public API during game-thread pre-physics ticking. Sync kinematic use is experimental; independent simulation timing is not guaranteed. Chaos Mover, async and rollback-deterministic gravity are **not supported**. See `MOVER_INTEGRATION.md`.


## Iteration 012: Native Character reliability compatibility

- `Orbit Character Controls` targets native `ACharacter` / `UCharacterMovementComponent`; it is **not** an adapter for Epic Mover or Chaos Mover. Manual IA_Move/IA_Look routing required, with old nodes disconnected.
- First `USpringArmComponent` on a locally controlled Character can be oriented in world space while the controls component is enabled. Spring Arm's original absolute-rotation and pawn-control-rotation flags are restored on disable/EndPlay. Projects with custom camera rigs require their own adapter.
- Experimental low-g landing is gated by the native UE 5.8.3 `ComputeFloorDist`/`IsValidLandingSpot` public APIs and normal `MOVE_Walking` transition. It is not a general collision or arbitrary-Pawn movement replacement.
- No source replication, deterministic Character client prediction or rollback guarantee has been added in this release.


## Iteration 015 behavior delta

- Two overlapping Point Override fields at equal Priority now arbitrate by **nearest center**, irrespective of source registration order. Explicit higher Priority still wins and non-Point ties retain prior semantics. This is a gameplay behavior change: test existing maps relying on order-sensitive overlapping Point sources.
- One-shot **Orient to Gravity** appears in the Level Editor actor context menu. It does not exist in cooked builds and does not change runtime gravity behavior.
- Crossing the gravity boundary can still abruptly reverse Up, especially between opposing inward Point fields. No source hysteresis, blending or assisted transfer is yet available. Networking/deterministic simulation are not supported in this milestone.
