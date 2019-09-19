#pragma once

#include "../common/BaseUtil.h"
#include "../common/MathHelper.h"
#include "../common/UploadBuffer.h"

extern const unsigned int MaxLights;

namespace FrameResource
{
// ====================================================================================================================
struct Vertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 normal;
};

// ====================================================================================================================
struct Light
{
    DirectX::XMFLOAT3 strength = { 0.5f, 0.5f, 0.5f };
    float             fallOffStart = 1.0f;
    DirectX::XMFLOAT3 direction = { 0.0f, -1.0f, 0.0f };
    float             fallOffEnd = 10.0f;
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    float             spotPower = 64.0f;
};

// ====================================================================================================================
struct PassConstants
{
    DirectX::XMFLOAT4X4 view = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 invView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 proj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 invProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 viewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 invViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3   eyePosW = {0.0f, 0.0f, 0.0f };
    float               cbPerfObjectPad1 = 0.0f;
    DirectX::XMFLOAT2   renderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2   invRenderTargetSize = { 0.0f, 0.0f };
    float               nearZ = 0.0f;
    float               farZ  = 0.0f;
    float               totalTime = 0.0f;
    float               deltaTime = 0.0f;
    DirectX::XMFLOAT4   ambientLight = {0.0f, 0.0f, 0.0f, 1.0f};
    Light               lights[MaxLights];
};

// ====================================================================================================================
struct ObjectConstants
{
    DirectX::XMFLOAT4X4 m_world = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 m_texTransform = MathHelper::Identity4x4();
};

// ====================================================================================================================
struct MaterialConstants
{
    DirectX::XMFLOAT4 diffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 fresnelR0     = {0.01f, 0.01f, 0.01f };
    float             roughness     = 0.25f;
    DirectX::XMFLOAT4X4 matTransform = MathHelper::Identity4x4();
};

// ====================================================================================================================
// Represents the transformation info for each object in the scene for one frame.
class FrameResource
{
    public:
        FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
        FrameResource(const FrameResource& rhs) = delete;
        FrameResource& operator=(const FrameResource& rhs) = delete;
        ~FrameResource();

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_cmdListAlloc;
        std::unique_ptr<UploadBuffer<PassConstants>>   m_passCb = nullptr;
        std::unique_ptr<UploadBuffer<ObjectConstants>> m_objCb = nullptr;
        std::unique_ptr<UploadBuffer<MaterialConstants>> m_matCb = nullptr;
        UINT64                                         m_fence = 0;
};
}