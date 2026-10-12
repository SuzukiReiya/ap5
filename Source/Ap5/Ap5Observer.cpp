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

FString AAp5Observer::GetSelectedToolName() const
{
    switch (SelectedHotbarIndex)
    {
    case 0: return TEXT("ハンドガン");
    case 1: return TEXT("大口径銃");
    case 2: return TEXT("グレネードランチャー");
    case 3: return TEXT("ドリル");
    case 4: return TEXT("修復ツール");
    case 5: return TEXT("カッター");
    case 6: return TEXT("接合ツール");
    case 7: return TEXT("グラブツール");
    default: return TEXT("不明");
    }
}

void AAp5Observer::SelectHotbarIndex(int32 Index)
{
    constexpr int32 SlotCount=8;
    Index=(Index%SlotCount+SlotCount)%SlotCount;
    if (IsValid(TestMonster))
    {
        if (bGrabMode) TestMonster->EndGrab();
        if (bJoinMode) TestMonster->ClearJoinSelection();
    }

    SelectedHotbarIndex=Index;
    bProjectileMode=false;
    bBlastMode=false;
    bGrabMode=false;
    bImpactMode=false;
    bRepairMode=false;
    bCutMode=false;
    bJoinMode=false;
    bDrawingCut=false;
    FireCooldown=0.0f;

    switch (SelectedHotbarIndex)
    {
    case 0:
        bProjectileMode=true;
        BrushRadius=12.0f;
        EditStatus=TEXT("ハンドガン：右手からクロスヘアへ通常弾を発射");
        break;
    case 1:
        bProjectileMode=true;
        BrushRadius=30.0f;
        EditStatus=TEXT("大口径銃：右手から大きな弾痕を作る実体弾を発射");
        break;
    case 2:
        bBlastMode=true;
        BrushRadius=30.0f;
        EditStatus=TEXT("グレネードランチャー：着弾地点を半径45 cmで爆破");
        break;
    case 3:
        BrushRadius=20.0f;
        EditStatus=TEXT("ドリル：クロスヘア方向へ貫通穴を開ける");
        break;
    case 4:
        bRepairMode=true;
        BrushRadius=20.0f;
        EditStatus=TEXT("修復ツール：クロスヘア方向の欠損を修復");
        break;
    case 5:
        bCutMode=true;
        EditStatus=TEXT("カッター：左ドラッグで切断面を指定");
        break;
    case 6:
        bJoinMode=true;
        EditStatus=TEXT("接合ツール：破片を選び、次に接合先を選択");
        break;
    case 7:
        bGrabMode=true;
        EditStatus=TEXT("グラブツール：破片を掴んで移動・投擲");
        break;
    default:
        break;
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
        SelectHotbarIndex(SelectedHotbarIndex-1);
    if (WasInputKeyJustPressed(EKeys::MouseScrollDown))
        SelectHotbarIndex(SelectedHotbarIndex+1);
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
    if (WasInputKeyJustPressed(EKeys::BackSpace))
    {
        TestMonster->ResetShape();
        bDrawingCut = false;
        EditStatus = TEXT("モンスターを初期状態へ戻しました");
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
                    ? FString::Printf(TEXT("グレネード発射：爆発半径 %.0f cm"),BrushRadius*1.5f)
                    : FString::Printf(TEXT("%s：弾痕半径 %.0f cm"),
                        *GetSelectedToolName(),BrushRadius);
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
    DrawRect(FLinearColor(0,0,0,0.70f),12,12,780,92);
    const AAp5Observer* Observer=Cast<AAp5Observer>(GetOwningPlayerController());
    if (Observer==nullptr) return;
    DrawText(FString::Printf(TEXT("装備：%s　マウスホイール：装備切替"),
        *Observer->GetSelectedToolName()),FColor::White,24,20);
    DrawText(TEXT("TPS：W/S 前後　A/D 横歩き　Shift 走る　Space ジャンプ　PageUp/Down カメラ距離"),
        FColor::White,24,44);
    DrawText(Observer->GetEditStatus(),FColor::Yellow,24,68);
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

    // Minecraft風の8枠ホットバー。数字キーは使わず、ホイールだけで選択する。
    if (Canvas!=nullptr)
    {
        constexpr float SlotSize=48.0f;
        constexpr float SlotGap=6.0f;
        constexpr int32 SlotCount=8;
        const float TotalWidth=SlotCount*SlotSize+(SlotCount-1)*SlotGap;
        const float StartX=(Canvas->SizeX-TotalWidth)*0.5f;
        const float BarY=Canvas->SizeY-96.0f;

        for (int32 Slot=0;Slot<SlotCount;++Slot)
        {
            const float X=StartX+Slot*(SlotSize+SlotGap);
            DrawRect(FLinearColor(0.0f,0.0f,0.0f,0.68f),X,BarY,SlotSize,SlotSize);

            const bool bSelected=Slot==Observer->GetSelectedHotbarIndex();
            const FLinearColor Border=bSelected ? FLinearColor::Yellow : FLinearColor(0.7f,0.7f,0.7f,0.9f);
            const float Thickness=bSelected ? 3.0f : 1.0f;
            DrawLine(X,BarY,X+SlotSize,BarY,Border,Thickness);
            DrawLine(X+SlotSize,BarY,X+SlotSize,BarY+SlotSize,Border,Thickness);
            DrawLine(X+SlotSize,BarY+SlotSize,X,BarY+SlotSize,Border,Thickness);
            DrawLine(X,BarY+SlotSize,X,BarY,Border,Thickness);

            const float CX=X+SlotSize*0.5f;
            const float CY=BarY+SlotSize*0.5f;
            const FLinearColor Icon=FLinearColor::White;
            switch (Slot)
            {
            case 0: // handgun
                DrawRect(Icon,CX-15,CY-8,25,7);
                DrawLine(CX+4,CY-1,CX-1,CY+12,Icon,4);
                break;
            case 1: // heavy gun
                DrawRect(Icon,CX-18,CY-9,32,10);
                DrawLine(CX+5,CY,CX,CY+14,Icon,5);
                break;
            case 2: // grenade launcher
                DrawRect(Icon,CX-15,CY-6,28,9);
                DrawRect(Icon,CX+8,CY-10,8,17);
                DrawLine(CX-3,CY+2,CX-6,CY+13,Icon,4);
                break;
            case 3: // drill
                DrawRect(Icon,CX-16,CY-6,20,12);
                DrawLine(CX+4,CY-8,CX+17,CY,Icon,2);
                DrawLine(CX+17,CY,CX+4,CY+8,Icon,2);
                break;
            case 4: // repair
                DrawRect(Icon,CX-4,CY-15,8,30);
                DrawRect(Icon,CX-15,CY-4,30,8);
                break;
            case 5: // cutter
                DrawLine(CX-14,CY+14,CX+13,CY-13,Icon,5);
                DrawLine(CX-16,CY+16,CX-8,CY+8,Icon,8);
                break;
            case 6: // join
                DrawRect(Icon,CX-17,CY-7,11,14);
                DrawRect(Icon,CX+6,CY-7,11,14);
                DrawLine(CX-6,CY,CX+6,CY,Icon,4);
                break;
            case 7: // grab
                DrawRect(Icon,CX-5,CY+2,10,11);
                DrawLine(CX-5,CY+3,CX-14,CY-8,Icon,3);
                DrawLine(CX,CY+2,CX,CY-13,Icon,3);
                DrawLine(CX+5,CY+3,CX+14,CY-8,Icon,3);
                break;
            default:
                break;
            }
        }

        const FString ToolName=Observer->GetSelectedToolName();
        float TextWidth=0.0f,TextHeight=0.0f;
        GetTextSize(ToolName,TextWidth,TextHeight,nullptr,1.0f);
        DrawText(ToolName,FColor::Yellow,
            Canvas->SizeX*0.5f-TextWidth*0.5f,BarY+SlotSize+6.0f);
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
