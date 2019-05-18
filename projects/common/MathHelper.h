#pragma once

#ifndef MATH_HELPER_H
#define MATH_HELPER_H

#include <DirectXMath.h>

// ==========================================================================================
class MathHelper
{
public:

    static const float Pi;
    static const float Infinity;

    static DirectX::XMFLOAT4X4 Identity4x4()
    {
        static DirectX::XMFLOAT4X4 I(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);

        return I;
    };
};

#endif MATH_HELPER_H
