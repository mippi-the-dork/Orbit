# Orbit: Gravity Source Actor Guide

**Version:** Orbit v0.1.9, Iteration 010  
**Unreal Engine target:** 5.8.3  
**Audience:** Level designers, technical artists, Blueprint users and programmers  
**Primary class:** `AOrbitGravitySource`  
**Source of truth:** `OrbitGravitySource.h/.cpp`, `OrbitGravityTypes.h/.cpp`, and `OrbitWorldSubsystem.cpp` in Orbit v0.1.9.

This is the guide to the **placeable Orbit Gravity Source Actor** and its properties. For the two addable Orbit components and all 13 custom Blueprint functions, see [`COMPONENTS_AND_BLUEPRINT_API.md`](COMPONENTS_AND_BLUEPRINT_API.md).

This documents what v0.1.9 actually implements. Gravity-aware NavMesh, EQS, AI path following, network prediction and camera integration are future work, not part of this Actor.

---

## 1. What a Gravity Source does

An **Orbit Gravity Source** describes an acceleration field. Its position, rotation, shape, direction, boundaries and strength determine which way eligible objects accelerate at a given location.

The Actor **does not generate physical collision geometry, a planet, a tunnel, or a navigable surface**. Use level geometry, meshes and collision separately. A visualized influence region is also not a collision region.

A source can affect:

- Eligible simulated primitive components through Orbit's experimental automatic physics integration.
- Characters carrying an **Orbit Character Gravity** component, using Unreal's native Character Movement integration.
- Blueprint systems that explicitly query Orbit gravity using its nodes.

A source does not automatically reconfigure arbitrary Pawn movement, AI navigation, animation, Niagara, or project-specific hardcoded world-Z calculations.

### First source setup

1. In the Level Editor, place an **Orbit Gravity Source** Actor.
2. Select the source directly from its Orbit viewport billboard or in the World Outliner.
3. In **Orbit > Field**, choose **Shape = Point (Spherical)** and **Direction = Inward / Down**.
4. Set **Strength = 980 cm/s²** and leave **Max Distance = 0** for an unbounded test.
5. Keep **Enabled**, **Affect Physics**, and **Gravity Channel 0** selected.
6. Place a physical sphere or static mesh planet with working collision at the source's position.
7. Place a physics-simulating cube just beyond the mesh surface. Ensure **Simulate Physics** and the cube's native **Enable Gravity** are initially enabled.
8. Simulate or play. The cube should accelerate toward the source and collide with the mesh.
9. Turn on **Show Gravity Preview** to see cyan acceleration arrows. Enable **Show Streamlines** to see purple gravity field lines.

**Note:** Orbit's Project Settings must have automatic physics integration enabled. A Gravity Source without compatible consumers or explicit gravity queries does not move anything by itself.

---

## 2. Components inside the Actor

| Component | What it is used for | Behavior |
| --- | --- | --- |
| `SceneRoot` | Actor transform | Defines source location and rotation. |
| `DownDirection` (`UArrowComponent`) | Visual orientation hint | Points along the Actor's local **-Z** direction by default. |
| `GravitySpline` (`USplineComponent`) | Spline shape authoring | Edit spline control points, tangents and roll in the viewport. Used when **Shape = Spline**. |
| `OrbitBillboard` (`UBillboardComponent`) | Viewport selection | Displays the supplied Orbit icon in the Editor; hidden during gameplay and excluded from cooked runtime content. |

The source's **local +Z axis** is fundamental: Axis gravity uses it as the line's direction; Directional and Volume gravity use **local -Z** as their default acceleration direction. Rotating the Actor therefore changes those fields.

For a Point source, rotating the Actor does not change simple inward/outward gravity, but **does** change the rotation axis used by its vortex modes.

---

## 3. Gravity Shapes

Select the shape with **Orbit > Field > Shape**.

| Shape | How gravity is calculated | Typical application |
| --- | --- | --- |
| **Point (Spherical)** | Radially toward or away from the Actor's location | Small planets, spherical interiors, gravity wells |
| **Axis (Cylindrical)** | Perpendicular to the infinite line through the Actor along local +Z | Ringworld interiors, cylinders, long rotating habitats |
| **Directional** | Constant spatial direction along local -Z, reversed for Outward | Sideways gravity, uniformly sloped or inverted spaces |
| **Volume (Directional)** | Same directional force as Directional, but limited by a selected three-dimensional volume | Gravity chambers and controlled environmental zones |
| **Spline (Curve / Torus)** | Relative to the closest sampled segment of the editable spline, or to its interpolated local Up | Toroidal planets, twisting corridors, curving artificial-gravity tubes |

