#pragma once
#include "Container/Array.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

class UParticleEmitter;
class UParticleSystemComponent;
class UParticleLODLevel;
class UParticleModule;
class UParticleModuleTypeDataMesh;
class UMaterial;

struct FBaseParticle;
struct FVector;
struct FDynamicEmitterReplayDataBase;
struct FDynamicEmitterDataBase;



struct FParticleEmitterInstance
{
    /** 이 인스턴스의 기반이 되는 UParticleEmitter 템플릿 (에디터 설정 원본). */
    UParticleEmitter* SpriteTemplate = nullptr;
    /** 이 이미터 인스턴스를 소유하는 UParticleSystemComponent. */
    UParticleSystemComponent* Component = nullptr;
    /** 현재 적용 중인 UParticleLODLevel. */
    UParticleLODLevel* CurrentLODLevel = nullptr;
    /** 현재 LOD 레벨의 인덱스. */
    int32 CurrentLODLevelIndex = 0;

    /** 이미터 활성화 여부 (false 시 틱/렌더링 제외 가능). */
    bool bEnabled = false;
    /** 이미터 인스턴스의 현재 지속 시간 (CurrentLODLevel에 의해 결정). */
    float EmitterDuration = 0.0f;
    /** 각 LOD 레벨별 이미터 지속 시간 배열. */
    TArray<float> EmitterDurations;
    /** 현재 루프 또는 스폰 시작 전의 지연 시간. */
    float CurrentDelay = 0.0f;
    /** 이미터의 내부 진행 시간 (루프 시 0으로 리셋 가능). */
    float EmitterTime = 0.0f;
    /** 마지막 틱에서의 델타 타임 (경과 시간). */
    float LastDeltaTime = 0.0f;
    /** 이 이미터 인스턴스 생성 후 경과한 총 시간(초) (루프와 무관하게 누적). */
    float SecondsSinceCreation = 0.0f;
    /** 현재 활성화되어 시뮬레이션 및 렌더링 중인 파티클 수. */
    int32 ActiveParticles = 0;
    /** 이미터가 완료한 루프(반복) 횟수. */
    int32 LoopCount = 0;
    /** 이 인스턴스에서 생성된 파티클의 누적 카운터 (주로 고유 ID 부여용). */
    uint32 ParticleCounter = 0;

    /** 파티클 스폰 간의 나머지 시간 비율(0.0~1.0) (버스트 스폰 시 정밀 타이밍 조절용). */
    float SpawnFraction = 0.0f;

    /** 이미터 인스턴스별 데이터 배열 포인터 (이미터 자체 귀속 데이터). */
    uint8* InstanceData = nullptr;
    /** InstanceData 배열의 페이로드(실제 데이터) 크기 (바이트 단위). */
    int32 InstancePayloadSize = 0;

    /** 실제 파티클 데이터가 저장된 메모리 버퍼 포인터 (각 파티클 속성 연속 저장). */
    uint8* ParticleData = nullptr;
    /** 개별 파티클 하나의 전체 데이터 크기 (바이트 단위). */
    int32 ParticleSize = 0;
    /** ParticleData 배열 내 다음 파티클 데이터 시작점까지의 간격 (바이트 단위). */
    int32 ParticleStride = 0;
    /** 각 파티클 데이터 내 특정 모듈 데이터(페이로드) 시작점 오프셋. */
    int32 PayloadOffset = 0;

    /** 활성 파티클의 인덱스를 저장하는 배열 포인터 (효율적 관리 및 접근용). */
    uint16* ParticleIndices = nullptr;
    /** 이 이미터가 동시에 유지할 수 있는 최대 활성 파티클 수. */
    int32 MaxActiveParticles = 0;

    /** 현재 틱에서 각 파티클에 일괄 적용될 위치 오프셋 (매 틱 종료 시 0으로 리셋). */
    FVector PositionOffsetThisTick;

    /** 이미터 인스턴스의 현재 위치 (월드 또는 부모 컴포넌트 로컬 공간). */
    FVector Location = FVector::ZeroVector;
    /** 이미터 인스턴스의 이전 틱(프레임)에서의 위치. */
    FVector OldLocation = FVector::ZeroVector;

    /** 이미터 로컬 공간에서 시뮬레이션 공간으로 변환하는 트랜스폼 행렬. */
    FMatrix EmitterToSimulation = FMatrix::Identity;
    /** 시뮬레이션 공간에서 월드 공간으로 변환하는 트랜스폼 행렬. */
    FMatrix SimulationToWorld = FMatrix::Identity;

    /** 현재 사용중인 머티리얼. */
    UMaterial* CurrentMaterial = nullptr;

    FParticleEmitterInstance() = default;
    virtual ~FParticleEmitterInstance() = default;

    FParticleEmitterInstance(const FParticleEmitterInstance&) = delete;
    FParticleEmitterInstance& operator=(const FParticleEmitterInstance&) = delete;
    FParticleEmitterInstance(FParticleEmitterInstance&&) = delete;
    FParticleEmitterInstance& operator=(FParticleEmitterInstance&&) = delete;

    virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);
    virtual void Init();

    // Tick관련
    virtual void Tick(float DeltaTime);
    virtual float Tick_EmitterTimeSetup(float DeltaTime, UParticleLODLevel* CurrentLODLevel);
    virtual float Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* InCurrentLODLevel, bool bFirstTime);
    virtual void Tick_ModuleUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);
    virtual void Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);
    virtual void Tick_ModuleFinalUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);

    UParticleLODLevel* GetCurrentLODLevelChecked() const;
    uint32 GetModuleDataOffset(UParticleModule* Module) const;

    void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity/*, struct FParticleEventInstancePayload* EventPayload */);

    virtual void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity);
    virtual float Spawn(float DeltaTime);
    virtual void PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime);

    // DynamicData
    virtual FDynamicEmitterDataBase* GetDynamicData();
    virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData);

    void UpdateTransforms();
    // 월드좌표 변경 시 호출
    virtual void ApplyWorldOffset(FVector InOffset, bool bWorldShift);
    virtual bool Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount);
    uint8* GetModuleInstanceData(UParticleModule* InModule) const;

    UMaterial* GetCurrentMaterial();

    void KillParticles();
    void KillParticle(int32 Index);

    void SetupEmitterDuration();
};

struct FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
    virtual FDynamicEmitterDataBase* GetDynamicData() override;
    // !TODO : FParticleSpriteEmitterInstance에 필요한 데이터들 추가
    // !TODO : FParticleSpriteEmitterInstance에 필요한 함수들 추가
};

struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
    UParticleModuleTypeDataMesh* MeshTypeData;

    virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) override;
    virtual bool Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount = true) override;
protected:
    virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData);
    // !TODO : FParticleMeshEmitterInstance에 필요한 데이터들 추가
    // !TODO : FParticleMeshEmitterInstance에 필요한 함수들 추가
};
