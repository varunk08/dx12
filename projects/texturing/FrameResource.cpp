#include "FrameResource.h"

namespace FrameResource
{

Resources::Resources(
    ID3D12Device* device,
    UINT          passCount,
    UINT          objectCount,
    UINT          materialCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                 IID_PPV_ARGS(m_cmdListAlloc.GetAddressOf())));

    m_passCb = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    m_objCb  = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
    m_matCb = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
}

Resources::~Resources()
{

}

}