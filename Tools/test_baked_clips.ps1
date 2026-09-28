param([string]$Engine='D:\Unreal\UE_5.8')
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
$Editor="$Engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
& $Editor "$Root/LAMDemo.uproject" -unattended -nop4 -nosplash -nullrhi '-ExecCmds=Automation RunTests LAM.Bake.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$Root/Artifacts/BakeTests" "-abslog=$Root/Artifacts/BakeTests.log" -LAMBakeSaveFixtures
if ($LASTEXITCODE) { throw 'Bake tests failed' }
& $Editor "$Root/LAMDemo.uproject" -run=pythonscript "-script=$Root/Tools/build_baked_examples.py" -unattended -nop4 -nosplash -nullrhi "-abslog=$Root/Artifacts/BakedExamples.log"
if ($LASTEXITCODE) { throw 'Baked example generation failed' }
& "$PSScriptRoot/test_playback_controls.ps1" -Engine $Engine -Configuration Editor -IncludeBaked
