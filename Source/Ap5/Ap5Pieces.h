#pragma once

#include "Ap5Volume.h"
#include <memory>

namespace Ap5Volume
{
// 分離前の形状を兄弟間で共有する。複数回切った破片も共通の分離元を探せる。
struct Separation
{
    Field Before;
    std::shared_ptr<const Separation> Parent;
};

// 本体と破片は同じデータを持ち、固定の有無だけで落下を切り替える。
struct Piece
{
    Field Volume;
    std::shared_ptr<const Separation> Origin;
    FallState Motion;
    bool Fixed=false;
    // Drivenは関節から拘束されている塊。JointAnchorを失った子は自由破片になる。
    bool Driven=false;
    // 駆動部位を識別する。-1は非駆動、0は従来の右腕、1/2は左右脚。
    int DriveId=-1;
    // ParentDriveId>=0 の部位は親側の ParentAnchor が残っている間だけ拘束される。
    // 例：下腿は大腿の膝側アンカーが欠損したら自由破片へ移行する。
    int ParentDriveId=-1;
    Point JointPivot, JointAnchor, ParentAnchor;
    // 足裏接地判定用。初期形状の最下段材料点を保持し、破壊後はVolume.Sampleで残存確認する。
    std::vector<Point> GroundSupportPoints;

    Point Translation;
    Point AxisX=Point(1,0,0), AxisY=Point(0,1,0), AxisZ=Point(0,0,1);
    // 親の原点における並進速度と角速度。UEとの受け渡しはモンスター座標系。
    Point OriginVelocity, AngularVelocity;
    Point ToLocalVector(const Point& V) const { return Point(V.Dot(AxisX),V.Dot(AxisY),V.Dot(AxisZ)); }
    Point ToWorldVector(const Point& V) const { return AxisX*V.X+AxisY*V.Y+AxisZ*V.Z; }
    Point ToLocal(const Point& World) const { return ToLocalVector(World-Translation-Point(0,0,Motion.OffsetZ)); }
    Point ToWorld(const Point& Local) const { return ToWorldVector(Local)+Translation+Point(0,0,Motion.OffsetZ); }
    Point VelocityAt(const Point& World) const { return OriginVelocity+AngularVelocity.Cross(World-ToWorld(Point())); }

    IndexedSurface RebuildIndexedSurface(double ChunkSize=0.0)
    {
        IndexedSurface Mesh=Volume.BuildIndexedSurface(ChunkSize);
        double MinimumZ=1e9;
        for (const Point& P : Mesh.Vertices) MinimumZ=std::min(MinimumZ,P.Z);
        Motion.MinimumZ=Mesh.Vertices.empty() ? 0 : MinimumZ;
        // 着地した破片を削る・分けると、支えていた面がなくなる場合がある。
        if (!Fixed) Motion.Landed=false;
        return Mesh;
    }

    std::vector<Triangle> RebuildSurface()
    {
        std::vector<Triangle> Mesh=Volume.Surface();
        double MinimumZ=1e9;
        for (const Triangle& Face : Mesh)
            for (const Point& P : Face.Vertices) MinimumZ=std::min(MinimumZ,P.Z);
        Motion.MinimumZ=Mesh.empty() ? 0 : MinimumZ;
        if (!Fixed) Motion.Landed=false;
        return Mesh;
    }
};

struct DrivenPartSetup
{
    Field Volume;
    Point JointPivot, JointAnchor, ParentAnchor;
    int DriveId=-1;
    int ParentDriveId=-1;
    bool GroundSupport=false;
};

class PieceCollection
{
public:
    std::vector<Piece> Items;

    void Reset(const Field& Initial,const std::vector<Point>& Supports = {})
    {
        Items.clear();
        SupportUsesStability=Supports.empty();
        SupportPoints=SupportUsesStability ? Initial.LowestMaterialPoints() : Supports;
        Piece Body;
        Body.Volume=Initial;
        Body.Fixed=HasStableSupport(Initial);
        Items.push_back(std::move(Body));
    }

