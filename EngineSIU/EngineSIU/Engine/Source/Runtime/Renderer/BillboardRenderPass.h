#pragma once

#include "IRenderPass.h"
#include "EngineBaseTypes.h"
#include "Container/Set.h"

#include "Define.h"

enum class EResourceType : uint8;
class UBillboardComponent;
class FDXDBufferManager;
class FGraphicsDevice;
class FDXDShaderManager;
class UWorld;
class FEditorViewportClient;

class FBillboardRenderPass : public IRenderPass
{
public:
    FBillboardRenderPass();
    virtual ~FBillboardRenderPass();

    virtual void Initialize(FDXDBufferManager* InBufferManager, FGraphicsDevice* InGraphics, FDXDShaderManager* InShaderManage) override;

    virtual void PrepareRenderArr() override;
    void UpdateObjectConstant(const FMatrix& WorldMatrix, const FVector4& UUIDColor, bool bIsSelected) const;

    virtual void Render(const std::shared_ptr<FEditorViewportClient>& Viewport) override;

    virtual void ClearRenderArr() override;

    void SetupVertexBuffer(ID3D11Buffer* pVertexBuffer, UINT NumVertices) const;
    // Texture 셰이더 관련
    void PrepareTextureShader() const;
    void PrepareSubUVConstant() const;

    // 상수 버퍼 업데이트 함수
    void UpdateSubUVConstant(FVector2D UVOffset, FVector2D UVScale) const;
    
    // Primitive 드로우 함수
    void RenderTexturePrimitive(ID3D11Buffer* pVertexBuffer, UINT NumVertices,
        ID3D11Buffer* pIndexBuffer, UINT NumIndices,
        ID3D11ShaderResourceView* TextureSRV, ID3D11SamplerState* SamplerState) const;

    void RenderTextPrimitive(ID3D11Buffer* pVertexBuffer, UINT NumVertices,
        ID3D11ShaderResourceView* TextureSRV, ID3D11SamplerState* SamplerState) const;

    void CreateShader();
    void UpdateShader();
    void ReleaseShader();

protected:
    TArray<UBillboardComponent*> BillboardComps;

    EResourceType ResourceType;

private:
    ID3D11VertexShader* VertexShader;
    
    ID3D11PixelShader* PixelShader;
    
    ID3D11InputLayout* InputLayout;

    FDXDBufferManager* BufferManager;
    
    FGraphicsDevice* Graphics;
    
    FDXDShaderManager* ShaderManager;

};
