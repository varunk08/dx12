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
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
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
};

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

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

#endif VKD3D12_UTIL_H
