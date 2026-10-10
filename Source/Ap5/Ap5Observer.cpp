#include "Ap5Observer.h"
#include "Ap5FrameProfiler.h"
#include "Ap5Monster.h"
#include "Ap5Projectile.h"
#include "EngineUtils.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"

AAp5Observer::AAp5Observer()
{
    bAutoManageActiveCameraTarget = false;
    bShowMouseCursor = true;
}

void AAp5Observer::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController())
    {
        return;
    }
    ObservationCamera = GetWorld()->SpawnActor<ACameraActor>();
    if (ObservationCamera == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("AP5_CAMERA_FAILED"));
        return;
    }
    ObservationCamera->GetCameraComponent()->SetFieldOfView(50.0f);
    SetViewTarget(ObservationCamera);
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    ResetView();
}

void AAp5Observer::ResetView()
{
    Yaw = 210.0f;
    Elevation = 12.0f;
    Distance = 750.0f;
    bDragging = false;
    UpdateView();
}

void AAp5Observer::UpdateView()
{
    if (ObservationCamera == nullptr)
    {
        return;
    }
    const FVector Target(0, 0, 170);
    const FVector Offset = FRotator(Elevation, Yaw, 0).Vector() * Distance;
    ObservationCamera->SetActorLocation(Target + Offset);
    ObservationCamera->SetActorRotation((-Offset).Rotation());
}

bool AAp5Observer::FireProjectile(bool bExplosive)
{
    FVector Start,Direction;
    if (!DeprojectMousePositionToWorld(Start,Direction)) return false;
    const FVector Axis=Direction.GetSafeNormal();
    if (Axis.IsNearlyZero()) return false;

    FAp5ProfileScope Profile(EAp5ProfileProcess::ProjectileSpawn);
    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AAp5Projectile* Projectile=GetWorld()->SpawnActor<AAp5Projectile>(
        Start+Axis*30.0f,Axis.Rotation(),Parameters);
    if (Projectile==nullptr) return false;

    Projectile->Launch(Axis,BrushRadius,bExplosive,BrushRadius*1.5f);
    return true;
}

