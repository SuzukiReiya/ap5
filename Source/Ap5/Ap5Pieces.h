#pragma once

#include "Ap5Volume.h"

namespace Ap5Volume
{
// 本体と破片は同じデータを持ち、固定の有無だけで落下を切り替える。
struct Piece
{
    Field Volume;
    FallState Motion;
    bool Fixed=false;

    Point Translation;
    Point AxisX=Point(1,0,0), AxisY=Point(0,1,0), AxisZ=Point(0,0,1);
    // 親の原点における並進速度と角速度。UEとの受け渡しはモンスター座標系。
    Point OriginVelocity, AngularVelocity;
    Point ToLocalVector(const Point& V) const { return Point(V.Dot(AxisX),V.Dot(AxisY),V.Dot(AxisZ)); }
    Point ToWorldVector(const Point& V) const { return AxisX*V.X+AxisY*V.Y+AxisZ*V.Z; }
    Point ToLocal(const Point& World) const { return ToLocalVector(World-Translation-Point(0,0,Motion.OffsetZ)); }
    Point ToWorld(const Point& Local) const { return ToWorldVector(Local)+Translation+Point(0,0,Motion.OffsetZ); }
    Point VelocityAt(const Point& World) const { return OriginVelocity+AngularVelocity.Cross(World-ToWorld(Point())); }

    std::vector<Triangle> RebuildSurface()
    {
        std::vector<Triangle> Mesh=Volume.Surface();
        double MinimumZ=1e9;
        for (const Triangle& Face : Mesh)
            for (const Point& P : Face.Vertices) MinimumZ=std::min(MinimumZ,P.Z);
        Motion.MinimumZ=Mesh.empty() ? 0 : MinimumZ;
        // 着地した破片を削る・分けると、支えていた面がなくなる場合がある。
        if (!Fixed) Motion.Landed=false;
        return Mesh;
    }
};

class PieceCollection
{
public:
    std::vector<Piece> Items;

    void Reset(const Field& Initial,const std::vector<Point>& Supports = {})
    {
        Items.clear();
        SupportPoints=Supports.empty() ? Initial.LowestMaterialPoints() : Supports;
        Piece Body;
        Body.Volume=Initial;
        Body.Fixed=HasSupport(Initial);
        Items.push_back(std::move(Body));
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

    // 手前の塊だけに当てる。次の一発では加工済みの表面を改めて探す。
    int Impact(const Point& Start,const Point& Direction,double Radius,double Depth,std::vector<int>& Changed,int MaximumPieces=33)
    {
        Changed.clear();
        if (Direction.Length()<1e-12 || Radius<=0 || Depth<=0) return 0;
        const Point Axis=Direction.Unit();
        double Nearest=4000;
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
        if (Target<0) return 0;
        Field Edited=Items[Target].Volume;
        const int Count=Edited.Dent(Items[Target].ToLocal(Start+Axis*Nearest),Items[Target].ToLocalVector(Axis),Radius,Depth);
        if (Count==0) return 0;
        std::vector<Field> Parts;
        if (!PrepareEdit(Edited,false,Parts)) return -1;
        if (Items.size()+Parts.size()-1>static_cast<size_t>(MaximumPieces)) return -1;
        ApplyParts(static_cast<size_t>(Target),Parts,Changed);
        return Count;
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

    bool HasSupport(const Field& Volume) const
    {
        for (const Point& P : SupportPoints)
            if (Volume.Sample(P)<0) return true;
        return false;
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
        const bool ParentFixed=Items[Index].Fixed;
        Piece ParentState;
        ParentState.Motion=Items[Index].Motion;
        ParentState.Translation=Items[Index].Translation;
        ParentState.AxisX=Items[Index].AxisX;
        ParentState.AxisY=Items[Index].AxisY;
        ParentState.AxisZ=Items[Index].AxisZ;
        ParentState.OriginVelocity=Items[Index].OriginVelocity;
        ParentState.AngularVelocity=Items[Index].AngularVelocity;
        Items[Index].Volume=std::move(Parts[Largest]);
        Items[Index].Fixed=ParentFixed && HasSupport(Items[Index].Volume);
        Items[Index].Motion.Landed=false;
        Changed.push_back(static_cast<int>(Index));
        // 大きさによらず、初期の支持点を残した子だけ固定する。自由になった塊は再固定しない。
        for (size_t J=0;J<Parts.size();++J)
        {
            if (J==Largest) continue;
            Piece Child=ParentState;
            Child.Volume=std::move(Parts[J]);
            Child.Fixed=ParentFixed && HasSupport(Child.Volume);
            Child.Motion.Landed=false;
            Changed.push_back(static_cast<int>(Items.size()));
            Items.push_back(std::move(Child));
        }
    }
};
}
