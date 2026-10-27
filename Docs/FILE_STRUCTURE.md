# Orbit File Structure (0.2.6)

**One installable Unreal plugin**, containing three internal modules:

```text
Orbit/
  Orbit.uplugin
  README.md
  CHANGELOG.md
  Config/
    FilterPlugin.ini
  Resources/
    Icon128.png
    Icons/
      Orbit-Icon.svg
      OrbitActor_16.svg
      OrbitAsset_64.svg
      OrbitBillboard.png
  Docs/
    ARCHITECTURE.md
    COMPATIBILITY.md
    COMPONENTS_AND_BLUEPRINT_API.md
    GRAVITY_SOURCE_ACTOR.md
    MOVER_INTEGRATION.md
    ITERATION_017.md
    ...earlier iteration guides and test plan...
  Source/
    Orbit/
      Orbit.Build.cs
      Public/
      Private/
        Tests/
    OrbitEditor/
      OrbitEditor.Build.cs
      Private/
    OrbitMover/
      OrbitMover.Build.cs
      Public/
      Private/
        Tests/
```

The module `OrbitMover` is packaged INSIDE `Orbit` and is not a separate `.uplugin`. It does not link or include Epic Mover. Epic Mover is optional at runtime, and can be enabled in the project when needed. All source is intended for UE 5.8.3. No engine source changes.
