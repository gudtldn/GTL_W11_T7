#include "ParticleModuleLifetime.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
    FBaseParticle& Particle = *(FBaseParticle*)ParticleBase;

    if (Particle.OneOverMaxLifetime > 0.f)
    {
        Particle.OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle.OneOverMaxLifetime);
    }
    else
    {
        Particle.OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
    }

    Particle.RelativeTime = Particle.RelativeTime > 1.0f ? Particle.RelativeTime : SpawnTime * Particle.OneOverMaxLifetime;
}
