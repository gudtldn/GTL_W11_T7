#include "ParticleEmitter.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModules/ParticleModule.h"
#include "Particles/ParticleModules/ParticleModuleTypeDataBase.h"

FParticleEmitterInstance* UParticleEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
    UE_LOG(ELogLevel::Error, TEXT("UParticleEmitter::CreateInstance is pure virtual"));
    return nullptr;
}

UParticleLODLevel* UParticleEmitter::GetCurrentLODLevel(const FParticleEmitterInstance* Instance) const
{
    // !NOTE : 지금은 LOD레벨 1개
    return Instance->CurrentLODLevel;
}

void UParticleEmitter::UpdateModuleLists()
{
    for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
    {
        UParticleLODLevel* LODLevel = LODLevels[LODIndex];
        if (LODLevel)
        {
            LODLevel->UpdateModuleLists();
        }
    }
    Build();
}

void UParticleEmitter::Build()
{
    CacheEmitterModuleInfo();
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
    ParticleSize = sizeof(FBaseParticle);
    TypeDataOffset = 0;
    TypeDataInstanceOffset = -1;
    ModuleOffsetMap.Empty();
    ModuleInstanceOffsetMap.Empty();
    ModulesNeedingInstanceData.Empty();

    UParticleLODLevel* HighLODLevel = GetLODLevel(0);
    assert(HighLODLevel);

    UParticleModuleTypeDataBase* HighTypeData = HighLODLevel->TypeDataModule;
    if (HighTypeData)
    {
        uint32 ReqBytes = HighTypeData->RequiredBytes(nullptr);
        if (ReqBytes)
        {
            TypeDataOffset = ParticleSize;
            ParticleSize += static_cast<int32>(ReqBytes);
        }

        int32 TempInstanceBytes = static_cast<int32>(HighTypeData->RequiredBytesPerInstance());
        if (TempInstanceBytes)
        {
            TypeDataInstanceOffset = ReqInstanceBytes;
            ReqInstanceBytes += TempInstanceBytes;
        }
    }

    UParticleModuleRequired* RequiredModule = HighLODLevel->RequiredModule;
    assert(RequiredModule);

    for (int32 ModuleIdx = 0; ModuleIdx < HighLODLevel->Modules.Num(); ModuleIdx++)
    {
        UParticleModule* ParticleModule = HighLODLevel->Modules[ModuleIdx];
        assert(ParticleModule);

        if (ParticleModule->IsA<UParticleModuleTypeDataBase>() == false) // 타입관련 모듈 아니라면(위에서 했음)
        {
            int32 ReqBytes = static_cast<int32>(ParticleModule->RequiredBytes(HighTypeData));
            if (ReqBytes)
            {
                ModuleOffsetMap.Add(ParticleModule, ParticleSize);
                // !TODO : 모듈들의 클래스 정보에 따라 따로 저장할 오프셋들을 지정, 저장
                ParticleSize += ReqBytes;

                int32 TempInstanceBytes = static_cast<int32>(ParticleModule->RequiredBytesPerInstance());
                if (TempInstanceBytes)
                {
                    ModuleInstanceOffsetMap.Add(ParticleModule, ReqInstanceBytes);
                    ModulesNeedingInstanceData.Add(ParticleModule);

                    ReqInstanceBytes += TempInstanceBytes;

                    // NOTE : 어차피 지금 LOD 하나라 여기 동작안함
                    for (int32 LODIdx = 1; LODIdx < LODLevels.Num(); LODIdx++)
                    {
                        UParticleLODLevel* CurLODLevel = LODLevels[LODIdx];
                        ModuleInstanceOffsetMap.Add(CurLODLevel->Modules[ModuleIdx], ReqInstanceBytes);
                    }
                    // ReqInstanceBytes += TempInstanceBytes; // TODO: 한번 확인해야 함
                }
            }
        }
    }
}

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODLevel)
{
    if (LODLevel >= LODLevels.Num())
    {
        return nullptr;
    }
    return LODLevels[LODLevel];
}

