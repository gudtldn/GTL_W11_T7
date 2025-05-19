#include <algorithm>
#include <cstdlib>

#include "ParticleEmitterInstances.h"
#include "ParticleEmitter.h"
#include "Particles/ParticleModules/ParticleModuleRequired.h"
#include "ParticleLODLevel.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleModules/ParticleModuleSpawn.h"
#include "Components/Material/Material.h"
#include "Components/SceneComponent.h"


void FParticleEmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
    SpriteTemplate = InTemplate;
    Component = InComponent;
    SetupEmitterDuration();
}

void FParticleEmitterInstance::Init()
{
    // !TODO : 여기서 뭐 해야돼..?
    assert(SpriteTemplate);
    SpawnFraction = 0;
    SecondsSinceCreation = 0;
    EmitterTime = 0;
    ParticleCounter = 0;

    UpdateTransforms();
    Location = Component->GetComponentLocation();
    OldLocation = Location;

    ParticleSize = SpriteTemplate->ParticleSize;
    
    PayloadOffset = ParticleSize;
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
    if (!SpriteTemplate || !SpriteTemplate->GetCurrentLODLevel(this))
    {
        return;
    }

    bool bFirstTime = SecondsSinceCreation <= 0.0f;

    UParticleLODLevel* LocalCurrentLODLevel = SpriteTemplate->GetCurrentLODLevel(this);
    float EmitterDelay = Tick_EmitterTimeSetup(DeltaTime, LocalCurrentLODLevel);

    if (bEnabled)
    {

        KillParticles();

        // ResetParticleParameters(); 아직 뭐하는 앤지 잘 모르겠음

        // 		CurrentMaterial = LODLevel->RequiredModule->Material;

        Tick_ModuleUpdate(DeltaTime, LocalCurrentLODLevel);

        SpawnFraction = Tick_SpawnParticles(DeltaTime, LocalCurrentLODLevel, bFirstTime);

        Tick_ModulePostUpdate(DeltaTime, LocalCurrentLODLevel);

        if (ActiveParticles > 0)
        {
            // !TODO : 얘네들도 알아보기
            // UpdateOrbitData(DeltaTime);
            // UpdateBoundingBox(DeltaTime);
        }
        Tick_ModuleFinalUpdate(DeltaTime, LocalCurrentLODLevel);

        // !TODO : 이 프로퍼티들도 어디에 쓰는 놈들인지 알아봐야 함
        EmitterTime += EmitterDelay;

        LastDeltaTime = DeltaTime;
    }
}

float FParticleEmitterInstance::Tick_EmitterTimeSetup(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
    Location = Component->GetComponentLocation();
    OldLocation = Location;

    UpdateTransforms();
    SecondsSinceCreation += DeltaTime;

    bool bLooped = false;

    EmitterTime = SecondsSinceCreation;
    if (EmitterDuration > KINDA_SMALL_NUMBER)
    {
        EmitterTime = FMath::Fmod(SecondsSinceCreation, EmitterDuration);
        bLooped = (SecondsSinceCreation - (EmitterDuration * LoopCount)) >= EmitterDuration;
    }
    if (bLooped)
    {
        LoopCount++;
        // !TODO : burst
    }

    float EmitterDelay = CurrentDelay;
    EmitterTime -= EmitterDelay;

    return EmitterDelay;
}

float FParticleEmitterInstance::Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* InCurrentLODLevel, bool bFirstTime)
{
    // !TODO : HaltSpawing관련 프로퍼티들 도입 검토
    if (EmitterTime >= 0.0f)
    {
        if ((InCurrentLODLevel->RequiredModule->EmitterLoops == 0) ||
            (LoopCount < InCurrentLODLevel->RequiredModule->EmitterLoops) ||
            (SecondsSinceCreation < (EmitterDuration * InCurrentLODLevel->RequiredModule->EmitterLoops)) ||
            bFirstTime)
        {
            bFirstTime = false;
            SpawnFraction = Spawn(DeltaTime);
        }
    }
    return SpawnFraction;
}

