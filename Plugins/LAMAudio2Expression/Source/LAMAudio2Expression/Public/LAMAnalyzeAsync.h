#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Containers/Ticker.h"
#include "LAMTypes.h"
#include "LAMAnalyzeAsync.generated.h"
class ULAMAudio2ExpressionComponent;
struct FLAMJob;
struct FStreamableHandle;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLAMAnalysisEvent, ULAMExpressionClip *, Clip, float, Progress, FString,
                                               Error);
UCLASS()
class LAMAUDIO2EXPRESSION_API ULAMAnalyzeAsync : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()
  public:
    UPROPERTY(BlueprintAssignable) FLAMAnalysisEvent Completed;
    UPROPERTY(BlueprintAssignable) FLAMAnalysisEvent Failed;
    UPROPERTY(BlueprintAssignable) FLAMAnalysisEvent Progress;
    UPROPERTY(BlueprintAssignable) FLAMAnalysisEvent Cancelled;
    UFUNCTION(BlueprintCallable, Category = "LAM",
              meta = (BlueprintInternalUseOnly = "true", DisplayName = "Analyze SoundWave Async"))
    static ULAMAnalyzeAsync *AnalyzeSoundWaveAsync(ULAMAudio2ExpressionComponent *Component, USoundWave *SoundWave,
                                                   FLAMAnalysisSettings Settings);
    void Activate() override;
    UFUNCTION(BlueprintCallable, Category = "LAM") void Cancel();
    void BeginDestroy() override;

  private:
    void StartWork();
    bool Poll(float Delta);
    void Finish(const FString &Error);
    UPROPERTY() TObjectPtr<USoundWave> Sound;
    UPROPERTY() TObjectPtr<class UNNEModelData> Model;
    TWeakObjectPtr<ULAMAudio2ExpressionComponent> Owner;
    FLAMAnalysisSettings Options;
    TSharedPtr<FLAMJob, ESPMode::ThreadSafe> Job;
    TSharedPtr<FStreamableHandle> LoadHandle;
    FTSTicker::FDelegateHandle Ticker;
    bool bFinished = false;
};
