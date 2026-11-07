# Orbit 0.2.30 | Iteration 041 — Live-world physics automation

## Purpose

Replace the repetitive manual Axis/Spline/Volume/mixed/fallback testing from Iteration 040 with genuine native Chaos simulations. The prior 29 `Orbit.Physics.GravityIntegration` tests remain unchanged and continue to test field calculations/eligibility. The new tests have a separate prefix, so a math-policy pass is never mistaken for live physics validation.

## Scope and architecture

- Editor-only C++ tests in `Source/OrbitEditor/Private/Tests/OrbitChaosWorldIntegrationTests.cpp`, compiled only with `WITH_DEV_AUTOMATION_TESTS`.
- Unreal Engine's built-in `FTestWorldWrapper` constructs and tears down a **temporary Game world**. Tests spawn source Actors and physics-simulated basic-shape Static Mesh Actors, invoke BeginPlay, tick the world, and inspect real physics velocity, sleep state, and `UOrbitWorldSubsystem::GetPhysicsBodyDiagnostics`.
- Fixture sets World Gravity Z to **0 only inside its temporary world**. A scope guard temporarily assigns **Experimental Chaos + Expanded Field Compatibility + Early Ownership** and disables manual rest/damping. Its destructor restores all touched `UOrbitSettings` fields **without SaveConfig**.
- Nothing runs from the user's open map. No test assets are created or saved. No separate testing plugin is required.
- The runtime modules, gravity evaluator, Chaos integration, public Blueprint APIs, and 127-column CSV writer are identical to Iteration 040 Hotfix 1. The plugin remains one install, and Standard remains the default.

## Test commands

In the **Unreal Editor Output Log**, run the commands one at a time:

```text
Automation RunTests Orbit.Physics.GravityIntegration
Automation RunTests Orbit.Physics.IntegrationWorld
```

The first command should still report **29/29**. The second command contains **9** new editor-only live-world tests. Don't change Project Settings or create a test level first; it is safe to leave your own project configured to **Standard**.

For more targeted investigation:

```text
Automation RunTests Orbit.Physics.IntegrationWorld.ExpandedAxisMotion
Automation RunTests Orbit.Physics.IntegrationWorld.ExpandedSplineUpDown
Automation RunTests Orbit.Physics.IntegrationWorld.ExpandedVolumeBoundaries
Automation RunTests Orbit.Physics.IntegrationWorld.ExpandedMixedPriorityMotion
Automation RunTests Orbit.Physics.IntegrationWorld.VortexFallsBackToStandard
Automation RunTests Orbit.Physics.IntegrationWorld.SourceMovementReversesAcceleration
Automation RunTests Orbit.Physics.IntegrationWorld.DirectionalNaturalSleepAndWake
Automation RunTests Orbit.Physics.IntegrationWorld.ReceiverChannelFallback
Automation RunTests Orbit.Physics.IntegrationWorld.SourceRemovalRestoresNativeGravity
```

## Coverage and criteria

| Test | What is actually simulated | Required assertions |
|---|---|---|
| ExpandedAxisMotion | Simulated cube near a vertical Axis source | Expanded ownership, acceleration toward axis, no world-Z impulse |
| ExpandedSplineUpDown | Two separate source/body worlds with a straight spline | Expanded ownership, actual opposite upward/downward physics velocity |
| ExpandedVolumeBoundaries | Four independent worlds: Box, Sphere, Cylinder, Capsule | Inside cube accelerates, outside cube remains in zero world gravity, inside uses expanded solver |
| ExpandedMixedPriorityMotion | Higher-priority Axis Additive + lower-priority Directional Override | Both X and Z motion contributions, expanded ownership |
| VortexFallsBackToStandard | Velocity-sensitive Point Vortex Continuous Force | All experimental mode flags off, but real body still accelerates |
| SourceMovementReversesAcceleration | Axis source moved from X=0 to X=1000 during runtime | Simulated X-velocity change follows the relocated source |
| DirectionalNaturalSleepAndWake | Dynamic cube falls onto a static collision floor, then gravity direction rotates | Cube falls, Chaos reports natural sleep with no Orbit intervention, then wakes/moves |
| ReceiverChannelFallback | Axis source/receiver both exclusively on Channel 1 | Standard owns body rather than expanded solver, actual acceleration remains correct |
| SourceRemovalRestoresNativeGravity | Runtime Axis source destruction while cube is moving | Custom acceleration stops, original native gravity enabled restored, Orbit releases the body |

The tests deliberately don't require exact final transforms or exact sleep timestamps. Chaos is not perfectly deterministic; broad directional/mode assertions are more stable than one-frame motion comparisons.

## Validation constraints

**Not yet UE-compiled or executed:** Source-level checks cannot prove that an isolated test world in the installed UE 5.8.3 build advances Chaos correctly. If the helper fails to create/tick the world, or physics isn't initialized in that world type, the test must fail and we should fix its harness rather than change Orbit's production gravity. Please send the **first compiler/UHT error** or the full failed test output.

**Still manual:** Visual spline/volume preview alignment, actual player/Character and Mover interactions, performance/Unreal Insights, network and streaming behavior, complex multi-body physics, and one packaged Development/Shipping smoke test after the parity work is complete. No need to repeat the 20-second CSV captures for the cases above if their live-world tests pass.

**Unchanged limitations:** Velocity-sensitive Vortex remains on Standard. Channel 1/2 multi-source solver ownership, articulated/compound bodies, and weak-gravity native Chaos unsupported sleeping remain pending parity work.

## Hotfix 1 — UE 5.8.3 test compilation

- Corrected `TestNotNull()` in the Spline integration test to pass `Source->GravitySpline.Get()` instead of a `TObjectPtr<USplineComponent>`. The Unreal automation assertion template requires a raw pointer.
- Test-only source change. No production runtime modules, gravity behavior, settings, public APIs, or Motion History CSV changes.
- The new nine-test live-world suite still requires a UE 5.8.3 compile and runtime execution; this hotfix fixes the reported C2672 compiler error, not any potential runtime test failures.
