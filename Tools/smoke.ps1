param([ValidateSet('Development','Shipping')][string]$Configuration='Development')
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
$Name=if ($Configuration -eq 'Shipping') {'LAMDemo-Win64-Shipping.exe'} else {'LAMDemo.exe'}
$Exe=Join-Path $Root "Artifacts/$Configuration/Windows/LAMDemo/Binaries/Win64/$Name"
if (!(Test-Path $Exe)) { throw "Package missing: $Exe" }
$Cases=@(
    @{Name='PlaybackGPU'; Flags=@()},
    @{Name='PlaybackCPU'; Flags=@('-LAMCPU')},
    @{Name='Blueprint'; Flags=@('-LAMBlueprintTest')},
    @{Name='LivePCM'; Flags=@('-LAMLiveTest')}
)
foreach ($Sound in @('short_inline','one_inline','fraction_stream','speech_stream','silence_inline','long_stream')) {
    $Cases+=@{Name=$Sound; Flags=@('-LAMAnalyzeOnly',"-LAMSound=/Game/Audio/$Sound.$Sound")}
}
$Results=@()
foreach ($Case in $Cases) {
    $Report=Join-Path $Root "Artifacts/$Configuration-$($Case.Name).txt"
    if (Test-Path $Report) { Remove-Item -LiteralPath $Report }
    $Args=@('/Game/LAMDemo','-nullrhi','-unattended','-LAMTest',"-LAMReport=`"$Report`"")+$Case.Flags
    $Watch=[Diagnostics.Stopwatch]::StartNew()
    $Process=Start-Process -FilePath $Exe -ArgumentList $Args -WindowStyle Hidden -PassThru
    $Peak=0L
    while (!$Process.HasExited) {
        $Process.Refresh()
        if (!$Process.HasExited) { $Peak=[Math]::Max($Peak,$Process.PeakWorkingSet64) }
        if ($Watch.Elapsed.TotalSeconds -gt 620) { $Process.Kill(); throw "Timeout: $($Case.Name)" }
        Start-Sleep -Milliseconds 100
    }
    $Process.WaitForExit()
    $Text=if (Test-Path $Report) {Get-Content $Report -Raw} else {'FAIL report missing'}
    $Results+=[pscustomobject]@{Case=$Case.Name; ExitCode=$Process.ExitCode; WallSeconds=[Math]::Round($Watch.Elapsed.TotalSeconds,3); PeakWorkingSetMiB=[Math]::Round($Peak/1MB,1); Result=$Text.Trim()}
    $Results | ConvertTo-Json | Set-Content (Join-Path $Root "Artifacts/$Configuration-smoke.json") -Encoding UTF8
    Write-Output "$Configuration $($Case.Name): $Text"
    if ($Process.ExitCode -ne 0 -or !$Text.StartsWith('PASS ')) {throw "Smoke test failed: $($Case.Name)"}
}
