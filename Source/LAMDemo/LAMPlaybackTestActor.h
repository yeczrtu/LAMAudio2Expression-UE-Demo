#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LAMPlaybackTypes.h"
#include "LAMPlaybackTestActor.generated.h"
class ULAMAudio2ExpressionComponent;
class USoundSubmix;
class USoundConcurrency;
class FLAMSubmixMeter;
UCLASS()
class ALAMPlaybackTestActor : public AActor
{
    GENERATED_BODY()
public:
    ALAMPlaybackTestActor();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<ULAMAudio2ExpressionComponent> A;
    UPROPERTY() TObjectPtr<ULAMAudio2ExpressionComponent> B;
    UPROPERTY() TObjectPtr<ULAMExpressionClip> Clip;
    UPROPERTY() TObjectPtr<ULAMExpressionClip> InlineClip;
    UPROPERTY() TArray<TObjectPtr<USoundSubmix>> Submixes;
    UPROPERTY() TObjectPtr<USoundConcurrency> Concurrency;
    UPROPERTY() TObjectPtr<class ULAMAnalyzeAsync> CompetingAnalysis;
    TArray<TSharedPtr<FLAMSubmixMeter, ESPMode::ThreadSafe>> Meters;
    UFUNCTION() void Ended(FLAMPlaybackInfo Info, ELAMPlaybackEndReason Reason);
    UFUNCTION() void Finished(FLAMPlaybackInfo Info);
    UFUNCTION() void Started(FLAMPlaybackInfo Info);
    UFUNCTION() void StateChanged(FLAMPlaybackInfo Info);
    bool Check(bool Condition, const FString& What);
    void Finish(bool OK, const FString& What);
    void Advance();
    void ResetMeters();
    void TickLive(float Delta);
    TSet<int64> EndIds;
    TArray<ELAMPlaybackEndReason> Reasons;
    FLAMAudioPlaybackSettings Settings;
    int Stage = 0, FinishedCount = 0, StartedCount = 0, LiveFrames = 0;
    int64 Samples = 0;
    double Began = 0, At = 0;
    float HeldTime = 0, LastLiveTime = -1;
    bool Done = false, Reentrant = false, LiveTest = false, SawLag = false, PausedFromState = false;
    int LiveRate = 48000, LiveChannels = 1;
    FString Details;
};