void AAp5Observer::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    FAp5FrameProfiler::Get().BeginFrame(DeltaTime);
    if (!IsLocalController() || ObservationCamera == nullptr)
    {
        return;
    }
    float MouseX = 0;
    float MouseY = 0;
    const bool bMouseAvailable = GetMousePosition(MouseX, MouseY);
    if (IsInputKeyDown(EKeys::RightMouseButton) && bMouseAvailable)
    {
        if (bDragging)
        {
            Yaw += (MouseX - PreviousMouse.X) * 0.35f;
            Elevation -= (MouseY - PreviousMouse.Y) * 0.25f;
        }
        PreviousMouse = FVector2D(MouseX, MouseY);
        bDragging = true;
    }
    else
    {
        bDragging = false;
    }
    // マウス操作が難しい環境でも同じ確認ができるようにする。
    const float Step = 60.0f * FMath::Min(DeltaTime, 0.1f);
    if (IsInputKeyDown(EKeys::Left)) Yaw += Step;
    if (IsInputKeyDown(EKeys::Right)) Yaw -= Step;
    if (IsInputKeyDown(EKeys::Up)) Elevation -= Step;
    if (IsInputKeyDown(EKeys::Down)) Elevation += Step;
    if (WasInputKeyJustPressed(EKeys::MouseScrollUp)) Distance -= 50;
    if (WasInputKeyJustPressed(EKeys::MouseScrollDown)) Distance += 50;
    if (IsInputKeyDown(EKeys::PageUp)) Distance -= Step * 5;
    if (IsInputKeyDown(EKeys::PageDown)) Distance += Step * 5;
    Elevation = FMath::Clamp(Elevation, 0.0f, 75.0f);
    Distance = FMath::Clamp(Distance, 400.0f, 1400.0f);
    Yaw = FMath::UnwindDegrees(Yaw);
    if (WasInputKeyJustPressed(EKeys::R)) ResetView();
    if (WasInputKeyJustPressed(EKeys::Escape))
    {
        UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
    }
    UpdateView();
    if (!IsValid(TestMonster))
    {
        for (TActorIterator<AAp5Monster> It(GetWorld()); It; ++It)
        {
            TestMonster = *It;
            break;
        }
    }
    if (WasInputKeyJustPressed(EKeys::P))
    {
        bProjectileMode=true; bGrabMode=false; bBlastMode=false; bJoinMode=false;
        bImpactMode=false; bRepairMode=false; bCutMode=false; bDrawingCut=false;
        FireCooldown=0.0f;
        EditStatus=TEXT("実体弾：左ボタン長押しで連射。飛翔後に衝突して弾痕と運動量を与えます。");
    }
    if (WasInputKeyJustPressed(EKeys::G))
    {
        bProjectileMode=false; bGrabMode=true; bBlastMode=false; bJoinMode=false; bImpactMode=false; bRepairMode=false; bCutMode=false; bDrawingCut=false;
        EditStatus=TEXT("把持：分離した破片を左ボタンで掴み、押したまま移動して離してください。");
    }
    if (WasInputKeyJustPressed(EKeys::Zero))
    {
        bProjectileMode=false; bGrabMode=false; bBlastMode=true; bJoinMode=false; bImpactMode=false; bRepairMode=false; bCutMode=false; bDrawingCut=false;
        FireCooldown=0.0f;
        EditStatus=TEXT("爆発弾：左ボタン長押しで連射。実体弾の着弾地点で球状破壊します。");
    }
    if (WasInputKeyJustPressed(EKeys::One)) { bProjectileMode = false; bGrabMode = false; bBlastMode = false; bJoinMode = false; bImpactMode = false; bRepairMode = false; bCutMode = false; bDrawingCut = false; }
    if (WasInputKeyJustPressed(EKeys::Two)) { bProjectileMode = false; bGrabMode = false; bBlastMode = false; bJoinMode = false; bImpactMode = false; bRepairMode = true; bCutMode = false; bDrawingCut = false; }
    if (WasInputKeyJustPressed(EKeys::Six)) { bProjectileMode = false; bGrabMode = false; bBlastMode = false; bJoinMode = false; bImpactMode = false; bCutMode = true; bDrawingCut = false; }
    if (WasInputKeyJustPressed(EKeys::Seven)) { bProjectileMode = false; bGrabMode = false; bBlastMode = false; bJoinMode = false; bImpactMode = true; bRepairMode = false; bCutMode = false; bDrawingCut = false; }
    if (WasInputKeyJustPressed(EKeys::Eight))
    {
        bProjectileMode = false; bGrabMode = false; bBlastMode = false; bJoinMode = true; bImpactMode = false; bRepairMode = false; bCutMode = false; bDrawingCut = false;
        if (IsValid(TestMonster)) TestMonster->ClearJoinSelection();
        EditStatus = TEXT("接合：戻す破片をクリックし、次に接合先をクリックしてください。");
    }
    if (WasInputKeyJustPressed(EKeys::Three)) BrushRadius = 12;
    if (WasInputKeyJustPressed(EKeys::Four)) BrushRadius = 20;
    if (WasInputKeyJustPressed(EKeys::Five)) BrushRadius = 30;
    if (!IsValid(TestMonster)) return;
    if (WasInputKeyJustPressed(EKeys::M))
    {
        const bool bEnabled=TestMonster->ToggleMaterialResistanceTest();
        EditStatus=bEnabled
            ? TEXT("材質差ON：弾痕7で比較。腕・脚=標準、胴体=硬質x2、頭=硬質x4。")
            : TEXT("材質差OFF：弾痕は全身とも標準深さ8 cmです。");
    }
    if (!bGrabMode) TestMonster->EndGrab();
    if (WasInputKeyJustPressed(EKeys::Nine))
    {
        bProjectileMode=false;
        bBlastMode=false;
        bGrabMode=false;
        bJoinMode=false;
        bDrawingCut=false;
        TestMonster->EndGrab();
        TestMonster->ClearJoinSelection();
        const bool bEnabled=TestMonster->ToggleArmMotionTest();
        EditStatus=bEnabled
            ? TEXT("右腕関節動作を開始。動いている腕を1/2/6/7で加工し、切断後は8で再接合できます。")
            : TEXT("右腕関節動作を終了し、通常の一体形状へ戻しました。");
    }
    if (!bJoinMode) TestMonster->ClearJoinSelection();
    if (WasInputKeyJustPressed(EKeys::BackSpace))
    {
        TestMonster->ResetShape();
        bDrawingCut = false;
        EditStatus = TEXT("全形状を初期状態に戻しました（修復操作とは別）");
    }
    // 案内の上と視点ドラッグ中は加工しない。
    const bool bOverHUD = bMouseAvailable && MouseX >= 12 && MouseX <= 1012 && MouseY >= 12 && MouseY <= 146;
    FireCooldown=FMath::Max(0.0f,FireCooldown-DeltaTime);
    if (bProjectileMode || bBlastMode)
    {
        if (IsInputKeyDown(EKeys::LeftMouseButton) && bMouseAvailable && !bDragging && !bOverHUD
            && FireCooldown<=0.0f)
        {
            const bool bExplosive=bBlastMode;
            if (FireProjectile(bExplosive))
            {
                FireCooldown=0.12f;
                EditStatus=bExplosive
                    ? FString::Printf(TEXT("爆発弾を発射：速度1800 cm/s / 着弾爆発半径 %.0f cm"),BrushRadius*1.5f)
                    : FString::Printf(TEXT("実体弾を発射：速度1800 cm/s / 弾痕半径 %.0f cm"),BrushRadius);
            }
            else
            {
                FireCooldown=0.12f;
                EditStatus=TEXT("実体弾の生成に失敗しました。");
            }
        }
        return;
    }
    if (bGrabMode)
    {
        FVector GrabStart,GrabDirection;
        if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bMouseAvailable && !bDragging && !bOverHUD
            && DeprojectMousePositionToWorld(GrabStart,GrabDirection))
        {
            EditStatus=TestMonster->BeginGrab(GrabStart,GrabDirection);
        }
        if (IsInputKeyDown(EKeys::LeftMouseButton) && bMouseAvailable && !bDragging
            && DeprojectMousePositionToWorld(GrabStart,GrabDirection))
        {
            TestMonster->UpdateGrab(GrabStart,GrabDirection);
        }
        if (WasInputKeyJustReleased(EKeys::LeftMouseButton))
        {
            TestMonster->EndGrab();
            EditStatus=TEXT("破片を離しました。移動中ならその速度を保って飛びます。");
        }
        return;
    }
    if (bDragging || !bMouseAvailable) bDrawingCut = false;
    if (bDrawingCut)
    {
        CutEnd = FVector2D(MouseX, MouseY);
        if (WasInputKeyJustReleased(EKeys::LeftMouseButton))
        {
            bDrawingCut = false;
            FVector StartOrigin, StartDirection, EndOrigin, EndDirection;
            if ((CutEnd - CutStart).Size() < 12 || bOverHUD)
            {
                EditStatus = TEXT("切断を取り消しました。案内の外で12ピクセル以上ドラッグしてください。");
            }
            else if (DeprojectScreenPositionToWorld(CutStart.X, CutStart.Y, StartOrigin, StartDirection)
                && DeprojectScreenPositionToWorld(CutEnd.X, CutEnd.Y, EndOrigin, EndDirection))
            {
                const FVector Normal = FVector::CrossProduct(StartDirection, EndDirection).GetSafeNormal();
                const int32 Detached = TestMonster->Cut(ObservationCamera->GetActorLocation(), Normal);
                if (Detached < 0) EditStatus = TEXT("破片上限32個です。Backspaceで全リセットしてください。");
                else if (Detached == 0) EditStatus = TEXT("切断なし：本体または破片を横切る線を引いてください。");
                else EditStatus = FString::Printf(TEXT("切断：%d個増加 / CPU処理 %.1f ms。破片も続けて加工できます。"),
                    Detached, TestMonster->LastEditMilliseconds);
            }
        }
    }
    if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bMouseAvailable && !bDragging && !bOverHUD)
    {
        if (bCutMode)
        {
            CutStart = CutEnd = FVector2D(MouseX, MouseY);
            bDrawingCut = true;
            return;
        }
        FVector Start, Direction;
        if (DeprojectMousePositionToWorld(Start, Direction))
        {
            if (bJoinMode)
            {
                EditStatus = TestMonster->ApplyJoin(Start, Direction);
                return;
            }
            const int32 ChangedSamples=bImpactMode
                ? TestMonster->ApplyImpact(Start,Direction,BrushRadius)
                : TestMonster->ApplyBrush(Start,Direction,BrushRadius,bRepairMode);
            if (ChangedSamples < 0) EditStatus = TEXT("分離上限のため加工しませんでした。Backspaceで全リセットできます。");
            else if (bImpactMode) EditStatus = FString::Printf(
                TEXT("弾痕：更新 %d 格子点 / 分離 %d 個 / CPU処理 %.1f ms / %s"),
                ChangedSamples,TestMonster->LastSeparatedPieces,TestMonster->LastEditMilliseconds,
                *TestMonster->GetLastImpactMaterialText());
            else EditStatus = FString::Printf(TEXT("%s：更新 %d 格子点 / 分離 %d 個 / CPU処理 %.1f ms"),
                bRepairMode ? TEXT("修復") : TEXT("穴あけ"),
                ChangedSamples, TestMonster->LastSeparatedPieces, TestMonster->LastEditMilliseconds);
        }
    }
}

