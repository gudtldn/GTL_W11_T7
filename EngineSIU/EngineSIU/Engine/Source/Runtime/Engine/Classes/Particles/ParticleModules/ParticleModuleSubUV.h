#pragma once
#include "ParticleModule.h"

class UParticleModuleSubUV : public UParticleModule
{
    DECLARE_CLASS(UParticleModuleSubUV, UParticleModule)
public:
    UParticleModuleSubUV() = default;
    virtual ~UParticleModuleSubUV() override = default;

    virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
    virtual void PostInitProperties();
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

    virtual FName GetModuleName() const override;
    UPROPERTY(
        EditAnywhere,
        int32, SubImages_Horizontal, = 1;
    )

    UPROPERTY(
        EditAnywhere,
        int32, SubImages_Vertical, = 1;
    )
};
