# Example: ABP_Unarmed_Orbit, gravity-relative locomotion and jump

**Engine:** UE 5.8.3   
**Source example:** user-provided ABP_Unarmed_Orbit and directional BS_Idle_Walk_Run   
**Status:** core locomotion correction verified by user in PIE, October 28, 2026. Jump change is new and untested in PIE.

This is a complete **node-by-node recipe**, not a prebuilt Animation Blueprint asset. Unreal Editor is required to create/edit `.uasset` graphs. The user's original source `.uasset` and graph text were used as references; **the original broken AnimBP is intentionally not packaged as a sample**.

## Existing graph shape

- `Event Blueprint Initialize Animation` -> `Get Owning Actor` -> `Cast to Character` -> cache `Character` and `Movement Component`.
- `Event Blueprint Update Animation` -> validated `Character` -> Sequence -> three original branches:
  1. `Velocity` and `Vector Length XY` -> `GroundSpeed`; `Calculate Direction(Velocity, Get Actor Rotation)` and `Select`/`Clamp -45..45` -> `Direction`.
  2. `GroundSpeed > 0.01 AND CurrentAcceleration != (0,0,0)` -> `ShouldMove`.
  3. Native `Is Falling` -> `IsFalling`.
- `AnimGraph`: `Main States` (Jump / Fall Loop / Land / Locomotion) with nested `Locomotion` Idle / Walk Run state machine.
- `Walk / Run` state: `BS_Idle_Walk_Run` inputs `Direction` and `GroundSpeed`.
- Original jump transition: `IsFalling AND Velocity.Z > 100`.

## Path A: pure node, no Animation Blueprint reparenting (the verified solution)

1. Duplicate the Animation Blueprint (your project's actual active class) before editing it.
2. Keep its existing `Character` reference initialized by `Blueprint Initialize Animation`.
3. In `Event Blueprint Update Animation`, after the validated Character branch, add the pure `Get Orbit Gravity Animation State` node. Connect the `Character` reference; keep thresholds **Min Ground Speed 3.0**, **Min Acceleration 0.1**.
4. Add `Break Orbit Gravity Animation State` to its return value.
5. On the normal execution path, assign `GroundSpeed` from `Ground Speed`, `Direction` from `Movement Direction Degrees`, `ShouldMove` from `Should Move`, and `IsFalling` from `Is Falling`. Add an AnimBP Float variable `GravityRelativeVerticalSpeed` from `Gravity Relative Vertical Speed`.
6. Remove the old setter chains, especially `Vector Length XY` and `Calculate Direction` -> `Clamp/Select`. Check that no downstream nodes reassign those variables later in the Event Graph.
7. Do **not** change the `BS_Idle_Walk_Run` node. Retain its pins `Direction -> Direction` and `GroundSpeed -> Speed` and the Idle / Walk Run transition rules based on `ShouldMove`.
8. For `To Falling -> Jump`, keep `IsFalling` and replace the world `Velocity.Z > 100` with `GravityRelativeVerticalSpeed > 100`. For optional falling-down decisions use `< -100`, not world `Velocity.Z`.
9. Compile and save the AnimBP. Confirm the Skeletal Mesh's **Anim Class** is the revised asset, then run in PIE.

```text
Event Blueprint Update Animation
  -> Validated Character
  -> Set GroundSpeed              <- Break State.Ground Speed
  -> Set Direction                <- Break State.Movement Direction Degrees
  -> Set ShouldMove               <- Break State.Should Move
  -> Set IsFalling                <- Break State.Is Falling
  -> Set GravityRelativeVerticalSpeed <- Break State.Gravity Relative Vertical Speed

Get Orbit Gravity Animation State(Character, 3.0, 0.1)
  -> Break Orbit Gravity Animation State

Blend Space: BS_Idle_Walk_Run
  Direction  <- AnimBP.Direction
  Speed      <- AnimBP.GroundSpeed

To Falling -> Jump:
  Can Enter = IsFalling AND (GravityRelativeVerticalSpeed > 100.0)
```

## Path B: inherited native data producer

1. Duplicate the Animation Blueprint, then reparent it (Class Settings) to `Orbit Gravity Anim Instance` (`UOrbitGravityAnimInstance`).
2. The native parent now gathers `OrbitAnimationState` during `NativeUpdateAnimation` (game thread). Read and `Break Orbit Animation State` from the inherited `OrbitAnimationState` property in the Event Graph.
3. Use that cached state to assign the same five variables listed in Path A, **without** calling `Get Orbit Gravity Animation State` a second time.
4. The native parent has `Orbit Min Ground Speed` (3.0) and `Orbit Min Acceleration` (0.1) defaults.
5. Compile, save, test; if you have a custom AnimInstance C++ parent, do not reparent it blindly. Keep Path A or integrate state gathering into your existing parent.

## Acceptance tests

- W held, no mouse, run around sphere equator/poles and around a rotated Halo-style ring: `GroundSpeed` ~600 cm/s at full speed, `Direction` ~0 degrees with Face Velocity When Moving.
- Verify world XY speed may vary strongly; it must not drive the Blend Space.
- S held: with Orbit facing following velocity, Direction may remain 0 when the actor turns and runs in the opposite direction. To intentionally play backward animations, configure an explicit backward movement model.
- Jump from top/side/underside of a planet or inner ring: a rise along local Up yields **positive** gravity-relative vertical speed; movement toward the surface yields **negative** speed, regardless of world Z sign.
- At apex, signed vertical speed passes through 0; Jump/Fall animation depends on original state-machine transition design and movement modes.
- Both source handoffs and gravity-zero zones need separate gravity movement-mode and animation transitions; do not infer a stable Up from a zero vector.

## Boundaries

- No `.uasset` shipped: cannot manufacture or validate a functional Unreal AnimBP in a source-only environment. The user-verified active AnimBP and Blend Space are project assets, not third-party assets to redistribute in Orbit.
- No automatic patching of custom user animation graphs.
- No guarantees for motion matching, custom warping, root-motion rotation, multi-threaded Blueprint function calls, or Mover animation state. Use game-thread cache or the native AnimInstance parent.
