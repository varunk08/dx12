#include <iostream>
#include "windows.h"
#include "BaseApp.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"

using namespace std;
using Microsoft::WRL::ComPtr;
using namespace DirectX;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const unsigned int NumFrameResources = 3;

// ====================================================================================================================
class RenderItem
{
public:
    RenderItem() = default;

    XMFLOAT4X4 world_ = MathHelper::Identity4x4();
    XMFLOAT4X4 texTransform_ = MathHelper::Identity4x4();

    int numFramesDirty_ = NumFrameResources;
    UINT objCbIndex_ = -1;
    Material* mat_ = nullptr;
    MeshGeometry* geo_ = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY primitiveType_ = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    UINT indexCount_ = 0;
    UINT startIndexLocation_ = 0;
    int baseVertexLocation_ = 0;
};

// ====================================================================================================================
class TextureDemo : public BaseApp
{
public:
    TextureDemo(HINSTANCE hInstance)
        :
        BaseApp(hInstance)
    {}

    virtual void Update(const BaseTimer& timer) override;
    virtual void Draw(const BaseTimer& timer) override;
    bool Initialize() override;

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildMaterials();
    void BuildRenderItems();
    void BuildFrameResources();

private:
    ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;
    
    std::unordered_map<std::string, std::unique_ptr<Texture>> textures_;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> shaders_;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> geometries_;
    std::unordered_map<std::string, std::unique_ptr<Material>> materials_;

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout_;
    std::vector<std::unique_ptr<RenderItem>> allItems_;
    std::vector<RenderItem*> opaqueItems_;
    std::vector<std::unique_ptr<FrameResource::Resources>> frameResources_;
};

// ====================================================================================================================
void TextureDemo::Update(const BaseTimer& timer)
{

}

// ====================================================================================================================
void TextureDemo::Draw(const BaseTimer& timer)
{

}

// ====================================================================================================================
bool TextureDemo::Initialize()
{
    auto success = BaseApp::Initialize();

    if (success == true)
    {
        ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

        LoadTextures();
        BuildRootSignature();
        BuildDescriptorHeaps();
        BuildMaterials();
        BuildRenderItems();
    }

    return success;
}

// ====================================================================================================================
void TextureDemo::LoadTextures()
{
    auto woodCrateTex = std::make_unique<Texture>();
    woodCrateTex->name_ = "woodCrateTex";
    woodCrateTex->filename_ = L"../../../projects/Textures/WoodCrate01.dds";

    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_d3dDevice.Get(),
                  m_commandList.Get(),
                  woodCrateTex->filename_.c_str(),
                  woodCrateTex->resource_,
                  woodCrateTex->uploadHeap_));

    textures_[woodCrateTex->name_] = std::move(woodCrateTex);
}

// ====================================================================================================================
void TextureDemo::BuildRootSignature()
{
    auto texTable = CD3DX12_DESCRIPTOR_RANGE();
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

    auto staticSamplers = GetStaticSamplers();

    // a root signature consists of root parameters
    // root parameters can be (descriptor tables, descriptor, root constant)
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, static_cast<uint32_t>(staticSamplers.size()), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
    }

    ThrowIfFailed(hr);
    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf())));
}

// ====================================================================================================================
void TextureDemo::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc =  {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvDescriptorHeap_)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());

    auto woodCrateTex = textures_["woodCrateTex"]->resource_;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = woodCrateTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = woodCrateTex->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    m_d3dDevice->CreateShaderResourceView(woodCrateTex.Get(), &srvDesc, hDescriptor);
}

// ====================================================================================================================
void TextureDemo::BuildShadersAndInputLayout()
{
    shaders_["standardVS"] = BaseUtil::CompileShader(L"shaders\\texureDemo.hlsl", nullptr, "VS", "vs_5_0");
    shaders_["opaquePS"] = BaseUtil::CompileShader(L"shaders\\texureDemo.hlsl", nullptr, "PS", "ps_5_0");

    inputLayout_ =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

// ====================================================================================================================
void TextureDemo::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
    MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);

    SubmeshGeometry boxSubmesh;
    boxSubmesh.indexCount = static_cast<UINT>(box.m_indices32.size());
    boxSubmesh.startIndexLocation = 0;
    boxSubmesh.baseVertexLocation = 0;

    std::vector<Vertex> vertices(box.m_vertices.size());

    for (uint32_t i = 0; i < box.m_vertices.size(); ++i)
    {
        vertices[i].m_position = box.m_vertices[i].m_position;
        vertices[i].m_normal = box.m_vertices[i].m_normal;
        vertices[i].m_texC = box.m_vertices[i].m_texC;
    }

    std::vector<std::uint16_t> indices = box.GetIndices16();

    const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    const UINT ibBytesSize = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));

    auto geo = std::make_unique<MeshGeometry>();
    geo->name = "boxGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->vertexBufferCPU));
    CopyMemory(geo->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibBytesSize, &geo->indexBufferCPU));
    CopyMemory(geo->indexBufferCPU->GetBufferPointer(), indices.data(), ibBytesSize);

    geo->vertexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(), m_commandList.Get(), vertices.data(), vbByteSize, geo->vertexBufferUploader);
    geo->indexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(), m_commandList.Get(), indices.data(), ibBytesSize, geo->indexBufferUploader);
    
    geo->vertexByteStride = sizeof(Vertex);
    geo->vertexBufferByteSize = vbByteSize;
    geo->indexFormat = DXGI_FORMAT_R16_UINT;
    geo->indexBufferByteSize = ibBytesSize;
    geo->drawArgs["box"] = boxSubmesh;

    geometries_[geo->name] = std::move(geo);
}

// ====================================================================================================================
void TextureDemo::BuildMaterials()
{
    auto woodCrate = std::make_unique<Material>();
    woodCrate->m_name = "woodCrate";
    woodCrate->m_matCbIndex = 0;
    woodCrate->m_diffuseSrvHeapIndex = 0;
    woodCrate->m_diffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    woodCrate->m_fresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    woodCrate->m_roughness = 0.2f;

    materials_["woodCrate"] = std::move(woodCrate);
}

// ====================================================================================================================
void TextureDemo::BuildRenderItems()
{
    auto boxItem = std::make_unique<RenderItem>();
    boxItem->objCbIndex_ = 0;
    boxItem->mat_ = materials_["woodCrate"].get();
    boxItem->geo_ = geometries_["boxGeo"].get();
    boxItem->primitiveType_ = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxItem->indexCount_ = boxItem->geo_->drawArgs["box"].indexCount;
    boxItem->startIndexLocation_ = boxItem->geo_->drawArgs["box"].startIndexLocation;
    boxItem->baseVertexLocation_ = boxItem->geo_->drawArgs["box"].baseVertexLocation;
    allItems_.push_back(std::move(boxItem));

    for (auto& e : allItems_)
    {
        opaqueItems_.push_back(e.get());
    }
}

// =====================================================================================================================
void TextureDemo::BuildFrameResources()
{
    using namespace FrameResource;
    for (int i = 0; i < NumFrameResources; ++i)
    {
        //frameResources_.push_back(std::make_unique<Resources);
    }
}


// =====================================================================================================================
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> TextureDemo::GetStaticSamplers()
{
    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_WRAP,D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(1, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(3, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(4, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(5, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp };
}

// ====================================================================================================================
// Write the win main func here
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    auto retCode = 0;
    TextureDemo demoApp(hInstance);

    if (demoApp.Initialize() == true)
    {
        retCode = demoApp.Run();
    }

    return retCode;
}



