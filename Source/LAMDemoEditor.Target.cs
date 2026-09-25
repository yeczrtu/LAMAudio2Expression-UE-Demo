using UnrealBuildTool;
public class LAMDemoEditorTarget : TargetRules {
 public LAMDemoEditorTarget(TargetInfo Target) : base(Target) { Type = TargetType.Editor; DefaultBuildSettings = BuildSettingsVersion.V7; IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8; ExtraModuleNames.Add("LAMDemo"); }
}