### 3.1 Point

- The gravity center is the Actor's origin.
- **Inward** points toward the origin; **Outward** points away.
- **Max Distance** measures the actual distance from the origin.
- At the exact center, radial direction is undefined. It can report influence with zero acceleration.
- **Vortex** rotates around the Actor's local +Z axis. The swirl decreases near its positive and negative poles.

### 3.2 Axis

- The axis is an **infinite straight line**, irrespective of the finite length of the debug cylinder.
- **Inward** points toward the line; **Outward** points away from it.
- **Max Distance** is the **perpendicular (radial)** distance to that line, not the distance to the Actor's origin.
- For an outward-gravity cylindrical interior, place the axis at the cylinder center and arrange solid collision surfaces around it.
- Rotating the source rotates the line and therefore its gravity plane. For example, a 90-degree pitch can make the axis horizontal.
- At the exact axis centerline, normal radial gravity has no unique direction.

### 3.3 Directional

- **Inward / Down** accelerates along the Actor's local **-Z**.
- **Outward / Reversed** accelerates along its local **+Z**.
- The direction is uniform across its influence; it does **not** point toward the Actor's position.
- With **Max Distance > 0**, the influence is bounded by a sphere around the Actor's location, even though acceleration remains directional.
- An optional box limit can additionally constrain that influence.

### 3.4 Volume

A Volume is a **bounded directional gravity field**, not radial attraction toward the volume center.

**Orbit > Volume > Volume Shape** selects the influence boundary:

| Volume shape | Properties that determine its size | Orientation |
| --- | --- | --- |
| **Box** | `Box Half Extents` (X, Y, Z in cm) | Rotates with the Actor |
| **Sphere** | `Volume Radius` (cm) | Rotation does not change the spherical boundary |
| **Cylinder** | `Volume Radius` and `Volume Half Height` (cm) | Long axis follows Actor local +Z |
| **Capsule** | `Volume Radius` and `Volume Half Height` (cm) | Long axis follows Actor local +Z; half-height includes end caps |

- **Inward / Down** points local -Z. **Outward / Reversed** points local +Z.
- A finite **Max Distance** adds a spherical limit centered on the Actor. The **effective region is the intersection** of that sphere and the volume.
- **Limit to Box** adds a second, optional rotated box restriction. For a Box Volume using the same `Box Half Extents`, this is redundant.
- The green editor wireframe shows the volume shape. The additional box limit is yellow, where applicable. The finite-distance limit is blue.

### 3.5 Spline

- Edit `GravitySpline` in the viewport, including its control-point rolls. Set **Closed Loop** on the spline component for a ring.
- **Inward** pulls toward the **nearest sampled spline segment**, not toward the source Actor's origin.
- **Outward** pushes away from that segment.
- **Spline Up (Oriented)** accelerates along the spline's interpolated local Up vector.
- **Spline Down (Oriented)** accelerates opposite that local Up vector.
- **Max Distance** measures distance to the sampled spline polyline, creating a tubular influence zone.
- **Spline Sample Spacing** controls how accurately Orbit approximates the authored curve. It is not a mesh tessellation setting.
- Tight bends and intersecting splines can cause sudden nearest-segment changes. Test continuity carefully.

For a **corkscrew hallway** whose floor direction twists along its length, use **Spline Down (Oriented)** and author the spline's local Up/roll to point toward the ceiling. For a toroidal world's **outer surface**, use radial **Spline Inward**; for an **interior tubular surface**, use **Spline Outward**. These fields alone do not guarantee that an existing player camera or animation setup follows the changing floor orientation.

---

## 4. Direction and Vortex Modes

Select the mode with **Orbit > Field > Direction**.

| Direction | Point / Axis / Spline | Directional / Volume |
| --- | --- | --- |
| **Inward / Down** | Toward center, axis or spline | Along local -Z |
| **Outward / Reversed** | Away from center, axis or spline | Along local +Z |
| **Spline Up (Oriented)** | **Intended for Spline**; uses spline-local Up | Not a dedicated behavior |
| **Spline Down (Oriented)** | **Intended for Spline**; uses reverse spline-local Up | Not a dedicated behavior |
| **Vortex Inward** | Inward radial force plus signed swirl | No spiral field; follows the underlying directional case |
| **Vortex Outward** | Outward radial force plus signed swirl | No spiral field; follows the underlying directional case |

