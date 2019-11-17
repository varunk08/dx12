#include <iostream>

#include "windows.h"

#include "BaseApp.h"

using namespace std;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

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

    void LoadTextures();
    void BuildRootSignature();

private:
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



