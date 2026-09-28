# 検証結果 — 2026-09-24 / 顔デモ追記 2026-09-25 / 母音変換追記 2026-09-28

環境：Windows x64、UE 5.8.2 (`D:\Unreal\UE_5.8`)、Visual Studio 2022 / MSVC 14.44、Core i7-12700、RTX 3070、メモリ64 GB。以下はこの環境での実行結果です。

## 母音・Oculus互換Viseme（2026-09-28）

新規 `LAM.Viseme` 3テストが成功しました。代表口形状、閉口、補正、2048件のランダム入力、閾値を横断する連続性、無効入力・設定、Oculus順序・名前プリセット、実AnimNodeのAlpha/Weight・復帰・設定スナップショットを検証しています。最終コードでも3テストを再実行して成功しました。

既存5テストを含む `LAM.` 全8テストも成功（PIEPlaybackのみWASAPI raw mode非対応の環境警告1件）。EditorビルドとShippingのBuild/Cook/Stage/Archiveが成功しました。

Face52の既存 `Fcl_MTH_A/I/U/E/O` と専用AnimBPで、Editor・Shippingそれぞれ6音声が成功しました。先頭音声では一時停止中のBlueprint変換値とAnimGraph出力の一致、フェード停止後の母音0、再開・リプレイ・シーク・自然終了を確認。全音声で母音合計の上限とARKit口カーブの二重適用がないことを確認しました。従来のARKit出力もShippingの先頭音声で回帰確認しています。

中立と5母音の代表入力を描画し、6枚の画像を目視確認しました。小開口の「い／う」は重み0.5、大開口の「あ／え／お」は1.0です。各モデルでの発音推定精度や、実マイクでの新規検証を意味しません。

[実行結果](Validation/viseme-results.json)・[使用方法と変換仕様](../Plugins/LAMAudio2Expression/Docs/VISEMES.md)。ローカルの画像は `Artifacts/VisemePoses`、音声再生結果は `Artifacts/VisemeDemo` と `Artifacts/VisemeShipping` に保存しています。

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

## Oculus15テンプレート逆算（2026-09-28）

UE 5.8.2 / Win64 / Core i7-12700 / RTX 3070で、OpenFaceFXとTalkingHeadの逆算を検証しました。既存5母音方式は維持しています。

| 検証 | 結果 |
|---|---|
| Runtime・既存回帰Automation 9件＋保存済みメッシュ検証1件 | すべて成功（既存PIE音声デバイス警告1件） |
| 独立したSciPy SLSQPとの174ケース比較 | 最大目的関数差 `7.36287828e-8`、基準`1e-5`以下 |
| 6音声 × 2方式 × Editor/Shipping | 24ケース成功 |
| 中立＋14形状 × 2方式 × Editor/Shipping | 60ケース成功 |
| 既存5母音・ARKit Shipping再生操作 | 各1ケース成功 |
| 保存後の生成モーフ28個とARKit配合の頂点差分 | 最大位置差分誤差0 cm、頂点順序も一致 |
| Shipping Build/Cook/Stage/Archive | 成功。両参照データ・MITライセンスの同梱を照合 |

Blueprint/AnimGraphの一致、Alpha/Weightの一度だけの適用、設定コピー、Pause/Seek、停止・入力消失、無効入力、観測マスク、倍率、範囲と連続性を検証しています。用意した数値ケースは単独ポーズ・混合・範囲外入力・一部観測欠落を含みます。両行列ともランク11で、係数そのものの一意な復元は受け入れ条件にしていません。

計算用行列を準備済みの変換関数を256ランダムフレームで測定した結果、Editor DevelopmentでOpenFaceFXの中央値23.8 µs / P95 61.7 µs、TalkingHeadの中央値57.3 µs / P95 109.0 µsでした。推論、設定準備、描画は含まず、他の環境の性能保証ではありません。

代表ポーズと6音声の静止画像を両ビルドで確認しました。Face52ではPPにも小さな口の隙間が残り、開口系の見た目が弱い一方、THの舌や丸め形状が強く見える場合があります。保存モーフの頂点差分は参照配合と一致しているため、これらは本モデルでの調整事項として扱います。参照用メッシュは完成したViseme素材ではありません。5母音のみのモデルには既存方式を使い、任意モデルでの発話の見た目は別途調整してください。静止画像による確認は、全遷移の知覚品質を保証するものではありません。

[詳細結果](Validation/oculus-viseme-results.json)に各ケース、性能、固定リビジョン、画像比較の情報を記録しています。再現方法は[Visemeガイド](../Plugins/LAMAudio2Expression/Docs/VISEMES.md)を参照してください。


## Wav2ARKit CPU（2026-09-28）

UE 5.8.2 / Win64 / Core i7-12700で、公開ONNXと外部重みを変更せずNNEへ取り込みました。共通推論処理に1入力モデルと動的出力形状の検証を追加し、既存の窓分割・Clip・ライブ・AnimGraphを使用します。

| 検証 | 結果 |
|---|---|
| Editorビルド・既存回帰を含むAutomation | 14件成功。従来LAMのCPU／DirectML数値比較も成功 |
| Wav2ARKitとPython ONNX Runtimeの数値比較 | 無音・固定乱数・実発話、初回／連続の6ケース。最大絶対誤差2.09e-7（基準1e-3） |
| 入出力検証 | 名前・型・形状の不一致、実行後に判明する65フレーム出力、非有限値を拒否 |
| Styleとキャッシュ | Style 0／11の出力一致。LAM→Wav2ARKit→LAMでキャッシュを分離 |
| Editor／Shippingの機能検証 | 各12件成功。解析・BP／AnimGraph・再生操作・ライブ3条件・保存済みClip・音声境界6条件 |
| Shipping Build／Cook／Stage／Archive | 成功。取得元ONNXと重みを一時的に改名した状態で全12ケースを実行 |
| モデル取得・取り込み | 両ファイルのSHA-256照合、欠落・破損の拒否、検証後の原本復元を確認 |

ライブは48kHz mono、16kHz mono、44.1kHz stereoで333→100→1000→33.3→500→333msへ変更し、時刻の非減少、有効な出力、Failed状態にならないこと、提示遅延の2秒上限を検証しました。1秒＋1サンプルの音声は16,001サンプル／31フレームとして解析できています。保存済みClipの再生ではモデルをロードしないことも別プロセスで確認しました。

Editor単窓（34,133サンプル）の純推論は79.10〜104.01ms、インスタンス初期化は939.97〜1051.67msでした。Shipping NullRHIのライブ測定ではTickが非常に多く、P95は約280〜320msでした。これは通常描画時の性能保証ではありません。33.3msなど処理時間より短い間隔では区間の破棄が発生します。

初回Shippingでは従来のライブ速度テストがP95 336.44msで「333.333ms未満」を満たせず失敗しました（表情出力は有効）。今回の計画では速度を合否条件にしないため、Wav2ARKit用ランナーは既存のライブ間隔変更テストを使用し、機能と遅延上限を検証します。旧テストの基準は変更していません。初回の失敗記録も結果JSONへ保存しました。

Automationの警告は不正なBake入力を渡す試験、複製音声のDDC警告、既存PIE音声デバイスのraw-mode警告です。Wav2ARKitのGPU・Android、実マイクの長時間運用、発話全体の見た目の品質評価は対象外です。

[数値・全ケース・初回失敗の記録](Validation/wav2arkit-cpu-2026-09-28.json) · [導入と再検証](../Plugins/LAMAudio2Expression/Docs/WAV2ARKIT.md)
