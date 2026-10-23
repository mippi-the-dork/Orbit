using UnrealBuildTool;

public class OrbitEditor : ModuleRules
{
    public OrbitEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Orbit",
            "Projects",
            "SlateCore"
        });
    }
}
