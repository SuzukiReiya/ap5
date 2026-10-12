#include "Ap5Observer.h"
#include "Ap5FrameProfiler.h"
#include "Ap5Monster.h"
#include "Ap5Projectile.h"
#include "Ap5PlayerCharacter.h"
#include "EngineUtils.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "Engine/GameViewportClient.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"

AAp5Observer::AAp5Observer()
{
    bAutoManageActiveCameraTarget=true;
    bShowMouseCursor=false;
}

void AAp5Observer::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;

    // TPSでも相対マウス入力を使う。Camera自体はCharacterのSpringArm配下。
    FInputModeGameOnly Mode;
    Mode.SetConsumeCaptureMouseDown(false);
    SetInputMode(Mode);
    if (UGameViewportClient* Viewport=GetWorld()->GetGameViewport())
    {
        Viewport->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
        Viewport->SetMouseLockMode(EMouseLockMode::LockAlways);
    }
}

void AAp5Observer::ResetView()
{
    if (!IsValid(PlayerCharacter)) return;
    const float BodyYaw=PlayerCharacter->GetActorRotation().Yaw;
    SetControlRotation(FRotator(-8.0f,BodyYaw,0.0f));
    PlayerCharacter->ResetCameraDistance();
}

bool AAp5Observer::GetAimRay(FVector& Start,FVector& Direction) const
{
    if (!IsValid(PlayerCharacter) || !IsValid(PlayerCharacter->GetFollowCamera()))
        return false;
    Start=PlayerCharacter->GetFollowCamera()->GetComponentLocation();
    Direction=PlayerCharacter->GetFollowCamera()->GetForwardVector().GetSafeNormal();
    return !Direction.IsNearlyZero();
}

bool AAp5Observer::FireProjectile(bool bExplosive)
{
    FVector CameraStart,CameraDirection;
    if (!GetAimRay(CameraStart,CameraDirection) || !IsValid(PlayerCharacter))
        return false;

    // まず画面中央クロスヘアの照準点をカメラから求める。
    // 弾そのものは右手から照準点へ向けて発射するため、肩越し視点でも狙いと弾道が一致する。
    constexpr float AimDistanceCm=10000.0f;
    FVector AimPoint=CameraStart+CameraDirection*AimDistanceCm;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(Ap5TPSAim),false,PlayerCharacter);
    QueryParams.AddIgnoredActor(PlayerCharacter);
    FHitResult AimHit;
    if (GetWorld()->LineTraceSingleByChannel(
        AimHit,CameraStart,AimPoint,ECC_Visibility,QueryParams))
    {
        AimPoint=AimHit.ImpactPoint;
    }

    const FVector Muzzle=PlayerCharacter->GetProjectileMuzzleLocation();
    const FVector Axis=(AimPoint-Muzzle).GetSafeNormal();
    if (Axis.IsNearlyZero()) return false;

    FAp5ProfileScope Profile(EAp5ProfileProcess::ProjectileSpawn);
    FActorSpawnParameters Parameters;
    Parameters.Owner=PlayerCharacter;
    Parameters.Instigator=PlayerCharacter;
    Parameters.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AAp5Projectile* Projectile=GetWorld()->SpawnActor<AAp5Projectile>(
        Muzzle,Axis.Rotation(),Parameters);
    if (Projectile==nullptr) return false;

    Projectile->Launch(Axis,BrushRadius,bExplosive,BrushRadius*1.5f);
    return true;
}