void FParticleEmitterInstance::Tick_ModuleUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
    UParticleLODLevel* HighestLODLevel = SpriteTemplate->GetCurrentLODLevel(this); // !NOTE : 지금은 하나만 사용중
    if (!HighestLODLevel)
    {
        return;
    }
    for (int32 ModuleIndex = 0; ModuleIndex < InCurrentLODLevel->UpdateModules.Num(); ModuleIndex++)
    {
        UParticleModule* CurrentModule = InCurrentLODLevel->UpdateModules[ModuleIndex];
        if (CurrentModule && CurrentModule->bEnabled && CurrentModule->bUpdateModule)
        {
            CurrentModule->Update(this, GetModuleDataOffset(HighestLODLevel->UpdateModules[ModuleIndex]), DeltaTime);
        }
    }
}

void FParticleEmitterInstance::Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
}

void FParticleEmitterInstance::Tick_ModuleFinalUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
}

UParticleLODLevel* FParticleEmitterInstance::GetCurrentLODLevelChecked() const
{
    assert(SpriteTemplate);
    UParticleLODLevel* LocalCurrentLODLevel = SpriteTemplate->GetCurrentLODLevel(this);
    assert(LocalCurrentLODLevel);
    assert(LocalCurrentLODLevel->RequiredModule);
    return LocalCurrentLODLevel;
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity)
{
    UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

    assert(ActiveParticles <= MaxActiveParticles);

    assert(ActiveParticles + Count <= MaxActiveParticles);

    Count = FMath::Min<int32>(Count, MaxActiveParticles - ActiveParticles);

    // !TODO : 소환 시 이벤트 Payload 처리
    
    UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels[0];
    float SpawnTime = StartTime;
    float Interp = 1.0f;
    const float InterpIncrement = (Count > 0 && Increment > 0.0f) ? (1.0f / (float)Count) : 0.0f;
    for (int32 i = 0; i < Count; i++)
    {
        if (!ParticleData || !ParticleIndices)
        {
            UE_LOG(ELogLevel::Error, TEXT("ParticleData is null or ParticleIndices is null"));
            continue;
        }

        uint16 NextFreeIndex = ParticleIndices[ActiveParticles];
        if (NextFreeIndex < MaxActiveParticles)
        {
            UE_LOG(ELogLevel::Error, TEXT("NextFreeIndex is out of range"));
            // !TODO : ParticleIndice 고치는 로직 추가
            continue;
        }

        DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * NextFreeIndex);
        const uint32 CurrentParticleIndex = ActiveParticles++;

        PreSpawn(Particle, InitialLocation, InitialVelocity);
        for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->SpawnModules.Num(); ModuleIndex++)
        {
            UParticleModule* SpawnModule = LODLevel->SpawnModules[ModuleIndex];
            if (SpawnModule->bEnabled)
            {
                UParticleModule* OffsetModule = HighestLODLevel->SpawnModules[ModuleIndex];
                SpawnModule->Spawn(this, GetModuleDataOffset(OffsetModule), SpawnTime, Particle);
            }
        }
        PostSpawn(Particle, Interp, SpawnTime);

        if (Particle->RelativeTime > 1.0f)
        {
            KillParticle(CurrentParticleIndex);

            continue;
        }

        // !TODO : Burst EventPayload
    }
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
    assert(Particle);
    assert(ParticleSize > 0);

    // memzero , TODO : FMemory 
    std::memset(Particle, 0, ParticleSize);

    Particle->Location = InitialLocation;
    Particle->BaseVelocity = InitialVelocity;
    Particle->Velocity = InitialVelocity;

    Particle->Location -= PositionOffsetThisTick;
}

