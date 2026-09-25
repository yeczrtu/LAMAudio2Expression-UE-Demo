using UnrealBuildTool;
public class LAMAudio2Expression : ModuleRules {
 public LAMAudio2Expression(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","AnimGraphRuntime","NNE","DeveloperSettings"});
  PrivateDependencyModuleNames.AddRange(new[]{"AudioMixer","AudioPlatformConfiguration","SignalProcessing","AudioCaptureCore","Projects"});
 }
}
