# Orbit: Components and Blueprint API Guide

**Version:** Orbit v0.2.6, Iteration 017  
**Unreal Engine target:** 5.8.3  
**Audience:** Blueprint users, gameplay designers, and developers integrating Orbit into an Unreal project  
**Source of truth:** Orbit v0.2.11 plugin headers and implementation, particularly `OrbitCharacterGravityComponent`, `OrbitGravityReceiverComponent`, `OrbitBlueprintLibrary`, `OrbitGravitySource`, and `OrbitWorldSubsystem`.

This document describes **functionality present in the Orbit v0.2.6 source package**, not planned navigation, EQS, camera, or Chaos solver integrations.

For the placeable Gravity Source Actor and the new embedded Gravity Source Component, its property settings, shape and vortex modes, visualization, and level setup examples, see [`GRAVITY_SOURCE_ACTOR.md`](GRAVITY_SOURCE_ACTOR.md).


## Iteration 016: two additional components and Blueprint nodes

**Orbit now has six addable components in one plugin**: Character Gravity, Character Controls, Gravity Receiver, Gravity Orientation, Gravity Source, and Orbit Mover Gravity. The sixth is useful only when Epic Mover is enabled in the project. Internally the adapter is in `Source/OrbitMover`, but there is no second plugin to install.

### Orbit Gravity Orientation Component (`UOrbitGravityOrientationComponent`)

Add it to any Actor whose **actor rotation** should track gravity (for example, a movable platform). Defaults to the combined field at the actor pivot and respects the Actor's Gravity Receiver channels.

| Property | Default | Function |
| --- | --- | --- |
| Enabled | True | Toggles the component's per-frame updates |
| Local Axis | +Z | Which **signed local axis** should aim toward gravitational **Up** (+X, -X, +Y, -Y, +Z, -Z) |
| Response Rate | 8 / second | Exponential smoothing; 0 snaps |
| Specific Source Actor | None | Evaluate only this `AOrbitGravitySource`, overriding combined fields |
| Specific Source Component | None | Evaluate only this embedded source, takes precedence over Source Actor |
| Hold Orientation Without Gravity | True | Keep last rotation when field is undefined, zero or source is out of range |

The component has **Orient Now**, a Blueprint-callable function that applies a one-shot rotation immediately and returns success/failure. `Last Gravity Up` and `Last Sample Valid` are runtime diagnostics. **It rotates the Actor, not individual meshes**, and does not simulate torques or align the actor's position. Requires movable components for continuously rotating collision geometry. Don't add it to a character when Character Gravity/Controls already rotate that same Actor.

### Orbit Gravity Source Component (`UOrbitGravitySourceComponent`)

Add **Orbit Gravity Source** to any Actor, attach it to an appropriate Scene Component, and choose Shape, Direction, Strength and Max Distance. For Shape = Volume, finite Max Distance caps the selected Box/Sphere/Cylinder/Capsule dimensions in source-local space (not an additional spherical mask); this applies identically to source Actors and embedded source components. Its field center, axis, and rotation use the **source component's world transform**, not the owner's pivot. A moving parent moves the field on the next runtime subsystem tick.

It shares the dedicated Gravity Source Actor's major field properties: all five shapes, inward/outward and vortex directions, strength, constant/linear/inverse-square falloff, Max Distance, Priority, Override/Additive, channels, Physics/Character flags, volume/bounding masks, and all Spiral Capture controls. A component configured for Spline uses its `Gravity Spline` component reference or the first `USplineComponent` on the owner; oriented spline Up uses the referenced spline roll.

- `Set Gravity Strength` (on the source component) immediately updates the current world snapshot.
- `Sample Source Component` samples just this component, using its own channels, without adding other sources.
- Dynamic actors/components register when they enter play and unregister on EndPlay.
- Editor visualization and editor-world property mirroring are **not** yet equivalent to the dedicated source Actor's billboard, debug preview and PIE mirroring. Runtime source logic is shared.
- A component placed on a moving Actor requires care: do not independently rotate its parent with an Orientation Component unless you intend to rotate the gravity field too.

### New Blueprint library functions

| Node | Inputs | Outputs | Use |
| --- | --- | --- | --- |
| **Get Orbit Gravity Up for Actor** | Actor, World Position, optional Specific Source (`Orbit Gravity Source` Actor or Component) | Boolean, Up vector | Read a valid gravitational Up; false when no matching influence/zero vector |
| **Make Orbit Axis Aligned Rotation** | Current Rotation, Gravity Up, Local Axis | Boolean, Aligned Rotation | Make a one-shot rotation that brings the chosen local axis to gravity Up with minimal twist |
| **Orient Actor to Orbit Gravity** | Actor (Self), Local Axis, optional Specific Source | Boolean | Sample and apply actor rotation in one Blueprint node |

No source selection reference means **combined field** and the Actor's receiver channels. Source references ignore other fields but still honor the source's enabled state, channels and range. These nodes cannot infer a unique Up at a perfectly cancelling field; they return false rather than inventing a direction.

### Updated Character Gravity settings

- **Use Source Hysteresis** (default On): when using Strongest Influence, retain the current source until a challenger exceeds its score by the **Character Switch Hysteresis** fraction set in Project Settings > Plugins > Orbit (default 0.15).
- **Smooth Gravity Direction** (default On): interpolate the native Character Movement gravity direction using a rotation, including near-opposite directions.
- **Gravity Direction Response Rate** (default 5/s): controls interpolation rate separately from Actor Orientation Response Rate. Setting Smooth Off restores instant handoffs.

The underlying gravity *magnitude* remains sampled directly; the smoothed direction is a character-specific movement adaptation, not a retroactive change to Blueprint gravity sampling, rigid-body physics, or Mover. Character Controls and Character Gravity can still compete for Actor rotation; that older integration issue remains open.

### Project Settings: competing sources

- `Override Resolution = Strongest Influence` is the new default for eligible **equal-priority Override** sources; the weight is instantaneous acceleration magnitude times a monotonic spatial proximity term. Sources with Constant falloff therefore still compete by distance.
- `Blended Influence` calculates a normalized weighted average of equal-priority Override acceleration vectors. Equal and opposite inputs can give a valid **zero-gravity** region. A blended field can produce smooth acceleration but does not mean a character's camera/rotation is automatically smooth.
- `Priority Override (Legacy)` retains the v0.2.4 closer-Point rule (and first-seen ties for other shapes).
- Higher **Priority** still selects the winning group before any equal-priority mode is applied. Lower or equal-priority Additive fields remain suppressed by an influencing Override, as before.

See [ITERATION_016.md](ITERATION_016.md) for a two-planet test that compares the three settings.

## Iteration 018: character facing, animation diagnostics, native rotation ownership

When **Orbit Character Controls** has **Face Movement Direction** enabled on a locally controlled or authoritative Character, it owns the character's **final** orientation. The Gravity component still updates native Character Movement gravity before movement but skips its independent actor rotation. Controls then calculates one gravity-aligned pose on its post-movement tick, combining native Up and a tangent Forward with parallel-transported heading even when the input is idle. This prevents the earlier two-rotation conflict. Characters without Controls owning facing keep the original Gravity actor alignment behavior.

`Disable Competing Native Rotation` defaults to true. Orbit temporarily disables native `Orient Rotation to Movement`, `Use Controller Desired Rotation` and Character `Use Controller Rotation Yaw` and restores the original values when it no longer owns facing or on EndPlay. Third-party Blueprint Set Actor Rotation nodes, root-motion orientation, and other custom systems can still compete and should be disabled or deliberately coordinated.

`Draw Locomotion Debug` shows four vectors (Up white, requested input blue, tangent velocity green, actor forward yellow); `Last Velocity to Facing Angle Degrees`, `Last Capsule Up Error Degrees`, `Last Gravity Relative Ground Speed`, `Last World XY Speed` and `Last Gravity Relative Direction Degrees` are visible instance-only diagnostics. World XY speed varying around a planet while gravity-relative speed remains consistent is **expected** and reveals unsuitable world-XY animation math.