    // 関節検証用に一つの体積を切りしろ無しで二分する。
    // JointAnchorを含む側だけを関節駆動し、もう一方は通常の支持点で固定する。
    bool ResetArticulated(const Field& Initial,const Point& PlanePoint,const Point& PlaneNormal,
        const Point& JointPivot,const Point& JointAnchor,const std::vector<Point>& Supports = {})
    {
        SupportUsesStability=Supports.empty();
        SupportPoints=SupportUsesStability ? Initial.LowestMaterialPoints() : Supports;
        std::vector<Field> Parts=Initial.Partition(PlanePoint,PlaneNormal);
        if (Parts.size()!=2)
        {
            Reset(Initial,Supports);
            return false;
        }
        int DrivenIndex=-1, DrivenCount=0;
        for (int I=0;I<2;++I)
        {
            if (Parts[I].Sample(JointAnchor)<0)
            {
                DrivenIndex=I;
                ++DrivenCount;
            }
        }
        if (DrivenCount!=1)
        {
            Reset(Initial,Supports);
            return false;
        }
        Items.clear();
        for (int I=0;I<2;++I)
        {
            Piece Part;
            Part.Volume=std::move(Parts[I]);
            Part.Driven=(I==DrivenIndex);
            Part.DriveId=Part.Driven ? 0 : -1;
            Part.Fixed=Part.Driven || HasStableSupport(Part.Volume);
            Part.JointPivot=JointPivot;
            Part.JointAnchor=JointAnchor;
            Items.push_back(std::move(Part));
        }
        return true;
    }

    // 複数の独立した関節Pieceを検証するための初期化。
    // 本体は明示支持点で固定し、各駆動部位はJointAnchorを保持している間だけ拘束する。
    bool ResetDrivenParts(const Field& Body,const std::vector<DrivenPartSetup>& DrivenParts,
        const std::vector<Point>& BodySupports)
    {
        if (Body.MaterialCount()==0 || DrivenParts.empty() || BodySupports.empty()) return false;
        SupportUsesStability=false;
        SupportPoints=BodySupports;
        Items.clear();

        Piece Root;
        Root.Volume=Body;
        Root.Fixed=HasSupport(Root.Volume);
        if (!Root.Fixed) return false;
        Items.push_back(std::move(Root));

        for (const DrivenPartSetup& Setup : DrivenParts)
        {
            if (Setup.Volume.MaterialCount()==0 || Setup.DriveId<0
                || Setup.Volume.Sample(Setup.JointAnchor)>=0)
            {
                Items.clear();
                return false;
            }
            Piece Part;
            Part.Volume=Setup.Volume;
            Part.Driven=true;
            Part.DriveId=Setup.DriveId;
            Part.ParentDriveId=Setup.ParentDriveId;
            Part.Fixed=true;
            Part.JointPivot=Setup.JointPivot;
            Part.JointAnchor=Setup.JointAnchor;
            Part.ParentAnchor=Setup.ParentAnchor;
            if (Setup.GroundSupport)
                Part.GroundSupportPoints=Part.Volume.LowestMaterialPoints();
            Items.push_back(std::move(Part));
        }
        return true;
    }

    // 親関節との材料接続を失った子関節を自由化する。
    // 多段チェーンにも使えるよう、変化がなくなるまで繰り返す。
    int ReleaseOrphanedDrivenParts(std::vector<int>& Released)
    {
        Released.clear();
        bool Changed=true;
        while (Changed)
        {
            Changed=false;
            for (size_t I=0;I<Items.size();++I)
            {
                Piece& Child=Items[I];
                if (!Child.Driven || Child.ParentDriveId<0) continue;

                const Piece* Parent=nullptr;
                for (const Piece& Candidate : Items)
                {
                    if (Candidate.Driven && Candidate.DriveId==Child.ParentDriveId)
                    {
                        Parent=&Candidate;
                        break;
                    }
                }
                if (Parent!=nullptr && Parent->Volume.Sample(Child.ParentAnchor)<0) continue;

                Child.Driven=false;
                Child.DriveId=-1;
                Child.ParentDriveId=-1;
                Child.Fixed=false;
                Child.Motion.Landed=false;
                Released.push_back(static_cast<int>(I));
                Changed=true;
            }
        }
        return static_cast<int>(Released.size());
    }

