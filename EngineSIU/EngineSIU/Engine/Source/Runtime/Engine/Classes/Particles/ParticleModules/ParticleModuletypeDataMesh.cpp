#include "ParticleModuletypeDataMesh.h"

#include "Engine/FObjLoader.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
    // !TODO : 기본 메시 세팅
    if(!Mesh)
        Mesh = FObjManager::GetStaticMesh(L"Contents/Reference/Reference.obj");

    FParticleMeshEmitterInstance* Instance = new FParticleMeshEmitterInstance();
    Instance->MeshTypeData = this;
    return Instance;
}

UParticleModuleTypeDataMesh::UParticleModuleTypeDataMesh()
{
};
