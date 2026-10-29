# Orbit | Gravity-relative Character Animation Integration

**Plugin:** Orbit 0.2.11, Iteration 022  
**Target:** UE 5.8.3 Third Person native `ACharacter` with Manny/Quinn animation setup  
**Scope:** Correct gravity-local locomotion parameters, not automatic mutation of existing Animation Blueprints

## Confirmed UE 5.8.3 solution: ABP_Unarmed_Orbit + BS_Idle_Walk_Run

**User-verified in PIE, October 28, 2026.** The user supplied their active `ABP_Unarmed_Orbit` Event Graph text, graph screenshots, and `BS_Idle_Walk_Run` asset, captured the movement state while semi-strafing on a large Halo-style ring, then reported **"that fixed it"** after replacing the Event Graph data producers with Orbit's gravity-local animation state. This is an observed integration fix, not a claim that Orbit automatically rewrites existing AnimBP assets.

The graph originally did **two incorrect things** on arbitrary-Up terrain:

1. `MovementComponent.Velocity -> Vector Length XY -> Set GroundSpeed`, ignoring velocity along world Z. At the symptom capture Orbit reported about **600 cm/s** gravity-tangent ground speed but only **234.5 cm/s** world-XY speed.
2. `Calculate Direction(Velocity, GetActorRotation) -> Clamp/Select by Orient Rotation to Movement -> Set Direction`, calculating a locomotion angle from the wrong world-horizontal frame and using Unreal's native rotation toggle as a proxy for Orbit's facing mode.

The animation graph used `BS_Idle_Walk_Run`, a **directional** Blend Space driven by `Direction` and `GroundSpeed`. Thus bad direction values actually selected strafe/diagonal gait samples. The supplied graph **did not contain Stride Warping or Orientation Warping**; those are optional issues for different projects, not the cause in this case.

**Verified replacement** in `Event Blueprint Update Animation`:

| Existing AnimBP variable | New source | Consumer |
| --- | --- | --- |
| `GroundSpeed` | `Get Orbit Gravity Animation State -> Ground Speed` | `BS_Idle_Walk_Run.Speed` |
| `Direction` | `Get Orbit Gravity Animation State -> Movement Direction Degrees` | `BS_Idle_Walk_Run.Direction` |
| `ShouldMove` | `Get Orbit Gravity Animation State -> Should Move` | Idle / Walk Run state transition |
| `IsFalling` | `Get Orbit Gravity Animation State -> Is Falling` | Main States jump/fall/land transitions |

Remove/disable the previous Set nodes; otherwise they can overwrite the correct variables. Keep the separate raw `Velocity` variable only for graph logic that explicitly needs world velocity.

The legacy `Velocity.Z > 100` test in the **To Falling -> Jump** transition remains world-Z-dependent. Starting in v0.2.11, replace it with **Gravity Relative Vertical Speed > 100 cm/s**, additionally AND with `IsFalling` as in the original graph. Positive values mean ascending away from local gravity, including when jumping along negative world Z.

See [the exact example graph wiring](Examples/ABP_UNARMED_ORBIT_VERIFIED_WORKFLOW.md). It is a complete **construction recipe**, not a generated `.uasset`; native Unreal Editor is required to create or edit Animation Blueprints.

## Optional Animation Instance parent (Iteration 022)

`UOrbitGravityAnimInstance` is now a native optional parent class for Animation Blueprints. Its `NativeUpdateAnimation` gathers `OrbitAnimationState` from the owning Character on the game thread. It does **not** replace existing Animation Blueprint state assignments or modify Blend Spaces automatically. Choose **one producer**: use the normal pure `Get Orbit Gravity Animation State` node with a default AnimInstance, or reparent to `Orbit Gravity Anim Instance` and break the inherited cached `OrbitAnimationState` struct. Never drive the same variables from both paths.

The inherited property is valid only with a Character pawn owner and is reset to `bValid=false` otherwise. Parent-class reparenting can affect customized AnimBP bases; duplicate and compile the asset before adopting this approach. Non-Character/Mover-specific animation instances need their own data producers.

## Gravity-relative jump and fall transitions (Iteration 022)

`FOrbitGravityAnimationState` adds:

