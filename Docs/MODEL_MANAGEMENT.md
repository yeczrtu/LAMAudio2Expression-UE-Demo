# ライセンスと学習済みモデルの運用

## 公開範囲

独自のUE統合コード、ツール、ドキュメントはルートのMIT Licenseで公開します。上流由来の `LAMCore.cpp`、`LAMTypes.cpp`、`LAMLive.cpp` はApache-2.0として表記を保持します。モデルもApache-2.0であり、MITへ変更しません。プラグインをコピーする際はLICENSE、Licenses、THIRD_PARTY_NOTICES.mdも同梱してください。

2026-09-25に確認した根拠：

- [上流ソースのLICENSE（固定リビジョン）](https://github.com/aigc3d/LAM_Audio2Expression/blob/02a703c3ea7d8e360eb43098eca85ee98a083529/LICENSE)
- [モデルカード（固定リビジョン）](https://huggingface.co/3DAIGC/LAM_audio2exp/blob/0fe5f4dbb283ec7d9c01688681e6e4b6ac314858/README.md)：`license: apache-2.0`
- [Apache-2.0 第4条](https://www.apache.org/licenses/LICENSE-2.0)：再配布時のライセンス、帰属、変更表記の保持。

確認した固定ソースには独立したNOTICEファイルはありません。モデルカードには学習データの詳細や権利保証の説明はありません。この確認は配布者のライセンス表示に基づくもので、学習データ全体の権利調査を意味しません。UE本体や別途入手したキャラクターの権利はそれぞれの提供条件に従います。

## 採用する運用

1. Gitにはソース、変換スクリプト、固定リビジョン、SHA-256、比較検証結果を保存します。
2. チェックポイント、ONNX、ニューラルモデルuasset、生成した検証用音声はGitから除外します。顔デモ一式はデモプロジェクトの `Content/LAMFaceDemo` と `Resources/Demo` に含め、各素材のライセンスを保持します。プラグイン本体は別リポジトリのサブモジュールで、顔・音声・CC BY-SA素材を含みません。
3. 新規cloneでは `Tools/setup.ps1` を実行して、公式Hugging Faceの固定リビジョンから取得・検証・変換・インポートします。Pythonとネットワークはこの開発工程だけで使用します。
4. 更新時はモデルのリビジョンとチェックサムを変更し、PyTorch/ONNX/UEの数値一致とパッケージテストを再実行します。ソース更新とモデル更新を同一PRで追跡できるようにします。

公式チェックポイントarchiveのSHA-256は `0877f419b1b7b6856479b743b71625ad478f97434404e91288f9f0d6d516b97a`。setupは展開前に照合します。ONNXの記録済みSHA-256はmodel-manifest.jsonにあり、再生成したONNXのハッシュが環境差で変わった場合は黙って上書きせず、数値一致を含めてレビューしてください。

## 利用者向け配布を追加する場合

変換済みモデルが必要な利用者向けには、バージョン固定のGitHub Release添付ファイル、または専用Hugging Faceモデルリポジトリに分離する方法を推奨します。ソースのタグ、UE版、モデル版、SHA-256、Apache-2.0本文、変更説明、検証結果をセットで配布します。今回、変換済みモデルの再配布やRelease作成は行っていません。

Git LFSも選択肢ですが、毎回のcloneでモデルを取得する必要や容量・転送量の管理が生じます。このプロジェクトでは公式配布先を参照する方式を既定にします。通常のGitHub Gitでは100 MiB超のファイルがブロックされるため、約384 MiBのモデルuassetを直接コミットしません。[GitHubの大きいファイルの説明](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github)

ShippingアプリにはCook済みモデルを同梱できます。ソースリポジトリからモデルを除外しても、アプリ実行時のオフライン動作は変わりません。
