#pragma once
#ifndef VKD3D12_UTIL_H
#define VKD3D12_UTIL_H

#include <wrl.h>
#include <dxgi1_6.h>
#include <comdef.h>
#include <d3dcompiler.h>
#include <fstream>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>

#include <DirectXMath.h>
#include <DirectXCollision.h>

#include "d3dx12.h"
#include "MathHelper.h"

extern const unsigned int NumFrameResources;
const unsigned int MaxLights = 16;

// ====================================================================================================================
namespace util
{

    static void ReleaseComObj(IUnknown* pComObj)
    {
        if (pComObj != nullptr)
        {
            pComObj->Release();
            pComObj = nullptr;
        }
    }
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}


// ====================================================================================================================
class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber)
        :
        ErrorCode(hr),
        FunctionName(functionName),
        Filename(filename),
        LineNumber(lineNumber)
    {
    }

    std::wstring DxException::ToString()const
    {
        // Get the string description of the error code.
        _com_error err(ErrorCode);
        std::wstring msg = err.ErrorMessage();

        return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
    }

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

// ====================================================================================================================
class BaseUtil
{
public:
    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        // Constant buffers must be a multiple of the minimum hardware
        // allocation size (usually 256 bytes).  So round up to nearest
        // multiple of 256.  We do this by adding 255 and then masking off
        // the lower 2 bytes which store all bits < 256.
        // Example: Suppose byteSize = 300.
        // (300 + 255) & ~255
        // 555 & ~255
        // 0x022B & ~0x00ff
        // 0x022B & 0xff00
        // 0x0200
        // 512
        return (byteSize + 255) & ~255;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
        const std::wstring& filename,
        const D3D_SHADER_MACRO* pDefines,
        const std::string& entryPoint,
        const std::string& target)
    {
        UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        HRESULT hr = S_OK;

        Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> errors   = nullptr;
        hr = D3DCompileFromFile(filename.c_str(),
                                pDefines,
                                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                entryPoint.c_str(),
                                target.c_str(),
                                compileFlags,
                                0,
                                &byteCode,
                                &errors);

        if (errors != nullptr)
        {
            OutputDebugStringA((char*)errors->GetBufferPointer());
        }

        return byteCode;
    }

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device*                           device,
        ID3D12GraphicsCommandList*              cmdList,
        const void*                             initData,
        UINT64                                  byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;

        // Create the actual default buffer resource.
        ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                      D3D12_HEAP_FLAG_NONE,
                                                      &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
                                                      D3D12_RESOURCE_STATE_COMMON,
                                                      nullptr,
                                                      IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

        // In order to copy CPU memory data into our default buffer, we need to create
        // an intermediate upload heap.
        ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                      D3D12_HEAP_FLAG_NONE,
                                                      &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr,
                                                      IID_PPV_ARGS(uploadBuffer.GetAddressOf())));


        // Describe the data we want to copy into the default buffer.
        D3D12_SUBRESOURCE_DATA subResourceData = {};
        subResourceData.pData                  = initData;
        subResourceData.RowPitch               = byteSize;
        subResourceData.SlicePitch             = subResourceData.RowPitch;

        // Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
        // will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
        // the intermediate upload heap data will be copied to mBuffer.
        cmdList->ResourceBarrier(1,
                                 &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
                                 D3D12_RESOURCE_STATE_COMMON,
                                 D3D12_RESOURCE_STATE_COPY_DEST));

        UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

        cmdList->ResourceBarrier(1,
                                 &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
                                 D3D12_RESOURCE_STATE_COPY_DEST,
                                 D3D12_RESOURCE_STATE_GENERIC_READ));

        // Note: uploadBuffer has to be kept alive after the above function calls because
        // the command list has not been executed yet that performs the actual copy.
        // The caller can Release the uploadBuffer after it knows the copy has been executed.


        return defaultBuffer;
    }
};

struct SubmeshGeometry
{
    UINT indexCount;
    UINT startIndexLocation = 0;
    UINT baseVertexLocation = 0;

    DirectX::BoundingBox Bounds;
};

struct MeshGeometry
{
    std::string name;

    Microsoft::WRL::ComPtr<ID3DBlob> vertexBufferCPU = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> indexBufferCPU  = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU  = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferUploader  = nullptr;

    UINT vertexByteStride     = 0;
    UINT vertexBufferByteSize = 0;
    DXGI_FORMAT indexFormat   = DXGI_FORMAT_R32_UINT;
    UINT indexBufferByteSize  = 0;

    std::unordered_map<std::string, SubmeshGeometry> drawArgs;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation           = vertexBufferGPU->GetGPUVirtualAddress();
        vbv.StrideInBytes            = vertexByteStride;
        vbv.SizeInBytes              = vertexBufferByteSize;
        return vbv;
    }

    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
        ibv.Format         = indexFormat;
        ibv.SizeInBytes    = indexBufferByteSize;
        return ibv;
    }
    void DisposeUploaders()
    {
        vertexBufferUploader = nullptr;
        indexBufferUploader = nullptr;
    }
};

// ====================================================================================================================
struct Material
{
    std::string m_name;
    int         m_matCbIndex = -1;
    int         m_diffuseSrvHeapIndex = -1;
    int         m_normalSrvHeapIndex  = -1;
    int         m_numFramesDirty = NumFrameResources;

    DirectX::XMFLOAT4   m_diffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3   m_fresnelR0 = {0.01f, 0.01f, 0.01f };
    float               m_roughness = 0.25f;
    DirectX::XMFLOAT4X4 m_matTransform = MathHelper::Identity4x4();
};

#endif VKD3D12_UTIL_H
