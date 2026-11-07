# Orbit | Experimental Gravity Performance Validation

The native test `Orbit.Physics.GravityIntegration.EvaluationCostBaseline` times **field evaluation only**. It is not a frame-time, Chaos physics, collision, or game packaging benchmark. The purpose of this guide is to produce a meaningful comparison across real PIE and packaged scenarios without requiring production changes.

## Controlled comparison

1. Use a dedicated test map (not the nine-source demo map). Keep collision meshes, body materials, frame-rate limits, World Gravity, support/damping settings, override resolution, and source transforms **identical** between runs.
2. Run with a known combination of 2, 8, or 32 eligible sources and 64 or 256 identical physics static meshes. For the experimental mode, use only its supported Point or mixed Point/Directional shapes. In Standard, use the exact same authored fields.
3. Keep `Stabilize Resting Rigid Bodies` and `Damp Supported Rigid Body Motion` **Off** to isolate the gravity integration. Keep the experimental pre-BeginPlay ownership setting identical where possible. Note that Standard does not implement the experimental early-ownership option, so differences in startup behavior can affect the number of awake bodies later.
4. Restart PIE between modes. Warm up each session, then record 30 seconds under static sources. Record a *separate* controlled source-movement sequence rather than mixing stationary and moving samples.
5. Repeat at least three times per configuration, interleaving modes where practical. Record hardware, UE version, build configuration, frame rate limit, and physics substep settings.
6. Record counts of **awake bodies**, because Chaos legitimately stops integrating sleeping bodies; a scene with 6 asleep vs 0 asleep does not make a fair like-for-like cost comparison unless that difference is reported.

## Unreal profiling

Use Unreal Insights Timing Insights with `cpu` and `frame` traces enabled. For example, the supported launch option is `-trace=cpu,frame,bookmark`. Locate the existing `OrbitExperimentalGravityPreIntegrate` native stat in the physics workload where available and measure total physics/game-thread costs alongside it. If the stat doesn't appear in the trace configuration, use Editor stat diagnostics and compare the overall physics track; don't assume a missing marker means zero work. For high-detail analysis use an Unreal Insights capture rather than the Motion History CSV.

Capture and compare:

| Metric | Why it matters |
|---|---|
| Game Thread mean, p95 and max | Field snapshot rebuild, discovery, component reads and updates |
| Physics Thread mean, p95 and max | Solver callback + native forces + contacts |
| `OrbitExperimentalGravityPreIntegrate` scope | Active callback cost, *not* whole physics step |
| Awake/sleeping body counts | Explains large differences in CPU consumption |
| Source count and shape | Field evaluation scales with eligible sources |
| Collision geometry and contact count | Rolling and impacts may dominate simulation |
| Source motion on/off | Rebuilding snapshots and waking bodies changes workload |
| Any missing bodies in Motion History | They may have left Orbit influence, not slept |

Do **not** fail automation based on the cost microbenchmark's absolute elapsed time: CPU, scheduling, development/editor load, and compiler flags affect it. Use repeatable captures and report variance.

## Motion History analyzer

From the plugin directory on Windows (with Python 3 available):

```powershell
python .\Scripts\AnalyzeGravityMotion.py "C:\OrbitCaptures\1spherefront.zip" --output-csv "C:\OrbitCaptures\summary.csv"
```

The analyzer supports `.csv` and `.zip` (multiple CSV captures), uses Python standard library only, and does not require NumPy or Pandas. It reports per-body sample coverage, speed windows, last observed awake state and solver diagnostic flags. It does not measure performance, contact impulses, friction, energy conservation, or source-specific force components. A body last recorded at 5 seconds in a 60-second run cannot be counted as asleep at 60 seconds.

If all velocities are steady while a body remains on the surface, investigate contact and angular motion **before** changing gravity acceleration. An instantaneous increase in speed while approaching a gravity source is not automatically an energy-injection bug. Real gravitational assist requires moving masses, but moving authored Orbit sources may do work on bodies.

## Validation status

Iteration 038's user-provided evaluator microbenchmarks were ~0.09–0.10 µs/body-evaluation with 2 sources, ~0.33–0.34 µs with 8, and ~1.05–1.15 µs with 32 (64/256 body variants). They are useful scaling data only. **No full frame-time or Unreal Insights benchmark and no packaged-game performance results have yet been supplied.**