- **Gravity Relative Vertical Speed** (`cm/s`, signed): `Dot(CharacterMovement.Velocity, GravityUp)`.
- **Is Ascending:** true for vertical speed greater than `+1 cm/s`.
- **Is Descending:** true for vertical speed less than `-1 cm/s`.

The pure node **Calculate Orbit Gravity Vertical Speed** accepts a world Velocity and world Gravity Up, returning a validity boolean and signed speed. Invalid Up produces false and speed zero. These values are intended for game-thread AnimBP updates; cache them to AnimBP variables for threaded graph evaluation.

For the supplied ABP's `To Falling -> Jump` transition, replace `Velocity.Z > 100` with `Gravity Relative Vertical Speed > 100`. Do not merely replace it with `Is Ascending`, which has a lower threshold and may trigger differently. `Is Falling` still comes from native Character Movement, so the original movement-mode gate is preserved. If jump animations play while movement mode remains Walking, inspect the native movement mode first; gravity-local speed cannot fix invalid native states.

## Why forward movement appears to strafe on a planet

Gravity-relative movement can be correct while an existing AnimBP or warping node interprets the velocity in **world XY**. Running along world Z at a planet's equator produces almost zero `Vector Length XY` even if the gravity-tangent ground speed is 300 cm/s. World yaw or world-Z `Calculate Direction` likewise ceases to match the character's local movement frame. The original GIFs showed gait transitions while holding only W or S; the exact cause was subsequently verified in the supplied ABP graph and corrected as described above.

Iteration 018 addresses a second suspected contributor: Orbit Gravity no longer applies a competing actor rotation when Orbit Controls is responsible for character facing, and Controls now parallel-transports its facing heading over curved surfaces. Orbit can optionally disable and restore native character rotation owners. The final orientation update runs on Character Movement's completed-movement delegate, rather than waiting for a separate late controls tick. These changes alone cannot correct an Animation Blueprint that consumes incompatible values.

## Required Character setup

1. Add **Orbit Character Gravity** and **Orbit Character Controls** to your Third Person Character.
2. Orbit Character Gravity: enable **Align Actor to Gravity** and **Match Gravity Magnitude**. Keep the desired gravity smoothing enabled for smooth planet transitions.
3. Orbit Character Controls: enable **Face Movement Direction**, **Face Velocity When Moving**, **Disable Competing Native Rotation**, and **Orient Camera to Gravity**.
4. Forward Enhanced Input IA_Move and IA_Look Vector2D values to Orbit's input functions **instead of** the existing template movement/look branches. If both run, movement may be doubled or rotate unpredictably.
5. Avoid simultaneous `SetActorRotation` code in the Character Event Graph, `Use Controller Rotation Pitch/Roll`, root motion that controls actor rotation, or any other Actor-facing system, unless deliberately coordinated.

## Gravity-local animation node hookup (manual and explicit)

**A. Locate the actual locomotion graph.** Open the Animation Blueprint(s) used by your character's Skeletal Mesh. Manny and Quinn may use separate AnimBP instances, linked animation layers, and motion-warp/locomotion graphs depending on the Third Person template. Inspect the actual active graph. Do not guess an asset name or rely on only one display-name value.

**B. Gather state on the game thread.** In the Animation Blueprint's **Event Blueprint Update Animation**:

```text
Event Blueprint Update Animation
  -> Try Get Pawn Owner
  -> Cast to Character (or use the known Character reference)
  -> Get Orbit Gravity Animation State
       Character = pawn
       Min Ground Speed = 3
       Min Acceleration = 0.1
  -> Break Orbit Gravity Animation State
  -> Cache outputs into AnimBP variables
```

Use the **game-thread Event Graph** rather than directly calling arbitrary Character/World Blueprint functions inside a thread-safe animation worker evaluation. If your AnimBP uses a thread-safe update/property-access pattern, write into variables on the game thread, and use those cached variables within the animation evaluation thread according to the AnimBP's setup.

**C. Replace incompatible data producers** wherever the active locomotion graph uses them:

