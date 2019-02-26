#pragma once
#ifndef VKD3D12_UTIL_H
#define VKD3D12_UTIL_H

#include <dxgi1_6.h>

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

#endif VKD3D12_UTIL_H