New pure node **Get Orbit Gravity Animation State (Character, Min Ground Speed=3, Min Acceleration=0.1)** returns `FOrbitGravityAnimationState`: Valid, Is Falling, Is Moving On Ground, Has Acceleration, Should Move, Ground Speed, Forward Speed, Right Speed, Movement Direction Degrees, Tangent Acceleration Magnitude, Gravity Up/Down, native Floor Normal, Tangent Velocity, World Tangent Move Direction, Tangent Acceleration and World XY Speed (diagnostic only). Query on the AnimBP's **game-thread Event Blueprint Update Animation**, cache the values into AnimBP variables, and connect those variables to the Blend Space / locomotion graph. This does **not** modify the AnimBP graph automatically. See [ANIMATION_INTEGRATION.md](ANIMATION_INTEGRATION.md).

## Iteration 022: Verified locomotion and gravity-relative jump animation

The user verified that replacing the active `ABP_Unarmed_Orbit` Event Graph's world-XY `GroundSpeed` and `Calculate Direction` data producers with **Get Orbit Gravity Animation State** fixed the semi-strafing gait on a curved ring. Four AnimBP variables were rewired: GroundSpeed, Direction, ShouldMove, IsFalling. The directional `BS_Idle_Walk_Run` was unchanged. See [Animation Integration](ANIMATION_INTEGRATION.md) and [Example ABP workflow](Examples/ABP_UNARMED_ORBIT_VERIFIED_WORKFLOW.md).

The existing `Get Orbit Gravity Animation State` node now additionally exposes `Gravity Relative Vertical Speed` (signed cm/s), `Is Ascending` (+1 cm/s threshold), and `Is Descending` (-1 cm/s threshold). Replace world `Velocity.Z > 100` jump transitions with `Is Falling AND Gravity Relative Vertical Speed > 100`. This follows local Up regardless of the ring/sphere orientation.

New pure Blueprint **Calculate Orbit Gravity Vertical Speed** accepts arbitrary world Velocity and Gravity Up and outputs `bool` valid plus signed cm/s. Invalid zero/NaN Up returns false and zero. Newly added optional native class **Orbit Gravity Anim Instance** (`UOrbitGravityAnimInstance`) caches `OrbitAnimationState` each NativeUpdateAnimation on the game thread; parent-class reparenting does **not** rewrite existing state machine variables or Blend Spaces. Existing pure-node Event Graph integration remains supported and unchanged.


## 1. How Orbit is organized

Orbit currently exposes:

- **Orbit Mover Gravity Component (optional integration):** Ships inside Orbit; enable Epic Mover in the project to use this integration. See [MOVER_INTEGRATION.md](MOVER_INTEGRATION.md).

- **Orbit Character Controls Component:** An opt-in third-person movement input and camera bridge for native Character Movement. Adds two Blueprint-callable input nodes and supports standard Spring Arm cameras. See section 10.
- **Orbit Gravity Source Actor:** A placeable **Actor** defining a gravity field; a separate **Orbit Gravity Source Component** can now embed a field in an ordinary Actor.
- **Orbit Character Gravity Component:** An optional bridge between Orbit gravity and Unreal's native Character Movement component.
- **Orbit Gravity Receiver Component:** An optional way to select which gravity channels affect an Actor.
- **Orbit World Subsystem:** A runtime, world-level authority for gravity source sampling and automatic, experimental rigid-body acceleration.
- **Orbit Blueprint Library:** Convenience nodes for sampling gravity and performing gravity-relative vector calculations.

Most ordinary simulated physics objects need **no Orbit components**. When automatic physics integration is enabled, Orbit discovers eligible physics components and applies field acceleration. The Character Gravity component is for native `ACharacter` movement. The Receiver component changes which gravity sources an Actor is eligible to receive.

### Important terminology: two different channel concepts

| Concept | Available values | Purpose |
| --- | --- | --- |
| **Gravity Channels** (`FOrbitGravityChannels`) | Channel 0, Channel 1, Channel 2 | Lighting-style source/receiver matching. Each source and receiver defaults to **Channel 0 only**. |
| **Query Channel** (`EOrbitGravityChannel`) | General, Physics, Character | Chooses the *kind of integration* whose field flags are evaluated. It does **not** select gravity channel 0/1/2. |

A query with **Physics** ignores sources whose **Affect Physics** setting is false. A **Character** query ignores sources whose **Affect Characters** setting is false. **General** does not apply either integration flag filter. All three query types still need a matching Gravity Channel mask.

---

## 2. Orbit Character Gravity Component

**C++ class:** `UOrbitCharacterGravityComponent`  
**Blueprint Add Component search:** `Orbit Character Gravity` (or search `Orbit`)  
**Intended owner:** An Actor derived from `ACharacter`.

### What it does

During PIE or runtime, the component samples Orbit's **Character** gravity field at the Character actor location and applies the valid local **Down** using native CharacterMovement `SetGravityDirection()`. Unlike earlier versions, it can also adjust `GravityScale` so the Character experiences the source's authored acceleration magnitude. An explicit zero-gravity source sets the scale to zero without trying to normalize an invalid Down vector.

It runs before native Character Movement, respects the Character's optional Receiver component and its Gravity Channels, and restores the prior native direction and scale when leaving the field if **Use Default Gravity Outside Fields** is enabled.

### Setup

1. Add **Orbit Character Gravity** to a Character Blueprint and place an Orbit Gravity Source in the level.
2. Leave the source and Character on Gravity Channel 0 initially. Keep **Affect Characters** enabled on the source.
3. Run PIE and test Strength 980, 490, and 0. When **Match Gravity Magnitude** is enabled, the Character's native gravity scale should follow those changes proportionally when WorldSettings gravity magnitude is 980 cm/s².
4. Move the character outside the source's influence and verify its previous native gravity direction and scale are restored.
5. For planet/corkscrew alignment, enable **Align Actor to Gravity** on the component. This is experimental; disable `Use Controller Rotation Yaw` and any other rotation system that competes with the Actor's rotation.
6. Align the **camera/input separately**. The component does not rotate the Player Controller, Camera Manager, spring arm, or input vectors for you.

### Component properties

| Property | Default | Behavior |
| --- | --- | --- |
| Use Default Gravity Outside Fields | `true` | Restore the gravity direction and scale captured at first Orbit control when no matching source influences the Character. `false` preserves the last Orbit movement state. |
| Match Gravity Magnitude | `true` | Convert sampled field acceleration to Character Movement `GravityScale`, relative to WorldSettings gravity. |
| Max Gravity Scale | `100` | Limits the magnitude conversion when sources are exceptionally strong. |
| Align Actor to Gravity | `false` | Opt-in rotation of the Character Actor/capsule so local +Z matches the sampled local Up; does not rotate camera/controller. |
| Orientation Response Rate | `8` | Exponential rotation interpolation rate. `0` snaps immediately. |
| Return to Default Up Outside Fields | `true` | If alignment was previously used, rotate back toward the captured native Up when no Orbit source influences the Character. |
| Last Field Acceleration | runtime read-only | Last matching source acceleration, or zero outside fields. |
| Inside Orbit Field | runtime read-only | True when a matching gravity field controls the Character at its current location. |

### Limitations

- Rotation is deliberately **off by default**. Standard third-person/first-person controller schemes frequently assume world +Z; enabling actor alignment without adapting the camera/input can make controls disorienting.
- No root-motion, foot IK, animation, complete capsule-floor locomotion, AI path following or production camera integration is claimed as complete. An optional experimental near-floor landing assist and Character Controls camera adapter are described in section 10.
- `GravityScale` conversion requires nonzero native WorldSettings gravity. If the world gravity is zero, Orbit logs a warning and cannot generate nonzero native Character Movement acceleration through a scale alone.
- The adapter does not implement custom network prediction or replicate source updates. Test in standalone single-player first.
- Character Movement receives the instantaneous spatial field. **Spiral Capture** remains a separate velocity-aware effect for physics-simulating bodies, not Characters.
- No automatic integration with arbitrary custom Pawn movement components.

