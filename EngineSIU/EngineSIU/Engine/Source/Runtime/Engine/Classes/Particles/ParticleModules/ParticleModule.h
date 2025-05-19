#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

struct FParticleEmitterInstance;
struct FBaseParticle;
class UParticleModuleTypeDataBase;


enum EModuleType : int
{
    /** General - all emitter types can use it			*/
    EPMT_General,
    /** TypeData - TypeData modules						*/
    EPMT_TypeData,
    /** Beam - only applied to beam emitters			*/
    EPMT_Beam,
    /** Trail - only applied to trail emitters			*/
    EPMT_Trail,
    /** Spawn - all emitter types REQUIRE it			*/
    EPMT_Spawn,
    /** Required - all emitter types REQUIRE it			*/
    EPMT_Required,
    /** Event - event related modules					*/
    EPMT_Event,
    /** Light related modules							*/
    EPMT_Light,
    /** SubUV related modules							*/
    EPMT_SubUV,
    EPMT_MAX,
};

class UParticleModule : public UObject
{
    DECLARE_CLASS(UParticleModule, UObject)

public:
    UParticleModule() = default;
    virtual ~UParticleModule() override = default;

    uint8 bSpawnModule : 1;
    uint8 bEnabled : 1;
    uint8 bUpdateModule : 1;

    virtual EModuleType GetModuleType() const { return EPMT_General; }
    virtual void PostEditChangeProperty();
    /**
    * 파티클 페이로드 블록의 크기를 반환
    * 
    * @param    TypeData    이 모듈을 소유하고 있는 UParticleModuleTypeDataBase
    * @return   uint32      파티클별 이 모듈이 필요로 하는 바이트의 수
    */
    virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) const;  // should be overridden by derived classes
    virtual uint32 RequiredBytesPerInstance() const;                            // should be overridden by derived classes

    virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase);
    virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime);
    virtual void FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime);
};
