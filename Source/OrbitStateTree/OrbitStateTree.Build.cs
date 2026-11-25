using UnrealBuildTool;

// Small opt-in adapter layer: StateTree controls decisions, Orbit only handles movement.
public class OrbitStateTree : ModuleRules
{
    public OrbitStateTree(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "StateTreeModule", "Orbit" });
        PrivateDependencyModuleNames.AddRange(new string[] { "AIModule" });
    }
}
