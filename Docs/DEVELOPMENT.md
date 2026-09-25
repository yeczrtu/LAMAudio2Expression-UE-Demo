# ソースからのセットアップと検証

動作を試すだけなら [モデル入りRelease](https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo/releases/tag/v0.2.0) を利用できます。このページはモデルの再生成やC++の開発を行う方向けです。コマンドはリポジトリのルートで実行します。

## 新規cloneからの準備

Git履歴にはニューラルモデルを含めていません（モデル入りReleaseはREADMEを参照）。顔デモ用のメッシュ・音声・マップはこのプロジェクトの `Content/LAMFaceDemo` に同梱しています。UE 5.8.2、Visual Studio 2022 C++、Python 3.10、Gitを用意して次を実行してください。

```powershell
git clone --recurse-submodules https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo.git
cd LAMAudio2Expression-UE-Demo
./Tools/setup.ps1 -Engine D:\Unreal\UE_5.8
```

既存cloneでは `git submodule update --init --recursive` でプラグインを取得します。setupはモデルの取得・SHA-256照合・ONNX変換・数値比較・モデルとテスト用アセットの生成を行います。`/Game/LAMDemo` と `/Game/Audio` のテストデータを再生成するため、これらを編集した場合は先にバックアップしてください。顔デモは再生成しません。

旧版の通常フォルダーが残り、サブモジュールの初期化で「空ではない」と表示される場合は、新規cloneへ移行し、生成済みの `Plugins/LAMAudio2Expression/Content/Models` をコピーしてください。

## 再生成・テスト

```powershell
./Tools/setup.ps1 -Engine D:\Unreal\UE_5.8
./Tools/test.ps1 -Engine D:\Unreal\UE_5.8
./Tools/package.ps1 -Configuration Development
./Tools/package.ps1 -Configuration Shipping
./Tools/smoke.ps1 -Configuration Development
./Tools/smoke.ps1 -Configuration Shipping
./Tools/test_playback_controls.ps1 -Configuration Editor
./Tools/test_playback_controls.ps1 -Configuration Development
./Tools/test_playback_controls.ps1 -Configuration Shipping
```

setup は専用の `.work/venv` を使用し、固定リビジョンの上流コードとチェックポイントから ONNX を生成・検証して UE アセットに取り込みます。Python、PyTorch、ネットワークは開発時だけ必要です。配布するアプリには不要です。

数値検証と確認範囲は [検証結果](VALIDATION.md)、モデル情報は [モデル情報](model-manifest.json) を参照してください。

ローカルでビルドしたデモは `Artifacts/Shipping/Windows/LAMDemo.exe` です。配布する場合は `Windows` フォルダー全体を使用してください。setup後のプラグインには約384 MiBのモデルアセットが生成されます。Gitには含めず、固定リビジョンから再生成します。

## 配布物を作る

モデル入りZIPの作成手順は [リリース手順](RELEASE.md)、モデルの版管理とライセンスは [モデル運用方針](MODEL_MANAGEMENT.md) を参照してください。

追加テスト用の `inline_concurrency` SoundWaveは `Tools/build_playback_test_assets.py` で生成します。通常はsetupに含まれます。テスト用音声は合成波形です。
