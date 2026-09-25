#include "LAMCore.h"
#include "AudioDecompress.h"
#include "Interfaces/IAudioFormat.h"
#include "ContentStreaming.h"
#include "Sound/SoundWave.h"

bool LAM::Decode(const TSharedRef<const FSoundWaveData> &Wave, TArray<float> &Mono, FLAMJob &Job)
{
    TUniquePtr<ICompressedAudioInfo> Decoder(
        IAudioInfoFactoryRegistry::Get().Create(Wave->GetRuntimeFormat(), Wave->GetFName()));
    if (!Decoder)
    {
        Job.Error = TEXT("No decoder for the cooked SoundWave format.");
        return false;
    }
    FSoundQualityInfo Info;
    const bool Streaming = Wave->IsStreaming();
    if (!(Streaming ? Decoder->StreamCompressedInfo(Wave, &Info)
                    : Decoder->ReadCompressedInfo(Wave->GetResourceData(), Wave->GetResourceSize(), &Info)))
    {
        Job.Error = TEXT("Cannot read SoundWave header.");
        return false;
    }
    if (Info.NumChannels < 1 || Info.NumChannels > 2 || Info.SampleRate < 8000 || Info.SampleRate > 192000 ||
        Info.SampleDataSize == 0 || double(Info.SampleDataSize) / (2 * Info.NumChannels * Info.SampleRate) > 300.01)
    {
        Job.Error = TEXT("Unsupported SoundWave: require mono/stereo, 8-192 kHz, 0-300 seconds.");
        return false;
    }
    const int32 Total = int32(Info.SampleDataSize / (2 * Info.NumChannels));
    TArray<float> Native;
    Native.Reserve(Total);
    constexpr int32 BlockFrames = 1024;
    TArray<int16> Block;
    Block.SetNumZeroed(BlockFrames * Info.NumChannels);
    int32 LastChunk = -1;
    TArray<FAudioChunkHandle> Pins;
    while (Native.Num() < Total)
    {
        if (Job.Cancelled)
            return false;
        if (Streaming)
        {
            const int Current = FMath::Max(0, Decoder->GetCurrentChunkIndex());
            if (Current != LastChunk)
            {
                Pins.Reset();
                LastChunk = Current;
                // Pin current + lookahead. Block only this worker, never the game/audio thread.
                for (int C = FMath::Max(1, Current); C < FMath::Min(int(Wave->GetNumChunks()), Current + 3); ++C)
                {
                    auto Handle = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(Wave, C, true);
                    if (!Handle.IsValid())
                    {
                        Job.Error = TEXT("Failed to load a streamed audio chunk.");
                        return false;
                    }
                    Pins.Add(MoveTemp(Handle));
                }
            }
        }
        int32 Bytes = Block.Num() * sizeof(int16);
        bool End = false;
        if (Streaming)
            End = Decoder->StreamCompressedData(reinterpret_cast<uint8 *>(Block.GetData()), false, Bytes, Bytes);
        else
            End = Decoder->ReadCompressedData(reinterpret_cast<uint8 *>(Block.GetData()), false, Bytes);
        if (Decoder->HasError() || Bytes <= 0)
        {
            Job.Error = TEXT("Audio decoder stalled or failed; missing data was not treated as silence.");
            return false;
        }
        const int Frames =
            FMath::Min3(Bytes / int(sizeof(int16) * Info.NumChannels), BlockFrames, Total - Native.Num());
        for (int I = 0; I < Frames; ++I)
        {
            float V = 0;
            for (uint32 C = 0; C < Info.NumChannels; ++C)
                V += Block[I * Info.NumChannels + C] / 32768.f;
            Native.Add(V / Info.NumChannels);
        }
        Job.Progress = 0.15f * float(Native.Num()) / Total;
        if (End && Native.Num() < Total)
        {
            Job.Error = TEXT("Audio ended before its declared sample count.");
            return false;
        }
    }
    Mono = Resample(Native, Info.SampleRate);
    Job.Progress = 0.2f;
    return true;
}