    // 支持を完全に失ったとき、現在の全Pieceを関節拘束から外して物理自由化する。
    // 歩行速度と転倒角速度を加算し、直前の関節速度は保持する。
    void ReleaseAllForPhysics(const Point& AddedVelocity,const Point& AddedAngularVelocity)
    {
        for (Piece& P : Items)
        {
            if (P.Volume.MaterialCount()==0) continue;
            P.Fixed=false;
            P.Driven=false;
            P.DriveId=-1;
            P.ParentDriveId=-1;
            P.Motion.Landed=false;
            P.OriginVelocity=P.OriginVelocity+AddedVelocity;
            P.AngularVelocity=P.AngularVelocity+AddedAngularVelocity;
        }
    }

    // 先に全対象の加工結果を準備する。分離上限に達したら一切変更しない。
    int Brush(const Point& Start,const Point& Direction,double Radius,bool Repair,std::vector<int>& Changed,int MaximumPieces=33)
    {
        Changed.clear();
        int Total=0, Added=0;
        const size_t Before=Items.size();
        std::vector<std::vector<Field>> Plans(Before);
        for (size_t I=0;I<Before;++I)
        {
            Field Edited=Items[I].Volume;
            const int Count=Edited.Brush(Items[I].ToLocal(Start),Items[I].ToLocalVector(Direction),Radius,Repair);
            if (Count==0) continue;
            if (!PrepareEdit(Edited,Repair,Plans[I])) return -1;
            Added+=static_cast<int>(Plans[I].size())-1;
            if (static_cast<int>(Before)+Added>MaximumPieces) return -1;
            Total+=Count;
        }
        for (size_t I=0;I<Before;++I) ApplyParts(I,Plans[I],Changed);
        return Total;
    }

    int Pick(const Point& Start,const Point& Direction,double& Nearest) const
    {
        Nearest=4000;
        if (Direction.Length()<1e-12) return -1;
        const Point Axis=Direction.Unit();
        int Target=-1;
        for (size_t I=0;I<Items.size();++I)
        {
            double Distance=0;
            if (Items[I].Volume.Trace(Items[I].ToLocal(Start),Items[I].ToLocalVector(Axis),Nearest,Distance))
            {
                Nearest=Distance;
                Target=static_cast<int>(I);
            }
        }
        return Target;
    }

    // 球形弾は「中心点から材料までの距離 <= 弾半径」で判定する。
    // 1フレームの移動量が弾径より大きいため、前回中心→現在中心を数点だけサンプリングし、
    // 9本のレイスイープより安価にトンネリングも防ぐ。
    int PickSwept(const Point& Start,const Point& End,double Radius,double& Nearest,Point& Hit) const
    {
        const Point Segment=End-Start;
        const double Length=Segment.Length();
        Nearest=Length+std::max(0.0,Radius);
        if (Length<1e-12 || Radius<=0) return -1;
        const Point Axis=Segment*(1.0/Length);
        int Target=-1;

        for (size_t I=0;I<Items.size();++I)
        {
            const Piece& Candidate=Items[I];
            const Point LocalStart=Candidate.ToLocal(Start);
            const Point LocalEnd=Candidate.ToLocal(End);
            if (!Candidate.Volume.SegmentIntersectsBounds(LocalStart,LocalEnd,Radius)) continue;

            const double Step=std::max(0.5,
                std::min(Radius*0.5,Candidate.Volume.GridSpacing()*0.5));
            const int Steps=std::max(1,static_cast<int>(std::ceil(Length/Step)));
            double PreviousT=0.0;
            double PreviousDistance=Candidate.Volume.CollisionDistance(LocalStart)-Radius;
            if (PreviousDistance<=0)
            {
                if (0.0<Nearest)
                {
                    Target=static_cast<int>(I);
                    Nearest=0.0;
                    Hit=Start+Axis*Radius;
                }
                continue;
            }

            for (int S=1;S<=Steps;++S)
            {
                const double T=static_cast<double>(S)/Steps;
                const Point LocalPoint=LocalStart+(LocalEnd-LocalStart)*T;
                const double Distance=Candidate.Volume.CollisionDistance(LocalPoint)-Radius;
                if (Distance>0)
                {
                    PreviousT=T;
                    PreviousDistance=Distance;
                    continue;
                }

                // 最初の交差区間だけ数回二分し、着弾位置のずれを抑える。
                double A=PreviousT,B=T;
                for (int Refine=0;Refine<5;++Refine)
                {
                    const double Middle=(A+B)*0.5;
                    const Point MidPoint=LocalStart+(LocalEnd-LocalStart)*Middle;
                    if (Candidate.Volume.CollisionDistance(MidPoint)<=Radius) B=Middle;
                    else A=Middle;
                }
                const double Along=B*Length;
                if (Along<Nearest)
                {
                    Target=static_cast<int>(I);
                    Nearest=Along;
                    const Point Center=Start+Axis*Along;
                    // 最初の接触では球の前端が表面付近にあるため、加工位置は前端で近似する。
                    Hit=Center+Axis*Radius;
                }
                break;
            }
        }
        return Target;
    }

