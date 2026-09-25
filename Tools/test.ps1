param([string]$Engine='D:\Unreal\UE_5.8')
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
& "$Engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$Root/LAMDemo.uproject" -unattended -nop4 -nosplash -nullrhi '-ExecCmds=Automation RunTests LAM.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$Root/Artifacts/Tests" "-abslog=$Root/Artifacts/Automation.log"
if ($LASTEXITCODE) { throw 'LAM automation tests failed' }
