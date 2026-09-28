param([ValidateSet('Editor','Shipping')][string]$Configuration='Editor',
      [string]$Engine='D:\Unreal\UE_5.8', [switch]$SkipAutomation, [string]$OnlyCase='')
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
$Editor="$Engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
$Model='/LAMAudio2Expression/Models/Wav2ARKit_CPU.Wav2ARKit_CPU'
if ($Configuration -eq 'Editor' -and !$SkipAutomation) {
    & $Editor "$Root/LAMDemo.uproject" -unattended -nop4 -nosplash -nullrhi '-ExecCmds=Automation RunTests LAM.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$Root/Artifacts/Wav2ARKit-Automation" "-abslog=$Root/Artifacts/Wav2ARKit-Automation.log" -LAMWav2ARKitTests -LAMBakeSaveFixtures '-ini:Game:[/Script/LAMAudio2Expression.LAMSettings]:Model=/LAMAudio2Expression/Models/LAM_A2E.LAM_A2E'
    if ($LASTEXITCODE) { throw 'Wav2ARKit/legacy automation failed' }
}
if ($Configuration -eq 'Editor') {
    $Exe=$Editor
    $Prefix=@("`"$Root/LAMDemo.uproject`"",'/Game/LAMDemo','-game')
} else {
    $Exe="$Root/Artifacts/Shipping/Windows/LAMDemo/Binaries/Win64/LAMDemo-Win64-Shipping.exe"
    $Prefix=@('/Game/LAMDemo')
}
$Cases=@(
    @{Name='Playback'; Flags=@('-LAMTest')},
    @{Name='Blueprint'; Flags=@('-LAMTest','-LAMBlueprintTest')},
    # The legacy -LAMLiveTest requires P95 < 333ms; use the functional interval harness instead.
    @{Name='LivePCM48k'; Flags=@('-LAMLiveIntervalTest','-LAMRate=48000')},
    @{Name='LiveIntervals'; Flags=@('-LAMLiveIntervalTest','-LAMRate=16000')},
    @{Name='LiveIntervals44kStereo'; Flags=@('-LAMLiveIntervalTest','-LAMRate=44100','-LAMChannels=2')},
    @{Name='BakedPlayback'; Map='/Game/Examples/LAM_BakedTest'; Flags=@('-LAMPlaybackTest','-LAMBakedClip=/Game/Wav2ARKitTests/speech_stream_LAMClip.speech_stream_LAMClip')}
)
foreach ($Sound in @('short_inline','one_inline','one_plus_inline','fraction_stream','speech_stream','silence_inline')) {
    $Cases+=@{Name=$Sound; Flags=@('-LAMTest','-LAMAnalyzeOnly',"-LAMSound=/Game/Wav2ARKitTests/$Sound.$Sound")}
}
if ($OnlyCase) {
    $Cases=@($Cases | Where-Object Name -eq $OnlyCase)
    if (!$Cases.Count) { throw "Unknown test case: $OnlyCase" }
}
$SummarySuffix=if ($OnlyCase) {"-$OnlyCase"} else {''}
$Renamed=@()
$Results=@()
try {
    # Prove the packaged game is independent of the original ONNX/external weight paths.
    if ($Configuration -eq 'Shipping') {
        foreach ($Name in @('wav2arkit_cpu.onnx','wav2arkit_cpu.onnx.data')) {
            $Source=Join-Path $Root ".work/wav2arkit_cpu/$Name"
            if (!(Test-Path -LiteralPath $Source) -or (Test-Path -LiteralPath "$Source.test-hidden")) {
                throw "Cannot temporarily hide source model: $Source"
            }
            Rename-Item -LiteralPath $Source -NewName "$Name.test-hidden"
            $Renamed+=$Source
        }
    }
    foreach ($Case in $Cases) {
        $Report="$Root/Artifacts/Wav2ARKit-$Configuration-$($Case.Name).txt"
        $Log="$Root/Artifacts/Wav2ARKit-$Configuration-$($Case.Name).log"
        if (Test-Path -LiteralPath $Report) { Remove-Item -LiteralPath $Report }
        $CasePrefix=$Prefix.Clone()
        if ($Case.Map) { $MapIndex=if ($Configuration -eq 'Editor') {1} else {0}; $CasePrefix[$MapIndex]=$Case.Map }
        $Arguments=$CasePrefix+@('-nullrhi','-unattended','-ExecCmds="t.MaxFPS 60"',"-LAMReport=`"$Report`"","-abslog=`"$Log`"","-LAMModel=$Model",'-LAMCPU')+$Case.Flags
        $Watch=[Diagnostics.Stopwatch]::StartNew()
        $Process=Start-Process -FilePath $Exe -ArgumentList $Arguments -WindowStyle Hidden -PassThru
        if (!$Process.WaitForExit(120000)) { $Process.Kill(); throw "Timeout: $($Case.Name)" }
        $Process.WaitForExit()
        $Text=if (Test-Path -LiteralPath $Report) {Get-Content -LiteralPath $Report -Raw} else {'FAIL report missing'}
        $Results+=[pscustomobject]@{Case=$Case.Name; ExitCode=$Process.ExitCode; WallSeconds=[Math]::Round($Watch.Elapsed.TotalSeconds,3); SourceModelsHidden=($Configuration -eq 'Shipping'); Result=$Text.Trim()}
        $Results | ConvertTo-Json | Set-Content "$Root/Artifacts/Wav2ARKit-$Configuration$SummarySuffix.json" -Encoding utf8
        Write-Output "$Configuration $($Case.Name): $Text"
        if ($Process.ExitCode -ne 0 -or !$Text.StartsWith('PASS ')) { throw "Wav2ARKit test failed: $($Case.Name)" }
    }
} finally {
    foreach ($Source in $Renamed) { Rename-Item -LiteralPath "$Source.test-hidden" -NewName ([System.IO.Path]::GetFileName($Source)) }
}
