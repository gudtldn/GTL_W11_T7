#include "DXDBufferManager.h"

#include <codecvt>
#include <locale>

#include "Particles/ParticleHelper.h"

void FDXDBufferManager::Initialize(ID3D11Device* InDXDevice, ID3D11DeviceContext* InDXDeviceContext)
{
    DXDevice = InDXDevice;
    DXDeviceContext = InDXDeviceContext;
    CreateQuadBuffer();
}

void FDXDBufferManager::ReleaseBuffers()
{
    for (auto& Pair : VertexBufferPool)
    {
        if (Pair.Value.VertexBuffer)
        {
            Pair.Value.VertexBuffer->Release();
            Pair.Value.VertexBuffer = nullptr;
        }
    }
    VertexBufferPool.Empty();

    for (auto& Pair : IndexBufferPool)
    {
        if (Pair.Value.IndexBuffer)
        {
            Pair.Value.IndexBuffer->Release();
            Pair.Value.IndexBuffer = nullptr;
        }
    }
    IndexBufferPool.Empty();
}

void FDXDBufferManager::ReleaseConstantBuffer()
{
    for (auto& Pair : ConstantBufferPool)
    {
        if (Pair.Value)
        {
            Pair.Value->Release();
            Pair.Value = nullptr;
        }
    }
    ConstantBufferPool.Empty();
}

void FDXDBufferManager::BindConstantBuffers(const TArray<FString>& Keys, UINT StartSlot, EShaderStage Stage) const
{
    const int Count = Keys.Num();
    TArray<ID3D11Buffer*> Buffers;
    Buffers.Reserve(Count);
    for (const FString& Key : Keys)
    {
        ID3D11Buffer* Buffer = GetConstantBuffer(Key);
        Buffers.Add(Buffer);
    }

    if (Stage == EShaderStage::Vertex)
    {
        DXDeviceContext->VSSetConstantBuffers(StartSlot, Count, Buffers.GetData());
    }
    else if (Stage == EShaderStage::Pixel)
    {
        DXDeviceContext->PSSetConstantBuffers(StartSlot, Count, Buffers.GetData());
    }
    else if (Stage == EShaderStage::Compute)
    {
        DXDeviceContext->CSSetConstantBuffers(StartSlot, Count, Buffers.GetData());
    }
    else if (Stage == EShaderStage::Geometry)
    {
        DXDeviceContext->GSSetConstantBuffers(StartSlot, Count, Buffers.GetData());
    }
}   

void FDXDBufferManager::BindConstantBuffer(const FString& Key, UINT StartSlot, EShaderStage Stage) const
{
    ID3D11Buffer* Buffer = GetConstantBuffer(Key);
    if (Stage == EShaderStage::Vertex)
        DXDeviceContext->VSSetConstantBuffers(StartSlot, 1, &Buffer);
    else if (Stage == EShaderStage::Pixel)
        DXDeviceContext->PSSetConstantBuffers(StartSlot, 1, &Buffer);
    else if (Stage == EShaderStage::Compute)
        DXDeviceContext->CSSetConstantBuffers(StartSlot, 1, &Buffer);
    else if (Stage == EShaderStage::Geometry)
        DXDeviceContext->GSSetConstantBuffers(StartSlot, 1, &Buffer);
}

FVertexInfo FDXDBufferManager::GetVertexBuffer(const FString& InName) const
{
    if (VertexBufferPool.Contains(InName))
        return VertexBufferPool[InName];
    return FVertexInfo();
}

FIndexInfo FDXDBufferManager::GetIndexBuffer(const FString& InName) const
{
    if (IndexBufferPool.Contains(InName))
        return IndexBufferPool[InName];
    return FIndexInfo();
}

FVertexInfo FDXDBufferManager::GetTextVertexBuffer(const FWString& InName) const
{
    if (TextAtlasVertexBufferPool.Contains(InName))
        return TextAtlasVertexBufferPool[InName];

    return FVertexInfo();
}

