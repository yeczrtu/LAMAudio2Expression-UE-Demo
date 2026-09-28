"""Download pinned, unmodified Wav2ARKit files; optionally make CPU test references."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
MODEL_DIR = ROOT / '.work/wav2arkit_cpu'
MANIFEST = ROOT / 'Plugins/LAMAudio2Expression/Docs/wav2arkit-model-manifest.json'


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def verify_files(directory=MODEL_DIR):
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    for name, expected in manifest['files'].items():
        path = directory / name
        if not path.is_file() or path.stat().st_size != expected['size'] or digest(path) != expected['sha256']:
            raise RuntimeError(f'Missing or corrupt Wav2ARKit file: {path}')
    return manifest


def download():
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    for name, expected in manifest['files'].items():
        path = MODEL_DIR / name
        if path.exists():
            if path.stat().st_size != expected['size'] or digest(path) != expected['sha256']:
                raise RuntimeError(f'Existing file fails SHA-256 verification: {path}')
            continue
        url = f"https://huggingface.co/{manifest['repository']}/resolve/{manifest['revision']}/{name}"
        part = path.with_name(path.name + '.part')
        print(f'Downloading {name}', flush=True)
        with urllib.request.urlopen(url, timeout=120) as response, part.open('wb') as output:
            shutil.copyfileobj(response, output)
        if part.stat().st_size != expected['size'] or digest(part) != expected['sha256']:
            raise RuntimeError(f'Download fails SHA-256 verification: {part}')
        part.replace(path)
    verify_files()


def make_fixtures():
    import math
    import time
    import numpy as np
    import onnx
    from onnx import TensorProto, helper, numpy_helper
    import onnxruntime as ort
    from scipy.io import wavfile
    from scipy.signal import resample_poly

    verify_files()
    folder = MODEL_DIR / 'fixtures'
    folder.mkdir(exist_ok=True)
    speech = ROOT / 'Resources/Demo/Audio/F1_happy_regular_38.wav'
    rate, raw = wavfile.read(speech)
    if np.issubdtype(raw.dtype, np.signedinteger):
        audio = raw.astype(np.float32) / float(2 ** (raw.dtype.itemsize * 8 - 1))
    elif np.issubdtype(raw.dtype, np.floating):
        audio = raw.astype(np.float32)
    else:
        raise RuntimeError(f'Unsupported speech fixture dtype: {raw.dtype}')
    if audio.ndim == 2:
        audio = audio.mean(axis=1)
    factor = math.gcd(rate, 16000)
    audio = resample_poly(audio, 16000 // factor, rate // factor).astype(np.float32)
    window = np.zeros((1, 34133), dtype=np.float32)
    segment = audio[16000:16000 + 34133]
    window[0, :len(segment)] = segment
    cases = {'silence': np.zeros_like(window),
             'noise': np.random.default_rng(42).normal(0, .1, window.shape).astype(np.float32),
             'speech': window}
    started = time.perf_counter()
    session = ort.InferenceSession(str(MODEL_DIR / 'wav2arkit_cpu.onnx'), providers=['CPUExecutionProvider'])
    report = {'onnxruntime': ort.__version__, 'initialization_ms': (time.perf_counter() - started) * 1000,
              'speech_source': str(speech.relative_to(ROOT)), 'speech_sha256': digest(speech), 'cases': []}
    for name, data in cases.items():
        started = time.perf_counter()
        result = session.run(None, {'audio_waveform': data})[0]
        assert result.shape == (1, 64, 52) and np.isfinite(result).all(), (name, result.shape)
        report['cases'].append({'name': name, 'inference_ms': (time.perf_counter() - started) * 1000})
        data.tofile(folder / (name + '.audio.f32'))
        result.tofile(folder / (name + '.expected.f32'))

    # Small invalid models exercise rejection, including dynamic output after RunSync.
    for name, output_name, dtype, shape, value, dynamic in [
        ('bad_name', 'wrong_output', TensorProto.FLOAT, [1, 64, 52], 0., False),
        ('bad_type', 'blendshapes', TensorProto.INT64, [1, 64, 52], 0, False),
        ('bad_shape', 'blendshapes', TensorProto.FLOAT, [1, 52, 64], 0., False),
        ('bad_dynamic', 'blendshapes', TensorProto.FLOAT, [1, 65, 52], 0., True),
        ('nonfinite', 'blendshapes', TensorProto.FLOAT, [1, 64, 52], float('nan'), False),
    ]:
        data = np.full(shape, value, dtype=np.int64 if dtype == TensorProto.INT64 else np.float32)
        node = helper.make_node('Constant', [], [output_name], value=numpy_helper.from_array(data))
        graph = helper.make_graph([node], name,
            [helper.make_tensor_value_info('audio_waveform', TensorProto.FLOAT, [1, 34133])],
            [helper.make_tensor_value_info(output_name, dtype, [1, 'frames', 52] if dynamic else shape)])
        if dynamic:
            # Frame count depends on the symbolic audio length, so it is only known after execution.
            nodes = [helper.make_node('Shape', ['audio_waveform'], ['audio_shape']),
                     helper.make_node('Gather', ['audio_shape', 'index'], ['samples']),
                     helper.make_node('Sub', ['samples', 'offset'], ['frames']),
                     helper.make_node('Concat', ['batch', 'frames', 'curves'], ['output_shape'], axis=0),
                     helper.make_node('ConstantOfShape', ['output_shape'], [output_name])]
            initializers = [numpy_helper.from_array(np.array(v, dtype=np.int64), n) for n, v in
                            [('index', [1]), ('offset', [34068]), ('batch', [1]), ('curves', [52])]]
            graph = helper.make_graph(nodes, name,
                [helper.make_tensor_value_info('audio_waveform', TensorProto.FLOAT, [1, 'samples'])],
                [helper.make_tensor_value_info(output_name, dtype, [1, 'frames', 52])], initializers)
        model = helper.make_model(graph, opset_imports=[helper.make_opsetid('', 18)], ir_version=10)
        onnx.checker.check_model(model)
        onnx.save(model, folder / (name + '.onnx'))
    # Audio assets for independent bake and packaged tests; existing fixtures are not overwritten.
    for name in ('short_inline', 'one_inline', 'fraction_stream', 'speech_stream', 'silence_inline', 'inline_concurrency'):
        source = ROOT / '.work/fixtures' / (('speech_stream' if name == 'inline_concurrency' else name) + '.wav')
        if not source.exists():
            raise RuntimeError(f'Run Tools/make_fixtures.py first: {source}')
        shutil.copyfile(source, folder / (name + '.wav'))
    wavfile.write(folder / 'one_plus_inline.wav', 16000, np.zeros(16001, dtype=np.int16))
    (folder / 'reference-report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--fixtures', action='store_true', help='Requires Tools/requirements.txt and existing demo fixtures')
    args = parser.parse_args()
    if args.verify_only:
        verify_files()
    else:
        download()
    if args.fixtures:
        make_fixtures()
    print('Wav2ARKit files verified. Run Tools/import_wav2arkit.py in UE to import.')


if __name__ == '__main__':
    main()
