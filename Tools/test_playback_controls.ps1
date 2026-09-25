param([ValidateSet('Editor','Development','Shipping')][string]$Configuration='Development',
      [string]$Engine='D:\Unreal\UE_5.8')
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
$Prefix=@()
if ($Configuration -eq 'Editor') {
    $Exe="$Engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
    $Prefix=@("`"$Root/LAMDemo.uproject`"",'/Game/LAMDemo','-game')
} else {
    $Name=if ($Configuration -eq 'Shipping') {'LAMDemo-Win64-Shipping.exe'} else {'LAMDemo.exe'}
    $Exe="$Root/Artifacts/$Configuration/Windows/LAMDemo/Binaries/Win64/$Name"
    $Prefix=@('/Game/LAMDemo')
}
$Cases=@(
    @{Name='PlaybackControls'; Flags=@('-LAMPlaybackTest')},
    @{Name='LiveIntervals48k'; Flags=@('-LAMLiveIntervalTest')},
    @{Name='LiveIntervals44kStereo'; Flags=@('-LAMLiveIntervalTest','-LAMRate=44100','-LAMChannels=2')},
    @{Name='LiveIntervals16kCPU'; Flags=@('-LAMLiveIntervalTest','-LAMRate=16000','-LAMCPU')},
    @{Name='LiveQueueContentionCPU'; Flags=@('-LAMLiveIntervalTest','-LAMRate=16000','-LAMCPU','-LAMLateAnalysisTest')}
)
$Results=@()
foreach ($Case in $Cases) {
    $Report="$Root/Artifacts/$Configuration-$($Case.Name).txt"
    $Log="$Root/Artifacts/$Configuration-$($Case.Name).log"
    if (Test-Path -LiteralPath $Report) { Remove-Item -LiteralPath $Report }
    $Arguments=$Prefix+@('-nullrhi','-unattended','-ExecCmds="t.MaxFPS 60"',"-LAMReport=`"$Report`"","-abslog=`"$Log`"")+$Case.Flags
    $Watch=[Diagnostics.Stopwatch]::StartNew()
    $Process=Start-Process -FilePath $Exe -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    if (!$Process.WaitForExit(120000)) { $Process.Kill(); throw "Timeout: $($Case.Name)" }
    $Text=if (Test-Path -LiteralPath $Report) {Get-Content -LiteralPath $Report -Raw} else {'FAIL report missing'}
    $Results+=[pscustomobject]@{Case=$Case.Name; ExitCode=$Process.ExitCode; WallSeconds=[Math]::Round($Watch.Elapsed.TotalSeconds,3); Result=$Text.Trim()}
    $Results | ConvertTo-Json | Set-Content "$Root/Artifacts/$Configuration-playback-controls.json" -Encoding utf8
    Write-Output "$Configuration $($Case.Name): $Text"
    if (!$Text.StartsWith('PASS ')) { throw "Extended test failed: $($Case.Name)" }
}