Use the spline-oriented directions **only with Spline shape**. For all other shapes, prefer Inward, Outward or a supported Vortex option to avoid misleading labels.

### 4.1 Vortex Spin

**Orbit > Vortex > Vortex Spin**, default `+1`:

- `0`: purely radial (no tangential acceleration).
- `+1`: a tangential-to-radial acceleration ratio of 1, approximately a 45-degree combined direction where the tangent is fully defined.
- `-1`: reverses the swirl while keeping the same ratio magnitude.
- Higher absolute values produce increasingly tangential acceleration, **not** stronger total acceleration. `Strength` remains the total acceleration magnitude in the normal field evaluator.
- Positive means **clockwise when viewed from the positive local axis toward the negative axis**; negative means counterclockwise from that same viewpoint.
- In Spline mode, the local spline tangent defines the spin axis at the nearest segment.

### 4.2 Vortex Physics Mode

Only **Vortex Inward** on **Point, Axis and Spline** uses the optional velocity-aware rigid-body capture model.

| Mode | Default? | What happens to simulated physics bodies |
| --- | --- | --- |
| **Spiral Capture (Recommended)** | Yes | Steers a body toward an inward, rotating velocity flow, with acceleration limits. Can brake excess sideways speed and reduce centerline ejection. |
| **Continuous Force (Legacy)** | No | Continuously applies the spatial tangential acceleration. May build speed and fling bodies back outward. |

**Vortex Outward** currently uses the ordinary force model, not Spiral Capture. Non-physics Blueprint samples and gravity field streamlines also show the ordinary spatial acceleration, regardless of the selected capture mode.

### 4.3 Spiral Capture properties

These settings appear under **Orbit > Vortex > Spiral Capture**. They control **automatic simulated rigid-body behavior**; they do not change standard Blueprint gravity sample results.

| Property | Default | Meaning |
| --- | ---: | --- |
| **Vortex Convergence Rate** | `0.5 /s` | Desired inward velocity increases with distance from the centerline. |
| **Vortex Angular Rate** | `63 deg/s` | Desired rotational rate at an absolute `Vortex Spin` of 1. |
| **Vortex Response Rate** | `6` | How rapidly physics steering attempts to correct body velocity. |
| **Vortex Max Flow Speed** | `1500 cm/s` | Caps the requested inward plus rotational flow speed. |
| **Vortex Max Acceleration Factor** | `3` | Caps steering acceleration relative to the field's available acceleration magnitude. |

For Axis and Spline fields, capture preserves velocity along the local axis or spline tangent. For Point fields it responds to the body's full velocity. Near the exact singularity, it can apply transverse braking rather than trying to construct a nonexistent radial direction.

**Practical tuning order:** Start at the defaults. Adjust **Convergence Rate** to control how quickly bodies approach the center; **Angular Rate** and signed **Spin** to control rotation; **Response Rate** for responsiveness; then **Max Flow Speed** and **Max Acceleration Factor** to limit unusually fast or sharp motion. High rates can still produce undesirable collision interactions because Orbit's current physics path operates on the game thread rather than through a Chaos substep callback.

---

## 5. Field Strength, Reach and Falloff

Properties live under **Orbit > Field**.

| Property | Default | Behavior |
| --- | ---: | --- |
| **Strength** | `980 cm/s²` | Maximum / reference acceleration before falloff. `0` creates a zero-strength field that can still override other fields. |
| **Falloff** | `Constant` | Controls how magnitude changes with distance. |
| **Max Distance** | `0 cm` | `0` means unlimited, except that a Volume still has its selected boundary. A positive value is an extra range limit. |
| **Inverse Square Reference Distance** | `100 cm` | Defines the distance at which inverse-square acceleration equals authored Strength. |

**Falloff modes:**

- **Constant:** Full Strength everywhere inside the valid influence region.
- **Linear:** With **Max Distance > 0**, decreases from Strength near the source/line/spline to zero at Max Distance. With Max Distance = 0, the current implementation does not apply a linear decrease.
- **Inverse Square:** Uses `Strength * (ReferenceDistance / max(Distance, ReferenceDistance))²`. It never exceeds Strength by approaching the center. A finite Max Distance still imposes a hard outer cutoff.

