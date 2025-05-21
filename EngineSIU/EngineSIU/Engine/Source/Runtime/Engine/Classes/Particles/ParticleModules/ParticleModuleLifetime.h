#pragma once
#include "ParticleModule.h"
#include "Distributions/DistributionFloat.h"

// NOTE : UE는 UParticleModuleLifetimeBase클래스의 상속이지만 그냥 바로 쓴다.
struct FParticleEmitterInstance;
struct FBaseParticle;

class UParticleModuleLifetime : public UParticleModule
{
    DECLARE_CLASS(UParticleModuleLifetime, UParticleModule)
public:
    UParticleModuleLifetime();
    virtual ~UParticleModuleLifetime() override = default;

    //float LifeTime = 1.0f;// 기본값 1초로 한다

    // !TODO : RawDistributionFloat 구현 후 그거 사용
    virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
    void SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, struct FRandomStream* InRandomStream, FBaseParticle* ParticleBase);

    virtual void PostInitProperties() override;
    void InitializeDefaults();


    virtual FName GetModuleName() const override
    {
        return FName(TEXT("Initial Lifetime"));
    }

    UPROPERTY_WITH_FLAGS(
        EditAnywhere,
        FRawDistributionFloat, LifeTime
    )
};