bool AAp5Observer::GetJoinMarker(FVector2D& ScreenPosition) const
{
    FVector Location;
    return bJoinMode && IsValid(TestMonster) && TestMonster->GetJoinSelectionLocation(Location)
        && ProjectWorldLocationToScreen(Location, ScreenPosition);
}

bool AAp5Observer::GetGrabMarker(FVector2D& ScreenPosition) const
{
    FVector Location;
    return bGrabMode && IsValid(TestMonster) && TestMonster->GetGrabLocation(Location)
        && ProjectWorldLocationToScreen(Location, ScreenPosition);
}

void AAp5ObserverHUD::DrawHUD()
{
    Super::DrawHUD();
    DrawRect(FLinearColor(0, 0, 0, 0.75f), 12, 12, 1000, 134);
    const AAp5Observer* Observer = Cast<AAp5Observer>(GetOwningPlayerController());
    if (Observer == nullptr) return;
    const float DisplayRadius=Observer->IsBlastMode() ? Observer->GetBrushRadius()*1.5f : Observer->GetBrushRadius();
    DrawText(FString::Printf(TEXT("%s　半径 %.0f cm　P：実体弾　0：爆発弾　G：把持　7：弾痕　1：穴　2：修復　6：切断　8：接合"),
        Observer->IsProjectileMode() ? TEXT("実体弾") : (Observer->IsGrabMode() ? TEXT("把持") : (Observer->IsBlastMode() ? TEXT("爆発弾") : (Observer->IsJoinMode() ? TEXT("接合") : (Observer->IsImpactMode() ? TEXT("弾痕") : (Observer->IsCutMode() ? TEXT("切断") : (Observer->IsRepairMode() ? TEXT("修復") : TEXT("穴あけ"))))))),
        DisplayRadius), FColor::White, 24, 20);
    DrawText(TEXT("3：細い　4：標準　5：太い　M：材質差 ON/OFF　9：右腕関節動作　Backspace：リセット"), FColor::White, 24, 44);
    DrawText(TEXT("右ドラッグ／矢印：回転　ホイール／PageUp・Down：ズーム　R：視点を戻す　Esc：終了"), FColor::White, 24, 68);
    DrawText(Observer->GetEditStatus(), FColor::Yellow, 24, 92);
    DrawText(TEXT("つながりを削り切ると分離・落下。本体・破片とも加工可能。修復は直近の分離・接合時の形まで。"), FColor::White, 24, 116);
    FVector2D JoinMarker;
    if (Observer->GetJoinMarker(JoinMarker))
    {
        DrawLine(JoinMarker.X - 10, JoinMarker.Y, JoinMarker.X + 10, JoinMarker.Y, FLinearColor::Yellow, 2);
        DrawLine(JoinMarker.X, JoinMarker.Y - 10, JoinMarker.X, JoinMarker.Y + 10, FLinearColor::Yellow, 2);
        DrawText(TEXT("接合元"), FColor::Yellow, JoinMarker.X + 12, JoinMarker.Y);
    }
    FVector2D GrabMarker;
    if (Observer->GetGrabMarker(GrabMarker))
    {
        DrawLine(GrabMarker.X - 10, GrabMarker.Y, GrabMarker.X + 10, GrabMarker.Y, FLinearColor::Yellow, 2);
        DrawLine(GrabMarker.X, GrabMarker.Y - 10, GrabMarker.X, GrabMarker.Y + 10, FLinearColor::Yellow, 2);
        DrawText(TEXT("把持点"), FColor::Yellow, GrabMarker.X + 12, GrabMarker.Y);
    }
    if (Observer->IsDrawingCut())
    {
        const FVector2D Start = Observer->GetCutStart();
        const FVector2D End = Observer->GetCutEnd();
        const FVector2D Direction = (End - Start).GetSafeNormal();
        const FVector2D A = Start - Direction * 4000;
        const FVector2D B = End + Direction * 4000;
        DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor::Yellow, 2.0f);
    }
}