Distance is shape-relative: radial from Point, perpendicular to Axis, nearest segment for Spline, and from Actor center for Directional and Volume.

**Important:** `Max Distance = 0` does not mean a Volume becomes infinite. Its box, sphere, cylinder or capsule boundary remains in effect.

---

## 6. Influence, Channels and Combining Sources

### 6.1 Influence switches

Properties under **Orbit > Influence**:

| Property | Default | Explanation |
| --- | --- | --- |
| **Enabled** | On | If off, the source is removed from the active runtime field snapshot. |
| **Affect Physics** | On | Allows Physics-type queries and automatic eligible rigid bodies to use the field. |
| **Affect Characters** | On | Allows Character-type queries and the Orbit Character Gravity adapter to use the field. |
| **Limit to Box** | Off | Adds a rotated rectangular mask, independent of Max Distance and any Volume boundary. |
| **Box Half Extents** | `(1000,1000,1000) cm` | Dimensions for the optional box mask and the Box Volume. |

The **General** Blueprint query type does not filter on Affect Physics / Affect Characters. It still respects enabled sources, spatial boundaries and channel matches.

### 6.2 Gravity Channels

Under **Orbit > Channels > Gravity Channels**:

| Channel | Default |
| --- | --- |
| Channel 0 | On |
| Channel 1 | Off |
| Channel 2 | Off |

As with Lighting Channels, an object and a source interact when **at least one selected Gravity Channel matches**. Ordinary Actors without an Orbit Gravity Receiver component implicitly use **Channel 0 only**. To opt an Actor into different channels, add the **Orbit Gravity Receiver** component to that Actor.

These are **not** collision channels, lighting channels, or the separate `General / Physics / Character` Blueprint **Query Channel** selector.

### 6.3 Source combining and Priority

Properties under **Orbit > Combining**:

| Property | Default | Behavior |
| --- | --- | --- |
| **Combine** | `Override` | Overrides eligible lower-priority contributions when this field influences a position. |
| **Priority** | `0` | Higher numbers win between overlapping Override sources. |

The evaluator uses these rules:

1. Find applicable **Override** fields, choosing the highest-priority one.
2. Sum applicable **Additive** fields whose priority is **strictly higher** than that winning Override's priority.
3. If no Override applies, combine applicable Additive fields.

Avoid depending on the ordering of **equal-priority overlapping Override** sources. Give intentionally dominant sources distinct priorities.

**Known v0.1.9 issue:** In the current combined-field evaluator, contributions from Additive-only sources can be calculated without setting `Has Influence` to true. At the World Subsystem level, the normal world-gravity fallback can therefore replace that result. Treat Additive-only setups as not yet reliably validated, especially when world-gravity fallback is enabled. This is a known follow-up fix, not expected intended behavior.

### 6.4 Source registration and runtime changes

- The Actor registers with the **Orbit World Subsystem** when play begins and unregisters at EndPlay.
- The subsystem rebuilds source definitions on its tick. Supported Blueprint property changes such as Strength, Shape, Direction, Channels, Priority and Max Distance are picked up during subsequent updates.
- Editing a `GravitySpline` at runtime is possible through Unreal's spline APIs, but field reconstruction and sampled nearest-segment evaluation can be expensive. Frequent edits need profiling.
- The source Actor has `bReplicates = false` in this version. Changing its properties on a server does **not** automatically synchronize them to clients.

---

## 7. Gravity Visualization and Debug Settings

All debug settings are on the Gravity Source Actor. The visualizations use Editor-only debug drawing and do **not** affect collision, physical forces, field boundaries or cooked Shipping behavior.

### 7.1 Display types

| Visualization | Property | Color / purpose |
| --- | --- | --- |
| **3D gravity arrows** | `Show Gravity Preview` | Cyan arrows show local instantaneous acceleration direction. |
| **Volume boundary** | `Show Volume Bounds` | Green bounds for a Volume shape; yellow for an additional box mask. |
| **Finite Max Distance** | `Show Max Distance Boundary` | Blue boundary for the spatial reach of the source. |
| **Gravity streamlines** | `Show Streamlines` | Purple field lines with optional lighter directional markers. |

