#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ap5Pieces.h"
#include "Ap5Monster.generated.h"

class UDynamicMeshComponent;
class UMaterialInterface;

// 全身の体積から外皮・内壁・切断面を生成する検証用モンスター。
UCLASS()
class AP5_API AAp5Monster : public AActor
{
    GENERATED_BODY()

public:
    AAp5Monster();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    int32 ApplyBrush(const FVector& Start, const FVector& Direction, float Radius, bool bRepair);
    int32 ApplyImpact(const FVector& Start, const FVector& Direction, float Radius);
    int32 Cut(const FVector& PlanePoint, const FVector& PlaneNormal);
    void ResetShape();
    FString ApplyJoin(const FVector& Start, const FVector& Direction);
    void ClearJoinSelection() { SelectedJoinPiece = INDEX_NONE; }
    bool GetJoinSelectionLocation(FVector& Location) const;
    bool ToggleArmMotionTest();
    bool IsArmMotionTestEnabled() const { return bArmMotionTest; }
    double LastEditMilliseconds = 0;
    int32 LastSeparatedPieces = 0;

private:
    void AddPart(FName Name, const FVector& Center, const FVector& Radii,
        const FRotator& Rotation = FRotator::ZeroRotator);
    UDynamicMeshComponent* CreatePiece();
    void RebuildMesh(int32 Index);
    void SyncPhysicsState();
    void RefreshPieces(const std::vector<int>& Changed);
    void UpdateArmMotion(float DeltaSeconds);
    int32 SelectedJoinPiece = INDEX_NONE;
    bool bArmMotionTest = false;
    float ArmMotionTime = 0.0f;
    std::vector<Ap5Volume::Ellipsoid> Shapes;
    Ap5Volume::Field InitialVolume;
    Ap5Volume::PieceCollection Pieces;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> GolemMaterial;

    UPROPERTY()
    TArray<TObjectPtr<UDynamicMeshComponent>> PieceMeshes;
};
