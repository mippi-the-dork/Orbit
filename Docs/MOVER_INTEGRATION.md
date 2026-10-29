# Orbit: Mover Integration

**Current Orbit version:** 0.2.11, Iteration 022  
**Engine target:** Unreal Engine 5.8.3  
**Scope:** Orbit functionality specific to Epic's Experimental Mover, not general Mover documentation.

## 1. Install and architecture

The whole release installs as **one** `Plugins/Orbit/` plugin with **one** `Orbit.uplugin`. It contains three internal modules:

```text
Orbit/
  Orbit.uplugin
  Source/
    Orbit/                  # gravity field evaluator, world subsystem and general components
    OrbitEditor/            # level editor and previews
    OrbitMover/             # Orbit-to-Mover adapter, policy and tests
```

`OrbitMover` is a module name, **not a second plugin**. There is no `OrbitMover.uplugin` and no separate plugin to enable. Orbit builds without Epic Mover because the internal OrbitMover module does **not** include Mover C++ headers, depend on the `Mover` module, or reference its linked symbols. It recognizes actual enabled Mover components at runtime through UObject reflection.

Enable Epic's **Mover** under Edit > Plugins only if using Mover characters. Restart/build your project after enabling it as required by the engine. The Orbit plugin stays enabled either way.

**Iteration 021 note:** No changes were made to OrbitMover or its reflection-based Mover adapter. The gravity evaluator now caps Volume Max Distance in local shape space, so Orbit sampling through Mover benefits from the same field boundaries as native Characters and physics. This is a core-field change, not a new Mover feature. The Mover backend/timing limitations documented below remain unchanged.

## 2. What Orbit adds to Mover

Orbit provides an addable **Orbit Mover Gravity** (`UOrbitMoverGravityComponent`) component, with:

- World-space gravity acceleration passed to Mover's reflected `SetGravityOverride` API.
- Local Up passed to `SetUpDirectionOverride`; zero-strength Orbit fields hold the previous stable Up.
- Source eligibility evaluated by the existing Orbit World Subsystem with the Actor's Orbit Gravity Receiver Channels and the `Character` query type.
- Native Mover gravity overrides released when leaving an Orbit field or disabling the adapter.
- A manual refresh node and an enable/disable node.

The module calls the UE 5.8.3 public **Blueprint-reflected** methods at runtime, so no engine modifications are necessary. Mover components are discovered by their class hierarchy (including subclasses of `UMoverComponent`).

### New technical behavior in v0.2.6

Previous Orbit development builds used Mover's **typed `OnPreSimulationTick` delegate**, which directly required the Mover C++ module. The consolidated optional integration now uses a **`TG_PrePhysics` game-thread tick** to update gravity. When the Mover component also participates in the normal tick graph, Orbit registers a tick prerequisite to run before it. This is an intentional portability tradeoff but **not equivalent to guaranteed pre-simulation execution** in all Mover backends. Do not assume correctness for network prediction, rollback or independently scheduled simulation without engine-level validation. The runtime protects against the explicitly detectable async and Chaos Character Mover cases, but other custom backends must be assessed by their author.

## 3. Setup in a Mover Pawn

1. Enable the Epic **Mover** plugin in your Unreal project, then enable **Orbit**.
2. Begin with a Pawn/Actor from Epic's **kinematic Mover examples** (not the ordinary native Third Person Character Movement sample).
3. Add **Orbit Mover Gravity** to that Pawn's components.
4. Place an **Orbit Gravity Source**, for example `Point`, `Inward`, `Strength=980`, `MaxDistance=0`, `Affect Characters=true`, Gravity Channel 0.
5. Optionally add **Orbit Gravity Receiver** if the Pawn should use Gravity Channel 1 or 2 instead.
6. Start PIE. Inspect `Backend Supported`, `Inside Orbit Field`, `Last Orbit Acceleration`, `Last Orbit Up`, `Last Sample Position` and `Samples Applied` on the component.

The adapter does not change Mover movement modes, automatic Pawn orientation, camera or input. Do not add the native **Orbit Character Gravity** adapter to a Mover Pawn; it targets `ACharacter`'s `UCharacterMovementComponent`, a different movement backend.

## 4. Orbit Mover Gravity settings

| Property | Default | Meaning |
| --- | --- | --- |
| Enable Orbit Gravity | true | Whether Orbit takes ownership of Mover's gravity/Up overrides in a matching source field |
| Inside Orbit Field | runtime read-only | Whether the most recent Orbit field sample had source influence |
| Last Orbit Acceleration | runtime read-only | Field acceleration, cm/s² |
| Last Orbit Up | runtime read-only | Last valid Up passed to Mover |
| Last Sample Position | runtime read-only | Actor pivot position used for field sampling |
| Samples Applied | runtime read-only | Number of successful samples applied to Mover |
| Backend Supported | runtime read-only | Detected synchronous non-Chaos Mover with the expected public reflected API |

### Two Blueprint nodes

`Set Orbit Mover Gravity Enabled(bool bEnabled)` enables or disables Orbit override ownership. Turning off attempts to return Mover to its own native override behavior.

`Refresh Orbit Mover Gravity()` manually resamples and applies the field immediately on the game thread. Normally the adapter updates every pre-physics tick.

