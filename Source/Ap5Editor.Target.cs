using UnrealBuildTool;
using System.Collections.Generic;

public class Ap5EditorTarget : TargetRules
{
    public Ap5EditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
        ExtraModuleNames.Add("Ap5");
    }
}
