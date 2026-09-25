# LAM Audio2Expression UE Demo

[![Release](https://img.shields.io/github/v/release/yeczrtu/LAMAudio2Expression-UE-Demo?style=flat-square&color=6366f1)](https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo/releases/latest)
![Unreal Engine 5.8.2](https://img.shields.io/badge/Unreal_Engine-5.8.2-313131?style=flat-square&logo=unrealengine)
![Windows x64](https://img.shields.io/badge/Windows-x64-0078d4?style=flat-square)
[![Code License MIT](https://img.shields.io/badge/Code_License-MIT-22c55e?style=flat-square)](LICENSE)

**音声を、キャラクターの表情に。** ARKit対応キャラクターと日本語音声6件で試せるUEデモです。モデル同梱・PC内で推論し、実行版はUEエディタやPythonなしで動作します。

**[ダウンロード](#ダウンロード)** · **[まず試す](#まず試す)** · **[Blueprintの接続例](Docs/FACE_DEMO.md)** · **[プラグイン本体](https://github.com/yeczrtu/LAMAudio2Expression-UE)**

## デモ動画

https://github.com/user-attachments/assets/e93530a4-197d-4a56-860c-0890da1002e7

6音声を順番に再生／1分24秒・音声あり。動画・音声・デモ演出：CC BY-SA 4.0。キャラクター：hinzka / VRoid、音声：JVNV / litagin。[出典・利用条件](Docs/VIDEO.md)

## ダウンロード

| 用途 | モデル入りZIP | 必要なもの |
|---|---|---|
| **すぐに試す** | [Windows実行版 · 約968 MiB](https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo/releases/download/v0.2.0/LAMAudio2Expression-0.2.0-Win64-Demo.zip) | Windows x64 |
| **UEで編集する** | [デモプロジェクト · 約401 MiB](https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo/releases/download/v0.2.0/LAMAudio2Expression-0.2.0-UE5.8.2-Project-Model.zip) | UE 5.8.2 |

どちらもプラグイン・モデル・キャラクター・音声を同梱。DirectML対応GPUを優先し、利用できない場合はCPUで動作します。[v0.2.0の詳細・チェックサム](https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo/releases/tag/v0.2.0)

> [!NOTE]
> GitHubの「Source code」ZIPにはモデルとプラグイン本体が含まれません。すぐに使う場合は上のZIPを選んでください。

## まず試す

1. Windows実行版を `C:\LAMDemo` など短いパスへ**全て展開**し、`LAMDemo.exe` を起動。
2. 左の音声ボタン、または **1〜6** キーで選択。解析後に音声と表情が再生されます。
3. **Space** で一時停止・再開、**R** で先頭から再生。

<details>
<summary>その他の操作・起動時の補足</summary>

| キー | 操作 |
|---|---|
| V / − / ＋ | ミュート / 音量を下げる / 上げる |
| O / F | 出力サブミックス切り替え / フェード停止 |
| M / I | マイク開始・停止 / ライブ推論間隔（100・333・1000 ms） |
| Alt + F4 | 終了 |

初回はモデルの読み込みに時間がかかります。VC++ランタイム不足の場合は同梱の `Engine/Extras/Redist/en-us/vc_redist.x64.exe` を実行してください。マイクはWindowsのアクセス許可が必要で、スピーカーへの折り返しはありません。実マイクの長時間運用は未検証です。

</details>

## UEで開く

デモプロジェクトを全て展開し、**UE 5.8.2** で `LAMDemo.uproject` → `/Game/LAMFaceDemo/Maps/LAM_FaceDemo` を開いて **Play**。

最新ソース版は解析・再生・UIを**Blueprintノードで実装**しています。`BP_FaceDemo` の `02_Analyze_And_Play` から読み始めてください。[BPの読み方](Docs/FACE_DEMO.md)

> [!NOTE]
> 公開済みのv0.2.0 ZIPと動画はBP化以前の版です。最新ソースのセットアップ・ビルドには[開発者向け手順](Docs/DEVELOPMENT.md)を参照してください。

## ドキュメント

[Blueprint・表情・ライブ入力](Docs/USAGE.md) · [再生制御・イベント](https://github.com/yeczrtu/LAMAudio2Expression-UE/blob/main/Docs/PLAYBACK_AND_LIVE.md) · [検証結果](Docs/VALIDATION.md) · [問題を報告](https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo/issues)

## ライセンス・クレジット

| 対象 | ライセンス・提供元 |
|---|---|
| 独自コード | [MIT](LICENSE) |
| 上流由来コード・学習済みモデル | Apache-2.0 / [LAM Audio2Expression](https://github.com/aigc3d/LAM_Audio2Expression) |
| 音声・デモ演出・紹介動画 | CC BY-SA 4.0 / JVNV・litagin |
| キャラクター | hinzkaの利用条件 / VRoid |

再利用・再配布は[素材の利用条件](Resources/Demo/README.md)と[第三者表記](THIRD_PARTY_NOTICES.md)を確認してください。Unreal EngineにはEpic Gamesの利用条件が適用されます。
