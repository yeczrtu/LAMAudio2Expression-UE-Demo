using UnrealBuildTool;
public class LAMAudio2ExpressionEditor : ModuleRules {
 public LAMAudio2ExpressionEditor(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","AnimGraph","BlueprintGraph","LAMAudio2Expression"});
  PrivateDependencyModuleNames.AddRange(new[]{"UnrealEd","NNE"});
 }
}
