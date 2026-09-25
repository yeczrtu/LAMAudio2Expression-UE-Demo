# プラグイン内の顔デモ

`/LAMAudio2Expression/Demo/Maps/LAM_FaceDemo` を開いてPlayしてください。
Content Browserの設定で「プラグインコンテンツを表示」を有効にします。
モデルアセット `/LAMAudio2Expression/Models/LAM_A2E` が必要です。

6種類のJVNV F1音声をクリックまたは1～6キーで選べます。Spaceは再生／一時停止、Rは先頭から再生です。選択直後は非同期解析を行い、完了すると顔と音声を同時に再生します。初回はモデルのロードと推論初期化に時間がかかります。

| 場所 | 内容 |
|---|---|
| `Demo/Maps/LAM_FaceDemo` | カメラ、照明、UIを含むデモマップ |
| `Demo/Blueprints/BP_FaceDemo` | 操作用デモActor。Samples配列で音声を変更可能 |
| `Demo/Blueprints/BP_AudioToFace` | ユーザーが設定済みの解析→再生BPを移植した接続例 |
| `Demo/Blueprints/ABP_Face52` | Face52用の設定済みAnimBP |
| `Demo/Character` | 52カーブ対応メッシュ、Skeleton、マテリアル、テクスチャ |
| `Demo/Audio` | 6音声のSoundWave |
| `Resources/Demo` | 出典、ライセンス適用範囲、原音声、ハッシュ |

デモはプラグインRuntime内のクラスだけを使用し、`/Game`、`/Game/Developers`、サンプルプロジェクトの `/Script/LAMDemo` に依存しません。移植元のアセットは元の場所に残しています。

別プロジェクトではプラグイン全体をコピーし、C++をビルドしてください。開いているエディタが古いDLLを読み込んでいる場合は、保存して終了後にビルド・再起動が必要です。パッケージする際はデモマップをMaps to Cook、`/LAMAudio2Expression/Models` をAdditional Asset Directories to Cookに追加してください。

音声と同期デモの映像表現は **CC BY-SA 4.0**。hinzkaモデルは作者の再配布許諾、ニューラルモデルはApache-2.0、自作プラグインコードはMIT（上流由来部分を除く）です。デモ動画の配布にも音声の帰属・継承条件が適用されます。詳細はプラグインに同梱した [出典・利用条件](../Plugins/LAMAudio2Expression/Resources/Demo/README.md) を参照してください。

`LAMAudio2Expression.Build.cs` はLICENSE、第三者表記、Licenses、Resources/DemoをNonUFSとしてステージします。音声原本と表記がCookされたpakの外にも残ります。

開発者向け：`Tools/build_demo_assets.py` はアセット移植用です。`.lam-demo-staging` のある独立したプロジェクトコピーでのみ動きます。利用者は実行する必要がありません。配布済みのDemoアセットをそのまま使用してください。

再作成する場合は、元のFace52・AnimBP・BP・Developer音声を含むプロジェクトを別フォルダーへコピーします。固定版の `VRoid_V110_Female_v1.1.3.vrm` を取得し、`python Tools/extract_demo_textures.py <VRMファイル> <入力フォルダー>` でPNGとマテリアル情報を抽出してください。コピーしたプロジェクト直下に `.lam-demo-staging` を作り、入力フォルダーの絶対パスをUTF-8で記入します。コピー先のEditorターゲットをビルド後、UnrealのPython commandletで `Tools/build_demo_assets.py` を実行します。元プロジェクトにはこのマーカーを作らないでください。

`Tools/test_plugin_demo.ps1 -Executable <ShippingまたはDevelopmentの実行ファイル>` は6音声を個別プロセスで実行し、AnimBPのjawOpen、レポート、スクリーンショットを確認します。結果は既定で `Artifacts/PluginDemoChecks` に出力されます。