FIndexInfo FDXDBufferManager::GetTextIndexBuffer(const FWString& InName) const
{
    if (TextAtlasIndexBufferPool.Contains(InName))
        return TextAtlasIndexBufferPool[InName];

    return FIndexInfo();
}


ID3D11Buffer* FDXDBufferManager::GetConstantBuffer(const FString& InName) const
{
    if (ConstantBufferPool.Contains(InName))
        return ConstantBufferPool[InName];

    return nullptr;
}

void FDXDBufferManager::CreateQuadBuffer()
{
    const TArray<QuadVertex> Vertices =
    {
        { {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },
        { { 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
        { { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
        { {-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f} },
    };

    FVertexInfo VertexInfo;
    CreateVertexBuffer(TEXT("QuadBuffer"), Vertices, VertexInfo);

    const TArray<short> Indices =
    {
        0, 1, 2,
        0, 2, 3
    };

    FIndexInfo IndexInfo;
    CreateIndexBuffer(TEXT("QuadBuffer"), Indices, IndexInfo);
}
HRESULT FDXDBufferManager::CreateDynamicVertexBuffer(const FString& KeyName, UINT ByteSize, FVertexInfo& OutVertexInfo)
{
    if (VertexBufferPool.Contains(KeyName))
    {
        OutVertexInfo = VertexBufferPool[KeyName];
        return S_OK;
    }

    D3D11_BUFFER_DESC BufferDesc = {};
    BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    BufferDesc.ByteWidth = ByteSize;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ID3D11Buffer* NewBuffer = nullptr;
    const HRESULT Result = DXDevice->CreateBuffer(&BufferDesc, nullptr, &NewBuffer);
    if (FAILED(Result))
    {
        UE_LOG(ELogLevel::Error, TEXT("CreateDynamicVertexBuffer failed: %s"), *KeyName);
        return Result;
    }

    OutVertexInfo.NumVertices = 0; // 동적 버퍼는 런타임에 채워짐
    OutVertexInfo.Stride = 0;      // 나중에 DrawInstanced 할 때 stride 따로 지정
    OutVertexInfo.VertexBuffer = NewBuffer;
    VertexBufferPool.Add(KeyName, OutVertexInfo);

    return S_OK;
}

void FDXDBufferManager::GetQuadBuffer(FVertexInfo& OutVertexInfo, FIndexInfo& OutIndexInfo)
{
    OutVertexInfo = GetVertexBuffer(TEXT("QuadBuffer"));
    OutIndexInfo = GetIndexBuffer(TEXT("QuadBuffer"));
}

void FDXDBufferManager::GetTextBuffer(const FWString& Text, FVertexInfo& OutVertexInfo, FIndexInfo& OutIndexInfo)
{
    OutVertexInfo = GetTextVertexBuffer(Text);
    OutIndexInfo = GetTextIndexBuffer(Text);
}


HRESULT FDXDBufferManager::CreateUnicodeTextBuffer(const FWString& Text, FBufferInfo& OutBufferInfo,
    float BitmapWidth, float BitmapHeight, float ColCount, float RowCount)
{
    if (TextAtlasBufferPool.Contains(Text))
    {
        OutBufferInfo = TextAtlasBufferPool[Text];
        return S_OK;
    }

    TArray<FVertexTexture> Vertices;

    // 각 글자에 대한 기본 쿼드 크기 (폭과 높이)
    constexpr float QuadWidth = 2.0f;
    [[maybe_unused]] constexpr float QuadHeight = 2.0f;

    // 전체 텍스트의 너비
    const float TotalTextWidth = QuadWidth * Text.size();
    // 텍스트의 중앙으로 정렬하기 위한 오프셋
    const float CenterOffset = TotalTextWidth / 2.0f;

    const float CellWidth = BitmapWidth / ColCount;                       // 컬럼별 셀 폭
    const float CellHeight = BitmapHeight / RowCount; // 행별 셀 높이

    const float TexelUOffset = CellWidth / BitmapWidth;
    const float TexelVOffset = CellHeight / BitmapHeight;

    for (int Idx = 0; Idx < Text.size(); Idx++)
    {
        // 각 글자에 대해 기본적인 사각형 좌표 설정 (원점은 -1.0f부터 시작)
        FVertexTexture LeftUp = { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f };
        FVertexTexture RightUp = { 1.0f,  1.0f, 0.0f, 1.0f, 0.0f };
        FVertexTexture LeftDown = { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f };
        FVertexTexture RightDown = { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f };

        // UV 좌표 관련 보정 (nTexel 오프셋 적용)
        RightUp.u *= TexelUOffset;
        LeftDown.v *= TexelVOffset;
        RightDown.u *= TexelUOffset;
        RightDown.v *= TexelVOffset;

        // 각 글자의 x 좌표에 대해 오프셋 적용 (중앙 정렬을 위해 centerOffset 만큼 빼줌)
        const float XOffset = QuadWidth * Idx - CenterOffset;
        LeftUp.x += XOffset;
        RightUp.x += XOffset;
        LeftDown.x += XOffset;
        RightDown.x += XOffset;

        FVector2D UVOffset;
        SetStartUV(Text[Idx], UVOffset);

        LeftUp.u += (TexelUOffset * UVOffset.X);
        LeftUp.v += (TexelVOffset * UVOffset.Y);
        RightUp.u += (TexelUOffset * UVOffset.X);
        RightUp.v += (TexelVOffset * UVOffset.Y);
        LeftDown.u += (TexelUOffset * UVOffset.X);
        LeftDown.v += (TexelVOffset * UVOffset.Y);
        RightDown.u += (TexelUOffset * UVOffset.X);
        RightDown.v += (TexelVOffset * UVOffset.Y);

        // 각 글자의 쿼드를 두 개의 삼각형으로 생성
        Vertices.Add(LeftUp);
        Vertices.Add(RightUp);
        Vertices.Add(LeftDown);
        Vertices.Add(RightUp);
        Vertices.Add(RightDown);
        Vertices.Add(LeftDown);
    }

    CreateVertexBuffer(Text, Vertices, OutBufferInfo.VertexInfo);
    TextAtlasBufferPool[Text] = OutBufferInfo;

    return S_OK;
}

void FDXDBufferManager::SetStartUV(wchar_t Hangul, FVector2D& UVOffset)
{
    //대문자만 받는중
    int StartU = 0;
    int StartV = 0;
    int Offset = -1;

    if (Hangul == L' ') {
        UVOffset = FVector2D(0, 0);  // Space는 특별히 UV 좌표를 (0,0)으로 설정
        Offset = 0;
        return;
    }
    else if (Hangul >= L'A' && Hangul <= L'Z') {

        StartU = 11;
        StartV = 0;
        Offset = Hangul - L'A'; // 대문자 위치
    }
    else if (Hangul >= L'a' && Hangul <= L'z') {
        StartU = 37;
        StartV = 0;
        Offset = (Hangul - L'a'); // 소문자는 대문자 다음 위치
    }
    else if (Hangul >= L'0' && Hangul <= L'9') {
        StartU = 1;
        StartV = 0;
        Offset = (Hangul - L'0'); // 숫자는 소문자 다음 위치
    }
    else if (Hangul >= L'가' && Hangul <= L'힣')
    {
        StartU = 63;
        StartV = 0;
        Offset = Hangul - L'가'; // 대문자 위치
    }

    if (Offset == -1)
    {
        UE_LOG(ELogLevel::Warning, "Text Error");
    }

    const int OffsetV = (Offset + StartU) / 106;
    const int OffsetU = (Offset + StartU) % 106;

    UVOffset = FVector2D(OffsetU, StartV + OffsetV);

}
