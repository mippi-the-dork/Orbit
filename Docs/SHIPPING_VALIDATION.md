# Orbit | Win64 game Development + Shipping validation (maintained)

**Plugin > Package is not the same as packaging an Unreal game containing Orbit.** The runtime `Orbit` and `OrbitMover` modules must compile and run in a non-editor target; `OrbitEditor` is editor-only.

## Preflight

1. Use a clean project copy or Perforce workspace, with `Plugins/Orbit/Orbit.uplugin`, its `Source`, `Config` and relevant resources. **Never submit `Intermediate`, `Saved`, UHT-generated `.gen.cpp` or `.generated.h` files to Perforce.** The source plugin and a packaged/precompiled plugin have different deployment implications: source plugins require an appropriate C++ toolchain to build non-editor targets.
2. Close Editor and active build tools before investigating a UHT failure. If generated files were copied into Perforce or made read-only, UHT can fail to rename `.tmp` outputs. This is not evidence of an Orbit C++ compile failure. Clear only *local generated* `Plugins/Orbit/Intermediate` where appropriate; don't delete game assets or source.
3. Confirm Unreal Engine 5.8.x / Win64 compiler toolchain and that project paths are writable. **Use separate output directories for Development and Shipping** so one archived build can't mask the other.
4. Begin with `Rigid Body Gravity Application = Standard`. After the Standard game starts and physics is correct, test the experimental modes one at a time in another build/configuration. Keep all experimental options default-off in the baseline.

## Script: Development then Shipping

From PowerShell (replace paths with yours):

```powershell
.\Plugins\Orbit\Scripts\RunShippingSmokeTest.ps1 `
  -Project "C:\Path\Game.uproject" `
  -EngineRoot "C:\Program Files\Epic Games\UE_5.8" `
  -Configuration Development `
  -ArchiveDirectory "C:\OrbitPackaging\Development"

.\Plugins\Orbit\Scripts\RunShippingSmokeTest.ps1 `
  -Project "C:\Path\Game.uproject" `
  -EngineRoot "C:\Program Files\Epic Games\UE_5.8" `
  -Configuration Shipping `
  -ArchiveDirectory "C:\OrbitPackaging\Shipping"
```

The `-Configuration` parameter was added in Iteration 039; omitting it still defaults to **Shipping** for compatibility with older invocations. The helper invokes Unreal AutomationTool `BuildCookRun -platform=Win64 -clientconfig=... -build -cook -stage -pak -archive`. It **does not delete files, interact with Perforce, or certify runtime success**.

## Mandatory runtime checks

- Launch each archived `.exe`. Confirm the startup map loads, one simulated cube accelerates toward an Orbit Point source, and the authored physics/character systems work. Test without the Editor present.
- Run a clean C++ game package first. Also test in a Blueprint-only project where practical; source plugins may still invoke C++ compilation during packaging.
- Test with Mover disabled. If the game uses Mover, test the Mover-enabled configuration separately.
- Record the first actual UHT, compilation, link, cook, stage, or runtime error. `OtherCompilationError`, generic `ExitCode=6`, or the trailing `BUILD FAILED` alone are **not sufficient diagnosis**.
- For experimental gravity, verify the authored setting persists in packaged runtime and that Physics Integration actually takes effect; editor-only automation tests don't establish Shipping behavior.

## Validation status

No user-confirmed clean Orbit-containing Win64 **game** Development/Shipping package or packaged executable smoke test has been supplied. UE 5.8.3 development editor and automation success does not fulfill this gate. This script is a local runner, not a previously completed build.