float FParticleEmitterInstance::Spawn(float DeltaTime)
{
    UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

    float SpawnRate = 0.0f;
    int32 SpawnCount = 0;
    int32 BurstCount = 0; //  !TODO : 버스트는 어떻게 할지 검토 필요
    float SpawnRateDivisor = 0.0f;
    float OldLeftover = SpawnFraction; //  이전 프레임에서 남은 SpawnFraction;

    UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels[0];

    bool bProcessSpawnRate = true;
    bool bProcessBurstList = true;

    //int32 DetailMode = Component->GetCurrentDetailMode(); // 디테일 필요한가
    for (int32 SpawnModIndex = 0; SpawnModIndex < LODLevel->SpawningModules.Num(); SpawnModIndex++)
    {
        UParticleModuleSpawnBase* SpawnModule = LODLevel->SpawningModules[SpawnModIndex];
        if (SpawnModule && SpawnModule->bEnabled)
        {
            UParticleModule* OffsetModule = HighestLODLevel->SpawningModules[SpawnModIndex];
            uint32 Offset = GetModuleDataOffset(OffsetModule);

            int32 Number = 0;
            float Rate = 0.0f;
            if (SpawnModule->GetSpawnAmount(this, Offset, OldLeftover, DeltaTime, Number, Rate) == false)
            {
                bProcessSpawnRate = false;
            }

            Number = FMath::Max<int32>(0, Number);
            Rate = FMath::Max<float>(0.0f, Rate);

            SpawnCount += Number;
            SpawnRate += Rate;

            // !TODO : Burst
        }
    }

    // 현재 프레임의 SpawnRate를 계산
    if (bProcessSpawnRate)
    {
        float RateScale = LODLevel->SpawnModule->GetSpawnRate();
        float SpawnScale = LODLevel->SpawnModule->GetSpawnScale();
        SpawnRate += RateScale * SpawnScale;
        SpawnRate = FMath::Max<float>(0.0f, SpawnRate);
    }

    // !TODO : Burst

    // 여기서 파티클 생성
    if (SpawnRate > 0.f || BurstCount > 0)
    {
        float SafetyLeftOver = OldLeftover;
        float NewLeftOver = OldLeftover + DeltaTime * SpawnRate;
        int32 Number = FMath::FloorToInt(NewLeftOver);
        float Increment = SpawnRate > 0.0f ? (1.f / SpawnRate) : 0.0f;
        float StartTime = DeltaTime + OldLeftover * Increment - Increment;
        NewLeftOver = NewLeftOver - Number;

        bool bProcessSpawn = true;
        int32 NewCount = ActiveParticles + Number + BurstCount; // 일단 버스트 기능 없으므로 BurstCount는 0

        if (NewCount > 1000) // 이 값은 언리얼엔진의 FXConsoleVariables::MaxCpuParticlesPerFrame
        {
            int32 MaxNewParticles = 1000 - ActiveParticles;
            BurstCount = FMath::Min(MaxNewParticles, BurstCount);
            MaxNewParticles -= BurstCount;
            Number = FMath::Min(MaxNewParticles, Number);
            NewCount = ActiveParticles + Number + BurstCount;
        }

        if (NewCount > MaxActiveParticles)
        {
            bProcessSpawn = Resize(NewCount * 2, true); // 두 배로 리사이즈
        }

        if (bProcessSpawn)
        {
            // !TODO : FParticleEventInstancePayload로 Spawn 이벤트 처리
            const FVector InitialLocation = EmitterToSimulation.GetOrigin();

            SpawnParticles(Number, StartTime, Increment, InitialLocation, FVector::ZeroVector);

            // !TODO : Burst
            return NewLeftOver;
        }
        return SafetyLeftOver;
    }

    return SpawnFraction;
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime)
{
    UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

    if (LODLevel->RequiredModule->bUseLocalSpace == false)
    {
        if (FVector::DistSquared(OldLocation, Location) > 1.f)
        {
            Particle->Location += (OldLocation - Location) * InterpolationPercentage;
        }
    }

    Particle->OldLocation = Particle->Location;
    Particle->Location += FVector(Particle->Velocity) * SpawnTime;

    // !TODO : 파티클 State 플래그 체크   
    //Particle->
}

