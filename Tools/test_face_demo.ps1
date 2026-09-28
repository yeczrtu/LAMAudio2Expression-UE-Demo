param(
    [string]$Executable,
    [string]$OutputDirectory,
    [ValidateSet('Editor','Development','Shipping')][string]$Configuration='Shipping',
    [string]$Engine='D:\Unreal\UE_5.8',
    [switch]$Visemes,
    [switch]$RepresentativePoses,
    [ValidateSet('OpenFaceFX','TalkingHead')][string]$VisemeTemplate
)
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
if (!$Executable) {
    if ($Configuration -eq 'Editor') { $Executable=Join-Path $Engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe' }
    else {
        $Binary=if ($Configuration -eq 'Shipping') { 'LAMDemo-Win64-Shipping.exe' } else { 'LAMDemo.exe' }
        $Executable=Join-Path $Root "Artifacts/$Configuration/Windows/LAMDemo/Binaries/Win64/$Binary"
    }
}
if (!$OutputDirectory) { $OutputDirectory=Join-Path $Root 'Artifacts/FaceDemoChecks' }
if (!(Test-Path -LiteralPath $Executable)) { throw "Package missing: $Executable" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory=(Resolve-Path -LiteralPath $OutputDirectory).Path
$Results=@()
$Count=if ($RepresentativePoses -and $VisemeTemplate) { 15 } else { 6 }
for ($Index=0; $Index -lt $Count; $Index++) {
    $Report=Join-Path $OutputDirectory "sample-$Index.txt"
    $Capture=Join-Path $OutputDirectory "sample-$Index.png"
    if (Test-Path -LiteralPath $Report) { Remove-Item -LiteralPath $Report }
    $Arguments=@('/Game/LAMFaceDemo/Maps/LAM_FaceDemo','-RenderOffscreen','-windowed','-ResX=1366','-ResY=800',
        '-unattended','-LAMPluginDemoTest',"-LAMDemoSample=$Index","-LAMReport=`"$Report`"","-LAMCapture=`"$Capture`"")
    $Arguments += "-abslog=`"$(Join-Path $OutputDirectory "sample-$Index.log")`""
    if ($Visemes -or $RepresentativePoses -or $VisemeTemplate) { $Arguments += '-LAMVisemeDemoTest' }
    if ($VisemeTemplate) { $Arguments += "-LAMVisemeTemplate=$VisemeTemplate" }
    if ($RepresentativePoses) {
        $Arguments = $Arguments | Where-Object { $_ -notlike '-LAMDemoSample=*' }
        $Arguments += @('-LAMDemoSample=0',"-LAMVisemePose=$Index")
    }
    if ($Configuration -eq 'Editor') { $Arguments=@("`"$Root/LAMDemo.uproject`"",'-game')+$Arguments }
    $Watch=[Diagnostics.Stopwatch]::StartNew()
    $Process=Start-Process -FilePath $Executable -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    if (!$Process.WaitForExit(150000)) { $Process.Kill(); throw "Sample $Index timed out" }
    $Text=if (Test-Path -LiteralPath $Report) { (Get-Content -LiteralPath $Report -Raw).Trim() } else { 'FAIL report missing' }
    $Results+=[pscustomobject]@{Sample=$Index; ExitCode=$Process.ExitCode; WallSeconds=[Math]::Round($Watch.Elapsed.TotalSeconds,3); Result=$Text}
    $Results | ConvertTo-Json | Set-Content (Join-Path $OutputDirectory 'results.json') -Encoding utf8
    Write-Output $Text
    if ($Process.ExitCode -ne 0 -or !$Text.StartsWith('PASS ')) { throw "Demo failed: $Index" }
}
