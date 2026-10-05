#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ap5Volume.h"
#include "Ap5Monster.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;

// 各部位を独立した閉じたメッシュとして生成する、静止検証用モンスター。
UCLASS()
class AP5_API AAp5Monster : public AActor
{
    GENERATED_BODY()

public:
    AAp5Monster();
    virtual void BeginPlay() override;
    int32 ApplyBrush(const FVector& Start, const FVector& Direction, float Radius, bool bRepair);
    void ResetShape();
    double LastEditMilliseconds = 0;

private:
    void AddPart(FName Name, const FVector& Center, const FVector& Radii,
        const FRotator& Rotation = FRotator::ZeroRotator);
    void RebuildPart(int32 PartIndex);
    std::vector<Ap5Volume::Field> Volumes;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> GolemMaterial;

    UPROPERTY()
    TArray<TObjectPtr<UDynamicMeshComponent>> Parts;
};
