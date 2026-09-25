#pragma once
#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "AnimNode_LAMARKit.h"
#include "AnimGraphNode_LAMARKit.generated.h"
UCLASS()
class LAMAUDIO2EXPRESSIONEDITOR_API UAnimGraphNode_LAMARKit : public UAnimGraphNode_Base
{
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, Category = "Settings") FAnimNode_LAMARKit Node;
    FText GetNodeTitle(ENodeTitleType::Type Type) const override
    {
        return FText::FromString(TEXT("Apply LAM ARKit Curves"));
    }
    FText GetTooltipText() const override
    {
        return FText::FromString(TEXT(
            "Applies ARKit curves from a LAM component. Empty Source Component selects the owning actor's component."));
    }
    FString GetNodeCategory() const override
    {
        return TEXT("LAM Audio2Expression");
    }
};
