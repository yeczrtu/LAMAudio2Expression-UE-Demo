param([string]$Engine='D:\Unreal\UE_5.8')
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
Push-Location $Root
try {
    if (!(Test-Path 'Plugins/LAMAudio2Expression/LAMAudio2Expression.uplugin')) {
        git submodule update --init --recursive
        if ($LASTEXITCODE) { throw 'Plugin submodule initialization failed' }
    }
    if (!(Test-Path '.work/venv/Scripts/python.exe')) { python -m venv .work/venv; if ($LASTEXITCODE) { throw 'venv failed' } }
    $Python=Join-Path $Root '.work/venv/Scripts/python.exe'
    & $Python -m pip install -r Tools/requirements.txt
    if ($LASTEXITCODE) { throw 'dependency installation failed' }
    if (!(Test-Path '.work/upstream/.git')) { git clone https://github.com/aigc3d/LAM_Audio2Expression.git .work/upstream }
    git -C .work/upstream checkout 02a703c3ea7d8e360eb43098eca85ee98a083529
    if ($LASTEXITCODE) { throw 'upstream checkout failed' }
    New-Item -ItemType Directory -Force .work/models | Out-Null
    if (!(Test-Path '.work/models/model.tar')) {
        Invoke-WebRequest 'https://huggingface.co/3DAIGC/LAM_audio2exp/resolve/0fe5f4dbb283ec7d9c01688681e6e4b6ac314858/LAM_audio2exp_streaming.tar' -OutFile '.work/models/model.tar'
    }
    $ExpectedArchiveHash='0877f419b1b7b6856479b743b71625ad478f97434404e91288f9f0d6d516b97a'
    if ((Get-FileHash .work/models/model.tar -Algorithm SHA256).Hash -ne $ExpectedArchiveHash) {
        throw 'Checkpoint SHA-256 mismatch. Verify the download before extracting or loading it.'
    }
    tar -xf .work/models/model.tar -C .work/models
    if ($LASTEXITCODE) { throw 'checkpoint extraction failed' }
    & $Python Tools/export_model.py --upstream .work/upstream --checkpoint .work/models/pretrained_models/lam_audio2exp_streaming.tar --output .work/export/LAM_A2E.onnx
    if ($LASTEXITCODE) { throw 'model export or parity check failed' }
    & $Python Tools/make_fixtures.py
    & "$Engine/Engine/Build/BatchFiles/Build.bat" LAMDemoEditor Win64 Development "-Project=$Root/LAMDemo.uproject" -WaitMutex -NoHotReloadFromIDE -NoSNDBS
    if ($LASTEXITCODE) { throw 'Editor build failed' }
    & "$Engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$Root/LAMDemo.uproject" -run=pythonscript "-script=$Root/Tools/import_assets.py" -unattended -nop4 -nosplash -nullrhi
    if ($LASTEXITCODE) { throw 'Asset import failed' }
} finally { Pop-Location }
