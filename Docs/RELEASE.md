# モデル入りReleaseの作成

Releaseは2つのリポジトリの同じバージョンタグへ公開します。Git履歴にはモデルを追加しません。

## ビルド

1. 固定版モデルをsetupで生成するか、以前の検証済みモデルを使用します。`Docs/model-manifest.json` のONNX・uassetハッシュを照合します。
2. `RunUAT.bat BuildPlugin -Plugin=<plugin>/LAMAudio2Expression.uplugin -Package=<new-short-output-path> -TargetPlatforms=Win64` を実行します。Windowsの260文字制約を避け、出力先は短いパスにします。このコマンドは指定した出力フォルダーを空にするので、新規フォルダーを指定してください。
3. UEの `Build.bat LAMDemoEditor Win64 Development <project>/LAMDemo.uproject -NoSNDBS` でEditorモジュールを更新します。
4. `.work` 内にデモプロジェクトの配布用コピーを作ります。個人用Content、キャッシュ、Git情報は除外し、検証用音声と顔デモを保持します。`Content/Examples` と `Content/LAMDemo.umap` はコピーせず、元リポジトリの `Tools/prepare_release_examples.py` をUE Python commandletで実行して生成します。既存の編集済み接続例を上書きしないため、このスクリプトは新規の配布用コピーだけを受け付けます。生成後はモデルuassetを検証済みBuildPlugin出力からコピーし、ハッシュを維持します。
5. 配布用コピーを `BuildCookRun -build -cook -stage -pak -archive -prereqs` でShippingビルドします（引数例は `Tools/package.ps1`）。VC++ランタイムのインストーラーもステージします。
6. 既存の自動テスト・顔デモを確認し、検証結果を記録します。コードをコミットし、デモのサブモジュール参照を更新します。

## ZIP作成

```powershell
python Tools/assemble_release.py --plugin <BuildPlugin-output> --project <disposable-project-directory> --shipping <Shipping-archive>/Windows --output <new-output-directory>
```

出力先は新規ディレクトリに限ります。作成物は以下です。

- UE 5.8.2用プラグイン（Editor / Gameプリコンパイル成果物、ソース、変換済みモデル、ライセンス）
- 編集用デモプロジェクト（プラグイン・モデル・Editor DLL・顔・音声・生成済み例）
- Win64 Shippingデモ（Cook済みモデル・素材、実行ファイル、ランタイムインストーラー）
- `release-manifest.json` と `SHA256SUMS.txt`

プラグインのプリコンパイルに必要なIntermediateは保持します。Git管理情報、PDB、個人用Developersアセット、キャッシュ、ログ、Python環境は含めません。ZIPのCRCを全件検査し、モデル・配布物のSHA-256を記録します。

## 公開前確認

ZIPを別の短いディレクトリへ展開して、EditorのモデルCPU / DirectML数値比較、顔デモ、Shippingの再生制御を実行します。起動時の追加ダウンロードやPythonへの依存がないことを確認します。GitHubが自動生成するSource code ZIPはサブモジュールやモデルを含まないため、Release本文では添付ZIPを案内します。

`gh release create` で両リポジトリにdraftを作成し、それぞれのZIP、マニフェスト、チェックサムを添付します。タグのコミット・各ファイルのサイズとGitHub側SHA-256を確認してから公開します。公開済みZIPを差し替える場合は、原則として新しいバージョンを作成します。

音声・キャラクターの表記はデモの `Resources/Demo` に、モデルの出典とApache-2.0はプラグインにも同梱します。
