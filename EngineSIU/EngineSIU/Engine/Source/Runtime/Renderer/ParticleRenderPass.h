// ParticleRenderPass.h
#pragma once

#include "IRenderPass.h"
#include "Define.h"

struct FParticleMeshEmitterInstance;
struct FDynamicMeshEmitterReplayDataBase;
struct FDynamicSpriteEmitterReplayDataBase;
struct FParticleEmitterInstance;
struct FTexture;
class UParticleSystemComponent;

class FParticleRenderPass : public IRenderPass
{
public:
    FParticleRenderPass();
    virtual ~FParticleRenderPass();

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManager) override;

    virtual void PrepareRenderArr() override;
    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;
    virtual void ClearRenderArr() override;
    void AddParticleComponent(UParticleSystemComponent* InParticleComponent);
private:
    void CreateShader();
    void ReleaseShader();
    void RenderSpriteEmitter(const std::shared_ptr<FEditorViewportClient>& Viewport,UParticleSystemComponent* Comp, FParticleEmitterInstance* Emitter, const FDynamicSpriteEmitterReplayDataBase& ReplayData);
    void RenderMeshEmitter(UParticleSystemComponent* Comp, FParticleMeshEmitterInstance* Emitter, const FDynamicMeshEmitterReplayDataBase& ReplayData);

    void PrepareRenderState(const std::shared_ptr<FEditorViewportClient>& Viewport);

private:
    FDXDBufferManager* BufferManager;
    FGraphicsDevice* Graphics;
    FDXDShaderManager* ShaderManager;

    // Sprite 전용
    FVertexInfo InstanceInfoSprite;
    FVertexInfo QuadVertexInfo;

    // Mesh 전용
    FVertexInfo InstanceInfoMesh;
    FVertexInfo StaticMeshVertexInfo;
    FIndexInfo StaticMeshIndexInfo;

    TArray<UParticleSystemComponent*> ParticleComponents;
    ID3D11BlendState* AlphaBlendState;
    ID3D11DepthStencilState* NoZWriteState;

    //FDynamicSpriteEmitterReplayDataBase의
    //MaterialInterface가 구현이 안 되어있어 임시 사용
    //std::shared_ptr<FTexture> TestTexture;

    //임시로 사용
    //FName TestMeshAssetName = L"Contents/Reference/Reference.obj";

};
