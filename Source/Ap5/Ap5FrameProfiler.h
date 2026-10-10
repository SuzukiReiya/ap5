#pragma once

#include "CoreMinimal.h"

// 低FPSフレームだけを対象に、処理カテゴリごとの時間をメモリ上で集計する。
// イベント内容やフレーム番号は保存せず、平均・最小・最大だけを固定サイズのCSVへ上書きする。
enum class EAp5ProfileProcess : uint8
{
    ProjectileSpawn,
    ProjectileSweep,
    PhysicsSync,
    VolumeEdit,
    SurfaceBuild,
    DynamicMeshBuild,
    SetMesh,
    CollisionBoxes,
    CollisionUpdate,
    PhysicsBodySetup,
    ComponentCreate,
    Unmeasured,
    Count
};

class FAp5FrameProfiler
{
public:
    static FAp5FrameProfiler& Get();

    // PlayerTickから各フレーム1回呼ぶ。DeltaSecondsはゲームフレーム全体の時間。
    void BeginFrame(float DeltaSeconds);
    void AddMilliseconds(EAp5ProfileProcess Process, double Milliseconds);

private:
    FAp5FrameProfiler() = default;
    void MoveToFrame(uint64 FrameNumber);
    void FinalizeCurrentFrame();
    void WriteSummaryIfNeeded();

    struct FSummary
    {
        double SumMilliseconds = 0.0;
        double MinMilliseconds = TNumericLimits<double>::Max();
        double MaxMilliseconds = 0.0;
    };

    static constexpr double SlowFrameThresholdMilliseconds = 20.0; // 50 FPS未満
    static constexpr int32 FileWriteIntervalFrames = 30;

    uint64 CurrentFrame = TNumericLimits<uint64>::Max();
    double CurrentFrameMilliseconds = 0.0;
    double CurrentMilliseconds[static_cast<int32>(EAp5ProfileProcess::Count)] = {};
    FSummary Summary[static_cast<int32>(EAp5ProfileProcess::Count)];

    uint64 SlowFrameCount = 0;
    int32 FramesSinceWrite = 0;
    bool bSummaryDirty = false;
};

class FAp5ProfileScope
{
public:
    explicit FAp5ProfileScope(EAp5ProfileProcess InProcess);
    ~FAp5ProfileScope();

private:
    EAp5ProfileProcess Process;
    double StartedSeconds = 0.0;
};
