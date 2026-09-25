# Third-party notices

LAM Audio2Expression upstream: https://github.com/aigc3d/LAM_Audio2Expression

Pinned source revision: `02a703c3ea7d8e360eb43098eca85ee98a083529`.
Upstream authors: the Alibaba 3DAIGC team and contributors. Apache License 2.0; see Licenses/Apache-2.0.txt.
The ARKit coefficient order and expression-processing concepts originate from this project.

Weights: https://huggingface.co/3DAIGC/LAM_audio2exp
Pinned revision: `0fe5f4dbb283ec7d9c01688681e6e4b6ac314858`. Repository metadata declares Apache-2.0.
The packaged ONNX model is converted from LAM_audio2exp_streaming.tar. Its digest and numerical parity results are recorded in Docs/model-manifest.json in the sample repository.

The model uses the upstream Wav2Vec2 implementation based on Hugging Face Transformers (Apache-2.0). Development tools use PyTorch (BSD-style license), ONNX (Apache-2.0), ONNX Runtime (MIT), NumPy and SciPy (BSD-style licenses). These Python packages are not distributed in the runtime plugin.

Runtime inference uses Unreal Engine's NNERuntimeORT plugin and its bundled ONNX Runtime / DirectML dependencies. Unreal Engine is supplied separately under Epic's license; this repository does not redistribute engine source or an independent inference DLL.

No FLAME meshes, Gaussian avatar renderer, or third-party avatar assets are included. Sample audio is synthesized by Tools/make_fixtures.py.

License scope: original integration code and tools are MIT (see LICENSE). The adapted LAMCore.cpp, LAMTypes.cpp and LAMLive.cpp retain Apache-2.0. They port the coefficient order and processing to UE C++, replace chunk-state handling with integer timelines, add selectable postprocessing and deterministic blinking, and integrate CPU/DirectML inference. Upstream weights retain Apache-2.0 and are not relicensed under MIT. No weights or generated model assets are tracked in Git.