    // 選んだ破片を接合先の姿勢へ合わせる。成功時だけ元の破片を除去する。
    // 戻り値：接合先の新番号、-1=対象不正、-2=共通の分離元なし、-3=つながらない。
    int Join(int Source,int Target)
    {
        if (Source<0 || Target<0 || Source==Target || Source>=static_cast<int>(Items.size())
            || Target>=static_cast<int>(Items.size()) || Items[Source].Fixed) return -1;
        std::shared_ptr<const Separation> Common;
        for (std::shared_ptr<const Separation> A=Items[Source].Origin; A && !Common; A=A->Parent)
            for (std::shared_ptr<const Separation> B=Items[Target].Origin; B; B=B->Parent)
                if (A==B) { Common=A; break; }
        if (!Common) return -2;
        Field Joined;
        if (!Items[Source].Volume.JoinAtSeam(Items[Target].Volume,Common->Before,Joined)) return -3;
        Items[Target].Volume=std::move(Joined);
        Items[Target].Origin=Common;
        Items[Target].Motion.Landed=false;
        Items.erase(Items.begin()+Source);
        const int NewTarget=Target-(Source<Target ? 1 : 0);
        // 全兄弟が再び一つになった履歴は畳み、切断・接合の反復で蓄積させない。
        while (Items[NewTarget].Origin)
        {
            bool HasSibling=false;
            for (size_t I=0;I<Items.size();++I)
            {
                if (static_cast<int>(I)==NewTarget || Items[I].Volume.MaterialCount()==0) continue;
                for (std::shared_ptr<const Separation> P=Items[I].Origin; P; P=P->Parent)
                    if (P==Items[NewTarget].Origin) { HasSibling=true; break; }
                if (HasSibling) break;
            }
            if (HasSibling) break;
            Items[NewTarget].Origin=Items[NewTarget].Origin->Parent;
        }
        return NewTarget;
    }

    // 球状に材料を除去し、影響を受けて自由になった塊へ放射状の初速度を与える。
    // 形状変更は上限確認後にまとめて適用するため、途中だけ爆発することはない。
    int Blast(const Point& Center,double Radius,double Speed,std::vector<int>& Changed,int MaximumPieces=33)
    {
        Changed.clear();
        if (Radius<=0 || Speed<0) return 0;
        const size_t Before=Items.size();
        std::vector<std::vector<Field>> Plans(Before);
        int Total=0, Added=0;
        for (size_t I=0;I<Before;++I)
        {
            Field Edited=Items[I].Volume;
            const int Count=Edited.Blast(Items[I].ToLocal(Center),Radius);
            if (Count==0) continue;
            if (!PrepareEdit(Edited,false,Plans[I])) return -1;
            Added+=static_cast<int>(Plans[I].size())-1;
            if (static_cast<int>(Before)+Added>MaximumPieces) return -1;
            Total+=Count;
        }
        for (size_t I=0;I<Before;++I) ApplyParts(I,Plans[I],Changed);
        for (int Index : Changed)
        {
            if (Index<0 || Index>=static_cast<int>(Items.size())) continue;
            Piece& P=Items[Index];
            if (P.Fixed || P.Volume.MaterialCount()==0) continue;
            Point Direction=P.ToWorld(P.Volume.MaterialCentroid())-Center;
            if (Direction.Length()<1e-9) Direction=Point(0,0,1);
            P.OriginVelocity=P.OriginVelocity+Direction.Unit()*Speed;
            P.Motion.Landed=false;
        }
        return Total;
    }

