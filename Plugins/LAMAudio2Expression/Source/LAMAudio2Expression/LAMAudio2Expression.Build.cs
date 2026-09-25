using UnrealBuildTool;
using System.IO;
public class LAMAudio2Expression : ModuleRules {
 public LAMAudio2Expression(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","AnimGraphRuntime","NNE","DeveloperSettings"});
  PrivateDependencyModuleNames.AddRange(new[]{"AudioMixer","AudioPlatformConfiguration","SignalProcessing","AudioCaptureCore","Projects","InputCore"});
  RuntimeDependencies.Add(Path.Combine(PluginDirectory, "LICENSE"), StagedFileType.NonUFS);
  RuntimeDependencies.Add(Path.Combine(PluginDirectory, "THIRD_PARTY_NOTICES.md"), StagedFileType.NonUFS);
  foreach (string DirectoryName in new[]{"Licenses", "Resources/Demo"}) {
   string Folder = Path.Combine(PluginDirectory, DirectoryName);
   if (Directory.Exists(Folder))
    foreach (string FileName in Directory.GetFiles(Folder, "*", SearchOption.AllDirectories))
     RuntimeDependencies.Add(FileName, StagedFileType.NonUFS);
  }
 }
}
