param([string]$Engine='D:\Unreal\UE_5.8',[ValidateSet('Development','Shipping')][string]$Configuration='Development')
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
& "$Engine/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun "-project=$Root/LAMDemo.uproject" -noP4 -platform=Win64 "-clientconfig=$Configuration" -build -cook -stage -pak -archive -prereqs "-archivedirectory=$Root/Artifacts/$Configuration" '-ubtargs=-NoSNDBS' -nodebuginfo -utf8output -unattended
if ($LASTEXITCODE) { throw 'Packaging failed' }
