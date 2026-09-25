"""Export the pinned upstream backbone without its training/UI dependencies."""
import argparse, hashlib, importlib.util, json, pathlib, sys, types
import numpy as np
import torch
import transformers
import onnxruntime as ort

REVISION = '02a703c3ea7d8e360eb43098eca85ee98a083529'

def load_backbone(root):
    for name in ['models', 'models.encoder', 'models.builder', 'torchaudio', 'models.encoder.wavlm']:
        sys.modules[name] = types.ModuleType(name)
    class Registry:
        def register_module(self, *a, **k): return lambda cls: cls
    sys.modules['models.builder'].MODELS = Registry()
    sys.modules['models.encoder.wavlm'].WavLMModel = None
    for name, path in [('models.encoder.wav2vec','models/encoder/wav2vec.py'),('models.network','models/network.py')]:
        spec=importlib.util.spec_from_file_location(name, root/path)
        mod=importlib.util.module_from_spec(spec); sys.modules[name]=mod; spec.loader.exec_module(mod)
    return sys.modules['models.network'].Audio2Expression(
        pretrained_encoder_path='', wav2vec2_config_path=str(root/'configs/wav2vec2_config.json'),
        num_identity_classes=12, identity_feat_dim=64, hidden_dim=512, expression_dim=52, norm_type='ln', use_transformer=False)

class ExportModel(torch.nn.Module):
    def __init__(self, model): super().__init__(); self.model=model
    def forward(self, audio, identity):
        return self.model({'input_audio_array':audio,'id_idx':identity,'time_steps':64})

def main():
    p=argparse.ArgumentParser(); p.add_argument('--upstream',type=pathlib.Path,required=True); p.add_argument('--checkpoint',type=pathlib.Path,required=True); p.add_argument('--output',type=pathlib.Path,required=True); a=p.parse_args()
    torch.set_num_threads(8); torch.manual_seed(42)
    model=load_backbone(a.upstream.resolve())
    ckpt=torch.load(a.checkpoint,map_location='cpu',weights_only=True)
    weights={k.removeprefix('module.').removeprefix('backbone.'):v for k,v in ckpt['state_dict'].items()}
    model.load_state_dict(weights,strict=True); model.eval()
    wrapper=ExportModel(model).eval()
    audio=torch.randn(1,34133)*0.1; identity=torch.eye(12)[0:1]
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with torch.no_grad():
        torch.onnx.export(wrapper,(audio,identity),str(a.output),input_names=['audio','identity'],output_names=['curves'],opset_version=17,do_constant_folding=True)
    session=ort.InferenceSession(str(a.output),providers=['CPUExecutionProvider'])
    results=[]
    for name,arr,style in [('noise',audio,0),('silence',torch.zeros_like(audio),0),('style11',audio,11)]:
        ident=torch.eye(12)[style:style+1]
        with torch.no_grad(): expected=wrapper(arr,ident).numpy()
        actual=session.run(None,{'audio':arr.numpy(),'identity':ident.numpy()})[0]
        error=float(np.max(np.abs(expected-actual))); assert error<=1e-3,(name,error)
        results.append({'case':name,'max_abs_error':error})
        arr.numpy().tofile(a.output.parent/(name+'.audio.f32')); ident.numpy().tofile(a.output.parent/(name+'.identity.f32')); expected.tofile(a.output.parent/(name+'.expected.f32'))
    manifest={'upstream_revision':REVISION,'sha256':hashlib.sha256(a.output.read_bytes()).hexdigest(),'inputs':{'audio':[1,34133],'identity':[1,12]},'output':[1,64,52],'opset':17,'validation':results}
    (a.output.parent/'manifest.json').write_text(json.dumps(manifest,indent=2))
    print(json.dumps(manifest,indent=2))
if __name__=='__main__': main()