---

## 3. Orbit Gravity Receiver Component

**C++ class:** `UOrbitGravityReceiverComponent`  
**Blueprint Add Component search:** `Orbit Gravity Receiver` (or search `Orbit`)  
**Intended owner:** Any Actor that needs non-default Gravity Channel selection.

### What it does

This component supplies a **three-bit Gravity Channel mask** for its owner. The world subsystem checks the mask when evaluating gravity for the Actor or its eligible simulated primitive components.

The component does **not** tick, calculate forces, or apply gravity itself.

### Setup

1. Open the Actor Blueprint to customize, for example `BP_MagneticCrate`.
2. Add **Orbit Gravity Receiver**.
3. In the component's **Orbit > Channels** section, edit **Gravity Channels**:
   - **Channel 0:** Enabled by default.
   - **Channel 1:** Disabled by default.
   - **Channel 2:** Disabled by default.
4. Select the corresponding **Orbit Gravity Source** Actor and configure its own **Gravity Channels** property.
5. Ensure **at least one channel matches** between the Source and Receiver.
6. For automatically accelerated rigid bodies, ensure the desired mesh/component has **Simulate Physics** enabled and is eligible under the global physics settings.

### Example: Two independent gravity fields

| Actor | Channel 0 | Channel 1 | Channel 2 |
| --- | --- | --- | --- |
| Planet A gravity source | On | Off | Off |
| Planet B gravity source | Off | On | Off |
| Player Receiver | On | Off | Off |
| Special crate Receiver | Off | On | Off |

Here, the Player is eligible for Planet A's gravity field and the crate for Planet B's. Both still have to be **inside the source's actual influence**, and the source's Physics/Character flags must permit the integration in question.

### Channel rules

- **With no Receiver component, an Actor is treated as Channel 0 only.** You do not need to add this component to normal Actors.
- **With a Receiver component, its configured channels replace the default channel mask.** They are not automatically added on top of Channel 0.
- Any overlap is sufficient. A source with channels 0 and 2 affects a receiver with channel 2 alone.
- All three channels off results in **no matching Orbit sources**.
- Channels have no relationship to collision channels, object types, or visibility/lighting channels.
- The same Receiver mask is used for all eligible primitive components of its owning Actor. Orbit v0.2.5 does not provide different Receiver channel masks for individual components within the same Actor.
- For rigid bodies with no compatible Orbit source, Orbit leaves them using native gravity instead of claiming them.

### Blueprint-visible property

**Gravity Channels** (`FOrbitGravityChannels`) is `BlueprintReadWrite`. You can get or set the struct in Blueprint. To change its three booleans, use **Make Orbit Gravity Channels** or **Set Members in Orbit Gravity Channels** and assign the result back to the Receiver's **Gravity Channels** property.

### When you should not add it

Do not add a Receiver merely to make a physics cube fall toward an Orbit source. The default Channel 0 behavior already handles that if the cube is otherwise eligible for automatic physics integration.

---

## 4. Complete list of Orbit Blueprint functions

The base Orbit plugin declares **24 Orbit-specific Blueprint-callable or Blueprint-pure functions**, including input adapters and runtime helpers, across its runtime classes. The names below are the names to search for in the Blueprint node picker. Unreal also generates normal property Get/Set nodes for exposed properties and struct Make/Break nodes, but those are not additional Orbit-authored functions.

### 4A. Orbit Blueprint Library (six foundational gravity/orientation nodes; other library nodes are documented in later sections)

These can be called from ordinary Blueprint graphs; they do not require a stored reference to the World Subsystem.

#### 1. Sample Orbit Gravity

**Category:** `Orbit | Gravity`  
**Class:** `UOrbitBlueprintLibrary`  
**Inputs:** `World Context Object` (normally supplied implicitly), `World Position` (`Vector`), `Channel` (`General` / `Physics` / `Character`, default `General`)  
**Output:** `FOrbitGravitySample`

Samples the **combined gravity field** at a given world-space position. Uses **Gravity Channel 0 only**; it does **not** infer an Actor's Receiver settings.

**Use when:** You want gravity for a location and are using default Channel 0 sources, for example to align a procedural effect to local Down.

**Example:** `Get Actor Location` -> `Sample Orbit Gravity` -> `Break Orbit Gravity Sample` -> `Down`.

**Caution:** Select `Character` or `Physics` in the *query* input only to filter by the source's Affect Characters / Affect Physics settings. This input is not the lighting-style gravity channel number.

#### 2. Sample Orbit Gravity for Actor

**Category:** `Orbit | Gravity`  
**Class:** `UOrbitBlueprintLibrary`  
**Inputs:** `World Context Object` (usually implicit), `Actor` (`Actor` reference), `World Position` (`Vector`), `Channel` (default `General`)  
**Output:** `FOrbitGravitySample`

Like **Sample Orbit Gravity**, but honors the specified Actor's **Orbit Gravity Receiver** mask, or Channel 0 when there is no Receiver.

The **Actor** determines channel eligibility. The **World Position** determines where gravity is sampled. They can be different locations.

**Use when:** Calculating gravity for a player, enemy, or physics Actor whose channels may differ from Channel 0.

**Example:** `Self` -> `Actor`, `Get Actor Location` -> `World Position`, `Character` -> `Channel` -> `Sample Orbit Gravity for Actor`.

If `Actor` is invalid or the world cannot be resolved, the function returns an empty sample.

#### 3. Project Onto Gravity Plane

**Category:** `Orbit | Gravity`  
**Class:** `UOrbitBlueprintLibrary`  
**Inputs:** `Vector`, `Up` (`Vector`)  
**Output:** Projected `Vector`

Removes the component of an input vector along local Up, leaving the portion tangent to the gravity-relative horizontal plane.

**Use when:** Converting a camera-forward vector into a walking direction on a planet or on a rotated floor.

**Example:** `Camera Forward Vector` plus `Sample.Up` -> `Project Onto Gravity Plane` -> normalize -> desired movement direction.

If `Up` is nearly zero, the function returns the original Vector unchanged. This node does not perform a world trace, check for walkability, or normalize the resulting vector.

#### 4. Gravity Up From Acceleration

**Category:** `Orbit | Gravity`  
**Class:** `UOrbitBlueprintLibrary`  
**Input:** `Acceleration` (`Vector`)  
**Outputs:** `Return Value` (`Boolean`), `Up` (`Vector`)

Returns **true** and a normalized Up vector opposite the acceleration when acceleration is finite and nonzero. Returns **false** and world `+Z` as a fallback when acceleration is nearly zero or invalid.

**Use when:** You have an acceleration vector from Orbit or another system and need a safe local Up direction without normalizing zero.

**Example:** Feed `Sample.Acceleration` into this node, branch on the Boolean, then use the returned Up for rotation or plane projection only if appropriate for your gameplay.

#### 5. Get Gravity Tangent Basis

**Category:** `Orbit | Gravity | Orientation`  
**Inputs:** `Up` (`Vector`), `Preferred Forward` (`Vector`)  
**Outputs:** `Forward` (`Vector`), `Right` (`Vector`), success (`Boolean`)

Generates a gravity-relative, perpendicular Forward/Right pair. Project the preferred heading onto the local tangent plane; if it is parallel to Up, use a safe fallback heading. Useful for gravity-relative directional input, traces and spawning objects.

#### 6. Make Gravity Aligned Rotation

**Category:** `Orbit | Gravity | Orientation`  
**Inputs:** `Current Rotation` (`Rotator`), `Up` (`Vector`)  
**Outputs:** `Aligned Rotation` (`Rotator`), success (`Boolean`)

Builds a rotation whose local +Z follows Up while retaining as much of the current heading as possible. This does not rotate any actor automatically. It is useful for manually orienting pawns, cameras, meshes, projectiles, or equipment.

