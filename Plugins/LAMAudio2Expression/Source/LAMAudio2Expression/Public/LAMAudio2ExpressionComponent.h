#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LAMTypes.h"
#include "LAMAudio2ExpressionComponent.generated.h"
class UAudioComponent;
class ULAMAnalyzeAsync;
struct FLAMLiveSession;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLAMStatusEvent, FString, Message);
UCLASS(ClassGroup = (Animation), meta = (BlueprintSpawnableComponent))
class LAMAUDIO2EXPRESSION_API ULAMAudio2ExpressionComponent : public UActorComponent
{
    GENERATED_BODY()
  public:
    ULAMAudio2ExpressionComponent();
    virtual ~ULAMAudio2ExpressionComponent();
    UPROPERTY(BlueprintAssignable, Category = "LAM") FLAMStatusEvent OnStatus;
    UPROPERTY(BlueprintReadOnly, Category = "LAM") TObjectPtr<ULAMExpressionClip> CurrentClip;
    UFUNCTION(BlueprintCallable, Category = "LAM")
    bool PlayExpressionClip(ULAMExpressionClip *Clip, float StartTime = 0);
    UFUNCTION(BlueprintCallable, Category = "LAM") void Pause();
    UFUNCTION(BlueprintCallable, Category = "LAM") void Resume();
    UFUNCTION(BlueprintCallable, Category = "LAM") void Stop();
    UFUNCTION(BlueprintCallable, Category = "LAM") bool Seek(float TimeSeconds);
    UFUNCTION(BlueprintCallable, Category = "LAM") void CancelAnalysis();
    UFUNCTION(BlueprintPure, Category = "LAM") FLAMExpressionFrame GetCurrentExpressionFrame() const
    {
        return Frame;
    }
    UFUNCTION(BlueprintPure, Category = "LAM") float GetARKitCurveValue(FName Name) const;
    UFUNCTION(BlueprintPure, Category = "LAM") static TArray<FName> GetARKitCurveNames();
    UFUNCTION(BlueprintCallable, Category = "LAM|Live")
    bool StartMicrophone(FLAMAnalysisSettings Settings, int32 DeviceIndex = -1);
    UFUNCTION(BlueprintCallable, Category = "LAM|Live") bool StartPCMStream(FLAMAnalysisSettings Settings);
    UFUNCTION(BlueprintCallable, Category = "LAM|Live") void StopMicrophone();
    UFUNCTION(BlueprintCallable, Category = "LAM|Live")
    bool PushPCMAudio(const TArray<float> &InterleavedPCM, int32 SampleRate, int32 Channels);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM|Live", meta = (ClampMin = "0.4", ClampMax = "2.0"))
    float PresentationDelay = 0.75f;
    UPROPERTY(BlueprintReadOnly, Category = "LAM|Live") float InferenceP95Milliseconds = 0;
    void SetAnalysis(ULAMAnalyzeAsync *Action);

  protected:
    void EndPlay(const EEndPlayReason::Type Reason) override;
    void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction *ThisTick) override;

  private:
    void PlaybackPercent(const UAudioComponent *Component, const USoundWave *Sound, float Percent);
    void PlaybackFinished(UAudioComponent *Component);
    void UpdateLive(float Delta);
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Audio;
    UPROPERTY(Transient) TObjectPtr<ULAMAnalyzeAsync> Analysis;
    UPROPERTY(Transient) TObjectPtr<class UNNEModelData> LiveModel;
    FLAMExpressionFrame Frame;
    double LastUpdate = 0;
    float AudioPosition = 0;
    bool bPlaying = false, bPaused = false, bHaveClock = false;
    TSharedPtr<FLAMLiveSession, ESPMode::ThreadSafe> Live;
    TSharedPtr<class FLAMCapture> Capture;
};
