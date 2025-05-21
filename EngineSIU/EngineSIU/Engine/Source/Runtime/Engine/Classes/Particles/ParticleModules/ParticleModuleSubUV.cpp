#include "ParticleModuleSubUV.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleEmitter.h"

void UParticleModuleSubUV::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
    Owner->SubImages_Horizontal = SubImages_Horizontal;
    Owner->SubImages_Vertical = SubImages_Vertical;
}

void UParticleModuleSubUV::PostInitProperties()
{
    bEnabled = true;
    bSpawnModule = true;
}

void UParticleModuleSubUV::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    
}

FName UParticleModuleSubUV::GetModuleName() const
{
    return FName(TEXT("SubUV"));
}
