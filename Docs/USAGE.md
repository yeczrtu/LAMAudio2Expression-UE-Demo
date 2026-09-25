# Blueprint・表情設定・ライブ入力

[デモの起動方法](../README.md) · [プラグインの導入](https://github.com/yeczrtu/LAMAudio2Expression-UE#導入)

通常のmono/stereo SoundWave、8〜192 kHz、最大300秒に対応します。SoundCue、MetaSound、Procedural SoundWave、外部WAV／MP3の直接読み込みは対象外です。

## Blueprint

```text
BeginPlay または任意のイベント
  → Analyze SoundWave Async(Component, SoundWave, Settings)
      Completed(Clip) → Component.Play Expression Clip(Clip, StartTime=0)
      Progress       → ロード表示を更新
      Failed         → Error を表示
      Cancelled      → ロード表示を終了
```

`Pause` / `Resume` / `Stop` / `Seek` は音声と表情を一緒に制御します。再生速度は1倍です。解析中の中断には `Cancel Analysis` を使用します。1コンポーネントで新しい解析を開始すると、そのコンポーネントの前の解析をキャンセルします。

`Get Current Expression Frame` には時刻、52値、Validity、適用 Weight が入ります。`Get ARKit Curve Value` は指定した名前の推定値を返します。停止フェードの強度は別の Weight です。解析結果を他の Actor で使う場合は Clip を BP の変数で保持してください。

## Animation Blueprint

```text
既存のポーズ → Apply LAM ARKit Curves → Output Pose
```

Source Component は対象コンポーネントを指定します。空欄の場合は SkeletalMesh の所有 Actor から検索します。所有 Actor に複数の LAM コンポーネントがある場合は明示的に指定してください。Alpha は0～1です。

`/Game/Examples/BP_LAMPlayback` は非同期解析→同期再生を配線済みの BP、`/Game/Examples/ABP_LAMCurves` は専用ノードを配線済みの AnimBP です。後者はエンジンのテスト用スケルトンでコンパイルされているため、ご自身の AnimBP へノード構成をコピーしてください。

標準の52カーブ名を出力するため、例えば `jawOpen` と同名の Morph Target を持つメッシュで動作します。表情ボーンを使うリグでは、出力カーブを既存のリグ／Control Rig の駆動に利用してください。このプラグインはカーブから独自のボーン配置を自動推定しません。

Data Asset の `LAMCurveProfile` で名前変換、無効化、倍率、オフセットを設定できます。指定しないカーブは標準名のまま有効です。値は補正後に0～1へ制限します。停止時は100 msで元のポーズに戻し、一時停止では表情を保持します。

## モデルと表情設定

- Style: 0～11、既定0。上流学習モデルの話者スタイル番号です。
- Smooth: 5フレームの Savitzky–Golay 平滑化と境界の補間。既定有効。
- Suppress Silent Mouth: RMS 0.001未満が7フレーム続く区間で口の動きを抑制。既定有効。
- Symmetrize / Auto Blink: 既定無効。瞬きは音声推定ではなく演出です。
- Blink Seed: 同じ解析結果を再現するための乱数シード。

約2.13秒の固定窓を1秒ずつ進めて推論します。全長一括推論ではなく、公式ストリーミング方式を基準にしています。後処理は全体の時系列に適用するため、上流デモのランダムな出力をそのまま再現するものではありません。

解析は専用の1ワーカースレッドで処理します。結果のメモリキャッシュは既定64 MiBです。再生中の Clip はキャッシュの追い出しで失われません。モデルメモリと利用者が保持する Clip は、このキャッシュ上限とは別です。

## マイク／PCM入力

`Start Microphone(Settings, DeviceIndex=-1)` は既定の録音デバイスを使用します。Windows のマイクアクセス許可が必要です。音声のスピーカーへの折り返しは行いません。

外部の音声ストリームは `Start PCM Stream` の後、ゲームスレッドから `Push PCM Audio(InterleavedPCM, SampleRate, Channels)` で渡せます。値域は -1～1 の float、mono/stereo、1コール最大2秒です。サンプルレートを変えるときはストリームを再開始してください。終了には `Stop Microphone` を使用します。

更新間隔は約33.3〜1000 ms（既定333.3 ms）で、実行中も `Set Live Inference Interval` から変更できます。提示遅延は既定750 msを下限に最大2秒まで自動調整します。`Get Live Metrics` と `On Live State Changed` で実行性能を確認できます。ライブ入力は初回モデルのロード時間を要するため、必要なら先に SoundWave を解析してアセットをロードしてください。音声解析ジョブとライブ推論は同じワーカーを使用するので、ライブ入力中の大量の解析は避けてください。

再生設定・イベントの詳細は [再生制御とライブAPI](https://github.com/yeczrtu/LAMAudio2Expression-UE/blob/main/Docs/PLAYBACK_AND_LIVE.md) を参照してください。
