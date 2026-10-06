#include "Ap5Observer.h"
#include "Ap5Monster.h"
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

void AAp5Observer::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
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
    if (WasInputKeyJustPressed(EKeys::One)) { bImpactMode = false; bRepairMode = false; bCutMode = false; bDrawingCut = false; }
    if (WasInputKeyJustPressed(EKeys::Two)) { bImpactMode = false; bRepairMode = true; bCutMode = false; bDrawingCut = false; }
    if (WasInputKeyJustPressed(EKeys::Six)) { bImpactMode = false; bCutMode = true; bDrawingCut = false; }
    if (WasInputKeyJustPressed(EKeys::Seven)) { bImpactMode = true; bRepairMode = false; bCutMode = false; bDrawingCut = false; }
    if (WasInputKeyJustPressed(EKeys::Three)) BrushRadius = 12;
    if (WasInputKeyJustPressed(EKeys::Four)) BrushRadius = 20;
    if (WasInputKeyJustPressed(EKeys::Five)) BrushRadius = 30;
    if (!IsValid(TestMonster)) return;
    if (WasInputKeyJustPressed(EKeys::BackSpace))
    {
        TestMonster->ResetShape();
        bDrawingCut = false;
        EditStatus = TEXT("全形状を初期状態に戻しました（修復操作とは別）");
    }
    // 案内の上と視点ドラッグ中は加工しない。長押しによる連続加工も行わない。
    const bool bOverHUD = bMouseAvailable && MouseX >= 12 && MouseX <= 1012 && MouseY >= 12 && MouseY <= 146;
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
            const int32 ChangedSamples = bImpactMode ? TestMonster->ApplyImpact(Start, Direction, BrushRadius)
                : TestMonster->ApplyBrush(Start, Direction, BrushRadius, bRepairMode);
            EditStatus = FString::Printf(TEXT("%s：更新 %d 格子点 / CPU処理 %.1f ms（描画完了までの時間は含みません）"),
                bImpactMode ? TEXT("弾痕") : (bRepairMode ? TEXT("修復") : TEXT("穴あけ")), ChangedSamples, TestMonster->LastEditMilliseconds);
        }
    }
}

void AAp5ObserverHUD::DrawHUD()
{
    Super::DrawHUD();
    DrawRect(FLinearColor(0, 0, 0, 0.75f), 12, 12, 1000, 134);
    const AAp5Observer* Observer = Cast<AAp5Observer>(GetOwningPlayerController());
    if (Observer == nullptr) return;
    DrawText(FString::Printf(TEXT("%s　半径 %.0f cm　7：弾痕　1：貫通穴　2：修復　6：切断（左ドラッグ）"),
        Observer->IsImpactMode() ? TEXT("弾痕") : (Observer->IsCutMode() ? TEXT("切断") : (Observer->IsRepairMode() ? TEXT("修復") : TEXT("穴あけ"))),
        Observer->GetBrushRadius()), FColor::White, 24, 20);
    DrawText(TEXT("3：細い（12 cm）　4：標準（20 cm）　5：太い（30 cm）　Backspace：形状を全リセット"), FColor::White, 24, 44);
    DrawText(TEXT("右ドラッグ／矢印：回転　ホイール／PageUp・Down：ズーム　R：視点を戻す　Esc：終了"), FColor::White, 24, 68);
    DrawText(Observer->GetEditStatus(), FColor::Yellow, 24, 92);
    DrawText(TEXT("弾痕は繰り返し当てると貫通。本体・破片とも加工可能。修復は各塊の切断時の形まで。"), FColor::White, 24, 116);
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
