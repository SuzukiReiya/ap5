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

    Point ToLocal(const Point& World) const { return World-Point(0,0,Motion.OffsetZ); }

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

    void Reset(const Field& Initial)
    {
        Items.clear();
        Piece Body;
        Body.Volume=Initial;
        Body.Fixed=true;
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
            const int Count=Edited.Brush(Items[I].ToLocal(Start),Direction,Radius,Repair);
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
            if (Items[I].Volume.Trace(Items[I].ToLocal(Start),Axis,Nearest,Distance))
            {
                Nearest=Distance;
                Target=static_cast<int>(I);
            }
        }
        if (Target<0) return 0;
        Field Edited=Items[Target].Volume;
        const int Count=Edited.Dent(Items[Target].ToLocal(Start+Axis*Nearest),Axis,Radius,Depth);
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
            Plans[I]=Items[I].Volume.Cut(Items[I].ToLocal(PlanePoint),Normal);
            if (Plans[I].size()<2) continue;
            Added+=static_cast<int>(Plans[I].size())-1;
            if (static_cast<int>(Before)+Added>MaximumPieces) return -1;
        }
        for (size_t I=0;I<Before;++I)
            if (Plans[I].size()>1) ApplyParts(I,Plans[I],Changed);
        return Added;
    }

private:
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
        const FallState ParentMotion=Items[Index].Motion;
        Items[Index].Volume=std::move(Parts[Largest]);
        Items[Index].Motion.Landed=false;
        Changed.push_back(static_cast<int>(Index));
        // 固定された塊を分けたときだけ、最大の子が固定を引き継ぐ。
        for (size_t J=0;J<Parts.size();++J)
        {
            if (J==Largest) continue;
            Piece Child;
            Child.Volume=std::move(Parts[J]);
            Child.Motion=ParentMotion;
            Child.Motion.Landed=false;
            Changed.push_back(static_cast<int>(Items.size()));
            Items.push_back(std::move(Child));
        }
    }
};
}
