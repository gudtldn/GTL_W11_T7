#pragma once
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "Particles/ParticleHelper.h"

struct FParticleEmitterInstance;
class UParticleSystem;
class UMaterial;


// !TODO : 지금은 하는게 없음. 기능 추가되면 별개의 스크립트로 분리
class UFXSystemComponent : public UPrimitiveComponent
{
    DECLARE_CLASS(UFXSystemComponent, UPrimitiveComponent)

public:
    UFXSystemComponent() = default;
    virtual ~UFXSystemComponent() override = default;
};

enum EParticleReplayState : int
{
    PRS_Disabled = 0,
    PRS_Capturing = 1,
    PRS_Replaying = 2,
    PRS_MAX
};

class UParticleSystemComponent : public UFXSystemComponent
{
    DECLARE_CLASS(UParticleSystemComponent, UFXSystemComponent)

public:
    UParticleSystemComponent() = default;
    virtual ~UParticleSystemComponent() override = default;

    void InitializeSystem();
    void InitParticles();
    void ResetParticles();

    virtual void TickComponent(float DeltaTime) override;
    void UpdateInstances();

    void SetTemplate(UParticleSystem* InTemplate);
    void ClearDynamicData();
    
    UMaterial* GetMaterial(int32 ElementIndex) const;
    void SetMaterial(int32 ElementIndex, UMaterial* Material);

    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

    UPROPERTY(
        EditAnywhere,
        UParticleSystem*, Template, = nullptr;
    )

    uint8 bWasCompleted : 1;

    int LODLevel = 0;
    float EmitterDelay = 0.f;

    TArray<FParticleEmitterInstance*> EmitterInstances;
    TArray<UMaterial*> EmitterMaterials;
};
