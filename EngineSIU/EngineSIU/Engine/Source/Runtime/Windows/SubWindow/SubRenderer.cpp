#include "SubRenderer.h"
#include "UnrealEd/EditorViewportClient.h"
#include "Engine/UnrealClient.h"
#include "Renderer/ParticleRenderPass.h"
#include "SubWindow/ParticleSubEngine.h"
void FSubRenderer::Initialize(FGraphicsDevice* InGraphics, FDXDBufferManager* InBufferManager, USubEngine* InEngine)
{
    Engine = InEngine;
    Graphics = InGraphics;
    BufferManager = InBufferManager;
    ParticleRenderPass = new FParticleRenderPass();
    ParticleRenderPass->Initialize(BufferManager, Graphics, FEngineLoop::Renderer.ShaderManager);
}

void FSubRenderer::PrepareRender(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    ParticleRenderPass->AddParticleComponent(((UParticleSubEngine*)Engine)->GetParticleSystemComponent());
   
    UpdateViewCamera(Viewport);
    
    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    ViewportResource->ClearDepthStencils(Graphics->DeviceContext);
    ViewportResource->ClearRenderTargets(Graphics->DeviceContext);

}

void FSubRenderer::Render(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    ParticleRenderPass->Render(Viewport);
}

void FSubRenderer::ClearRender()
{
    ParticleRenderPass->ClearRenderArr();
}

void FSubRenderer::Release()
{
    if (ParticleRenderPass)
    {
        delete ParticleRenderPass;
        ParticleRenderPass = nullptr;
    }
}

void FSubRenderer::UpdateViewCamera(const std::shared_ptr<FEditorViewportClient>& Viewport)
{
    FCameraConstantBuffer CameraConstantBuffer;
    CameraConstantBuffer.ViewMatrix = Viewport->GetViewMatrix();
    CameraConstantBuffer.InvViewMatrix = FMatrix::Inverse(CameraConstantBuffer.ViewMatrix);
    CameraConstantBuffer.ProjectionMatrix = Viewport->GetProjectionMatrix();
    CameraConstantBuffer.InvProjectionMatrix = FMatrix::Inverse(CameraConstantBuffer.ProjectionMatrix);
    CameraConstantBuffer.ViewLocation = Viewport->GetCameraLocation();
    CameraConstantBuffer.NearClip = Viewport->GetCameraNearClip();
    CameraConstantBuffer.FarClip = Viewport->GetCameraFarClip();
    BufferManager->UpdateConstantBuffer("FCameraConstantBuffer", CameraConstantBuffer);
}