| Typical input / computation | Replace with Orbit state |
| --- | --- |
| `Vector Length XY` of World Velocity | **Ground Speed** |
| World-Z `Calculate Direction` or world yaw projection | **Movement Direction Degrees** (0 forward, +90 right, -90 left, ±180 back) |
| World XY `Velocity` used to classify running/strafing | **Forward Speed / Right Speed**, or **Tangent Velocity** if a vector is needed |
| World XY acceleration / `Is Acceleration` | **Has Acceleration** or **Tangent Acceleration Magnitude** |
| Moving/idle gate built from ground speed + acceleration | **Should Move** (thresholds configurable on node) |
| Native Character Movement `Is Falling` | **Is Falling** (same movement mode, exposed for convenience) |

`World XY Speed` is supplied for diagnostics only. **Do not use it to drive locomotion on planets.** Preserve animation-design intent: a graph may expect velocity in mesh-local axes or a normalized gait parameter, not speed in cm/s. Adapt units and ranges accordingly.

**D. Check UE 5.8 Animation Warping gravity-vector inputs.** Unreal's `FAnimNode_StrideWarping` has a `GravityDirection` vector that defaults to **world (0,0,-1)**, and a `FloorNormalDirection` that defaults to **world (0,0,1)**. These defaults are inappropriate on the sides and underside of a planet. If your AnimGraph contains this node, configure the exposed inputs, where supported, to use **Gravity Down** and **Floor Normal** from Orbit Gravity Animation State rather than constant world-Z. `Floor Normal` uses Character Movement's native CurrentFloor surface normal when walkable, otherwise local Gravity Up. `Gravity Up`, `Gravity Down`, `Floor Normal` and `World Tangent Move Direction` are vectors in **world space**; match each animation node's expected vector space and representation (e.g., `FWarpingVectorValue`) instead of connecting mismatched spaces.

`FAnimNode_OrientationWarping` in UE 5.8 can accept a **world-space LocomotionDirection** or an angle calculated from an animation-local rotation frame. Where that is exposed, supply Orbit's **World Tangent Move Direction** or **Movement Direction Degrees**, respectively, accounting for mesh-to-actor facing offsets and the node's chosen Rotation Axis. Check warping evaluation mode and root-motion requirements; do not blindly supply both an angle and a direction if the node chooses one path. These are supported node interfaces, **not automatically wired by Orbit**.

**E. Validate any orientation/stride warping separately.** Unreal's warping nodes or the template's locomotion graph may calculate direction internally from World Velocity, Controller Yaw, mesh root, or a fixed world Up. If the Blend Space and cached state are correct but legs still cross-step, inspect that portion of the AnimGraph next, especially whether Stride Warping still uses world-down gravity or Orientation Warping still receives world-yaw-derived direction. Depending on the chosen animation assets, warping may require a gravity-frame input, a custom node, or disabling warping as a **diagnostic**. Orbit 0.2.11 does **not** claim full Manny/Quinn warping compatibility or automatically modify warping nodes.

## Practical Planet A + Sphere test

1. Center a spherical mesh and Point/Inward gravity source on `(0,0,2000)`. Place the player along +X or at the top for comparison.
2. In Orbit Character Controls, enable **Draw Locomotion Debug** and inspect its PIE runtime Details.
3. Hold W and do not move the mouse. The debug colors are **Up white**, **requested input blue**, **actual tangent velocity green**, **Actor forward yellow**. Request, velocity and facing should have a sensible relationship as local Up rotates.
4. Compare `Last Gravity Relative Ground Speed` versus `Last World XY Speed`. On near-vertical stretches, the world XY reading can approach zero despite continuous running. Only gravity-relative speed belongs in a ground locomotion graph.
5. Inspect `Last Velocity to Facing Angle Degrees` and `Last Capsule Up Error Degrees`: unexplained large offsets indicate actor orientation drift, independent of the gait graph.
6. Feed the node into the active AnimBP, and verify cached `Movement Direction Degrees` remains near 0 while facing and moving forward. If it does but the character semi-strafes, **the remaining fault is downstream in the animation graph** and merits a capture of its BlendSpace/warping nodes.
7. Repeat at poles, underside, between two overlapping sources, idle, and with S held. Backward input may intentionally turn the character to face its travel direction when `Face Movement Direction` is enabled. If your game specifically wants backwards locomotion rather than turning, disable facing for that mode and drive the AnimBP's backwards gait with the signed direction.

