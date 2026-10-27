using UnrealBuildTool;

// This module intentionally has NO build-time dependency on Epic's Experimental Mover.
// Runtime discovery of its Blueprint-callable API keeps the entire Orbit plugin optional-Mover-safe.
public class OrbitMover : ModuleRules
{
    public OrbitMover(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Orbit" });
    }
}