bool FParticleEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
    if (!SpriteTemplate || ActiveParticles <= 0 || !bEnabled)
    {
        return false;
    }

    UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);
    if (!LODLevel || LODLevel->bEnabled == false)
    {
        return false;
    }
    OutData.eEmitterType = DET_Unknown;

    OutData.ActiveParticleCount = ActiveParticles;
    OutData.ParticleStride = ParticleStride;
    //OutData.SortMode = SortMode;

    OutData.Scale = FVector(1.0f, 1.0f, 1.0f);
    if (Component)
    {
        OutData.Scale = Component->GetComponentScale3D();
    }

    int32 ParticleMemSize = MaxActiveParticles * ParticleStride;

    // 여기서 데이터의 메모리 할당 및 복사
    OutData.DataContainer.Alloc(ParticleMemSize, MaxActiveParticles);
    std::memcpy(OutData.DataContainer.ParticleData, ParticleData, ParticleMemSize);
    std::memcpy(OutData.DataContainer.ParticleIndices, ParticleIndices, OutData.DataContainer.ParticleIndicesNumShorts * sizeof(uint16));
    {
        // !TODO : 여기서 Sprite관련 기타 프로퍼티 및 Offset들 복사(데이터들의 기본 클래스는 Sprite이기 때문)
        FDynamicSpriteEmitterReplayDataBase* NewReplayData =
            static_cast<FDynamicSpriteEmitterReplayDataBase*>(&OutData);
    }

    return true;
}

void FParticleEmitterInstance::UpdateTransforms()
{
    assert(SpriteTemplate);

    UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
    FMatrix ComponentToWorld = Component != nullptr ?
        Component->GetComponentTransform().ToMatrixNoScale() : FMatrix::Identity;

    // !NOTE : 뭔가 이상하면 여기 살펴보기
    FTransform EmitterLocalTransform(
        LODLevel->RequiredModule->EmitterRotation,
        LODLevel->RequiredModule->EmitterOrigin,
        FVector(0, 0, 0)
    );

    FMatrix EmitterToComponent = EmitterLocalTransform.ToMatrixNoScale();

    if (LODLevel->RequiredModule->bUseLocalSpace)
    {
        EmitterToSimulation = EmitterToComponent;
        SimulationToWorld = ComponentToWorld;
    }
    else
    {
        EmitterToSimulation = EmitterToComponent * ComponentToWorld;
        SimulationToWorld = FMatrix::Identity;
    }
}

uint32 FParticleEmitterInstance::GetModuleDataOffset(UParticleModule* Module) const
{
    assert(SpriteTemplate);

    uint32* Offset = SpriteTemplate->ModuleOffsetMap.Find(Module);
    return (Offset != nullptr) ? *Offset : 0;
}

void FParticleEmitterInstance::ApplyWorldOffset(FVector InOffset, bool bWorldShift)
{
    UpdateTransforms();
    Location += InOffset;
    OldLocation += InOffset;

    UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

    if (!LODLevel->RequiredModule->bUseLocalSpace)
    {
        PositionOffsetThisTick = InOffset;
    }
}

// !NOTE : FMemory 클래스를 만들어야 할 수도 있음
bool FParticleEmitterInstance::Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount)
{
    if (NewMaxActiveParticles < 0)
    {
        return false;
    }

    // 파티클 데이터 realloc
    ParticleData = (uint8*)realloc(ParticleData, ParticleStride * NewMaxActiveParticles);
    assert(ParticleData);

    if (ParticleIndices == nullptr)
    {
        MaxActiveParticles = 0;
    }
    ParticleIndices = (uint16*)realloc(ParticleIndices, sizeof(uint16) * (NewMaxActiveParticles + 1));

    for (int32 i = MaxActiveParticles; i < NewMaxActiveParticles; i++)
    {
        ParticleIndices[i] = i;
    }

    MaxActiveParticles = NewMaxActiveParticles;

    return true;
}

uint8* FParticleEmitterInstance::GetModuleInstanceData(UParticleModule* InModule) const
{
    if (InstanceData)
    {
        uint32* Offset = SpriteTemplate->ModuleInstanceOffsetMap.Find(InModule);
        if (Offset)
        {
            assert(*Offset < (uint32)InstancePayloadSize);
            return &(InstanceData[*Offset]);
        }
    }
    return nullptr;
}

