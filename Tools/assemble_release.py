"""Assemble tested UE build outputs; Python is a maintainer-only dependency.

BuildPlugin output must contain Editor, UnrealGame Development and Shipping builds.
BuildCookRun must include -prereqs. This script never uploads or changes Git history.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args]).decode('utf-8').strip()


def files(folder):
    for p in sorted(folder.rglob('*')):
        if p.is_file():
            yield p, p.relative_to(folder).as_posix()


def write_archive(path, entries, extra):
    seen = set()
    with zipfile.ZipFile(path, 'x', zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as archive:
        for source, name in entries:
            if name in seen:
                continue
            assert not name.startswith('/') and '..' not in Path(name).parts
            assert '.git' not in Path(name).parts
            if source.suffix.lower() == '.ini':
                # AFS editor may persist a local token. Do not distribute credentials.
                for line in source.read_text(encoding='utf-8-sig').splitlines():
                    assert not line.strip().lower().startswith('securitytoken='), source
            archive.write(source, name)
            seen.add(name)
        for name, text in extra.items():
            assert name not in seen
            archive.writestr(name, text)
    with zipfile.ZipFile(path) as archive:
        bad = archive.testzip()
        assert bad is None, bad
    print(f'{path.name}: {path.stat().st_size / 1024**2:.1f} MiB', flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--plugin', type=Path, required=True)
    parser.add_argument('--shipping', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--engine', type=Path, default=Path('D:/Unreal/UE_5.8'))
    parser.add_argument('--project', type=Path, help='Disposable release project with regenerated examples; defaults to repository')
    parser.add_argument('--version', default='0.2.0')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    plugin = root / 'Plugins/LAMAudio2Expression'
    project = args.project.resolve() if args.project else root
    args.output.mkdir(parents=True, exist_ok=False)
    descriptor = json.loads((args.plugin / 'LAMAudio2Expression.uplugin').read_text(encoding='utf-8-sig'))
    assert descriptor['Installed'] and descriptor['VersionName'] == args.version
    model_manifest = json.loads((plugin/'Docs/model-manifest.json').read_text(encoding='utf-8-sig'))
    assert digest(args.plugin/'Content/Models/LAM_A2E.uasset') == model_manifest['unreal_asset']['sha256']
    for required in ['Binaries/Win64/UnrealEditor-LAMAudio2Expression.dll',
                     'Binaries/Win64/UnrealEditor-LAMAudio2ExpressionEditor.dll',
                     'LICENSE', 'Licenses/Apache-2.0.txt', 'THIRD_PARTY_NOTICES.md']:
        assert (args.plugin/required).is_file(), required
    for configuration in ['Development', 'Shipping']:
        assert list((args.plugin/'Intermediate/Build').glob(f'**/UnrealGame/{configuration}/LAMAudio2Expression/*.precompiled')), configuration
    assert (args.shipping/'LAMDemo.exe').is_file()
    assert (args.shipping/'Engine/Extras/Redist/en-us/vc_redist.x64.exe').is_file()
    # Refresh documentation after a documentation-only validation update.
    plugin_entries = {name:p for p,name in files(args.plugin)
                      if p.suffix.lower() != '.pdb' and 'HostProject' not in Path(name).parts}
    for folder in ['Docs', 'Licenses']:
        plugin_entries.update({folder+'/'+name:p for p,name in files(plugin/folder)})
    for name in ['README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md']:
        plugin_entries[name] = plugin/name
    version = args.version
    plugin_zip = args.output/f'LAMAudio2Expression-{version}-UE5.8.2-Win64-Model.zip'
    write_archive(plugin_zip, [(p,'LAMAudio2Expression/'+n) for n,p in plugin_entries.items()], {})
    project_entries = []
    for name in git(root, 'ls-files', '-z').split('\0'):
        p = root/name
        if p.is_file() and not name.startswith('Plugins/') and name not in ['.gitmodules', '.gitignore']:
            project_entries.append((p, 'LAMAudio2Expression-Demo/'+name))
    # Include generated examples and audio fixtures needed to run without setup.py.
    for folder in ['Content/Audio', 'Content/Examples']:
        project_entries.extend((p, 'LAMAudio2Expression-Demo/'+folder+'/'+n) for p,n in files(project/folder))
    project_entries.append((project/'Content/LAMDemo.umap', 'LAMAudio2Expression-Demo/Content/LAMDemo.umap'))
    for name in ['UnrealEditor-LAMDemo.dll', 'UnrealEditor-LAMDemoEditor.dll', 'UnrealEditor.modules']:
        project_entries.append((project/'Binaries/Win64'/name, 'LAMAudio2Expression-Demo/Binaries/Win64/'+name))
    project_entries.extend((p,'LAMAudio2Expression-Demo/Plugins/LAMAudio2Expression/'+n) for n,p in plugin_entries.items())
    project_zip = args.output/f'LAMAudio2Expression-{version}-UE5.8.2-Project-Model.zip'
    write_archive(project_zip, project_entries, {})
    demo_zip = args.output/f'LAMAudio2Expression-{version}-Win64-Demo.zip'
    demo_entries = [(p, 'LAMAudio2Expression-Demo/'+name) for p,name in files(args.shipping)
                    if p.suffix.lower() != '.pdb' and not name.startswith('Manifest_')
                    and 'Saved' not in Path(name).parts]
    demo_entries += [(plugin/'Docs/model-manifest.json','LAMAudio2Expression-Demo/model-manifest.json')]
    # Preserve the engine supplier's notice collection, including bundled ORT/DirectML.
    # Some notices cover engine components not used by this demo; do not relabel them.
    notice_root = args.engine/'Engine/Source/ThirdParty/Licenses'
    assert notice_root.is_dir()
    demo_entries.extend((p, 'LAMAudio2Expression-Demo/Engine/Licenses/'+name) for p,name in files(notice_root))
    readme = '''LAM Audio2Expression Demo 0.2.0 / Windows x64

ZIPを全て展開し、LAMDemo.exeを起動してください。UEエディタ・Python・ネットワークは不要です。
ランタイム不足の表示が出た場合: Engine/Extras/Redist/en-us/vc_redist.x64.exe を実行してください。
モデルの初回ロード・解析が完了してから再生します。

1〜6: 音声選択 / Space: 一時停止・再開 / R: 再生 / V: ミュート / +/-: 音量
O: 出力ルート / F: フェード停止 / M: マイク / I: ライブ推論間隔
終了: Alt+F4。マイクは実機での長時間運用を未検証です。

独自コード: MIT / 学習済みモデル・上流由来部分: Apache-2.0
音声とデモ演出: CC BY-SA 4.0 / キャラクター: hinzkaの個別利用条件
LAMDemo/THIRD_PARTY_NOTICES.md、LAMDemo/Resources/Demo/README.md、各Licensesを参照してください。
これらの表記と元WAVを保持してください。UEと同梱ランタイムは各提供元の条件に従います。
Engine/LicensesにはUE提供元の第三者ライセンス集を保持しています（未使用の構成要素の表記も含みます）。

LAM Audio2Expression Demo uses Unreal Engine. Unreal Engine is a trademark or registered
trademark of Epic Games, Inc. in the United States of America and elsewhere.
Unreal Engine, Copyright 1998 - 2026, Epic Games, Inc. All rights reserved.

https://github.com/yeczrtu/LAMAudio2Expression-UE-Demo/releases/tag/v0.2.0
'''
    write_archive(demo_zip, demo_entries, {'LAMAudio2Expression-Demo/README-FIRST.txt':readme})
    manifest = {'version':version, 'engine':'5.8.2', 'platform':'Win64',
                'plugin_commit':git(plugin,'rev-parse','HEAD'), 'demo_commit':git(root,'rev-parse','HEAD'),
                'model':model_manifest, 'assets':[]}
    for path in [plugin_zip, project_zip, demo_zip]:
        manifest['assets'].append({'file':path.name, 'bytes':path.stat().st_size, 'sha256':digest(path)})
    manifest_path = args.output/'release-manifest.json'
    manifest_path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    sums = [f"{a['sha256']}  {a['file']}" for a in manifest['assets']]
    sums.append(f'{digest(manifest_path)}  release-manifest.json')
    (args.output/'SHA256SUMS.txt').write_text('\n'.join(sums)+'\n',encoding='utf-8')


if __name__ == '__main__':
    main()
