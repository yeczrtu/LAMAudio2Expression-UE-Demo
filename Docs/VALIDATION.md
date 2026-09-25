# 検証結果 — 2026-09-24 / 顔デモ追記 2026-09-25

環境：Windows x64、UE 5.8.2 (`D:\Unreal\UE_5.8`)、Visual Studio 2022 / MSVC 14.44、Core i7-12700、RTX 3070、メモリ64 GB。以下はこの環境での実行結果です。

## Blueprint版の顔デモ（2026-09-25）

現在のソース版の `BP_FaceDemo`／`BP_FaceDemoHUD`／`BP_FaceDemoGameMode` は標準のActor／HUD／GameModeBaseを親とします。デモの解析・再生制御・画面表示はBPノードで実装しています。旧C++デモクラスへの参照はなく、44アセットの依存関係に `/Script/LAMDemo` と `/Script/LAMDemoEditor` がないことを確認しました。

| 検証 | 結果 |
|---|---|
| 3つのBPのコンパイル | エラー0、警告0。360ノード、実際の非同期解析ノード1件、C++デモ関数呼び出し0件 |
| Directional Light | マップと各実行環境で1つ |
| EditorのStandalone | 6音声すべて成功 |
| Win64 Development | Cook・パッケージ成功、6音声すべて成功 |
| Win64 Shipping | Cook・パッケージ成功、6音声すべて成功 |
| 操作経路 | BPで作成したHUDの当たり判定に座標を渡し、クリックイベント→音声選択→解析→再生→AnimGraphのjawOpen出力を確認 |
| 再生制御 | 一時停止・再開、ミュート、音量、Submix、推論間隔切り替え、フェード停止、リプレイ、自然終了のBPイベントが成功 |
| 既存テストへの遷移 | Developmentの再生制御・ルーティング・Concurrency・ゲーム停止の回帰テスト成功 |

マウスカーソルとクリックイベントがBPで有効化されることも検証しています。キーボードの実機入力とマイクの実機入力は今回の自動検証には含めていません。操作テストの実測秒数には初期化や他のビルドとの競合も含まれ、性能比較用ではありません。

[18件の実行結果](Validation/blueprint-demo-results.json)・[アセットの依存関係](Validation/face-demo-dependencies.json)を保存しています。以下のモデル・プラグイン検証には以前の版での結果も含まれます。公開済みv0.2.0のZIPはBP化以前の版です。

## モデル一致

固定窓、FP32、opset17、後処理前の3,328値をPyTorch基準値と比較しました。

| 入力 | Python ONNX Runtime | UE CPU | UE DirectML |
|---|---:|---:|---:|
| noise / style 0 | 7.2122e-6 | 7.2122e-6 | 1.7136e-6 |
| silence / style 0 | 1.7509e-7 | 1.70e-7 | 6.3e-8 |
| noise / style 11 | 4.2617e-6 | 4.232e-6 | 1.088e-6 |

いずれも最大絶対誤差1e-3以下です。モデルのリビジョン、形状、ONNX SHA-256は [model-manifest.json](model-manifest.json)。

## 自動実行

UE Automationの4テストは全件成功しました。

- `LAM.Audio.CookedDecoder`: 通常／ストリーミングの圧縮音声、16/44.1/48kHz、mono/stereo、デコード後の長さ。
- `LAM.Core.BoundariesAndCurves`: 52名の一意性、jawOpenの上流順、整数／端数の窓数、補間、無音抑制、リサンプル。
- `LAM.Model.Parity`: CPU/DirectML数値一致、GPUモデル不在時のCPU代替、両モデル不在のエラー、キャンセル済みジョブ。
- `LAM.Editor.PIEPlayback`: PIEで再生、停止、pause/resume、停止中のseek、Clip共有再生、32回連続キャンセル、Procedural SoundWave拒否、解析中Actor破棄。キャンセル後のCompleted通知がないこと。

Standaloneも確認しました。DevelopmentとShippingはそれぞれ10ケースをパッケージ内のデータで実行し、全件成功しました。

| ケース | 結果 |
|---|---|
| DirectML／CPU再生 | 解析、再生、pause/resume、seek、100ms停止フェード |
| Blueprint / AnimGraph | 実際の非同期BPノード、専用AnimNode、jawOpen、名前変換、倍率、オフセット、マスク、Alpha、停止復帰 |
| Live PCM | 48kHz連続入力、16kHzへの連続変換、表情提示、p95判定 |
| 0.08秒・16kHz mono | 1,280サンプル、3フレーム |
| 1秒・16kHz mono | 16,000サンプル、30フレーム |
| 2.137秒・44.1kHz stereo | 34,193サンプル、65フレーム |
| 6秒・48kHz mono | 96,000サンプル、180フレーム |
| 無音1秒 | 16,000サンプル、30フレーム |
| 5分・48kHz stereo | 4,800,000サンプル、9,000フレーム |

端数音声は元の整数サンプル数から16kHzへ切り上げ変換しています。音声の秒数表示をfloatで丸め直してフレーム数を求めていません。