These switches can be used independently. For a disabled source, bounded wireframes are gray rather than active green/blue. The source billboard is a viewport-selection aid, not the influence boundary.

### 7.2 Arrow preview controls

| Property | Default | Explanation |
| --- | ---: | --- |
| **Fit Preview to Max Distance** | On | When Max Distance is positive, spread the preview across its range rather than limiting it to Preview Extent. |
| **Preview Density** | `4` | 1 to 10 relative resolution, mapping to 3 to 12 samples per spatial axis in non-spline fields. |
| **Preview Max Arrows** | `2048` | Upper bound on displayed arrow count, to protect editor performance. |
| **Preview Extent** | `900 cm` | Visible range for unbounded sources, when Auto Fit is off, and finite visible axial slice length. |
| **Preview Arrow Length** | `120 cm` | Base arrow size, independent of Max Distance. |
| **Scale Preview by Strength** | On | Shorten or lengthen arrows to reflect local falloff relative to authored Strength. |

Increasing Max Distance expands the depicted region when Auto Fit is enabled, but does not automatically multiply the number of samples. Raise Preview Density to see finer spatial detail.

**Bounds match the source distance metric:** Point / Directional / Volume show spherical Max Distance bounds, Axis draws a finite **illustrative cylinder** around an infinite axis, and Spline draws an approximated tube around the curve. Volume wireframe and Max Distance sphere intersect.

### 7.3 Streamline controls

Streamlines integrate the **normalized instantaneous acceleration direction** using a midpoint (RK2) path trace through the field. They do not simulate a moving body's velocity, collisions, or Spiral Capture correction.

| Property | Default | Explanation |
| --- | ---: | --- |
| **Show Streamlines** | Off | Draw independent curved field lines. |
| **Streamline Density** | `4` | Relative seed coverage and desired line count, from 1 (sparse) to 10 (dense). |
| **Streamline Max Lines** | `32` | Maximum separate paths (up to 96). |
| **Streamline Steps** | `64` | Maximum trace steps per path (8 to 128). |
| **Streamline Length** | `2.0` | Scale of potential traced distance relative to the preview size. |
| **Streamline Max Segments** | `2048` | Hard total segment budget (128 to 8192). |
| **Show Streamline Direction Markers** | On | Draw arrows along traced curves to clarify flow direction. |
| **Streamline Thickness** | `1.5` | Viewport line width. |

Tracing stops at field boundaries, at undefined directions and near certain radial reversals. Results are cached and periodically rebuilt during editor preview; raising line counts, step counts, or changing complex splines still carries a debugging cost.

### Recommended editor presets

| Task | Suggested preview |
| --- | --- |
| General level work | Boundaries on, arrows and streamlines off |
| Check which way a cube will accelerate | Show arrows, Density 3-4 |
| Debug fine differences around intersections | Show arrows, Density 7-10, manage Max Arrows |
| Debug vortex spin direction | Show streamlines, Density 4-7, Markers on |
| Debug a large ring / torus | Fit to Max Distance, show boundary, moderate density |
| Diagnose viewport performance | Disable streamlines first; lower arrow density and budgets |

---

## 8. Practical Examples

### A. Small planet, walkable exterior

- Gravity Source centered inside the planet mesh: **Shape = Point**, **Direction = Inward**, **Strength = 980**, **Channel 0 = On**.
- Give the planet mesh solid collision.
- Spawn a physics cube off the surface to confirm gravity.
- For an `ACharacter`, add **Orbit Character Gravity** and optionally enable experimental actor alignment; camera, input, animation and controller orientation need their own adaptation.

### B. Cylindrical habitat / ringworld interior

- Position the source along the habitat's central line.
- **Shape = Axis**, **Direction = Outward**.
- Rotate the Actor to align its local +Z with the ring or cylinder's intended axis.
- Set **Max Distance** to cover the radius of the interior wall, or `0` for an unlimited test.
- Add inner-wall geometry and collision. The source does not make a floor on its own.

For a **Halo-style circular ringworld**, an Axis field is appropriate only where the intended gravity behaves radially around a straight central axis. For a world built around a **closed tube** whose cross section changes along its loop, use Spline instead.

### C. Toroidal surface or gravity tube