    // 実体弾など、衝突で対象破片が既知の場合にその命中点へ直接加工する。
    int ImpactAt(int Target,const Point& Hit,const Point& Direction,double Radius,double Depth,
        std::vector<int>& Changed,int MaximumPieces=33)
    {
        Changed.clear();
        if (Target<0 || Target>=static_cast<int>(Items.size())
            || Direction.Length()<1e-12 || Radius<=0 || Depth<=0) return 0;
        const Point Axis=Direction.Unit();
        Field Edited=Items[Target].Volume;
        const int Count=Edited.Dent(Items[Target].ToLocal(Hit),Items[Target].ToLocalVector(Axis),Radius,Depth);
        if (Count==0) return 0;
        std::vector<Field> Parts;
        if (!PrepareEdit(Edited,false,Parts)) return -1;
        if (Items.size()+Parts.size()-1>static_cast<size_t>(MaximumPieces)) return -1;
        ApplyParts(static_cast<size_t>(Target),Parts,Changed);
        return Count;
    }

    // 手前の塊だけに当てる。次の一発では加工済みの表面を改めて探す。
    int Impact(const Point& Start,const Point& Direction,double Radius,double Depth,std::vector<int>& Changed,int MaximumPieces=33)
    {
        Changed.clear();
        if (Direction.Length()<1e-12 || Radius<=0 || Depth<=0) return 0;
        const Point Axis=Direction.Unit();
        double Nearest=0;
        const int Target=Pick(Start,Axis,Nearest);
        if (Target<0) return 0;
        return ImpactAt(Target,Start+Axis*Nearest,Axis,Radius,Depth,Changed,MaximumPieces);
    }

    // 同じ面が横切る全ての塊を加工する。上限超過時は一つも変更しない。
    int Cut(const Point& PlanePoint,const Point& Normal,std::vector<int>& Changed,int MaximumPieces=33)
    {
        Changed.clear();
        const size_t Before=Items.size();
        std::vector<std::vector<Field>> Plans(Before);
        int Added=0;
        for (size_t I=0;I<Before;++I)
        {
            Plans[I]=Items[I].Volume.Cut(Items[I].ToLocal(PlanePoint),Items[I].ToLocalVector(Normal));
            if (Plans[I].size()<2) continue;
            Added+=static_cast<int>(Plans[I].size())-1;
            if (static_cast<int>(Before)+Added>MaximumPieces) return -1;
        }
        for (size_t I=0;I<Before;++I)
            if (Plans[I].size()>1) ApplyParts(I,Plans[I],Changed);
        return Added;
    }

private:
    std::vector<Point> SupportPoints;
    bool SupportUsesStability=false;

    bool HasSupport(const Field& Volume) const
    {
        for (const Point& P : SupportPoints)
            if (Volume.Sample(P)<0) return true;
        return false;
    }

    // 自動取得した足裏支持では、材料が触れているだけでなく重心投影が支持範囲内かを見る。
    // 明示Supportsはテストや固定アンカー用途なので従来どおり「含むか」だけを判定する。
    bool HasStableSupport(const Field& Volume) const
    {
        if (!HasSupport(Volume)) return false;
        if (!SupportUsesStability) return true;

        double MinX=1e9,MinY=1e9,MaxX=-1e9,MaxY=-1e9;
        int Count=0;
        for (const Point& P : SupportPoints)
        {
            if (Volume.Sample(P)>=0) continue;
            MinX=std::min(MinX,P.X); MinY=std::min(MinY,P.Y);
            MaxX=std::max(MaxX,P.X); MaxY=std::max(MaxY,P.Y);
            ++Count;
        }
        if (Count==0) return false;
        const Point Center=Volume.MaterialCentroid();
        // 5cm格子1セル分だけ許容する。20cmでは片脚時に中央重心まで支持扱いになった。
        const double Margin=5.0;
        return Center.X>=MinX-Margin && Center.X<=MaxX+Margin
            && Center.Y>=MinY-Margin && Center.Y<=MaxY+Margin;
    }