These nodes are authored in Orbit's internal `OrbitMover` module and appear without a separate companion plugin. They do nothing meaningful unless the actor has a supported enabled Mover component.

## 5. Gravity behavior and limitations

- **No source influence:** clears any overrides owned by Orbit. A world fallback sample without source influence is not treated as an Orbit source.
- **Explicit zero-strength source:** sends zero acceleration while maintaining a stable previous Up vector.
- **Overlapping sources:** uses Orbit's regular shared field arbitrator (Priority, Strongest Influence, Blended Influence), not a Mover-specific field evaluator.
- **Component gravity sources:** embedded Orbit Gravity Source components work through the same World Subsystem.
- **Multiple competing override writers:** there is no public ownership stack for Mover's previous gravity override settings; using another system that also calls SetGravityOverride requires explicit integration design.
- **Async/Chaos Mover:** not supported; the bridge rejects detectable unsupported backend variants.
- **Rollback/network prediction:** not deterministic or validated. A pre-physics sample is not automatically part of Mover's historical simulation state.
- **Swimming/other default Mover modes:** some Epic systems still assume world Z; do not assume automatic compatibility with arbitrary gravity across every movement mode.
- **PIE live-edit:** changes made on the actual PIE source should be sampled on the next appropriate game-thread update. Editor-world to PIE mirroring is implemented separately in the Orbit Source Actor and should be validated.

## 6. Regression tests

### Automatic tests (written, not run here)

Run `Orbit.Mover.*` through Unreal's Automation Test interface:

- `Orbit.Mover.ZeroGravityPreservesUp`
- `Orbit.Mover.ExitReleasesOverrides`
- `Orbit.Mover.SidewaysAcceleration`
- `Orbit.Mover.MissingMoverSafe`

### Manual UE 5.8.3 matrix

| Scenario | Expected |
| --- | --- |
| Orbit enabled, Epic Mover **disabled** | Project builds, opens and samples gravity normally; no Mover dependency failures |
| Epic Mover enabled, compatible kinematic Pawn | `Backend Supported = true`; samples applied during PIE |
| Inward Point field at X-offset | Acceleration points toward source, Up opposite |
| Change Strength during PIE | `Last Orbit Acceleration` updates; physical response requires gameplay verification |
| Zero-strength source | Zero acceleration, stable Up, no crash |
| No matching Receiver Gravity Channels | No Orbit source influence; native Mover overrides released |
| Exit finite Max Distance | Orbit relinquishes override ownership |
| Disable/re-enable component | Override released/restored when appropriate |
| Chaos Character Mover or async backend | `Backend Supported = false`; Orbit doesn't claim compatibility |
| Network prediction/rollback | **Not supported**, do not treat a successful local test as proof |

## 7. Related Orbit documentation

- [Components and Blueprint API](COMPONENTS_AND_BLUEPRINT_API.md): all Orbit components and nodes.
- [Gravity Source Actor](GRAVITY_SOURCE_ACTOR.md): field shapes, strength, channels, precedence and visualization.
- [Iteration 017](ITERATION_017.md): implementation changes and consolidation acceptance criteria.

**Build status:** Source and ZIP verification only. This release has not been compiled against the user's UE 5.8.3 installation.


## Iteration 018 update scope

No Mover adapter logic, build dependencies, or Mover Blueprint API changed in Iteration 018. Character-facing ownership and animation state apply to native Character Movement + Orbit Character Controls, **not** Epic Mover-driven pawns. The optional internal `OrbitMover` module remains in the same single Orbit plugin and requires its own runtime validation.


## Iteration 019 scope

v0.2.8 changes the **native Character Controls** facing and diagnostic behavior only. The internal OrbitMover adapter, reflected Mover gravity/up calls, load behavior and synchronous kinematic support boundaries are unchanged. The new Character Controls fallback must not be interpreted as adding ChaosMover, async or rollback support.

## Iteration 020 scope

v0.2.9 only changes the native `UOrbitCharacterControlsComponent` rotation application, math helper and diagnostics. The internal `OrbitMover` module, optional reflected Mover integration and synchronous kinematic compatibility scope are unchanged. The new native Character-facing controls are not applied to Mover-driven Pawns. ChaosMover, async prediction and rollback are still unsupported.

## Iteration 022 note: native Character animation only

`UOrbitGravityAnimInstance`, `Get Orbit Gravity Animation State` signed vertical speed, and `Calculate Orbit Gravity Vertical Speed` are **native Character Movement animation conveniences**. They do not change the OrbitMover component, synchronous kinematic Mover adapter, reflection bridge, tick order, supported backends, or plugin optionality. A Mover Pawn should query its own velocity/gravity frame and can call the pure `Calculate Orbit Gravity Vertical Speed` helper for an animation transition, but the `UOrbitGravityAnimInstance` default producer expects an `ACharacter` with native Character Movement. Full Mover-specific animation-state integration remains future work.

## Physics Rest Diagnostics (v0.2.13)

Orbit's automatic simulated rigid-body gravity uses center-of-mass sampling and near-contact diagnostics in 0.2.13. **These changes do not alter Orbit's optional synchronous kinematic Mover adapter**, which does not use the rest-contact sweep or rigid-body damping settings. No new Chaos Mover support is implied.