CookはBink Audio、ForceInlineとLoadOnDemandを使用。パッケージ側で元WAVやRawPCMDataには依存しません。ShippingにはPython、UnrealEd、LAMAudio2ExpressionEditorの実行ファイル／DLLを含めていません。ネットワーク切断状態を別途再現したテストは未実施ですが、推論・音声読み込みに外部通信はありません。

## 性能測定

Shipping、DirectML、各ケース別プロセス、NullRHIでの測定です。時間はサンプルアプリのBeginPlay以降で、プロセス起動時間は含みません。

| 音声 | 推論＋後処理 | モデルロード／デコードを含む完了時間 |
|---|---:|---:|
| 0.08秒 | 1.261秒 | 1.586秒 |
| 1秒 | 1.008秒 | 1.297秒 |
| 2.137秒 | 1.066秒 | 1.519秒 |
| 6秒 | 1.284秒 | 1.688秒 |
| 5分 | 3.489秒 | 8.220秒 |

最初のNNEインスタンス作成を推論時間に含むため、短い音声でも約1秒を要しました。インスタンス作成後の固定窓は最新の同一プロセス内テストでCPU約50～57ms、DirectML約6.7ms。ただしドライバーの初期化、OSキャッシュ、別プロセスの負荷により変動します。

CPU強制で同じ5分音声を解析した別プロセス試験も成功し、9,000フレーム、推論＋後処理74.965秒、ロード／デコードを含め78.519秒でした。短い数値比較ケースの速度をそのまま長時間解析へ外挿できません。

ライブPCMのp95はDevelopment約120.06ms、Shipping約119.87msで、今回の12秒試験では333ms未満でした。タイミングには初回インスタンス作成と後処理を含み、キューで待つ時間とマイクデバイスの遅延は含みません。長時間・同時解析時の保証値ではありません。

Shippingプロセス全体のピークWorking Setは約1,748～1,775MiBでした。モデル、NNE、UE本体を含み、GPU専用メモリを分離計測した値ではありません。64MiBの結果キャッシュ予算とは別です。

ゲームスレッドはDevelopmentのCSV Profilerで480フレーム、60fps制限、NullRHI、6秒音声の解析・再生を記録しました。先頭20フレームを除いた460フレームの `Exclusive/GameThread/TickActors` は平均0.059ms、p95 0.079ms、最大0.127ms。これはサンプル内のActor/Component全体の値で、プラグイン単体の差分ではありません。NullRHIの `GameThreadTime` は0を返したため評価に使っていません。

## 顔デモ（2026-09-25、デモプロジェクトへの分離後）

Face52、設定済みAnimBP、JVNV F1音声6件はデモプロジェクトの `/Game/LAMFaceDemo` に配置しています。操作Actor/HUD/GameModeは `/Script/LAMDemo` に移し、以前のクラス参照をCoreRedirectsで読み替えてアセットを保存しました。40アセットのハード／ソフト参照を調べ、旧 `/LAMAudio2Expression/Demo` やDeveloper内の素材への依存がないことを確認しました。標準名のARKit 52 Morph Targetと14マテリアルの参照も確認しています。

UE 5.8.2 Editorビルド、Standalone表示、Win64 ShippingのBuild/Cook/Stageに成功しました。ShippingではBink Audio / LoadOnDemandの各音声を別プロセスで解析し、冒頭約3.1秒の再生中に実際のAnimBPのjawOpenを観測しました。

| 音声 | 解析フレーム数 | 観測したjawOpen最大値 | 照合時のカーブ差 |
|---|---:|---:|---:|
| F1_anger_regular_31 | 326 | 0.2770 | 0.000000 |
| F1_disgust_regular_38 | 417 | 0.2845 | 0.000000 |
| F1_fear_regular_23 | 312 | 0.3318 | 0.000000 |
| F1_happy_regular_38 | 362 | 0.2975 | 0.000000 |
| F1_sad_regular_10 | 504 | 0.1744 | 0.000000 |
| F1_surprise_regular_11 | 324 | 0.3309 | 0.000000 |

すべてDirectML、終了コード0。原音声の整数サンプル数から求めたフレーム数とも一致します。カーブ差は終了前の1時点でAnimBPとコンポーネントを比較した値で、音声出力デバイスとの実測同期誤差ではありません。テクスチャと口形状の表示をShippingのスクリーンショットで確認しました。発話全体のリップシンク品質の主観評価は別途必要です。

デモプロジェクト側のCC BY-SA 4.0全文、出典、変更説明、原音声6件がNonUFSでステージされることを確認しました。プラグイン側にはMIT/Apache文書のみをステージします。パッケージにはCook済みニューラルモデルを含み、外部Pythonを呼び出しません。

記録：[Shipping結果](Validation/face-demo-shipping.json)、[アセット依存関係](Validation/face-demo-dependencies.json)、[表示](face-demo.png)。再実行は `Tools/test_face_demo.ps1`。今回のローカル配布物は `Artifacts/Shipping/Windows` です。

## 確認を残している範囲

