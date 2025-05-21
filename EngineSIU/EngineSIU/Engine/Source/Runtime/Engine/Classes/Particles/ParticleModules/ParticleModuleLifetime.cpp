#include "ParticleModuleLifetime.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "UObject/ObjectFactory.h"
#include <Distributions/DistributionFloatUniform.h>


UParticleModuleLifetime::UParticleModuleLifetime()
{
    bEnabled = true;
    bSpawnModule = true;
    bUpdateModule = true;
}

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
    SpawnEx(Owner, Offset, SpawnTime, &GetRandomStream(Owner), ParticleBase);
}

void UParticleModuleLifetime::SpawnEx(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FRandomStream* InRandomStream, FBaseParticle* ParticleBase)
{
    SPAWN_INIT;

    float MaxLifeTime = LifeTime.GetValue(Owner->EmitterTime, (UObject*)Owner->Component, InRandomStream);
    if (Particle.OneOverMaxLifetime > 0.f)
    {
        Particle.OneOverMaxLifetime = 1.f / (MaxLifeTime + 1.f / Particle.OneOverMaxLifetime);
    }
    else
    {
        Particle.OneOverMaxLifetime = MaxLifeTime > 0.f ? 1.f / MaxLifeTime : 0.f;
    }

    Particle.RelativeTime = Particle.RelativeTime > 1.0f ? Particle.RelativeTime : SpawnTime * Particle.OneOverMaxLifetime;
    //UE_LOG(ELogLevel::Warning, "OneOverMaxLifeTime : %f, RelativeTime : %f", Particle.OneOverMaxLifetime, Particle.RelativeTime);
}

void UParticleModuleLifetime::PostInitProperties()
{
    Super::PostInitProperties();
    InitializeDefaults();
    bEnabled = true;
}

void UParticleModuleLifetime::InitializeDefaults()
{
    if (!LifeTime.IsCreated())
    {
        LifeTime.Distribution = 
            FObjectFactory::ConstructObject<UDistributionFloatUniform>(this, TEXT("DistributionLifetime"));
    }
}

