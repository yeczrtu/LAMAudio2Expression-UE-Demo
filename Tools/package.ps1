param([string]$Engine='D:\Unreal\UE_5.8',[ValidateSet('Development','Shipping')][string]$Configuration='Development',
      [switch]$IncludeBaked, [switch]$SkipEditorBuild)
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
$Extra=@()
if ($IncludeBaked) {
    if (!(Test-Path "$Root/Content/Examples/LAM_BakedTest.umap")) { throw 'Run Tools/test_baked_clips.ps1 to generate baked fixtures first' }
    $Extra += '-map=/Game/LAMDemo+/Game/LAMFaceDemo/Maps/LAM_FaceDemo+/Game/Examples/LAM_BakedTest'
}
if ($SkipEditorBuild) { $Extra += '-nocompileeditor' }
& "$Engine/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun "-project=$Root/LAMDemo.uproject" -noP4 -platform=Win64 "-clientconfig=$Configuration" -build -cook -stage -pak -archive -prereqs "-archivedirectory=$Root/Artifacts/$Configuration" '-ubtargs=-NoSNDBS -NoHotReloadFromIDE' -nodebuginfo -utf8output -unattended @Extra
if ($LASTEXITCODE) { throw 'Packaging failed' }