## Limitations and diagnostic boundaries

- The node returns motion state for **native Character Movement**. It is not a Mover simulation data API.
- No existing `.uasset` is patched. An Animation Blueprint cannot be corrected merely by including the plugin, and its internal Orientation Warping and Stride Warping settings remain unchanged.
- The plugin does not invent or transform native physics velocity to fool world-horizontal animation graphs.
- The debug arrows and diagnostics are opt-in and intended for PIE. Native third-party camera, rotation or root-motion systems may still interfere.
- Character rotation work is tested mathematically only until your UE 5.8.3 PIE test confirms there is no foot sliding, unwanted yaw, unwanted capsule tilt, or replication issue.

## Epic UE 5.8 source references

- [FAnimNode_StrideWarping API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/AnimationWarpingRuntime/FAnimNode_StrideWarping): GravityDirection, FloorNormalDirection, LocomotionSpeed, StrideDirection.
- [FAnimNode_OrientationWarping API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/AnimationWarpingRuntime/FAnimNode_OrientationWarping): LocomotionDirection (world-space), LocomotionAngle, RotationAxis, evaluation mode.
- [Pose Warping in UE 5.8](https://dev.epicgames.com/documentation/unreal-engine/pose-warping-in-unreal-engine): example graphs and native warping debug display.


## v0.2.10 inactive controls regression correction

In v0.2.9, an existing Character Blueprint could retain `Auto Activate = false` on its Orbit Character Controls component. Unlike movement-input and debug drawing, facing ownership checks `IsActive()`. A user observed **Debug Tick Count 4144**, **Native Movement Callback Count 4144**, **Orbit Move Input Count 590**, but **Facing Update Attempt Count 0**, `Last Facing Debug Status = Controls component is not active`, and no cyan desired-facing arrow. This is an activation failure, **not evidence that movement vectors or gravity Up are wrong**. v0.2.10 defaults to Auto Activate and activates the component during BeginPlay when Face Movement Direction is enabled, even for saved Blueprint component instances. `Facing Begin Play Activation Count` increments on repair. When disabling facing intentionally during runtime, call `Deactivate` AFTER BeginPlay. Recheck cyan/yellow arrows and facing counters before investigating the AnimBP.

## v0.2.8 facing/debug visibility correction

In v0.2.7 the debug arrows were incorrectly gated on active facing ownership. If the character was not locally controlled or the component was inactive, the debug display silently disappeared. v0.2.8 draws when requested (provided the component ticks) and adds PIE-visible `Last Facing Debug Status`, `Debug Tick Count`, `Native Movement Callback Count`, `Tick Facing Fallback Count` and `Orbit Move Input Count`. When native Character Movement does not emit a callback for a frame, a post-update Controls tick can perform the facing update. With Face Velocity When Moving enabled, actual gravity-tangent velocity can select a facing heading even if the Orbit IA_Move bridge did not supply a recent input vector. This does **not** alter Animation Blueprint locomotion Blend Spaces or Warping nodes; the manual gravity-relative animation hookup above remains necessary.

## v0.2.9 post-movement facing update and desired-heading debug

The actor yaw remaining fixed while input and velocity were correct in v0.2.8 is treated separately from the Manny/Quinn animation graph problem. Orbit Controls now applies its final actor rotation in **every eligible PostUpdateWork tick**. The native movement callback only records arrival. Enable Draw Locomotion Debug and compare **cyan desired heading** to **yellow actual Actor Forward**; they should converge while holding W around a planet. Runtime read-only counters and angular errors appear on Orbit Character Controls. See `ITERATION_020.md`. Existing AnimBP world-XY locomotion or world-Up warping remains an independent integration task even after actor yaw works.

## Scope of v0.2.13 physics diagnostics

Center-of-mass gravity sampling, rigid-body near-contact support grace and physics rest debug apply to simulated physics components only. `Get Orbit Gravity Animation State`, the verified `ABP_Unarmed_Orbit` locomotion workflow, and gravity-relative jump data are unchanged.
