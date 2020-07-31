#ifndef CAMERA_H
#define CAMERA_H

#include "BaseUtil.h"

// =====================================================================================================================
class Camera
{
public:
    Camera();
    ~Camera() {}

    void SetLens(float fovY, float aspect, float zn, float zf);
private:
    DirectX::XMFLOAT3 m_position   = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_right      = {1.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 m_up         = {0.0f, 1.0f, 0.0f};
    DirectX::XMFLOAT3 m_look       = {0.0f, 0.0f, 1.0f};

    float m_nearZ                  = 0.0f;
    float m_farZ                   = 0.0f;
    float m_aspect                 = 0.0f;
    float m_fovY                   = 0.0f;
    float m_nearWindowHeight       = 0.0f;
    float m_farWindowHeight        = 0.0f;

    bool m_viewDirty               = true;

    DirectX::XMFLOAT4X4 m_view     = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 m_proj     = MathHelper::Identity4x4();
};

// =====================================================================================================================
Camera::Camera() {
    SetLens(0.25f * MathHelper::Pi, 1.0f, 1.0f, 1000.0f);
}

#endif // CAMERA_H