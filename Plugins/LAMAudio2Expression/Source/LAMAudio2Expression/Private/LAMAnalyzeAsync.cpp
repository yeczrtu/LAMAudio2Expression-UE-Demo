#include "LAMAnalyzeAsync.h"
#include "LAMAudio2ExpressionComponent.h"
#include "LAMSettings.h"
#include "LAMCore.h"
#include "Sound/SoundWave.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Misc/SecureHash.h"

// Accessed exclusively by the single inference worker. Live clips own copies.
struct FLAMCacheEntry
{
    TArray<float> Curves;
    float Duration;
    FString Backend;
    uint64 Touch;
    int32 NumSamples;
};
static TMap<FString, FLAMCacheEntry> Cache;
static uint64 Clock = 0;
ULAMAnalyzeAsync *ULAMAnalyzeAsync::AnalyzeSoundWaveAsync(ULAMAudio2ExpressionComponent *C, USoundWave *S,
                                                          FLAMAnalysisSettings O)
{
    auto *A = NewObject<ULAMAnalyzeAsync>();
    A->Owner = C;
    A->Sound = S;
    A->Options = O;
    if (C)
    {
        A->RegisterWithGameInstance(C);
        C->SetAnalysis(A);
    }
    return A;
}
void ULAMAnalyzeAsync::Activate()
{
    if (!Owner.IsValid() || !Sound || Sound->IsProcedurallyGenerated() || Sound->bIsSourceBus ||
        Sound->NumChannels < 1 || Sound->NumChannels > 2 || Sound->Duration <= 0 || Sound->Duration > 300 ||
        Options.Style < 0 || Options.Style > 11)
    {
        Finish(TEXT("Invalid component, style or SoundWave (mono/stereo, 0-300 seconds required)."));
        return;
    }
    Job = MakeShared<FLAMJob, ESPMode::ThreadSafe>();
    Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ULAMAnalyzeAsync::Poll));
    const auto Path = GetDefault<ULAMSettings>()->Model.ToSoftObjectPath();
    LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        Path, FStreamableDelegate::CreateUObject(this, &ULAMAnalyzeAsync::StartWork));
    if (!LoadHandle)
        Finish(TEXT(
            "Cannot request model asset. Import LAM_A2E.onnx and configure Project Settings / LAM Audio2Expression."));
}
void ULAMAnalyzeAsync::StartWork()
{
    if (bFinished || !Job || Job->Cancelled)
        return;
    Model = GetDefault<ULAMSettings>()->Model.Get();
    if (!Model)
    {
        Finish(TEXT("Model asset missing. Run Tools/import_assets.py in the editor."));
        return;
    }
    if (!Sound->IsStreaming())
        Sound->InitAudioResource(Sound->GetRuntimeFormat());
    auto Wave = Sound->GetSoundWaveProxy()->GetSoundWaveDataRef();
    auto Models = LAM::CreateModels(Model, GetDefault<ULAMSettings>()->bPreferGPU);
    if (!Models.CPU && !Models.GPU)
    {
        Finish(TEXT("Neither NNE DirectML nor CPU can load the model."));
        return;
    }
    const int64 Budget = int64(FMath::Max(0, GetDefault<ULAMSettings>()->CacheMiB)) * 1024 * 1024;
    const auto O = Options;
    auto Work = Job;
    LAM::Queue(
        [Wave, Models, O, Work, Budget]()
        {
            TArray<float> PCM;
            if (LAM::Decode(Wave, PCM, *Work) && !Work->Cancelled)
            {
                FSHA1 Hash;
                Hash.Update(reinterpret_cast<const uint8 *>(PCM.GetData()), PCM.Num() * sizeof(float));
                Hash.Final();
                uint8 Bytes[20];
                Hash.GetHash(Bytes);
                const FString Key = BytesToHex(Bytes, 20) + Models.Key +
                                    FString::Printf(TEXT(":%d:%d:%d:%d:%d:%d:v1"), O.Style, O.bSmooth,
                                                    O.bSuppressSilentMouth, O.bSymmetrize, O.bAutoBlink, O.BlinkSeed);
                if (auto *Hit = Cache.Find(Key))
                {
                    Work->Curves = Hit->Curves;
                    Work->Duration = Hit->Duration;
                    Work->NumSamples = Hit->NumSamples;
                    Work->Backend = Hit->Backend;
                    Hit->Touch = ++Clock;
                }
                else
                {
                    LAM::Analyze(PCM, Models, O, *Work);
                    if (!Work->Cancelled && Work->Error.IsEmpty() && Budget > 0)
                    {
                        Cache.Add(Key, {Work->Curves, Work->Duration, Work->Backend, ++Clock, Work->NumSamples});
                        for (;;)
                        {
                            int64 Size = 0;
                            FString Oldest;
                            uint64 Age = MAX_uint64;
                            for (const auto &Pair : Cache)
                            {
                                Size += Pair.Value.Curves.GetAllocatedSize();
                                if (Pair.Value.Touch < Age)
                                {
                                    Age = Pair.Value.Touch;
                                    Oldest = Pair.Key;
                                }
                            }
                            if (Size <= Budget || Cache.IsEmpty())
                                break;
                            Cache.Remove(Oldest);
                        }
                    }
                }
            }
            Work->Done = true;
        });
}
bool ULAMAnalyzeAsync::Poll(float)
{
    if (bFinished)
        return false;
    if (!Owner.IsValid())
    {
        Cancel();
        return false;
    }
    if (Job->Done)
    {
        if (!Job->Error.IsEmpty())
            Finish(Job->Error);
        else
        {
            auto *Clip = NewObject<ULAMExpressionClip>(Owner.Get());
            Clip->SoundWave = Sound;
            Clip->Duration = Job->Duration;
            Clip->NumSamples = Job->NumSamples;
            Clip->Curves = MoveTemp(Job->Curves);
            Clip->Settings = Options;
            Clip->Backend = Job->Backend;
            Clip->AnalysisSeconds = Job->Seconds;
            bFinished = true;
            Completed.Broadcast(Clip, 1, TEXT(""));
            SetReadyToDestroy();
        }
        return false;
    }
    Progress.Broadcast(nullptr, FMath::Clamp(Job->Progress.Load(), 0.f, 1.f), TEXT(""));
    return true;
}
void ULAMAnalyzeAsync::Finish(const FString &Error)
{
    if (bFinished)
        return;
    bFinished = true;
    if (Job)
        Job->Cancelled = true;
    FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
    Failed.Broadcast(nullptr, 0, Error);
    SetReadyToDestroy();
}
void ULAMAnalyzeAsync::Cancel()
{
    if (bFinished)
        return;
    bFinished = true;
    if (Job)
        Job->Cancelled = true;
    if (LoadHandle)
        LoadHandle->CancelHandle();
    FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
    Cancelled.Broadcast(nullptr, 0, TEXT("Cancelled"));
    SetReadyToDestroy();
}
void ULAMAnalyzeAsync::BeginDestroy()
{
    if (Job)
        Job->Cancelled = true;
    FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
    Super::BeginDestroy();
}
