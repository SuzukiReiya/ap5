#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ap5Monster.generated.h"

class UDynamicMeshComponent;

// 各部位を独立した閉じたメッシュとして生成する、静止検証用モンスター。
UCLASS()
class AP5_API AAp5Monster : public AActor
{
    GENERATED_BODY()

public:
    AAp5Monster();
    virtual void BeginPlay() override;

private:
    void AddPart(FName Name, const FVector& Center, const FVector& Radii,
        const FRotator& Rotation = FRotator::ZeroRotator);

    UPROPERTY()
    TArray<TObjectPtr<UDynamicMeshComponent>> Parts;
};
