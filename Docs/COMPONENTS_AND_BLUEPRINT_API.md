# Orbit: Components and Blueprint API Guide

**Version:** Orbit v0.1.9, Iteration 010  
**Unreal Engine target:** 5.8.3  
**Audience:** Blueprint users, gameplay designers, and developers integrating Orbit into an Unreal project  
**Source of truth:** Orbit v0.1.9 plugin headers and implementation, particularly `OrbitCharacterGravityComponent`, `OrbitGravityReceiverComponent`, `OrbitBlueprintLibrary`, `OrbitGravitySource`, and `OrbitWorldSubsystem`.

This document describes **functionality actually present in v0.1.9**, not planned navigation, EQS, camera, or Chaos solver integrations.

For the placeable Gravity Source Actor, its property settings, shape and vortex modes, visualization, and level setup examples, see [`GRAVITY_SOURCE_ACTOR.md`](GRAVITY_SOURCE_ACTOR.md).

## 1. How Orbit is organized

Orbit currently exposes:

- **Orbit Gravity Source:** A placeable **Actor** defining a gravity field. It is **not** one of the two components described below.
- **Orbit Character Gravity Component:** An optional bridge between Orbit gravity and Unreal's native Character Movement component.
- **Orbit Gravity Receiver Component:** An optional way to select which gravity channels affect an Actor.
- **Orbit World Subsystem:** A runtime, world-level authority for gravity source sampling and automatic, experimental rigid-body acceleration.
- **Orbit Blueprint Library:** Convenience nodes for sampling gravity and performing gravity-relative vector calculations.

Most ordinary simulated physics objects need **neither component**. When automatic physics integration is enabled, Orbit discovers eligible physics components and applies field acceleration. The Character Gravity component is for native `ACharacter` movement. The Receiver component changes which gravity sources an Actor is eligible to receive.

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
- No root-motion, foot IK, animation, capsule-floor locomotion, AI path following or camera integration is claimed as complete.
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
- The same Receiver mask is used for all eligible primitive components of its owning Actor. v0.1.9 does not provide different Receiver channel masks for individual components within the same Actor.
- For rigid bodies with no compatible Orbit source, Orbit leaves them using native gravity instead of claiming them.

### Blueprint-visible property

**Gravity Channels** (`FOrbitGravityChannels`) is `BlueprintReadWrite`. You can get or set the struct in Blueprint. To change its three booleans, use **Make Orbit Gravity Channels** or **Set Members in Orbit Gravity Channels** and assign the result back to the Receiver's **Gravity Channels** property.

### When you should not add it

Do not add a Receiver merely to make a physics cube fall toward an Orbit source. The default Channel 0 behavior already handles that if the cube is otherwise eligible for automatic physics integration.

---

## 4. Complete list of Orbit Blueprint functions

The v0.1.9 plugin declares **13 Orbit-specific Blueprint-callable or Blueprint-pure functions** across four classes. The names below are the names to search for in the Blueprint node picker. Unreal also generates normal property Get/Set nodes for exposed properties and struct Make/Break nodes, but those are not additional Orbit-authored functions.

### 4A. Orbit Blueprint Library (6 nodes)

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

**Important v0.1.9 detail:** Internally this node uses the evaluator's default **Gravity Channel 0** mask and `General` query. Therefore a source configured **only** for Channel 1 or 2 can return no influence through this convenience function. To sample sources on those channels, use the World Subsystem's **Sample Gravity With Channels** node instead. This is a current API limitation, not an intended channel rule for physics.

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

For a basic test, keep the first two enabled and leave the native physics cube's **Simulate Physics** and **Enable Gravity** checked. Orbit disables native gravity *while it owns* that body, to avoid applying world gravity twice, and restores the original state when management ends.

### Optional physics-component tags

These belong on the **simulated Primitive Component's Component Tags**, not the owning Actor's Actor Tags.

| Tag | Effect |
| --- | --- |
| `Orbit.Ignore` | Excludes that component from automatic Orbit physics gravity. |
| `Orbit.Force` | Allows Orbit to manage a component whose native Enable Gravity started disabled. |
| `Orbit.NoRest` | Prevents experimental rest stabilization for that component. |

These are not Blueprint library nodes, and none of them are required for ordinary Channel 0 cubes.

---

## 8. Current limits relevant to Blueprint use

- **Only two addable Orbit Actor Components exist in v0.1.9:** Character Gravity and Gravity Receiver. The Gravity Source is a placeable Actor; Orbit World Subsystem is created by the game world.
- **Character integration now supports direction and magnitude, with optional Actor/capsule alignment.** Camera/controller/input orientation, advanced locomotion, animation and networking still require further work.
- **Automatic rigid-body gravity is experimental.** It uses game-thread `AddForce(..., bAccelChange = true)`, not Chaos simulation callbacks or substep forces.
- **No gravity-aware NavMesh, EQS, path following, vehicle, Niagara, or projectile integrations are included** in this version.
- **No multiplayer gravity source replication/prediction** is implemented.
- **World Subsystem sampling works in game worlds.** If you need editor-only preview data, use the Gravity Source visualization rather than relying on a PIE subsystem existing in the editor world.
- **Sample Source is a single-source Channel 0 convenience call.** It is not a substitute for the combined Actor/channel-aware world query.
- **Spiral Capture differs from the field preview.** Visualization and Blueprint spatial samples do not account for a body's velocity and collision response.

## 9. Quick node index

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

*Documentation generated from Orbit v0.1.9 source. This is an API-level review, not proof of UE compilation or PIE behavior.*