### 4B. Orbit Gravity Source Actor (1 node)

#### 7. Sample Source

**Category:** `Orbit | Field`  
**Class:** `AOrbitGravitySource`  
**Inputs:** Target (Orbit Gravity Source reference), `World Location` (`Vector`)  
**Output:** `FOrbitGravitySample`

Samples **this Gravity Source alone**, not the combined World Subsystem field. It respects the source's `Enabled` setting and influence boundary, but it does not consider overlapping sources or their priorities and does not apply world-gravity fallback.

**Use when:** Debugging a particular gravity source or comparing a single source's field with the combined world result.

**Important current behavior:** Internally this node uses the evaluator's default **Gravity Channel 0** mask and `General` query. Therefore a source configured **only** for Channel 1 or 2 can return no influence through this convenience function. To sample sources on those channels, use the World Subsystem's **Sample Gravity With Channels** node instead. This is a current API limitation, not an intended channel rule for physics.

### 4C. Orbit Gravity Receiver Component (1 node)

#### 8. Get Gravity Channel Mask

**Category:** `Orbit | Channels`  
**Class:** `UOrbitGravityReceiverComponent`  
**Input:** Target (Orbit Gravity Receiver component reference)  
**Output:** `Integer`

Returns the receiver's three-bit channel mask:

| Selected channels | Returned mask |
| --- | ---: |
| None | `0` |
| Channel 0 | `1` |
| Channel 1 | `2` |
| Channel 2 | `4` |
| Channels 0 + 1 | `3` |
| Channels 0 + 2 | `5` |
| Channels 1 + 2 | `6` |
| All channels | `7` |

**Use when:** Debugging the effective channel configuration or building an in-game display of the Receiver's selection.

This node **reads** the mask. It does not alter source or receiver channels.

### 4D. Orbit World Subsystem (5 nodes)

**How to access:** In a gameplay Blueprint, use Unreal's **Get World Subsystem** node and choose **Orbit World Subsystem** as the subsystem class. Call the functions below on the returned reference. The subsystem exists in **game worlds**, including PIE. It is not an editor-only world query service.

#### 9. Sample Gravity

**Category:** `Orbit | Gravity`  
**Inputs:** `World Location` (`Vector`), `Channel` (default `General`)  
**Output:** `FOrbitGravitySample`

Samples the combined field using **Gravity Channel 0** as the receiver mask. This is the World Subsystem counterpart of **Sample Orbit Gravity**.

**Use when:** You already have the subsystem reference and need standard-channel gravity samples.

#### 10. Sample Gravity With Channels

**Category:** `Orbit | Channels`  
**Inputs:** `World Location` (`Vector`), `Receiver Channels` (`FOrbitGravityChannels`), `Channel` (default `General`)  
**Output:** `FOrbitGravitySample`

Samples the combined field using **the three channel bits provided explicitly**, without requiring an Actor or Receiver component.

**Use when:** You need to query Channel 1/2 from a position for procedural logic, debugging, or a system that is not represented by an Actor.

**Example:** `Make Orbit Gravity Channels` with only Channel 2 enabled -> `Sample Gravity With Channels` -> `Break Orbit Gravity Sample`.

#### 11. Sample Gravity For Actor

**Category:** `Orbit | Channels`  
**Inputs:** `Actor` reference, `World Location` (`Vector`), `Channel` (default `General`)  
**Output:** `FOrbitGravitySample`

Uses the given Actor's Orbit Gravity Receiver (if present) to choose the mask. Without a Receiver, defaults to Channel 0. This is the subsystem implementation called by the Blueprint library's **Sample Orbit Gravity for Actor** convenience node.

**Use when:** You have a subsystem reference and want the Actor's real eligibility rules applied.

#### 12. Get Gravity Acceleration

**Category:** `Orbit | Gravity`  
**Inputs:** `World Location` (`Vector`), `Channel` (default `General`)  
**Output:** `Acceleration` (`Vector`)

Returns just the acceleration vector from **Sample Gravity**. Useful when the full sample struct isn't needed.

**Important:** This node uses default **Gravity Channel 0**, not an Actor's Receiver mask. If Channel 1/2 matters, call **Sample Gravity With Channels** or **Sample Gravity For Actor** and read the `Acceleration` output.

#### 13. Get Active Source Count

**Category:** `Orbit | Gravity`  
**Inputs:** None beyond the subsystem Target  
**Output:** `Integer`

Returns the number of currently active (enabled, registered) gravity field definitions in the runtime World Subsystem.

**Use when:** Building a debug HUD, checking runtime registration, or identifying why no sources seem to be participating.

The count is **not** the number of sources influencing a particular world location or matching a particular receiver channel.

---

## 5. Understanding the gravity sample struct

All `Sample...` nodes return `FOrbitGravitySample`. Right-click its result pin and choose **Split Struct Pin**, or use **Break Orbit Gravity Sample**.

| Field | Type | Meaning |
| --- | --- | --- |
| **Acceleration** | Vector | World-space acceleration in `cm/s²`, including world gravity fallback when enabled and no Orbit field is active. |
| **Down** | Vector | Normalized acceleration direction when valid. Zero when no valid direction exists. |
| **Up** | Vector | Opposite of Down when valid; defaults to world `+Z` when invalid. |
| **Magnitude** | Double | Length of the acceleration vector in `cm/s²`. |
| **Has Influence** | Boolean | `true` when an Orbit source was evaluated as influencing the sample, even if the resulting acceleration is zero. |
| **Has Valid Direction** | Boolean | `true` when a nonzero acceleration gives a meaningful local Up/Down. |
| **Dominant Source ID** | Integer | Runtime diagnostic ID of the dominant contributing source, if known. Not a stable serialized or replicated Actor identifier. |

### Has Influence vs Has Valid Direction

These should not be treated as the same flag.

- **Has Influence = false**, **Has Valid Direction = true** can occur when **Use World Gravity Outside Fields** is enabled and the subsystem returns native world gravity as fallback.
- **Has Influence = true**, **Has Valid Direction = false** can occur in an explicitly zero-strength field, at a singularity, or when combined accelerations cancel.
- **Has Influence = false**, **Has Valid Direction = false** can occur where no source is active and world fallback is disabled.

If orientation matters, check **Has Valid Direction** before using `Up` or `Down` as a true gravity-relative orientation. The default `Up = (0,0,1)` is a safe fallback value, not proof that world Z is physically up at that position.

### Important note about Spiral Capture

Blueprint samples, including **Sample Orbit Gravity**, report the **instantaneous spatial acceleration field**. The special **Spiral Capture** calculation additionally considers the body's current velocity and is used by Orbit's automatic rigid-body simulation path. A Blueprint spatial sample does **not** reproduce the velocity-dependent capture correction or predict a moving body's trajectory.

---

## 6. Practical Blueprint recipes

### Recipe A: Make a movement input vector tangent to local gravity

**Goal:** Get a forward direction suitable for movement along a curved surface.

```text
Get Actor Location
  -> Sample Orbit Gravity for Actor
      Actor = Self
      World Position = Actor Location
      Channel = Character
  -> Break Orbit Gravity Sample
      Has Valid Direction -> Branch
      Up -> Project Onto Gravity Plane (Up)

Camera/Get Control Rotation -> Get Forward Vector
  -> Project Onto Gravity Plane (Vector)
  -> Get Safe Normal
  -> Use as gravity-tangent movement direction
```

This is a **calculation example**, not a complete character locomotion solution. You'll still need the correct controller/camera alignment, collision handling, and movement implementation.

### Recipe B: Two crates responding to two different fields

1. Source A: Channel 0 only.
2. Source B: Channel 1 only.
3. Crate A: physics enabled, **no Receiver** (implicitly Channel 0).
4. Crate B: physics enabled, **Orbit Gravity Receiver** with Channel 0 off and Channel 1 on.
5. Place both inside both fields and play.