- Set **Shape = Spline** and make `GravitySpline` a **closed loop** with the desired ring shape.
- For the tube's **external surface**, use **Inward** toward the spline centerline.
- For the tube's **internal surface**, use **Outward** away from the spline centerline.
- Set **Max Distance** to cover the expected distance from tube surfaces to their centerline.
- Use **Show Max Distance Boundary** to verify influence coverage and decrease **Spline Sample Spacing** if tight bends look inaccurate.

### D. Corkscrew hallway with a rotating floor

- Set **Shape = Spline**, **Direction = Spline Down (Oriented)**.
- Author the spline along the hallway and roll its control points so spline **Up** corresponds to the desired ceiling direction.
- Enable Show Gravity Preview. The cyan arrows should point toward the floor as roll changes.
- Put the collision hallway and the Character Gravity component in the level.
- Enable character **Align Actor to Gravity** if desired, but expect to adapt movement input and camera separately. This is not yet a complete ready-made walk-anywhere Character.

### E. Circular inward vortex

- Set **Shape = Axis**, **Direction = Vortex Inward**, **Spin = +1**.
- Leave **Vortex Physics Mode = Spiral Capture** to test converging physics cubes.
- Enable both arrows and streamlines to compare the spatial field with the observed physical motion.
- Change Spin to `-1` for reverse rotation. Compare with **Continuous Force (Legacy)** to see why unbounded tangential acceleration can eject bodies.

### F. Local zero-gravity chamber

- Set **Shape = Volume**, choose a Box / Sphere / Cylinder / Capsule, set **Strength = 0**, and leave **Combine = Override**.
- Set a higher **Priority** than surrounding override sources.
- Keep matching channels and relevant Affect flags enabled.
- Confirm the volume's actual green bounds and test a compatible body or Character moving across its boundary.
- A zero-strength field can still count as an influence; it does not provide a valid direction for alignment.

### G. Two independent gravity layers

- Source A: Channel 0 only. Source B: Channel 1 only.
- Normal cube: No receiver required; defaults to Channel 0.
- Special cube: Add **Orbit Gravity Receiver** and select only Channel 1.
- Place both cubes where both sources influence. Each should use the compatible source, subject to Physics flags and actual field boundaries.

---

## 9. Blueprint Access to Gravity Source

### Editing source parameters

Store or obtain an `Orbit Gravity Source` Actor reference and use ordinary Blueprint property **Get** and **Set** nodes. For example:

```text
Event / gameplay action
  -> Gravity Source reference
  -> Set Strength (new cm/s² value)
  -> Set Direction
  -> Set Max Distance
```

Supported property edits are reflected in subsequent World Subsystem snapshots. No special `Refresh Gravity Source` node exists in v0.1.9. These changes are **not network-replicated** automatically.

### Orbit-authored node on the Actor: Sample Source

| Item | Value |
| --- | --- |
| Blueprint node | **Sample Source** |
| Category | `Orbit | Field` |
| Target | `Orbit Gravity Source` Actor reference |
| Input | `World Location` (world-space Vector) |
| Output | `Orbit Gravity Sample` struct |

`Sample Source` evaluates **only the referenced source**, without applying World Subsystem fallback gravity, other sources or their priorities.

**Current limitation:** `Sample Source` uses the evaluator's **Channel 0** receiver mask and the **General** integration query. If this source is configured for Channel 1/2 only, it may return no influence. For a channel-correct query, use **Sample Gravity With Channels** on the Orbit World Subsystem instead.

Other relevant nodes, including `Sample Orbit Gravity for Actor`, `Sample Gravity`, and `Get Active Source Count`, are documented in [`COMPONENTS_AND_BLUEPRINT_API.md`](COMPONENTS_AND_BLUEPRINT_API.md).

---

## 10. Known Limitations and Troubleshooting

