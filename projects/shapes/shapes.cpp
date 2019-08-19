#include <array>
#include <vector>

#include <Windows.h>

#include <DirectXCollision.h>
#include <DirectXColors.h>
#include <DirectXMath.h>

#include "../common/BaseApp.h"
#include "../common/d3dx12.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;


class ShapesDemo : public BaseApp
{
public:
    ShapesDemo(HINSTANCE hInstance);
    bool Initialize();
protected:
private:
    void Update(const BaseTimer& gt);
    void Draw(const BaseTimer& gt);

    void ShapesBuildRootSignature();

    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
};


bool ShapesDemo::Initialize()
{
    bool result = BaseApp::Initialize();

    if (result == true)
    {
        ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

        ShapesBuildRootSignature();
    }


    return result;
}


ShapesDemo::ShapesDemo(HINSTANCE hInstance)
:
BaseApp(hInstance)
{
}



void ShapesDemo::Update(const BaseTimer& gt)
{

}


void ShapesDemo::Draw(const BaseTimer& gt)
{

}


void ShapesDemo::ShapesBuildRootSignature()
{
    // Create a couple of "descriptor range"s. These map registers in the shader to the
    // resources here.
    CD3DX12_DESCRIPTOR_RANGE cbvTable0;
    cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE cbvTable1;
    cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

    // A "root parameter" can be a descriptor table, root constant or descriptor.
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
    slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSignDesc(2,
                                             slotRootParameter,
                                             0,
                                             nullptr,
                                             D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializeRootSign = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSignDesc,
                                             D3D_ROOT_SIGNATURE_VERSION_1,
                                             serializeRootSign.GetAddressOf(),
                                             errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0,
                                                   serializeRootSign->GetBufferPointer(),
                                                   serializeRootSign->GetBufferSize(),
                                                   IID_PPV_ARGS(m_rootSignature.GetAddressOf())));
}


// Write the win main func here
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    ShapesDemo demoApp(hInstance);
    
    int retCode = 0;
    if (demoApp.Initialize() == 0)
    {
        retCode = demoApp.Run();
    }
    else
    {
        cout << "Error initializing the shapes app...\n";
    }

    return retCode;
}

