#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Ap5Observer.generated.h"

class AAp5PlayerCharacter;
class AAp5Monster;
class AAp5Projectile;

UCLASS()
class AP5_API AAp5Observer : public APlayerController
{
    GENERATED_BODY()
public:
    AAp5Observer();
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    bool IsJoinMode() const { return bJoinMode; }
    bool IsBlastMode() const { return bBlastMode; }
    bool IsGrabMode() const { return bGrabMode; }
    bool IsProjectileMode() const { return bProjectileMode; }
    bool GetJoinMarker(FVector2D& ScreenPosition) const;
    bool GetGrabMarker(FVector2D& ScreenPosition) const;
    bool IsImpactMode() const { return bImpactMode; }
    bool IsRepairMode() const { return bRepairMode; }
    bool IsCutMode() const { return bCutMode; }
    bool IsDrawingCut() const { return bDrawingCut; }
    FVector2D GetCutStart() const { return CutStart; }
    FVector2D GetCutEnd() const { return CutEnd; }
    float GetBrushRadius() const { return BrushRadius; }
    int32 GetSelectedHotbarIndex() const { return SelectedHotbarIndex; }
    int32 GetHotbarCount() const { return 8; }
    FString GetSelectedToolName() const;
    const FString& GetEditStatus() const { return EditStatus; }
private:
    void ResetView();
    void SelectHotbarIndex(int32 Index);
    bool GetAimRay(FVector& Start,FVector& Direction) const;
    bool FireProjectile(bool bExplosive);
    UPROPERTY()
    TObjectPtr<AAp5PlayerCharacter> PlayerCharacter;
    UPROPERTY()
    TObjectPtr<AAp5Monster> TestMonster;
    bool bJoinMode = false;
    bool bBlastMode = false;
    bool bGrabMode = false;
    bool bProjectileMode = true;
    bool bImpactMode = false;
    bool bRepairMode = false;
    bool bCutMode = false;
    bool bDrawingCut = false;
    FVector2D CutStart = FVector2D::ZeroVector;
    FVector2D CutEnd = FVector2D::ZeroVector;
    float BrushRadius = 12.0f;
    float FireCooldown = 0.0f;
    int32 SelectedHotbarIndex = 0;
    FString EditStatus = TEXT("ハンドガン：左クリック長押しで射撃");
};

UCLASS()
class AP5_API AAp5ObserverHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