UMaterial* FParticleEmitterInstance::GetCurrentMaterial()
{
    UMaterial* RenderMaterial = CurrentMaterial;

    // !TODO : CurrentMaterial이 null인 경우 기본 머티리얼 할당해주는 로직 추가
    return RenderMaterial;
}

void FParticleEmitterInstance::KillParticles()
{
    for (int32 i = ActiveParticles - 1; i >= 0; i--)
    {
        const int32 CurrentIndex = ParticleIndices[i];
        if (CurrentIndex < MaxActiveParticles)
        {
            const uint8* ParticleBase = ParticleData + CurrentIndex * ParticleStride;
            FBaseParticle& Particle = *(FBaseParticle*)ParticleBase;

            if (Particle.RelativeTime > 1.0f)
            {
                // !TODO : EventGenerator통해서 HandleParticleKilled 호출
                ParticleIndices[i] = ParticleIndices[ActiveParticles - 1];
                ParticleIndices[ActiveParticles - 1] = CurrentIndex;
                ActiveParticles--;
            }
        }
    }

    // !TODO : CorruptIndice 발견 시 로직 추가 필요
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
    if (Index < ActiveParticles)
    {
        UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
        
        int32 KillIndex = ParticleIndices[Index];

        // Move it to the 'back' of the list
        for (int32 i = Index; i < ActiveParticles - 1; i++)
        {
            ParticleIndices[i] = ParticleIndices[i + 1];
        }
        ParticleIndices[ActiveParticles - 1] = KillIndex;
        ActiveParticles--;

        // !TODO : EventPayload
    }
}

void FParticleEmitterInstance::SetupEmitterDuration()
{
    if (SpriteTemplate == nullptr)
    {
        return;
    }

    int32 EDCount = EmitterDurations.Num();
    // 어차피 지금은 LOD 하나만 사용중이라 1개임
    if (EDCount == 0 || EDCount != SpriteTemplate->LODLevels.Num())
    {
        EmitterDurations.Empty();
        EmitterDurations.AddUninitialized(SpriteTemplate->LODLevels.Num());
    }

    for (int32 LODIndex = 0; LODIndex < SpriteTemplate->LODLevels.Num(); LODIndex++)
    {
        UParticleLODLevel* TempLOD = SpriteTemplate->LODLevels[LODIndex];
        UParticleModuleRequired* RequiredModule = TempLOD->RequiredModule;

        CurrentDelay = RequiredModule->EmitterDelay + Component->EmitterDelay;
        EmitterDurations[TempLOD->Level] = RequiredModule->EmitterDuration + CurrentDelay;

        // !TODO : Range 기반 랜덤값 사용
    }
}

FDynamicEmitterDataBase* FParticleEmitterInstance::GetDynamicData()
{
    return nullptr;
}

FDynamicEmitterDataBase* FParticleSpriteEmitterInstance::GetDynamicData()
{

    UParticleLODLevel* LODLevel = SpriteTemplate->GetCurrentLODLevel(this);

    FDynamicSpriteEmitterData* NewEmitterData = new FDynamicSpriteEmitterData(LODLevel->RequiredModule);
    if (!FillReplayData(NewEmitterData->Source))
    {
        delete NewEmitterData;
        return nullptr;
    }

    NewEmitterData->Init();

    return NewEmitterData;
}

void FParticleMeshEmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{

}

bool FParticleMeshEmitterInstance::Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount)
{
    return false;
}

bool FParticleMeshEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
    if (!FParticleEmitterInstance::FillReplayData(OutData))
    {
        return false;
    }

    OutData.eEmitterType = DET_Sprite;

    FDynamicSpriteEmitterReplayData* NewReplayData = static_cast<FDynamicSpriteEmitterReplayData*>(&OutData);

    // !TODO : material
    NewReplayData->Material = GetCurrentMaterial();

    return true;
}
