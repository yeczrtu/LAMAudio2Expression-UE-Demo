using UnrealBuildTool;
public class LAMDemoEditor : ModuleRules {
 public LAMDemoEditor(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PrivateDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","UnrealEd","BlueprintGraph","Kismet","KismetCompiler","InputCore","LAMAudio2Expression"});
 }
}
