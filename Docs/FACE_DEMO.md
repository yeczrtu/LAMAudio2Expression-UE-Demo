# デモプロジェクトの顔デモ

`LAMDemo.uproject` で `/Game/LAMFaceDemo/Maps/LAM_FaceDemo` を開いてPlayしてください。
モデルアセット `/LAMAudio2Expression/Models/LAM_A2E` が必要です。

6種類のJVNV F1音声をクリックまたは1～6キーで選べます。Spaceは再生／一時停止、Rは先頭から再生です。選択直後は非同期解析を行い、完了すると顔と音声を同時に再生します。初回はモデルのロードと推論初期化に時間がかかります。

| 場所 | 内容 |
|---|---|
| `Content/LAMFaceDemo/Maps/LAM_FaceDemo` | カメラ、照明、UIを含むデモマップ |
| `Content/LAMFaceDemo/Blueprints/BP_FaceDemo` | Actorを親とするBP。解析・再生・操作をノードで実装。Samples配列で音声を変更可能 |
| `Content/LAMFaceDemo/Blueprints/BP_FaceDemoHUD` | HUDを親とするBP。画面表示とクリック処理。DrawButton／DrawLabel／DrawPanelもBP関数 |
| `Content/LAMFaceDemo/Blueprints/BP_FaceDemoGameMode` | GameModeBaseを親とするBP。デモHUDを指定 |
| `Content/LAMFaceDemo/Blueprints/BP_AudioToFace` | 設定済みの解析→再生BPを移植した接続例 |
| `Content/LAMFaceDemo/Blueprints/ABP_Face52` | Face52用の設定済みAnimBP |
| `Content/LAMFaceDemo/Character` | 52カーブ対応メッシュ、Skeleton、マテリアル、テクスチャ |
| `Content/LAMFaceDemo/Audio` | 6音声のSoundWaveと切り替え用Submix 2件 |
| `Resources/Demo` | 出典、ライセンス適用範囲、原音声、ハッシュ |

顔・音声・マップ・操作UIはこのデモプロジェクトに属します。独立した [プラグインリポジトリ](https://github.com/yeczrtu/LAMAudio2Expression-UE) を `Plugins/LAMAudio2Expression` のサブモジュールとして使用します。プラグインだけを導入した場合、デモのCC BY-SA素材は含まれません。

デモを別プロジェクトに移す場合は、プラグインを導入し、Content BrowserのMigrateで `Content/LAMFaceDemo` を移してください。デモ用C++クラスは不要です。素材の出典・ライセンスも保持してください。通常の利用ではプラグインだけをコピーし、自分のキャラクターと音声を設定してください。デモをパッケージする際はデモマップをMaps to Cook、`/LAMAudio2Expression/Models` をAdditional Asset Directories to Cookに追加します。

## Blueprintの読み方

まず `BP_FaceDemo` の `02_Analyze_And_Play` を開いてください。`SelectSample` → 前の処理のキャンセル → `Analyze SoundWave Async` → `Completed` → `Play Expression Clip` が実際のデモで使用する接続です。`Progress`・`Failed`・`Cancelled` も接続済みです。

| グラフ | 確認できる処理 |
|---|---|
| `01_Setup` | カメラ、入力の有効化、コンポーネントの更新順、初回の音声選択 |
| `02_Analyze_And_Play` | 音声選択、非同期解析、進捗・失敗表示、完了後の再生 |
| `03_Playback_Controls` | 一時停止・再開、リプレイ、フェード停止、ミュート、音量、出力Submix |
| `04_Microphone_And_Interval` | マイク開始・停止、100／333.3／1000 msの推論間隔切り替え |
| `05_Status_Events` | 自然終了、再生エラー、状態通知 |
| `06_Keyboard` | キー入力から上記のBPイベントを呼ぶ接続 |

画面ボタンは `BP_FaceDemoHUD` の `03_Button_Clicks` から同じBPイベントを呼びます。表示の入口は `02_Draw_Interface`、ボタンの表示名・配置は `04_Draw_Voice_Buttons` と `05_Draw_Playback_Buttons`、状態表示は `06_Draw_Status` です。描画用BP関数を開くと標準Canvasノードも確認できます。表情の適用は `ABP_Face52` のAnimGraphにある `Apply LAM ARKit Curves` で行います。

マップのDirectional Lightは**1つ**です。World SettingsのGameMode Overrideは `BP_FaceDemoGameMode` を指定しています。

## 開発者向け

生成済みのBPはそのまま編集・実行できます。`Source/LAMDemoEditor` はBPアセットを作成するエディタ専用コードで、デモ実行時の処理ではありません。通常のデモは標準UEノードとプラグインの公開ノードで動作します。`Source/LAMDemo` に残るC++は明示的なテストフラグを使う自動検証用です。

`Tools/rebuild_blueprint_demo.py` は3つのBPグラフとマップ上のデモActor・Directional Lightを再作成します。**編集したBPを上書きするため、通常の利用では実行しないでください。** 再生成時はEditorターゲットをビルドし、UEのPython commandletで実行します。

音声と同期デモの映像表現は **CC BY-SA 4.0**。hinzkaモデルは作者の再配布許諾、ニューラルモデルはApache-2.0、自作コードはMIT（上流由来部分を除く）です。デモ動画の配布にも音声の帰属・継承条件が適用されます。詳細はデモ側の [出典・利用条件](../Resources/Demo/README.md) を参照してください。

デモの `LAMDemo.Build.cs` がプロジェクトのLICENSE、第三者表記、Licenses、Resources/DemoをNonUFSとしてステージします。プラグインは自身のMIT/Apache文書だけをステージします。

開発者向け：`Tools/build_demo_assets.py` はアセット移植用です。`.lam-demo-staging` のある独立したプロジェクトコピーでのみ動きます。利用者は実行する必要がありません。配布済みのDemoアセットをそのまま使用してください。

再作成する場合は、元のFace52・AnimBP・BP・Developer音声を含むプロジェクトを別フォルダーへコピーします。固定版の `VRoid_V110_Female_v1.1.3.vrm` を取得し、`python Tools/extract_demo_textures.py <VRMファイル> <入力フォルダー>` でPNGとマテリアル情報を抽出してください。コピーしたプロジェクト直下に `.lam-demo-staging` を作り、入力フォルダーの絶対パスをUTF-8で記入します。コピー先のEditorターゲットをビルド後、UnrealのPython commandletで `Tools/build_demo_assets.py` を実行します。元プロジェクトにはこのマーカーを作らないでください。

`Tools/test_face_demo.ps1 -Executable <ShippingまたはDevelopmentの実行ファイル>` は6音声を個別プロセスで実行し、BPのHUDクリックから解析・再生、AnimBPのjawOpen、レポート、スクリーンショットを確認します。先頭音声では再生制御と自然終了イベントも検証します。結果は既定で `Artifacts/FaceDemoChecks` に出力されます。
