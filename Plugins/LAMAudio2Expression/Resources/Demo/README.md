# LAM Face Demo — attribution and usage

Open `/LAMAudio2Expression/Demo/Maps/LAM_FaceDemo` and press Play.
Click a voice sample or press 1–6. Space pauses/resumes; R replays.
The neural model `/LAMAudio2Expression/Models/LAM_A2E` is required.

## License scope

The included audio and our audiovisual demo arrangement (the demo map and its
audio-synchronized presentation) are distributed under **CC BY-SA 4.0**.
The standalone plugin C++ code remains MIT except for its documented Apache-2.0
portions. The neural network weights remain Apache-2.0. The original character
and textures retain their author's separate permission described below.

Redistributed recordings/videos of this audio-synchronized demonstration must
also observe CC BY-SA 4.0. Keep the attribution, license link and modification
notice. Do not apply DRM or contractual restrictions to the CC-licensed material.
The original selected WAV files are provided in this directory's `Audio/` folder
so that they remain available separately from Unreal's cooked assets.

License: https://creativecommons.org/licenses/by-sa/4.0/
Full text: `../../Licenses/CC-BY-SA-4.0.txt`.

## Voice samples — JVNV, F1

Original work: **JVNV: A Corpus of Japanese Emotional Speech with Verbal Content
and Nonverbal Expressions**.
Creators: **Detai Xin, Junfeng Jiang, Shinnosuke Takamichi, Yuki Saito,
Akiko Aizawa, Hiroshi Saruwatari**.

Original corpus and license:
https://sites.google.com/site/shinnosuketakamichi/research-topics/jvnv_corpus

Adaptation: **litagin**, `jvnv_corpus_v1_no_nv`, which removes the annotated
nonverbal intervals and their transcript text. This prior modification is retained.
https://huggingface.co/datasets/litagin/jvnv_corpus_v1_no_nv
Pinned revision: `0ca4908ee5b9610bc3b739e1e67dff6120df0675`.

Selected files (all from `F1/`):

- F1_anger_regular_31.wav
- F1_disgust_regular_38.wav
- F1_fear_regular_23.wav
- F1_happy_regular_38.wav
- F1_sad_regular_10.wav
- F1_surprise_regular_11.wav

Our changes: imported as Unreal SoundWave assets, encoded with Bink Audio for
runtime playback, and analyzed to generate synchronized facial animation. The
separately included WAV files are unchanged downloads from the pinned adaptation.
Attribution identifies provenance; the creators do not endorse this plugin.

## Character and textures

**hinzka — 52blendshapes-for-VRoid-face**, based on VRoid Studio / pixiv.
https://github.com/hinzka/52blendshapes-for-VRoid-face
Permission source, pinned:
https://github.com/hinzka/52blendshapes-for-VRoid-face/blob/756f5abab7d2295ad5b5dbc2cd86972c388c48d2/README.md

The README updated on 2026-09-18 expressly permits personal/commercial use,
modification, redistribution and sale, without individual permission or mandatory
credit. This demo retains voluntary attribution and that source link.

The old VRM binary metadata still contains `Redistribution_Prohibited`; the newer
author-published README explicitly grants redistribution of these models and
modified data. This distribution relies on that later express permission, and
records the discrepancy rather than relabeling the original model as MIT/CC0.

Our changes: the user's FBX-converted `Face52` mesh and configured animation
Blueprint were copied into the plugin; references were remapped. The female
v1.1.3 model's textures were extracted and assigned by the FBX material slot names.
UE materials use masked, lit rendering instead of the original MToon shader.
The demo creates no avatar-authoring or model-export functionality.

No warranty is provided by the upstream model author. Applicable third-party
rights and VRoid terms remain in force:
https://vroid.pixiv.help/hc/ja/articles/4405813333657

See `sources.json` for fixed versions and SHA-256 hashes.
