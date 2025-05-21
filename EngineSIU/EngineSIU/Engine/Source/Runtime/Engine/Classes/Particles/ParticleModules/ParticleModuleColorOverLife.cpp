#include "ParticleModuleColorOverLife.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleEmitter.h"
#include "UObject/Casts.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleSystem.h"
#include "Distributions/DistributionFloatUniform.h"
#include "Distributions/DistributionVectorUniform.h"
#include "UObject/ObjectFactory.h"
void UParticleModuleColorOverLife::InitializeDefaults()
{

}

void UParticleModuleColorOverLife::PostInitProperties()
{
    Super::PostInitProperties();
    InitializeDefaults();
    bEnabled = true;
}

void UParticleModuleColorOverLife::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    InitializeDefaults();
    Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UParticleModuleColorOverLife::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
    SPAWN_INIT
    float t = Particle.RelativeTime; // 0.0 ~ 1.0

    FLinearColor FinalColor;
    FinalColor.Lerp(StartColor, EndColor, t);

    Particle.Color = FinalColor;
    Particle.BaseColor = FinalColor;
}

void UParticleModuleColorOverLife::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
    if ((Owner == NULL) || (Owner->ActiveParticles <= 0) ||
        (Owner->ParticleData == NULL) || (Owner->ParticleIndices == NULL))
    {
        return;
    }

    BEGIN_UPDATE_LOOP;
    {
        float t = Particle.RelativeTime;

        FLinearColor FinalColor;
        FinalColor.Lerp(StartColor, EndColor, t);

        Particle.Color = FinalColor;
    }
    END_UPDATE_LOOP;
}
