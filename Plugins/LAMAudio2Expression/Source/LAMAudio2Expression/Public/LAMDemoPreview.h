// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameModeBase.h"
#include "LAMTypes.h"
#include "LAMDemoPreview.generated.h"

class ULAMAudio2ExpressionComponent;
class ULAMAnalyzeAsync;
class USkeletalMeshComponent;
class UCameraComponent;
class USoundWave;

/** Optional sample actor used only by Content/Demo. No /Game dependencies. */
UCLASS()
class LAMAUDIO2EXPRESSION_API ALAMDemoPreview : public AActor
{
    GENERATED_BODY()
  public:
    ALAMDemoPreview();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Demo") TObjectPtr<USkeletalMeshComponent> Face;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Demo")
    TObjectPtr<ULAMAudio2ExpressionComponent> Expression;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Demo") TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demo") TArray<TObjectPtr<USoundWave>> Samples;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Demo") TArray<FString> SampleLabels;
    UPROPERTY(BlueprintReadOnly, Category = "Demo") FString Status = TEXT("Loading...");
    UPROPERTY(BlueprintReadOnly, Category = "Demo") int32 Selected = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Demo") float Progress = 0;
    UFUNCTION(BlueprintCallable, Category = "Demo") void SelectSample(int32 Index);
    UFUNCTION(BlueprintCallable, Category = "Demo") void TogglePlayback();
    UFUNCTION(BlueprintCallable, Category = "Demo") void Replay();
    bool IsPaused() const
    {
        return bPaused;
    }
    bool IsAnalyzing() const
    {
        return bAnalyzing;
    }
    void BeginPlay() override;
    void Tick(float DeltaSeconds) override;

  private:
    UFUNCTION() void Completed(ULAMExpressionClip *Clip, float Value, FString Error);
    UFUNCTION() void Failed(ULAMExpressionClip *Clip, float Value, FString Error);
    UFUNCTION() void Updated(ULAMExpressionClip *Clip, float Value, FString Error);
    void Report(bool Success, const FString &Detail);
    UPROPERTY() TObjectPtr<ULAMAnalyzeAsync> Action;
    bool bPaused = false, bAnalyzing = false, bReported = false, bTest = false, bCaptured = false;
    double Started = 0, PlaybackStarted = 0;
    float PeakJaw = 0;
};

UCLASS()
class LAMAUDIO2EXPRESSION_API ALAMDemoPreviewHUD : public AHUD
{
    GENERATED_BODY()
  public:
    void DrawHUD() override;
    void NotifyHitBoxClick(FName BoxName) override;
};

UCLASS()
class LAMAUDIO2EXPRESSION_API ALAMDemoPreviewGameMode : public AGameModeBase
{
    GENERATED_BODY()
  public:
    ALAMDemoPreviewGameMode();
};
