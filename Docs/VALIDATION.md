# 検証結果 — 2026-09-24 / 顔デモ追記 2026-09-25

環境：Windows x64、UE 5.8.2 (`D:\Unreal\UE_5.8`)、Visual Studio 2022 / MSVC 14.44、Core i7-12700、RTX 3070、メモリ64 GB。以下はこの環境での実行結果です。

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

## プラグイン内の顔デモ（2026-09-25）

Face52、設定済みAnimBP、JVNV F1音声6件をプラグインに移植しました。40アセットのハード／ソフト参照を調べ、`/Game` と `/Script/LAMDemo` への依存がないことを確認しました。メッシュには標準名のARKit 52 Morph Targetがすべて存在し、14マテリアルは保存・再起動後も参照を保持しています。

UE 5.8.2 Editorビルド、Standalone表示、Win64 ShippingのBuild/Cook/Stageに成功しました。ShippingではBink Audio / LoadOnDemandの各音声を別プロセスで解析し、冒頭約3.1秒の再生中に実際のAnimBPのjawOpenを観測しました。

| 音声 | 解析フレーム数 | 観測したjawOpen最大値 | 照合時のカーブ差 |
|---|---:|---:|---:|
| F1_anger_regular_31 | 326 | 0.2770 | 0.000000 |
| F1_disgust_regular_38 | 417 | 0.2845 | 0.000000 |
| F1_fear_regular_23 | 312 | 0.3318 | 0.000000 |
| F1_happy_regular_38 | 362 | 0.2975 | 0.000000 |
| F1_sad_regular_10 | 504 | 0.1745 | 0.000000 |
| F1_surprise_regular_11 | 324 | 0.3309 | 0.000000 |

すべてDirectML、終了コード0。原音声の整数サンプル数から求めたフレーム数とも一致します。カーブ差は終了前の1時点でAnimBPとコンポーネントを比較した値で、音声出力デバイスとの実測同期誤差ではありません。テクスチャと口形状の表示をShippingのスクリーンショットで確認しました。発話全体のリップシンク品質の主観評価は別途必要です。

Apache-2.0、CC BY-SA 4.0の全文、プラグインLICENSE、第三者表記、デモ出典、変更説明、原音声6件がNonUFSでステージされることを確認しました。パッケージにはCook済みニューラルモデルを含み、外部Pythonを呼び出しません。

記録：[Shipping結果](Validation/face-demo-shipping.json)、[アセット依存関係](Validation/face-demo-dependencies.json)、[表示](face-demo.png)。再実行は `Tools/test_plugin_demo.ps1`。今回のローカル配布物は `Artifacts/PluginDemo/Windows` です。

## 確認を残している範囲

- 実マイクの取得、デバイス切断／オーバーフロー、長時間のライブ運転。
- 発話全体のリップシンク品質の主観評価、他のキャラクターへの適用。Face52とJVNV音声での表示・カーブ接続は上記で確認しました。
- 音声出力デバイスを含む実測同期誤差。コールバックと補間、pause/seekの動作は確認済みですが、外部計測による「1フレーム以内」の保証は未確定です。
- 実際のGPUデバイス喪失・ドライバーエラー。テストした代替経路はGPUモデルを利用できない場合です。
- 破損したCookチャンク、大量の同時キャラクター、長時間のキャッシュ圧迫／メモリリーク試験。

## 再実行と記録

`Tools/test.ps1` はUE Automation、`Tools/package.ps1` はCook・ビルド、`Tools/smoke.ps1` は各パッケージの10ケースとメモリ測定を行います。原ログは `Artifacts/Automation.log`、`Artifacts/Tests`、`Artifacts/*-smoke.json`。要約用の数値記録を [Validation](Validation) に保存しています。

表示確認のスクリーンショットは [demo.png](demo.png) です。
