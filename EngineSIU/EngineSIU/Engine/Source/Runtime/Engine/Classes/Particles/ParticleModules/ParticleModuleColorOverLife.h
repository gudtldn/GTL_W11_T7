#pragma once
#include "ParticleModuleColorBase.h"
#include "Math/Color.h"

class FPropertyChangedEvent;
class UParticleModuleColorOverLife :
    public UParticleModuleColorBase
{
    DECLARE_CLASS(UParticleModuleColorOverLife, UParticleModuleColorBase)
public:
    UParticleModuleColorOverLife() = default;
    virtual ~UParticleModuleColorOverLife() override = default;
    
    UPROPERTY_WITH_FLAGS(
        EditAnywhere,
        FLinearColor, StartColor
    )

    UPROPERTY_WITH_FLAGS(
        EditAnywhere,
        FLinearColor, EndColor
    )

    virtual FName GetModuleName() const override
    {
        return FName(TEXT("Color Over Life"));
    }

    void InitializeDefaults();
    virtual void PostInitProperties() override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
    virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
};

