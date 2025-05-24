#pragma once
#include "PhysicsCore/BodySetupCore.h"
#include "AggregateGeom.h"

/* 실제 물리 Body 생성 및 시뮬레이션 적용
 * 일반 물리 Body에서만 사용
 */
class UBodySetup : public UBodySetupCore
{
    DECLARE_CLASS(UBodySetup, UBodySetupCore)
public:
    UBodySetup() = default;


    FKAggregateGeom AggGeom;
};
