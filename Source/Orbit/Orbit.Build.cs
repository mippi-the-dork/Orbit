// Copyright Mippithedork 2026, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Orbit : ModuleRules
{
    public Orbit(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings" });
        // Editor-only billboard sprite is loaded from the plugin's Resources directory.
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("Projects");
        }
    }
}
