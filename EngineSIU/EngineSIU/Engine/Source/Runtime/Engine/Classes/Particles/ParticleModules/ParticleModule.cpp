#include "ParticleModule.h"
#include "Particles/ParticleSystem.h"
#include "UObject/Casts.h"
#include "Particles/ParticleHelper.h"

void UParticleModule::PostEditChangeProperty()
{
    UParticleSystem* ParticleSystem = Cast<UParticleSystem>(GetOuter());
    if (ParticleSystem)
    {
        ParticleSystem->BuildEmitters();
    }
}

uint32 UParticleModule::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
    return 0;
}

uint32 UParticleModule::RequiredBytesPerInstance() const
{
    return 0;
}

void UParticleModule::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
}

void UParticleModule::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
}

void UParticleModule::FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
}