Both crates can still collide with the world. Gravity Channels determine **field eligibility**, not collision behavior.

### Recipe C: Query gravity for an Actor with custom channels

```text
Event Tick (or another event)
  -> Get Actor Location
  -> Sample Orbit Gravity for Actor
      Actor = Self
      World Position = Get Actor Location
      Channel = General
  -> Break Orbit Gravity Sample
  -> Read Magnitude, Down, Has Influence
```

Prefer event-driven or less frequent polling when you do not need a sample every frame. The world subsystem currently rebuilds source definitions every tick, including spline samples, so dense sampling and dynamic spline authoring should be performance-tested.

### Recipe D: Change Gravity Source parameters at runtime

Store or obtain a reference to an **Orbit Gravity Source** and use ordinary Blueprint **Set** property nodes, such as `Set Strength`, `Set Direction`, `Set Max Distance`, or `Set Gravity Channels`.

The World Subsystem rebuilds its active field snapshot every game tick, so changing supported source properties in PIE is reflected in subsequent gravity queries and physics updates. This does **not** imply those changes automatically replicate across the network.

### Recipe E: Debug one gravity source against the combined field

1. Call **Sample Source** on the particular Orbit Gravity Source.
2. Separately call **Sample Orbit Gravity** (for default Channel 0) or **Sample Orbit Gravity for Actor** (for an Actor with custom channels) at the same position.
3. Compare `Acceleration`, `Has Influence`, and `Dominant Source ID`.
4. If they differ, inspect other overlapping sources, channel selection, source priority, and additive/override combination.

If the source is not on Channel 0, use **Sample Gravity With Channels** instead of Sample Source for channel-correct diagnostics.

---

## 7. Required project settings for automatic rigid-body gravity

Open **Project Settings > Plugins > Orbit**.

| Setting | Default | What it controls |
| --- | --- | --- |
| Automatically Apply to Physics Bodies | On | Discovers and applies Orbit gravity to eligible simulated primitive components. |
| Enable Physics Integration | On | Master switch for Orbit's experimental force application. |
| Include Initially Gravity Disabled Bodies | Off | Opts in physics components whose native Enable Gravity was already disabled. |
| Physics Discovery Interval | `0.20 s` | Frequency of scanning for new simulated components. `0` requests a scan every tick. |
| Use World Gravity Outside Fields | On | Provides the world's normal Z gravity when no matching Orbit field influences a sample. |
| Stabilize Resting Rigid Bodies | On | Experimental support-based sleep handling for ordinary static-mesh bodies. |
| Rest Time | `0.65 s` | Required stable contact duration before supported slow bodies are put to sleep. |
| Rest Linear Speed | `5 cm/s` | Maximum linear velocity for rest stabilization. |
| Rest Angular Speed | `0.08 rad/s` | Maximum angular velocity for rest stabilization. |
| Damp Supported Rigid Body Motion | On | Contact-only, friction-like damping for rolling/sleep-resistant simulated props. |
| Supported Tangential Damping | `1.5 /s` | Opposes sideways-on-surface movement but leaves gravity-normal motion alone. |
| Supported Angular Damping | `2.0 /s` | Opposes spin for gently supported bodies. |
| Rest Support Probe Distance | `3 cm` | Short sweep using the component's collision geometry, not its bounding sphere. |
| Rest Max Normal Contact Speed | `75 cm/s` | Disables contact damping for fast falling/bouncing bodies. |
| Rest Support Contact Grace Seconds | `0.08 s` | Retain only previously verified contact across a very brief sweep miss, with clearance/launch constraints. |
| Draw Physics Rest Contact Debug | Off | Cyan verified / blue temporary grace / amber unsupported / red support normal mismatch; displays radial alignment and source-to-surface-center offset. |

**Resting on planets:** If a cube slowly spins like a top or a small sphere rolls indefinitely, start with the new **Damp Supported Rigid Body Motion** setting (default On). Orbit applies sideways drag and angular deceleration only to gently supported static-mesh physics bodies; it does not damp flight. Bodies must have functional simple collision to be detected by the support sweep. If a sphere still rolls or settles at a particular latitude, enable **Draw Physics Rest Contact Debug**: inspect Point **radial** alignment and **center offset**, not just damping. A correctly centered Point/Inward field has no preferred equator. The contact probe may briefly miss as Chaos resolves physics; blue indicates a bounded remembered contact. Do not merely raise damping to conceal a center mismatch or competing force. See [Iteration 024](ITERATION_024.md).

For a basic test, keep the first two enabled and leave the native physics cube's **Simulate Physics** and **Enable Gravity** checked. Orbit disables native gravity *while it owns* that body, to avoid applying world gravity twice, and restores the original state when management ends.

### Optional physics-component tags

These belong on the **simulated Primitive Component's Component Tags**, not the owning Actor's Actor Tags.

| Tag | Effect |
| --- | --- |
| `Orbit.Ignore` | Excludes that component from automatic Orbit physics gravity. |
| `Orbit.Force` | Allows Orbit to manage a component whose native Enable Gravity started disabled. |
| `Orbit.NoRest` | Prevents supported-body damping AND automatic rest sleeping for that component. Gravity is still applied. |

These are not Blueprint library nodes, and none of them are required for ordinary Channel 0 cubes.

---

## 8. Current limits relevant to Blueprint use

- **Orbit has six addable Actor Components:** Character Gravity, Gravity Receiver, Character Controls, Gravity Orientation, Gravity Source and Orbit Mover Gravity (functional with Epic Mover enabled). The dedicated Gravity Source Actor is a separate placeable Actor; Orbit World Subsystem is created by the game world.
- **Character integration supports direction, magnitude, optional Actor/capsule alignment, plus separate opt-in tangent-plane input, facing and gravity-relative Spring Arm camera handling.** Advanced locomotion, animation, and networking still require further work.
- **Automatic rigid-body gravity is experimental.** It uses game-thread `AddForce(..., bAccelChange = true)`, not Chaos simulation callbacks or substep forces.
- **No gravity-aware NavMesh, EQS, path following, vehicle, Niagara, or projectile integrations are included** in this version.
- **No multiplayer gravity source replication/prediction** is implemented.
- **World Subsystem sampling works in game worlds.** If you need editor-only preview data, use the Gravity Source visualization rather than relying on a PIE subsystem existing in the editor world.
- **Sample Source is a single-source Channel 0 convenience call.** It is not a substitute for the combined Actor/channel-aware world query.
- **Spiral Capture differs from the field preview.** Visualization and Blueprint spatial samples do not account for a body's velocity and collision response.

## 8A. Optional Orbit Mover component and its nodes

**Plugin:** `Orbit` (internal `OrbitMover` module). Epic's Experimental Mover is optional; Orbit has no build-time Mover dependency.

**Component:** `UOrbitMoverGravityComponent`, Add Component search **Orbit Mover Gravity**. Intended for Actors/Pawns with a **synchronous kinematic `UMoverComponent`**. The bridge applies Orbit's gravity acceleration and local Up using Mover's reflected public functions during a pre-physics game-thread tick, with a tick prerequisite where applicable. Unlike the prior pre-simulation callback, this timing is not guaranteed for independent Mover simulations. It is **not compatible with Chaos Mover or asynchronous backends**. It respects the same Gravity Receiver channels as the Character Gravity adapter. For full setup and caveats see [`MOVER_INTEGRATION.md`](MOVER_INTEGRATION.md).

**Orbit-authored Blueprint nodes, always defined in the Orbit plugin (functional on a compatible Mover actor):**

1. **Set Orbit Mover Gravity Enabled** (`Orbit | Mover`): Input `bEnabled` Boolean. Enables/disables integration; disabling relinquishes its overrides to native Mover.
2. **Refresh Orbit Mover Gravity** (`Orbit | Mover`): Re-samples and applies Orbit gravity manually for the owner. Not normally needed per tick.