void AAp5Observer::PlayerTick(float DeltaTime)
{
    // Singleton生成時にOnBeginFrame/OnEndFrameへ登録される。
    FAp5FrameProfiler::Get();
    Super::PlayerTick(DeltaTime);
    if (!IsLocalController()) return;

    if (!IsValid(PlayerCharacter))
    {
        PlayerCharacter=Cast<AAp5PlayerCharacter>(GetPawn());
        if (!IsValid(PlayerCharacter)) return;
        SetViewTarget(PlayerCharacter);
    }

    float MouseDeltaX=0.0f;
    float MouseDeltaY=0.0f;
    GetInputMouseDelta(MouseDeltaX,MouseDeltaY);

    FRotator ViewRotation=GetControlRotation();
    if (!bDrawingCut)
    {
        constexpr float MouseSensitivityX=0.65f;
        constexpr float MouseSensitivityY=0.35f;
        ViewRotation.Yaw+=MouseDeltaX*MouseSensitivityX;
        ViewRotation.Pitch+=MouseDeltaY*MouseSensitivityY;
    }

    const float RotationStep=60.0f*FMath::Min(DeltaTime,0.1f);
    if (IsInputKeyDown(EKeys::Left)) ViewRotation.Yaw-=RotationStep;
    if (IsInputKeyDown(EKeys::Right)) ViewRotation.Yaw+=RotationStep;
    if (IsInputKeyDown(EKeys::Up)) ViewRotation.Pitch+=RotationStep;
    if (IsInputKeyDown(EKeys::Down)) ViewRotation.Pitch-=RotationStep;
    ViewRotation.Pitch=FMath::Clamp(
        FRotator::NormalizeAxis(ViewRotation.Pitch),-70.0f,65.0f);
    ViewRotation.Yaw=FMath::UnwindDegrees(ViewRotation.Yaw);
    ViewRotation.Roll=0.0f;
    SetControlRotation(ViewRotation);

    // 移動方向とは独立に、身体のYawと銃口Pitchを常にクロスヘア方向へ固定する。
    // A/D/Sでストレイフ・後退しても見た目と射出口の向きは変えない。
    PlayerCharacter->SetActorRotation(
        FRotator(0.0f,ViewRotation.Yaw,0.0f),ETeleportType::None);
    PlayerCharacter->SetAimPitch(ViewRotation.Pitch);

    // TPS移動はカメラYaw基準。上下視点角は移動方向へ混ぜない。
    const FRotator MoveRotation(0.0f,ViewRotation.Yaw,0.0f);
    const FVector HorizontalForward=MoveRotation.Vector();
    const FVector HorizontalRight=FRotationMatrix(MoveRotation).GetUnitAxis(EAxis::Y);
    FVector Move=FVector::ZeroVector;
    if (IsInputKeyDown(EKeys::W)) Move+=HorizontalForward;
    if (IsInputKeyDown(EKeys::S)) Move-=HorizontalForward;
    if (IsInputKeyDown(EKeys::A)) Move-=HorizontalRight;
    if (IsInputKeyDown(EKeys::D)) Move+=HorizontalRight;
    PlayerCharacter->SetSprintEnabled(
        IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift));
    if (!Move.IsNearlyZero())
        PlayerCharacter->AddMovementInput(Move.GetSafeNormal(),1.0f);

    if (WasInputKeyJustPressed(EKeys::SpaceBar)) PlayerCharacter->Jump();
    if (WasInputKeyJustReleased(EKeys::SpaceBar)) PlayerCharacter->StopJumping();

    if (WasInputKeyJustPressed(EKeys::MouseScrollUp))
        PlayerCharacter->AdjustCameraDistance(-25.0f);
    if (WasInputKeyJustPressed(EKeys::MouseScrollDown))
        PlayerCharacter->AdjustCameraDistance(25.0f);
    const float ZoomSpeed=160.0f*FMath::Min(DeltaTime,0.1f);
    if (IsInputKeyDown(EKeys::PageUp))
        PlayerCharacter->AdjustCameraDistance(-ZoomSpeed);
    if (IsInputKeyDown(EKeys::PageDown))
        PlayerCharacter->AdjustCameraDistance(ZoomSpeed);

    if (WasInputKeyJustPressed(EKeys::R)) ResetView();
    if (WasInputKeyJustPressed(EKeys::Escape))
    {
        UKismetSystemLibrary::QuitGame(this,this,EQuitPreference::Quit,false);
    }
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
        EditStatus=TEXT("実体弾：右手から中央クロスヘアへ発射。左ボタン長押しで連射します。");
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
        EditStatus=TEXT("爆発弾：右手から中央クロスヘアへ発射。着弾地点で球状破壊します。");
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
    if (WasInputKeyJustPressed(EKeys::K))
    {
        const bool bEnabled=TestMonster->ToggleStructuralStrengthTest();
        EditStatus=bEnabled
            ? TEXT("構造強度ON：細い接続部は自己荷重で破断します。肩などを削って確認してください。")
            : TEXT("構造強度OFF：完全に非連結になるまで形状だけでは分離しません。");
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
    if (WasInputKeyJustPressed(EKeys::L))
    {
        bProjectileMode=false;
        bBlastMode=false;
        bGrabMode=false;
        bJoinMode=false;
        bDrawingCut=false;
        TestMonster->EndGrab();
        TestMonster->ClearJoinSelection();
        const bool bEnabled=TestMonster->ToggleLegMotionTest();
        EditStatus=bEnabled
            ? TEXT("簡易歩行を開始。接地脚／遊脚を切り替えながら前進します。脚欠損時は前進を停止します。")
            : TEXT("簡易歩行を終了し、開始位置で通常の一体形状へ戻しました。");
    }
    if (!bJoinMode) TestMonster->ClearJoinSelection();
    if (WasInputKeyJustPressed(EKeys::BackSpace))
    {
        TestMonster->ResetShape();
        bDrawingCut = false;
        EditStatus = TEXT("全形状を初期状態に戻しました（修復操作とは別）");
    }
    FireCooldown=FMath::Max(0.0f,FireCooldown-DeltaTime);
    if (bProjectileMode || bBlastMode)
    {
        if (IsInputKeyDown(EKeys::LeftMouseButton) && FireCooldown<=0.0f)
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
        if (WasInputKeyJustPressed(EKeys::LeftMouseButton)
            && GetAimRay(GrabStart,GrabDirection))
        {
            EditStatus=TestMonster->BeginGrab(GrabStart,GrabDirection);
        }
        if (IsInputKeyDown(EKeys::LeftMouseButton)
            && GetAimRay(GrabStart,GrabDirection))
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

    // 切断だけはFPS照準を起点に、押している間の相対マウス移動で2Dの切断線を描く。
    if (bDrawingCut)
    {
        int32 ViewX=0,ViewY=0;
        GetViewportSize(ViewX,ViewY);
        CutEnd.X=FMath::Clamp(CutEnd.X+MouseDeltaX,0.0f,static_cast<float>(ViewX));
        CutEnd.Y=FMath::Clamp(CutEnd.Y+MouseDeltaY,0.0f,static_cast<float>(ViewY));
        if (WasInputKeyJustReleased(EKeys::LeftMouseButton))
        {
            bDrawingCut=false;
            FVector StartOrigin,StartDirection,EndOrigin,EndDirection;
            if ((CutEnd-CutStart).Size()<12.0f)
            {
                EditStatus=TEXT("切断を取り消しました。左ボタンを押したままマウスを12px以上動かしてください。");
            }
            else if (DeprojectScreenPositionToWorld(CutStart.X,CutStart.Y,StartOrigin,StartDirection)
                && DeprojectScreenPositionToWorld(CutEnd.X,CutEnd.Y,EndOrigin,EndDirection))
            {
                const FVector Normal=FVector::CrossProduct(StartDirection,EndDirection).GetSafeNormal();
                const int32 Detached=TestMonster->Cut(StartOrigin,Normal);
                if (Detached<0) EditStatus=TEXT("破片上限32個です。Backspaceで全リセットしてください。");
                else if (Detached==0) EditStatus=TEXT("切断なし：本体または破片を横切る方向へ線を引いてください。");
                else EditStatus=FString::Printf(
                    TEXT("切断：%d個増加 / CPU処理 %.1f ms。破片も続けて加工できます。"),
                    Detached,TestMonster->LastEditMilliseconds);
            }
        }
        return;
    }

    if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
    {
        if (bCutMode)
        {
            int32 ViewX=0,ViewY=0;
            GetViewportSize(ViewX,ViewY);
            CutStart=CutEnd=FVector2D(ViewX*0.5f,ViewY*0.5f);
            bDrawingCut=true;
            return;
        }

        FVector Start,Direction;
        if (GetAimRay(Start,Direction))
        {
            if (bJoinMode)
            {
                EditStatus=TestMonster->ApplyJoin(Start,Direction);
                return;
            }
            const int32 ChangedSamples=bImpactMode
                ? TestMonster->ApplyImpact(Start,Direction,BrushRadius)
                : TestMonster->ApplyBrush(Start,Direction,BrushRadius,bRepairMode);
            if (ChangedSamples<0) EditStatus=TEXT("分離上限のため加工しませんでした。Backspaceで全リセットできます。");
            else if (bImpactMode) EditStatus=FString::Printf(
                TEXT("弾痕：更新 %d 格子点 / 分離 %d 個 / CPU処理 %.1f ms / %s"),
                ChangedSamples,TestMonster->LastSeparatedPieces,TestMonster->LastEditMilliseconds,
                *TestMonster->GetLastImpactMaterialText());
            else EditStatus=FString::Printf(TEXT("%s：更新 %d 格子点 / 分離 %d 個 / CPU処理 %.1f ms"),
                bRepairMode ? TEXT("修復") : TEXT("穴あけ"),
                ChangedSamples,TestMonster->LastSeparatedPieces,TestMonster->LastEditMilliseconds);
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
    DrawText(TEXT("3：細い　4：標準　5：太い　M：材質差　K：構造強度　9：右腕関節　L：簡易歩行　Backspace：リセット"), FColor::White, 24, 44);
    DrawText(TEXT("TPS：クロスヘア正面固定　W/S：前後　A/D：横歩き　Shift：走る　Space：ジャンプ　ホイール：距離"), FColor::White, 24, 68);
    DrawText(Observer->GetEditStatus(), FColor::Yellow, 24, 92);
    DrawText(TEXT("つながりを削り切ると分離・落下。本体・破片とも加工可能。修復は直近の分離・接合時の形まで。"), FColor::White, 24, 116);
    if (Canvas != nullptr)
    {
        const float CenterX=Canvas->SizeX*0.5f;
        const float CenterY=Canvas->SizeY*0.5f;

        // 肩越しTPSでも狙い位置が明確になる常時クロスヘア。
        // 黒い太線を下地にしてから白線を重ね、明るい背景でも見失いにくくする。
        const float Gap=5.0f;
        const float Length=10.0f;
        const float OuterThickness=3.5f;
        const float InnerThickness=1.5f;
        const FLinearColor Outline(0.0f,0.0f,0.0f,0.85f);
        const FLinearColor Foreground=FLinearColor::White;

        DrawLine(CenterX-Gap-Length,CenterY,CenterX-Gap,CenterY,Outline,OuterThickness);
        DrawLine(CenterX+Gap,CenterY,CenterX+Gap+Length,CenterY,Outline,OuterThickness);
        DrawLine(CenterX,CenterY-Gap-Length,CenterX,CenterY-Gap,Outline,OuterThickness);
        DrawLine(CenterX,CenterY+Gap,CenterX,CenterY+Gap+Length,Outline,OuterThickness);

        DrawLine(CenterX-Gap-Length,CenterY,CenterX-Gap,CenterY,Foreground,InnerThickness);
        DrawLine(CenterX+Gap,CenterY,CenterX+Gap+Length,CenterY,Foreground,InnerThickness);
        DrawLine(CenterX,CenterY-Gap-Length,CenterX,CenterY-Gap,Foreground,InnerThickness);
        DrawLine(CenterX,CenterY+Gap,CenterX,CenterY+Gap+Length,Foreground,InnerThickness);
        DrawRect(FLinearColor::White,CenterX-1.0f,CenterY-1.0f,2.0f,2.0f);
    }
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
