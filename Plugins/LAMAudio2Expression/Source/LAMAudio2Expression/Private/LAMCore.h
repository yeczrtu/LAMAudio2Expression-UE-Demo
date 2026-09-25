#pragma once
#include "CoreMinimal.h"
#include "LAMTypes.h"
#include "NNERuntimeCPU.h"
#include "NNERuntimeGPU.h"

struct FLAMJob
{
    TAtomic<bool> Cancelled{false}, Done{false};
    TAtomic<float> Progress{0};
    TArray<float> Curves;
    FString Error, Backend;
    float Duration = 0, Seconds = 0;
    int32 NumSamples = 0;
};
class FSoundWaveData;
class UNNEModelData;
namespace LAM
{
struct FModels
{
    TSharedPtr<UE::NNE::IModelCPU> CPU;
    TSharedPtr<UE::NNE::IModelGPU> GPU;
    FString Key;
};
FModels CreateModels(UNNEModelData *Data, bool PreferGPU);
void Queue(TFunction<void()> Task);
bool Decode(const TSharedRef<const FSoundWaveData> &Wave, TArray<float> &Mono, FLAMJob &Job);
TArray<float> Resample(const TArray<float> &Input, int32 SourceRate);
void MakeWindow(const TArray<float> &PCM, int64 EndSample, TArray<float> &Out);
void Postprocess(TArray<float> &Curves, const TArray<float> &PCM, const FLAMAnalysisSettings &Settings, int32 Hop = 30);
void Analyze(const TArray<float> &PCM, const FModels &Models, const FLAMAnalysisSettings &Settings, FLAMJob &Job);
bool InferWindow(const TArray<float> &Window, int32 Style, const FModels &Models,
                 TSharedPtr<UE::NNE::IModelInstanceRunSync> &Instance, bool &UsingGPU, TArray<float> &Output);
} // namespace LAM