- 実マイクの取得、デバイス切断／オーバーフロー、長時間のライブ運転。
- 発話全体のリップシンク品質の主観評価、他のキャラクターへの適用。Face52とJVNV音声での表示・カーブ接続は上記で確認しました。
- 音声出力デバイスを含む実測同期誤差。コールバックと補間、pause/seekの動作は確認済みですが、外部計測による「1フレーム以内」の保証は未確定です。
- 実際のGPUデバイス喪失・ドライバーエラー。テストした代替経路はGPUモデルを利用できない場合です。
- 破損したCookチャンク、大量の同時キャラクター、長時間のキャッシュ圧迫／メモリリーク試験。

## 再実行と記録

`Tools/test.ps1` はUE Automation、`Tools/package.ps1` はCook・ビルド、`Tools/smoke.ps1` は各パッケージの10ケースとメモリ測定を行います。原ログは `Artifacts/Automation.log`、`Artifacts/Tests`、`Artifacts/*-smoke.json`。要約用の数値記録を [Validation](Validation) に保存しています。

表示確認のスクリーンショットは [demo.png](demo.png) です。


## 0.2 再生制御・ライブ間隔（2026-09-25）

UE 5.8.2、Win64 Development / ShippingをBuild・Cookし、Editor Standaloneとパッケージ内で実行しました。プラグインのモデル形状、SoundWave解析の1秒刻み、解析キャッシュキーは変更していません。

| 検証 | 結果 |
|---|---|
| UE Automation | 5件成功。デコーダー、境界/カーブ、PIE破棄とキャンセル、ライブ時刻計算、CPU/DirectML数値一致 |
| 追加テスト：Editor / Development / Shipping | 各5件成功。再生制御1件とライブ4条件 |
| 既存パッケージ回帰：Development / Shipping | 各10件成功。BP/AnimGraph、再生、ライブPCM、短音声〜5分音声 |
| Shipping顔デモ | JVNV 6音声すべて成功。表示、jawOpen、AnimBPと供給カーブの照合 |

再生テストは同じクリップを2コンポーネントから別々の主出力へ再生し、第三のサブミックスへ追加送信しています。バッファで観測したピークは約0.145137 / 0.072568 / 0.036284で、指定した1 / 0.5 / 0.25に対応します。主出力変更、追加送信解除、継承への復帰、共有SoundWaveを変更しないことも確認しました。音声スレッドの停止処理をバッファで確認してから測定します。

ミュート中の表情進行、PauseとSeek、Pause中のフェード保持、フェード停止、音声終端からのCompleted、Replaced、StopOldestによるInterrupted、PreventNewによるFailedを確認しました。Endedの重複検出、イベント内での次回再生、State Changed内でのPause、SoundWave内のConcurrency Overrides共有も含みます。3DのRootへの接続と減衰設定、ゲーム停止時の保持/UI再生、UI用SoundClassよりPlay When Game Pausedを優先する処理も確認しています。PIEテストではActor破棄時のBP終了通知が抑止されることを確認しました。

ライブは333→100→1000→33.3→500→333 msの順に実行中変更し、48kHz mono / 44.1kHz stereoのDirectML、16kHz monoのCPU、同じワーカーへ長音声解析を投入するCPU競合を試しました。フレーム計算の単体試験では1/3/10/15/30フレームの連続入力、間隔変更、48時間相当の整数サンプル位置を確認しています。描画時刻の非減少、遅延の2秒上限、Lagging通知、区間破棄後の有効な出力を確認しました。

0.2のP95はモデル/インスタンス初期化を分離し、結果到着遅延にはワーカー待ちを含めます。上記の初版のP95とは定義が異なります。追加テストは負荷時の機能確認で、通常描画時の性能保証ではありません。特にShippingのNullRHI試験では起動引数のフレーム制限が効かず、多数のTickが走ります。CPUのP95が指定間隔を超えた場合も全出力を停止せず、利用可能な表情を提示できることを確認しています。数値の詳細は以下のJSONに保存しました。

- [Automation](Validation/automation-0.2.json)
- [Editor追加テスト](Validation/Editor-playback-controls-0.2.json)
- [Development追加テスト](Validation/Development-playback-controls-0.2.json)
- [Shipping追加テスト](Validation/Shipping-playback-controls-0.2.json)
- [Development回帰](Validation/Development-smoke-0.2.json)、[Shipping回帰](Validation/Shipping-smoke-0.2.json)
- [顔デモ6音声](Validation/face-demo-0.2.json)

再実行はTools/test_playback_controls.ps1（Configuration: Editor / Development / Shipping）。追加の合成音声フィクスチャはsetupから生成します。Shippingで起動引数によるマップ指定が無視される場合も、デモのGameModeがテストフラグを検出してテストマップへ移動します。通常のデモ起動には影響しません。

実マイクの取得/切断、長時間運転、実ソケットへの追従、距離ごとの減衰量・定位の聴感、出力デバイスを含む外部計測での1フレーム以内の同期保証は未検証です。従来の「確認を残している範囲」も引き続き適用します。
