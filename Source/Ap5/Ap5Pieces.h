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

    int Brush(const Point& Start,const Point& Direction,double Radius,bool Repair,std::vector<int>& Changed)
    {
        Changed.clear();
        int Total=0;
        for (size_t I=0;I<Items.size();++I)
        {
            Piece& Item=Items[I];
            const int Count=Item.Volume.Brush(Item.ToLocal(Start),Direction,Radius,Repair);
            if (Count>0) { Changed.push_back(static_cast<int>(I)); Total+=Count; }
        }
        return Total;
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
        {
            std::vector<Field>& Parts=Plans[I];
            if (Parts.size()<2) continue;
            size_t Largest=0;
            int LargestCount=0;
            for (size_t J=0;J<Parts.size();++J)
            {
                const int Count=Parts[J].MaterialCount();
                if (Count>LargestCount) { LargestCount=Count; Largest=J; }
            }
            const FallState ParentMotion=Items[I].Motion;
            Items[I].Volume=std::move(Parts[Largest]);
            Changed.push_back(static_cast<int>(I));
            // 固定された塊を切ったときだけ、最大の子が固定を引き継ぐ。
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
        return Added;
    }
};
}