**New runtime diagnostic properties**: Inside Orbit Field, Last Orbit Acceleration, Last Orbit Up, Last Sample Position, Samples Applied, Backend Supported. These are ordinary property Get nodes, not additional authored Blueprint functions. Orbit exposes **32** authored functions across its internal modules: 30 core and 2 Mover-specific. Enabling Epic Mover is required for those latter functions to affect movement.

---

## 9. Quick node index (v0.2.5)

These are **authored Blueprint-callable/pure functions**, not automatically generated property Get/Set or Make/Break struct nodes. The two Mover nodes are part of Orbit, but require a compatible Epic Mover component to have any effect.

| # | Blueprint node | Owner | Category |
| ---: | --- | --- | --- |
| 1 | Sample Orbit Gravity | Orbit Blueprint Library | Orbit / Gravity |
| 2 | Sample Orbit Gravity for Actor | Orbit Blueprint Library | Orbit / Gravity |
| 3 | Project Onto Gravity Plane | Orbit Blueprint Library | Orbit / Gravity |
| 4 | Gravity Up From Acceleration | Orbit Blueprint Library | Orbit / Gravity |
| 5 | Get Gravity Tangent Basis | Orbit Blueprint Library | Orbit / Gravity / Orientation |
| 6 | Make Gravity Aligned Rotation | Orbit Blueprint Library | Orbit / Gravity / Orientation |
| 7 | Sample Source | Orbit Gravity Source | Orbit / Field |
| 8 | Get Gravity Channel Mask | Orbit Gravity Receiver Component | Orbit / Channels |
| 9 | Sample Gravity | Orbit World Subsystem | Orbit / Gravity |
| 10 | Sample Gravity With Channels | Orbit World Subsystem | Orbit / Channels |
| 11 | Sample Gravity For Actor | Orbit World Subsystem | Orbit / Channels |
| 12 | Get Gravity Acceleration | Orbit World Subsystem | Orbit / Gravity |
| 13 | Get Active Source Count | Orbit World Subsystem | Orbit / Gravity |
| 14 | Set Gravity Strength | Orbit Gravity Source Actor | Orbit / Field / Runtime |
| 15 | Refresh Active Sources | Orbit World Subsystem | Orbit / Gravity / Runtime |
| 16 | Apply Gravity Relative Move | Orbit Character Controls Component | Orbit / Controls / Input |
| 17 | Apply Gravity Relative Look | Orbit Character Controls Component | Orbit / Controls / Input |
| 18 | Make Gravity Frame | Orbit Blueprint Library | Orbit / Gravity / Transforms |
| 19 | Make Gravity Frame For Actor | Orbit Blueprint Library | Orbit / Gravity / Transforms |
| 20 | Gravity Local to World Transform | Orbit Blueprint Library | Orbit / Gravity / Transforms |
| 21 | Gravity World to Local Transform | Orbit Blueprint Library | Orbit / Gravity / Transforms |
| 22 | Apply Orbit Gravity Relative Move | Orbit Blueprint Library | Orbit / Controls / Input |
| 23 | Apply Orbit Gravity Relative Look | Orbit Blueprint Library | Orbit / Controls / Input |
| 24 | Get Gravity Relative Locomotion | Orbit Blueprint Library | Orbit / Animation |
| 25 | Get Orbit Gravity Up for Actor | Orbit Blueprint Library | Orbit / Gravity / Orientation |
| 26 | Make Orbit Axis Aligned Rotation | Orbit Blueprint Library | Orbit / Gravity / Orientation |
| 27 | Orient Actor to Orbit Gravity | Orbit Blueprint Library | Orbit / Gravity / Orientation |
| 28 | Set Gravity Strength | Orbit Gravity Source Component | Orbit / Field / Runtime |
| 29 | Sample Source Component | Orbit Gravity Source Component | Orbit / Field |
| 30 | Orient Now | Orbit Gravity Orientation Component | Orbit / Orientation |
| 31 | Set Orbit Mover Gravity Enabled | Orbit Mover Gravity Component (optional) | Orbit / Mover |
| 32 | Refresh Orbit Mover Gravity | Orbit Mover Gravity Component (optional) | Orbit / Mover |

**Component Target vs Actor Self:** Nodes 16/17 must target an **Orbit Character Controls Component reference**. The static convenience nodes 22/23 accept a **Character** reference and default that pin to `Self` inside a Character Blueprint. They require an Orbit Character Controls component installed on that Character and do nothing without it. Use one input path, not both. 

*Documentation updated against Orbit v0.2.7 source, not proof of UE compilation or in-editor behavior.*

---

## 10. Character input/camera and landing reliability (introduced in v0.2.1, expanded in v0.2.2)

### Orbit Character Controls Component

**C++:** `UOrbitCharacterControlsComponent`  
**Add Component picker:** `Orbit Character Controls`  
**Owner:** `ACharacter` with native `UCharacterMovementComponent`. Add alongside `Orbit Character Gravity` for planet traversal.

This is one of the **five base Orbit components**, alongside Character Gravity, Gravity Receiver, Gravity Orientation and Gravity Source. It does NOT automatically consume Enhanced Input bindings: the user explicitly forwards input through its two nodes.

| Property | Default | Effect |
| --- | --- | --- |
| Orient Camera to Gravity | true | Use a transported gravity-local heading and pitch |
| Prefer Spring Arm Camera | true | Drive the first Spring Arm in absolute world rotation using quaternions; temporarily disable Pawn Control Rotation; restore prior flags on EndPlay or disable |
| Yaw Degrees Per Unit | 1 | Local-Up yaw input sensitivity |
| Pitch Degrees Per Unit | 1 | Gravity-local pitch input sensitivity |
| Min Pitch / Max Pitch | -80 / +80 | Clamp pitch relative to **local gravity Up**, not world Z |
| Face Movement Direction | true | Turns Actor/capsule toward an appropriate tangent-plane movement heading (not camera yaw) |
| Face Velocity When Moving | true | Above threshold, face actual tangent-plane velocity rather than latest raw input. Useful for animation consistency. |
| Velocity Facing Threshold | 30 cm/s | Below this speed use input-facing fallback. |
| Facing Response Rate | 12 | Smooth quaternion response; 0 snaps |
| Facing Input Threshold | 0.1 | Ignore tiny input; keep heading without movement |
| Disable Competing Native Rotation | true | Temporarily disable native movement/controller yaw rotation while Controls owns facing; restore original values when released |
| Draw Locomotion Debug | false | Draw requested input (blue), actual velocity (green), desired Forward (cyan), actual Actor Forward (yellow) and gravity Up (white); no debug draw cost while disabled |
| Debug Vector Length | 120 cm | Debug arrow length |
| Last Velocity to Facing Angle Degrees | runtime | Mismatch between actual gravity-tangent velocity and Actor forward |
| Last Capsule Up Error Degrees | runtime | Error between Actor local +Z and Movement gravity Up |
| Last Gravity Relative Ground Speed | runtime | Correct tangent-plane locomotion speed |
| Last World XY Speed | runtime | Debug comparison for world-horizontal graph bugs |
| Last Gravity Relative Direction Degrees | runtime | Signed gravity-local movement direction |
| Last Facing Direction | runtime | Read-only world-space actual Actor Forward following a successful rotation |
| Last Gravity Up | runtime | Read-only sampled movement Up |
| Last Move Forward / Right | runtime | Read-only tangent input basis for PIE debugging |

#### Iteration 012 Orbit Blueprint nodes

| # | Blueprint node | Target | Input | Output / Behavior |
| ---: | --- | --- | --- | --- |
| 14 | **Set Gravity Strength** | Orbit Gravity Source Actor | New Strength (double cm/s²) | Immediately updates runtime actor's Strength and its game-world source snapshot; rejects non-finite or negative inputs |
| 15 | **Refresh Active Sources** | Orbit World Subsystem | none | Rebuilds the registered runtime source snapshot immediately. Useful after several direct property assignments |
| 16 | **Apply Gravity Relative Move** | Orbit Character Controls Component | Move Axis (Vector2D), X right / Y forward | Applies forward/right movement **tangent to current gravity Up** through native `AddMovementInput` |
| 17 | **Apply Gravity Relative Look** | Orbit Character Controls Component | Look Axis (Vector2D), X yaw / Y pitch | Maintains gravity-local camera heading and pitch. Prefers quaternion-driven Spring Arm; uses Controller Rotation only as fallback |