| Symptom / situation | Check first |
| --- | --- |
| Physics cube falls along world Z | Project Settings automatic physics integration; simulated component's Enable Gravity and tags; source Enabled, Affect Physics, channels and bounds. |
| Gravity arrows don't reach Max Distance | Enable Fit Preview to Max Distance, check Preview Extent and the shape's distance metric. |
| Axis cylinder visualization seems to end | The visual cylinder is a finite slice. Actual axis gravity may continue indefinitely along the axis. |
| Spline motion changes unexpectedly at a tight bend | Check spline shape, nearest-segment discontinuities and lower Spline Sample Spacing. |
| Physics objects spiral inward then fly outward | Compare Spiral Capture against legacy Continuous Force; inspect response rate and acceleration cap, collisions and high incoming velocity. |
| Character capsule aligns but camera does not | Character actor alignment intentionally does not rotate Player Controller, Camera Manager or input mapping. |
| A custom-channel source is invisible to `Sample Source` | Use World Subsystem `Sample Gravity With Channels`. The Actor convenience node currently queries Channel 0. |
| All fields are Additive and gravity sampling behaves strangely | See the known v0.1.9 Additive-only influence-reporting issue described in Section 6.3. |
| Physics and Blueprint previews disagree for an inward vortex | Spiral Capture is velocity-aware for rigid bodies. Preview arrows and streamlines show the spatial acceleration field. |
| Streamline preview becomes expensive | Reduce Steps, Max Lines or Max Segments; consider disabling preview when finished debugging. |

**Other current boundaries:**

- Gravity Source visualizations are editor-focused, not a runtime gameplay VFX system.
- Automatic simulated-body gravity uses game-thread `AddForce(..., bAccelChange = true)`, not an integrated Chaos solver/substep callback.
- There is no gravity-aware NavMesh, EQS, AI path following or advanced vehicle/cloth/water support in v0.1.9.
- A source has no automatic replication. Network consistency and predicted character gravity remain future work.
- A Gravity Source Actor is **not a Content Browser gravity asset type** in this version.
- The plugin does not automatically generate planet, torus, hallway or ringworld geometry.

---

## 11. Compact Property Reference

| Category | Property | Default |
| --- | --- | --- |
| Field | Shape | Point |
| Field | Direction | Inward |
| Field | Strength | 980 cm/s² |
| Field | Falloff | Constant |
| Field | Max Distance | 0 cm (unlimited) |
| Field | Inverse Square Reference Distance | 100 cm |
| Vortex | Vortex Spin | +1 |
| Vortex | Vortex Physics Mode | Spiral Capture |
| Vortex / Spiral Capture | Vortex Convergence Rate | 0.5 |
| Vortex / Spiral Capture | Vortex Angular Rate | 63 deg/s |
| Vortex / Spiral Capture | Vortex Response Rate | 6 |
| Vortex / Spiral Capture | Vortex Max Flow Speed | 1500 cm/s |
| Vortex / Spiral Capture | Vortex Max Acceleration Factor | 3 |
| Combining | Combine | Override |
| Combining | Priority | 0 |
| Channels | Channel 0 / 1 / 2 | On / Off / Off |
| Influence | Enabled | On |
| Influence | Affect Physics / Affect Characters | On / On |
| Influence | Limit to Box | Off |
| Influence | Box Half Extents | 1000 / 1000 / 1000 cm |
| Volume | Volume Shape | Box |
| Volume | Volume Radius | 1000 cm |
| Volume | Volume Half Height | 1000 cm |
| Spline | Spline Sample Spacing | 200 cm |
| Debug | Show Gravity Preview | Off |
| Debug | Show Volume Bounds | On |
| Debug | Show Max Distance Boundary | On |
| Debug | Fit Preview to Max Distance | On |
| Debug | Preview Density | 4 |
| Debug | Preview Max Arrows | 2048 |
| Debug | Preview Extent | 900 cm |
| Debug | Preview Arrow Length | 120 cm |
| Debug | Scale Preview by Strength | On |
| Debug / Streamlines | Show Streamlines | Off |
| Debug / Streamlines | Streamline Density | 4 |
| Debug / Streamlines | Streamline Max Lines | 32 |
| Debug / Streamlines | Streamline Steps | 64 |
| Debug / Streamlines | Streamline Length | 2 |
| Debug / Streamlines | Streamline Max Segments | 2048 |
| Debug / Streamlines | Show Streamline Direction Markers | On |
| Debug / Streamlines | Streamline Thickness | 1.5 |

---

## 12. Documentation Maintenance

This guide must be updated whenever `AOrbitGravitySource`, gravity enums, field evaluation rules, vortex handling or visualization settings change. The companion components/API guide must be updated whenever an Orbit component, Blueprint function, struct or related behavior changes.

For every future Orbit release, compare both files with the actual plugin headers and implementation, then update their version labels and feature/limitation descriptions. Do not document planned features as implemented.

*Prepared against Orbit v0.1.9 source. Documentation review is not a substitute for an Unreal 5.8.3 compile and runtime test.*