    static bool PrepareEdit(Field& Edited,bool Repair,std::vector<Field>& Parts)
    {
        if (!Repair)
        {
            bool Exceeded=false;
            Parts=Edited.Components(&Exceeded,true);
            if (Exceeded) return false;
        }
        // 分離しなかった加工では修復上限を更新しない。へこみを元まで戻せる。
        // 全材料を削り切った場合も空の形状として保持し、無効な分離扱いにしない。
        if (Parts.size()<2)
        {
            Parts.clear();
            Parts.push_back(std::move(Edited));
        }
        return true;
    }

    // 切断・穴あけ・弾痕で共通の分離処理。親の位置と速度を子へ引き継ぐ。
    void ApplyParts(size_t Index,std::vector<Field>& Parts,std::vector<int>& Changed)
    {
        if (Parts.empty()) return;
        size_t Largest=0;
        int LargestCount=0;
        for (size_t J=0;J<Parts.size();++J)
        {
            const int Count=Parts[J].MaterialCount();
            if (Count>LargestCount) { LargestCount=Count; Largest=J; }
        }
        std::shared_ptr<const Separation> NewOrigin=Items[Index].Origin;
        if (Parts.size()>1)
        {
            std::shared_ptr<Separation> Record=std::make_shared<Separation>();
            Record->Before=Items[Index].Volume;
            Record->Parent=Items[Index].Origin;
            NewOrigin=Record;
        }
        const bool ParentFixed=Items[Index].Fixed;
        Piece ParentState;
        ParentState.Origin=NewOrigin;
        ParentState.Motion=Items[Index].Motion;
        ParentState.Driven=Items[Index].Driven;
        ParentState.DriveId=Items[Index].DriveId;
        ParentState.ParentDriveId=Items[Index].ParentDriveId;
        ParentState.JointPivot=Items[Index].JointPivot;
        ParentState.JointAnchor=Items[Index].JointAnchor;
        ParentState.ParentAnchor=Items[Index].ParentAnchor;
        ParentState.GroundSupportPoints=Items[Index].GroundSupportPoints;
        ParentState.Translation=Items[Index].Translation;
        ParentState.AxisX=Items[Index].AxisX;
        ParentState.AxisY=Items[Index].AxisY;
        ParentState.AxisZ=Items[Index].AxisZ;
        ParentState.OriginVelocity=Items[Index].OriginVelocity;
        ParentState.AngularVelocity=Items[Index].AngularVelocity;
        Items[Index].Volume=std::move(Parts[Largest]);
        Items[Index].Origin=NewOrigin;
        Items[Index].Driven=ParentState.Driven && Items[Index].Volume.Sample(ParentState.JointAnchor)<0;
        Items[Index].DriveId=Items[Index].Driven ? ParentState.DriveId : -1;
        Items[Index].ParentDriveId=Items[Index].Driven ? ParentState.ParentDriveId : -1;
        Items[Index].JointPivot=ParentState.JointPivot;
        Items[Index].ParentAnchor=ParentState.ParentAnchor;
        Items[Index].JointAnchor=ParentState.JointAnchor;
        Items[Index].Fixed=ParentFixed
            && (ParentState.Driven ? Items[Index].Driven : HasStableSupport(Items[Index].Volume));
        Items[Index].Motion.Landed=false;
        Changed.push_back(static_cast<int>(Index));
        // 大きさによらず、初期の支持点を残した子だけ固定する。自由になった塊は再固定しない。
        for (size_t J=0;J<Parts.size();++J)
        {
            if (J==Largest) continue;
            Piece Child=ParentState;
            Child.Volume=std::move(Parts[J]);
            Child.Driven=ParentState.Driven && Child.Volume.Sample(ParentState.JointAnchor)<0;
            Child.DriveId=Child.Driven ? ParentState.DriveId : -1;
            Child.ParentDriveId=Child.Driven ? ParentState.ParentDriveId : -1;
            Child.Fixed=ParentFixed
                && (ParentState.Driven ? Child.Driven : HasStableSupport(Child.Volume));
            Child.Motion.Landed=false;
            Changed.push_back(static_cast<int>(Items.size()));
            Items.push_back(std::move(Child));
        }
    }
};
}
