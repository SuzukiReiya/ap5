using UnrealBuildTool;
using System.Collections.Generic;

public class Ap5Target : TargetRules
{
    public Ap5Target(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("Ap5");
    }
}
