using UnrealBuildTool;

public class MyProject11 : ModuleRules
{
    public MyProject11(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore",
            "Sockets", "Networking", "Json"
        });
    }
}