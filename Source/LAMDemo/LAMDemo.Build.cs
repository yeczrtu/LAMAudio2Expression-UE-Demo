using UnrealBuildTool;
using System.IO;
public class LAMDemo : ModuleRules {
 public LAMDemo(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","LAMAudio2Expression","NNE","InputCore"});
  string ProjectRoot=Path.GetFullPath(Path.Combine(ModuleDirectory,"../.."));
  foreach (string Name in new[]{"LICENSE","THIRD_PARTY_NOTICES.md"})
   RuntimeDependencies.Add(Path.Combine(ProjectRoot,Name),StagedFileType.NonUFS);
  foreach (string Folder in new[]{"Resources/Demo","Licenses"})
   foreach (string FileName in Directory.GetFiles(Path.Combine(ProjectRoot,Folder),"*",SearchOption.AllDirectories))
    RuntimeDependencies.Add(FileName,StagedFileType.NonUFS);
 }
}