The base `Orbit` plugin now authors **25 Blueprint-callable/pure functions**, plus the two built-in `OrbitMover` module functions documented in [`MOVER_INTEGRATION.md`](MOVER_INTEGRATION.md). Normal UObject property setters, Blueprint Make/Break struct nodes, and engine-native nodes are not included in these counts.

**Iteration 018 additional Blueprint node:** `Get Orbit Gravity Animation State`, declared by `UOrbitBlueprintLibrary`, category `Orbit | Animation`, pure, input `Character`, `Min Ground Speed` (3), `Min Acceleration` (0.1), output `FOrbitGravityAnimationState` with 17 readable properties. This was the 25th base function; Iteration 022 adds **Calculate Orbit Gravity Vertical Speed** as another pure Blueprint node (26 base library functions). It is also documented in [ANIMATION_INTEGRATION.md](ANIMATION_INTEGRATION.md).

### Additional Orbit Character Gravity component settings

| Property | Default | Effect |
| --- | --- | --- |
| Enable Near Floor Landing Assist | true | Allows *native* near-floor landing after a confirmed walkable capsule-sweep hit and valid landing spot |
| Near Floor Landing Distance | 10 cm | Maximum sweep-validated gap, hard bounded to 1–20 cm |
| Near Floor Max Approach Speed | 600 cm/s (new default) | Caps near-floor assistance; initial drops from several hundred centimeters can exceed the original 100 cm/s limit. Native walkable-floor validation remains mandatory. |
| Last Applied Gravity Scale | runtime read-only | Actual value last applied to native Character Movement |
| Last Dominant Source ID | runtime read-only | Diagnostic ID from the active field sample, not a stable serialized Actor identifier |
| Last Walkable Floor Distance | runtime read-only | Validated sweep distance in cm or -1 when no suitable sweep hit |
| Landing Assist Triggered | runtime read-only | Whether the optional near-floor assist performed the transition to native Walking that tick |
| Landing Assist Count | runtime read-only | Lifetime count of successful native assist transitions, easier to observe in PIE |

A zero-strength field does not trigger near-floor assist. The landing method calls native `K2_FindFloor` and uses its gravity-relative walkable sweep result. The previous shortened-capsule sweep hit was incorrectly revalidated by `IsValidLandingSpot` (a path meant for full collision landing hits). It does not synthesize hits or teleport. Native Character Movement still validates its own floor when entering Walking. The outcome still requires PIE validation.

| Near Floor Walkable | runtime read-only | True when native FindFloor found a nonpenetrating, walkable sweep hit |
| Last Native Floor Distance | runtime read-only | FloorDist reported by native FindFloor, or -1 when no blocking floor sweep |


### Exact Third Person template hookup

1. Add `Orbit Character Gravity` and `Orbit Character Controls` to `BP_ThirdPersonCharacter`. Enable **Align Actor to Gravity** on the former; keep the latter's **Orient Camera to Gravity** enabled. Keep a normal Spring Arm/Camera in the Components panel.
2. Under Character Movement, turn **Orient Rotation to Movement** and **Use Controller Desired Rotation** off for this test. Under Self, disable **Use Controller Rotation Yaw/Pitch/Roll**.
3. Open Event Graph. In **IA_Move (Triggered)**, take its `Action Value` through a 2D Axis Value extraction and connect to **Apply Gravity Relative Move (Move Axis)** targeting Orbit Character Controls. Remove/disconnect the template's two original `AddMovementInput` calls.
4. In **IA_Look (Triggered)**, connect its 2D Axis Value to **Apply Gravity Relative Look (Look Axis)** on the same component. Remove/disconnect the template's original `AddControllerYawInput` and `AddControllerPitchInput` calls. Leave Jump logic unchanged.
5. Compile, save, run PIE. A source/planet at `(0,0,2000)` with `PlayerStart` at `(900,0,2000)` tests the equator; `(0,0,2800)` tests low-g landing on a 10x basic Sphere.
6. For live Strength testing: **F8, select the PIE-world source**, not its editor-world counterpart. Watch the PIE Character's `Last Field Acceleration` and `Last Applied Gravity Scale`. To eliminate selection ambiguity, call the source's **Set Gravity Strength** node from the Level Blueprint during PIE.

See [`ITERATION_012.md`](ITERATION_012.md) for acceptance tests and honest limitations.

---

## 11. Iteration 013: live Details edits, body facing, transform tools

This section describes Orbit changes in v0.2.2. See [`ITERATION_013.md`](ITERATION_013.md) for the exact planet test and a comparison of the native floor-detection path.

### Runtime Details editing

The placeable Gravity Source has **Mirror Editor Changes to PIE** (default true). While PIE is running, editing an editable source property on its editor-world level Actor attempts to match its PIE copy by actor GUID or exact actor name and matching level package and updates that PIE source. The `Last Editor PIE Mirror Count` property reveals how many PIE copies were updated (normally 1). Editing the PIE-world source directly continues to work. The new behavior is editor-only and not replicated. Editing an editor-world source still changes its editable unsimulated level copy, so undo it if you do not want to retain the authored change. The Blueprint Strength setter is retained.

### Landing diagnostics

In addition to **Last Walkable Floor Distance** and **Landing Assist Count**, the Character Gravity component exposes **Near Floor Walkable** and **Last Native Floor Distance**. When Falling near the planet, inspect these properties on the **PIE-world character instance**. `Near Floor Walkable = false` indicates native FindFloor didn't find the required walkable sweep, even if the render mesh appears near the surface. `Last Native Floor Distance > Near Floor Landing Distance` indicates a gap larger than the configured assist range.

### Facing a direction on curved surfaces

The Orbit Character Controls component now defaults **Face Movement Direction = true**. As of Iteration 018, it is the sole final character-rotation owner when locally controlling facing. It uses the most recent nontrivial movement input direction projected into the gravity tangent plane, interpolating the Actor/capsule rotation with a quaternion. **Facing Response Rate = 12**, **Facing Input Threshold = 0.1**. The camera remains independently oriented through its Spring Arm. It does not modify third-person animation selections or control custom character controllers. Its `Disable Competing Native Rotation` option (default true) now temporarily handles native Character Movement and Controller Yaw settings, restoring them when control ends. Still turn off custom Blueprint, root-motion or third-party actor rotation. If a custom locomotion system controls rotation, turn Orbit facing off.

### Gravity frame construction and transform conversions

**Make Gravity Frame**: Inputs `World Origin` (Vector), `Gravity Up` (Vector), `Preferred Forward` (Vector). Outputs `Return Value` (bool) and `Gravity Frame` (Transform). Gravity Frame is world-space and unit-scaled, with local +Z = Up and +X tangent to Preferred Forward. Invalid zero/NaN Up returns false and an identity transform.

**Make Gravity Frame For Actor**: Inputs `Actor` (defaults to Self), `World Position`, `Preferred Forward`. Outputs bool and Gravity Frame. Samples the Character integration channel with the Actor's Receiver gravity channels. Returns false if no valid direction is available; explicitly construct a frame with a cached Up during zero gravity.

**Gravity Local to World Transform**: Inputs `Relative Transform` and `Gravity Frame`, returns World Transform. Composition: `RelativeTransform * GravityFrame`.

**Gravity World to Local Transform**: Inputs `World Transform` and `Gravity Frame`, returns Relative Transform. Uses `WorldTransform.GetRelativeTransform(GravityFrame)`. For positions/vectors/directions alone, use Unreal's ordinary `Transform Location`, `Inverse Transform Location`, `Transform Direction`, `Inverse Transform Direction` nodes with the same Gravity Frame.

