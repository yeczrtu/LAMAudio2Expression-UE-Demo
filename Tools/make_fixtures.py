import pathlib, wave
import numpy as np
root=pathlib.Path(__file__).resolve().parents[1]/'.work/fixtures'; root.mkdir(parents=True,exist_ok=True)
for name,rate,channels,seconds in [('short_inline',16000,1,.08),('one_inline',16000,1,1),('fraction_stream',44100,2,2.137),('speech_stream',48000,1,6),('silence_inline',16000,1,1),('long_stream',48000,2,300)]:
    t=np.arange(round(seconds*rate))/rate
    envelope=np.maximum(0,np.sin(2*np.pi*2.3*t))*(.3+.1*np.sin(2*np.pi*.73*t))
    signal=envelope*(np.sin(2*np.pi*145*t)+.3*np.sin(2*np.pi*290*t))
    if name.startswith('silence'): signal*=0
    x=np.repeat(signal[:,None],channels,axis=1)
    with wave.open(str(root/(name+'.wav')),'wb') as f:
        f.setnchannels(channels); f.setsampwidth(2); f.setframerate(rate); f.writeframes((x*32767).astype('<i2').tobytes())
