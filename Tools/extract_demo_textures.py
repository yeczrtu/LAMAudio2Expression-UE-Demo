"""Extract unchanged embedded PNGs from the pinned hinzka female VRM (no dependencies).

Usage: python Tools/extract_demo_textures.py path/to/female.vrm path/to/staging-inputs
The output supplies materials-source.json and textures/ to build_demo_assets.py.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('vrm', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    manifest = json.loads((Path(__file__).resolve().parents[1] /
        'Resources/Demo/sources.json').read_text(encoding='utf-8'))
    data = args.vrm.read_bytes()
    if hashlib.sha256(data).hexdigest() != manifest['vrm_sha256']:
        raise ValueError('VRM does not match the pinned female v1.1.3 source')
    magic, version, size = struct.unpack_from('<4sII', data)
    if magic != b'glTF' or version != 2 or size != len(data):
        raise ValueError('Invalid GLB header')
    chunks = {}
    offset = 12
    while offset < size:
        length, kind = struct.unpack_from('<II', data, offset)
        offset += 8
        chunks[kind] = data[offset:offset + length]
        offset += length
    document = json.loads(chunks[0x4E4F534A])
    binary = chunks[0x004E4942]
    specs = []
    for material in document['materials']:
        pbr = material['pbrMetallicRoughness']
        source = document['textures'][pbr['baseColorTexture']['index']]['source']
        specs.append(dict(name=material['name'], texture=source, color=pbr.get('baseColorFactor', [1]*4),
                          alpha=material.get('alphaMode', 'OPAQUE'), cutoff=material.get('alphaCutoff', .5),
                          double_sided=material.get('doubleSided', False)))
    textures = args.output / 'textures'
    textures.mkdir(parents=True, exist_ok=True)
    for index in sorted({spec['texture'] for spec in specs}):
        image = document['images'][index]
        if image['mimeType'] != 'image/png':
            raise ValueError('Expected PNG texture')
        view = document['bufferViews'][image['bufferView']]
        start = view.get('byteOffset', 0)
        (textures / f'T_VRoid_{index:02d}.png').write_bytes(binary[start:start + view['byteLength']])
    (args.output / 'materials-source.json').write_text(json.dumps(specs, indent=2), encoding='utf-8')
    print(f'Extracted {len(specs)} material textures to {args.output}')


if __name__ == '__main__':
    main()
