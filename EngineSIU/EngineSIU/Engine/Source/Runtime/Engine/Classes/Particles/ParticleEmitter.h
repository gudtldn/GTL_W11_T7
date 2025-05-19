#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class UParticleLODLevel;
class UParticleSystemComponent;
class UParticleModule;
struct FParticleEmitterInstance;


class UParticleEmitter : public UObject
{
    DECLARE_CLASS(UParticleEmitter, UObject)

public:
    UParticleEmitter() = default;
    virtual ~UParticleEmitter() override = default;

    virtual FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* InComponent);
    UParticleLODLevel* GetCurrentLODLevel(const FParticleEmitterInstance* Instance) const;
    virtual void UpdateModuleLists();

    void Build();
    void CacheEmitterModuleInfo();
    UParticleLODLevel* GetLODLevel(int32 LODLevel);

    UPROPERTY_WITH_FLAGS(
        EditAnywhere,
        FName, EmitterName
    )

    int32 ParticleSize = 0;
    int32 TypeDataOffset = 0;

    int32 TypeDataInstanceOffset = 0;
    int32 ReqInstanceBytes = 0;

    TArray<UParticleLODLevel*> LODLevels;

    TMap<UParticleModule*, uint32> ModuleOffsetMap;
    TMap<UParticleModule*, uint32> ModuleInstanceOffsetMap;

    TArray<UParticleModule*> ModulesNeedingInstanceData;
};
