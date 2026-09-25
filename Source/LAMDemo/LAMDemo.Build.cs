using UnrealBuildTool;
public class LAMDemo : ModuleRules {
 public LAMDemo(ReadOnlyTargetRules Target) : base(Target) { PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs; PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","LAMAudio2Expression","NNE","InputCore"}); }
}