**Apply Orbit Gravity Relative Move** and **Apply Orbit Gravity Relative Look**: Actor-friendly convenience Blueprint Library nodes. Inputs `Character` (`Self` in a Character Blueprint) and `Move Axis` / `Look Axis` (Vector2D). Find the Character's existing Orbit Character Controls component and call its instance method. No additional input interception, component creation or target cast occurs.

#### Example: Spawn an effect at an offset local to gravity

1. `Make Gravity Frame For Actor(Self, GetActorLocation, CameraForward)` → Frame and success.
2. Make a Transform with Location `(100, 0, 50)` and desired local rotation, Scale `(1,1,1)`.
3. `Gravity Local to World Transform(RelativeTransform, Frame)` → final world-space Transform for `Spawn Actor` or `Spawn System At Location`.
4. To convert an existing world object back into the same gravity-relative frame, use `Gravity World to Local Transform`.


## 12. Iteration 015: gravity-relative animation and first-spawn landing

### Get Gravity Relative Locomotion (new Blueprint Library node)

**Category:** `Orbit | Animation`  
**C++:** `UOrbitBlueprintLibrary::GetGravityRelativeLocomotion`  
**Input:** `Character` (reference to `ACharacter`, usually the result of `Try Get Pawn Owner` -> `Cast to Character` inside an Animation Blueprint; defaults to Self when Self is a Character)  
**Return:** `FOrbitGravityRelativeLocomotion` (split or Break the struct):

| Field | Type | Meaning |
| --- | --- | --- |
| Valid | Boolean | The character and movement component exist and a tangent basis was valid. |
| Gravity Up | Vector | Normalized inverse of native Character Movement gravity, or actor Up if undefined. |
| Tangent Velocity | Vector | Full world-space velocity projected to the gravity-tangent walking plane, **not** world XY. |
| Ground Speed | Float, cm/s | Magnitude of tangent velocity, useful for idle/walk/run BlendSpaces. |
| Forward Speed | Float, cm/s | Tangent velocity dotted with character's projected local Forward, positive forward and negative backward. |
| Right Speed | Float, cm/s | Tangent velocity dotted with character's projected local Right, positive right and negative left. |
| Movement Direction Degrees | Float | `atan2(RightSpeed,ForwardSpeed)` in degrees, 0 forward, +90 right, -90 left, +/-180 backward. |

The node does **not** replace the existing template animation graph automatically. If the current AnimBP derives GroundSpeed with `VectorLengthXY`, that will change as the character runs around the planet because world XY omits world Z velocity. Substitute this gravity-tangent `Ground Speed` output for animation BlendSpace speed and its signed `Movement Direction Degrees` only if your graph uses a directional BlendSpace. On newer templates with root yaw offsets/Orientation Warping, inspect those nodes separately before disabling them; their inputs may also require gravity-relative values. This node reads current native movement velocity, not input intent, and does not predict gait, foot IK or root-motion movement.

### Character Controls facing update

`Face Velocity When Moving` defaults to true. Above `Velocity Facing Threshold = 30 cm/s`, the actor rotates toward the actual tangent-plane velocity; below that threshold it can use the requested move input. It remains optional and independent of the camera. Disable **Face Movement Direction** if your existing locomotion/animation system intentionally controls actor yaw. The previous controls nodes, including the actor-friendly wrappers, are unchanged.

### Distinguishing spawn falls from jumps

The near-floor assist runs from a post-native-movement delegate while gravity direction/scale are still injected pre-movement. `Last Landing Assist Result`, `Last Landing Descent Speed`, `Last Landing Lateral Speed`, `Has Been Grounded` and `Pre First Landing Checks` help distinguish initial spawn fall from a jump following a valid walking state. Defaults: **Near Floor Landing Distance = 10 cm**, **Near Floor Max Approach Speed = 600 cm/s** (previously 100). A result such as `Native FindFloor found no surface` is evidence for a floor-probe/collision/orientation problem rather than an approach-speed gate; `Speed exceeds landing-assist limit` suggests raising the cap only for a controlled diagnostic. This remains experimental and requires in-editor validation.

---

## Iteration 015: shared-field arbitration and editor action

No Orbit-authored Blueprint nodes or components are added/removed in v0.2.5. All `Sample...` nodes, both Character components, Gravity Receiver and the optional OrbitMover adapter use the shared field evaluator. In overlapping regions with equal-priority Point/Override sources, the sample now comes from **the nearer source center**, independent of source registration order. Higher Priority still wins and other shape ties are unchanged. Watch `Dominant Source ID` / `Last Dominant Source ID` as a diagnostic for inter-planet transfers.

`Orbit > Orient to Gravity` is an **Editor-only actor right-click action**, **not a Blueprint node** or runtime component. It samples current editor-world sources for each selected actor, honors its Receiver channels, aligns its local +Z to gravitational Up and supports Undo. See [`ITERATION_015.md`](ITERATION_015.md).


## Iteration 017: single-plugin consolidation (v0.2.6)

The `OrbitMover` code is now inside `Orbit/Source/OrbitMover/` and its component and two functions ship in the **one Orbit plugin**. You do not install, enable or distribute a separate `OrbitMover.uplugin`. The engine's Experimental Mover can remain disabled unless the project uses Mover. The adapter uses runtime reflection and game-thread pre-physics ticking rather than a typed Mover pre-simulation callback, so independent simulation timing and rollback remain unverified. See [MOVER_INTEGRATION.md](MOVER_INTEGRATION.md).


## Orbit Character Controls v0.2.8 diagnostics

When debugging a Character with fixed gravity-relative yaw or missing debug arrows, inspect **the spawned PIE actor's** Orbit Character Controls component. `Draw Locomotion Debug` now works even when Orbit does not own orientation, provided the component ticks. Runtime read-only fields: `Debug Tick Count`, `Native Movement Callback Count`, `Tick Facing Fallback Count`, `Orbit Move Input Count`, `Last Facing Debug Status`. A zero Tick Count indicates that the component did not tick; the status field distinguishes control and facing-ownership conditions. Rotation falls back to the Controls tick when no native movement callback arrived, and tangent velocity can steer facing without the Orbit-specific input bridge if Face Velocity When Moving is enabled. The Blueprint nodes and native component interfaces are unchanged. See `ITERATION_019.md` and `ANIMATION_INTEGRATION.md`.

## Iteration 020: post-movement facing diagnostics (v0.2.9)

`Orbit Character Controls` auto-activates when Face Movement Direction is enabled at BeginPlay, including pre-existing Blueprint component instances with saved Auto Activate off. Explicit runtime deactivation remains possible. `Facing Begin Play Activation Count` diagnoses this repair. `Orbit Character Controls` applies gravity-relative character facing **once per eligible `TG_PostUpdateWork` tick**, regardless of whether `OnCharacterMovementUpdated` broadcast. The callback is diagnostic only. Ownership accepts locally controlled or authoritative Characters; simulated proxies remain excluded. Native orientation flags are actively held off until ownership is released, when their previous settings are restored.

`Draw Locomotion Debug` uses **white Up, blue requested input, green actual tangent velocity, cyan desired facing and yellow actual Actor Forward**. New visible instance-only BlueprintReadOnly properties: `Facing Post Movement Tick Count`, `Facing Update Attempt Count`, `Facing Heading Chosen Count`, `Facing Rotation Applied Count`, `Facing Rotation Rejected Count`, `Last Facing Application Status`, `Last Facing Target Error Degrees`, `Last Facing Applied Delta Degrees`, `Last Facing Remaining Error Degrees`, `Last Facing Rotation Drift Degrees`, `Last Desired Facing Direction`. Their meaning and PIE test workflow are in `ITERATION_020.md`. There are **no new Blueprint-callable functions** in this pass and the established total of 25 base nodes remains unchanged.
