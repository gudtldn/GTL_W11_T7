#include "ParticleSpriteEmitter.h"
#include "ParticleLODLevel.h"
#include "ParticleModules/ParticleModuleTypeDataBase.h"
#include "ParticleEmitterInstances.h"


FParticleEmitterInstance* UParticleSpriteEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
    FParticleEmitterInstance* Instance = nullptr;

    UParticleLODLevel* LODLevel = GetLODLevel(0);
    assert(LODLevel);

    if (LODLevel->TypeDataModule)
    {
        Instance = LODLevel->TypeDataModule->CreateInstance(this, InComponent);
    }
    else
    {
        assert(InComponent);
        Instance = new FParticleSpriteEmitterInstance();
        assert(Instance);
    }

    return Instance;
}

