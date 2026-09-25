# LAM Audio2Expression UE Demo

**音声からキャラクターの口や表情を動かす、Unreal Engine用プラグインのデモです。**

ARKit対応のキャラクターと日本語音声6件を使い、音声に合わせて生成された表情を確認できます。学習済みモデルを含むWindows実行版を用意しているので、UEエディタなしでも試せます。

## デモ動画

https://github.com/user-attachments/assets/e93530a4-197d-4a56-860c-0890da1002e7

1分24秒・音声あり。6種類の音声を順番に再生した様子です。プレイヤーの再生ボタンから、このページ内で視聴できます。

動画・音声・デモ演出：CC BY-SA 4.0。キャラクター：hinzka / VRoid、音声：JVNV / litagin。[出典・利用条件](Docs/VIDEO.md)

## ダウンロード

最新版は **v0.2.0 / Windows x64** です。用途に合わせて選んでください。

| やりたいこと | ダウンロード | 必要なもの |
|---|---|---|
| まず動きを試す | [Windows実行版（約968 MiB）](https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo/releases/download/v0.2.0/LAMAudio2Expression-0.2.0-Win64-Demo.zip) | Windows PC |
| UEでデモを開いて編集する | [デモプロジェクト（約401 MiB）](https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo/releases/download/v0.2.0/LAMAudio2Expression-0.2.0-UE5.8.2-Project-Model.zip) | UE 5.8.2 |
| 自分のプロジェクトへ導入する | [プラグイン本体](https://github.com/yeczrtu/LAMAudio2Expression-UE#導入) | UE 5.8.2・対応キャラクター |

実行版とデモプロジェクトには、**プラグイン・学習済みモデル・キャラクター・音声が含まれています**。Python、モデル変換、モデルの追加ダウンロードは不要です。推論はPC内で行い、DirectML対応GPUを優先し、利用できない場合はCPUで動作します。

GitHubの「Source code (zip / tar.gz)」にはモデルとプラグインの実体が含まれません。上のダウンロードリンクを使用してください。[リリース詳細・チェックサム](https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo/releases/tag/v0.2.0)

## まず試す

1. Windows実行版のZIPを、`C:\LAMDemo` など短いパスへ**全て展開**します。
2. 展開先の `LAMDemo.exe` を起動します。
3. 左の音声ボタンをクリックするか、1〜6キーで音声を選びます。解析が完了すると、音声と表情が再生されます。

初回はモデルの読み込みと解析に時間がかかります。VC++ランタイム不足のメッセージが出た場合は、同梱の `Engine/Extras/Redist/en-us/vc_redist.x64.exe` を実行してください。

### 操作

| 操作 | キー |
|---|---|
| 音声を選ぶ | 1〜6、または左のボタン |
| 一時停止・再開 | Space |
| 先頭から再生 | R |
| ミュート | V |
| 音量を下げる・上げる | − / ＋ |
| 出力サブミックスを切り替える | O |
| フェードアウトして停止 | F |
| マイク入力を開始・停止 | M |
| ライブ推論間隔を切り替える | I（100 / 333 / 1000 ms） |
| 終了 | Alt + F4 |

マイクにはWindowsのマイクアクセス許可が必要です。マイク音声をスピーカーへ折り返す機能はありません。実マイクの長時間運用は未検証です。

## UEで開く

1. デモプロジェクトのZIPを全て展開します。
2. **UE 5.8.2** で `LAMDemo.uproject` を開きます。
3. `/Game/LAMFaceDemo/Maps/LAM_FaceDemo` を開き、Playを押します。

Blueprintの接続例は `Content/LAMFaceDemo/Blueprints` にあります。現在のソース版は、音声選択・解析・再生・画面表示をBPノードで実装しています。まず `BP_FaceDemo` の `02_Analyze_And_Play` を開くと、実際のデモで使う接続を確認できます。Directional Lightは1つです。[アセット構成とBPの読み方](Docs/FACE_DEMO.md)

公開済みのv0.2.0 ZIPと紹介動画は、このBP化以前の版です。BP化したデモは現在のソース版に含まれます。C++のプラグインや自動テストを再ビルドする場合は、Visual Studio 2022のC++開発環境が必要です。

## 自分のキャラクターで使う

[プラグイン本体の導入手順](https://github.com/yeczrtu/LAMAudio2Expression-UE#導入) に沿って、自分のプロジェクトへプラグインを追加してください。ARKit 52カーブに対応したキャラクターが必要です。カーブ名が異なる場合はプロファイルで対応付けできます。

基本の接続は `Analyze SoundWave Async` → `Play Expression Clip`、AnimBP側は `Apply LAM ARKit Curves` です。[Blueprint・表情設定・ライブ入力の詳細](Docs/USAGE.md)

## 詳しい情報

- [プラグインの再生制御・イベント・ライブ推論設定](https://github.com/yeczrtu/LAMAudio2Expression-UE/blob/main/Docs/PLAYBACK_AND_LIVE.md)
- [動作確認済みの範囲と検証結果](Docs/VALIDATION.md)
- [開発者向け：ソースからのセットアップ・ビルド・テスト](Docs/DEVELOPMENT.md)
- [問題を報告する](https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo/issues) — UEのバージョン、CPU/GPU、再現手順を添えてください。

## ライセンス・クレジット

| 対象 | ライセンス・提供元 |
|---|---|
| 独自コード | [MIT](LICENSE) |
| 上流由来コード・学習済みモデル | Apache-2.0 / [LAM Audio2Expression](https://github.com/aigc3d/LAM_Audio2Expression) |
| 音声・デモ演出・紹介動画 | CC BY-SA 4.0 / JVNV・litagin |
| キャラクター | hinzkaの利用条件 / VRoid |

素材を再利用・再配布する際は、[出典と利用条件](Resources/Demo/README.md)・[第三者表記](THIRD_PARTY_NOTICES.md)を確認してください。Unreal EngineにはEpic Gamesの利用条件が適用されます。
