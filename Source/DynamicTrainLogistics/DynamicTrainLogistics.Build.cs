using UnrealBuildTool;

public class DynamicTrainLogistics : ModuleRules
{
    public DynamicTrainLogistics(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp17;
        bLegacyPublicIncludePaths = false;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "FactoryGame",
            "SML",
            "FicsitWiremod"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "InputCore",
            "Slate",
            "SlateCore",
            "UMG"
        });
    }
}
