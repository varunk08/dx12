#pragma once
#ifndef CAMERA_H
#define CAMERA_H

#include "BaseUtil.h"

using namespace DirectX;

// =====================================================================================================================
class Camera
{
public:
    Camera();
    ~Camera() {}

    void SetLens(float fovY, float aspect, float zn, float zf);
    XMMATRIX GetView();
    XMMATRIX GetProj();
    XMFLOAT3 Camera::GetPosition3f()const;

    void Pitch(float value);
    void RotateY(float value);
    void Walk(float value);
    void Strafe(float value);
    void UpdateViewMatrix();
    void LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp);
    void LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up);
    XMFLOAT4X4 GetProj4x4f();

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

void Camera::SetLens(float fovY, float aspect, float zn, float zf) {
    m_fovY                   = fovY;
    m_aspect                 = aspect;
    m_nearZ                  = zn;
    m_farZ                   = zf;
    m_nearWindowHeight       = 2.0f * m_nearZ * tanf( 0.5f*m_fovY );
    m_farWindowHeight        = 2.0f * m_farZ * tanf( 0.5f *m_fovY );

    XMStoreFloat4x4(&m_proj, XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, m_farZ));
}

XMMATRIX Camera::GetView() {
    assert(!m_viewDirty);
    return XMLoadFloat4x4(&m_view);
}

XMMATRIX Camera::GetProj() {
    return XMLoadFloat4x4(&m_proj);
}
 
XMFLOAT3 Camera::GetPosition3f() const {
    return m_position;
}

void Camera::Pitch(float angle) {
    XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&m_right), angle);
    XMStoreFloat3(&m_up,   XMVector3TransformNormal(XMLoadFloat3(&m_up), R));
    XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), R));
    m_viewDirty = true;
}

void Camera::RotateY(float angle) {
    XMMATRIX R = XMMatrixRotationY(angle);
    XMStoreFloat3(&m_right,   XMVector3TransformNormal(XMLoadFloat3(&m_right), R));
    XMStoreFloat3(&m_up, XMVector3TransformNormal(XMLoadFloat3(&m_up), R));
    XMStoreFloat3(&m_look, XMVector3TransformNormal(XMLoadFloat3(&m_look), R));
    m_viewDirty = true;
}

void Camera::Walk(float d) {
    XMVECTOR s = XMVectorReplicate(d);
    XMVECTOR l = XMLoadFloat3(&m_look);
    XMVECTOR p = XMLoadFloat3(&m_position);
    XMStoreFloat3(&m_position, XMVectorMultiplyAdd(s, l, p));
    m_viewDirty = true;
}

void Camera::Strafe(float d) {
    XMVECTOR s = XMVectorReplicate(d);
    XMVECTOR r = XMLoadFloat3(&m_right);
    XMVECTOR p = XMLoadFloat3(&m_position);
    XMStoreFloat3(&m_position, XMVectorMultiplyAdd(s, r, p));
    m_viewDirty = true;
}

void Camera::UpdateViewMatrix() {
    if(m_viewDirty) {
        XMVECTOR R = XMLoadFloat3(&m_right);
        XMVECTOR U = XMLoadFloat3(&m_up);
        XMVECTOR L = XMLoadFloat3(&m_look);
        XMVECTOR P = XMLoadFloat3(&m_position);

        // Keep camera's axes orthogonal to each other and of unit length.
        L = XMVector3Normalize(L);
        U = XMVector3Normalize(XMVector3Cross(L, R));

        // U, L already ortho-normal, so no need to normalize cross product.
        R = XMVector3Cross(U, L);

        // Fill in the view matrix entries.
        float x = -XMVectorGetX(XMVector3Dot(P, R));
        float y = -XMVectorGetX(XMVector3Dot(P, U));
        float z = -XMVectorGetX(XMVector3Dot(P, L));

        XMStoreFloat3(&m_right, R);
        XMStoreFloat3(&m_up, U);
        XMStoreFloat3(&m_look, L);

        m_view(0, 0) = m_right.x;
        m_view(1, 0) = m_right.y;
        m_view(2, 0) = m_right.z;
        m_view(3, 0) = x;

        m_view(0, 1) = m_up.x;
        m_view(1, 1) = m_up.y;
        m_view(2, 1) = m_up.z;
        m_view(3, 1) = y;

        m_view(0, 2) = m_look.x;
        m_view(1, 2) = m_look.y;
        m_view(2, 2) = m_look.z;
        m_view(3, 2) = z;

        m_view(0, 3) = 0.0f;
        m_view(1, 3) = 0.0f;
        m_view(2, 3) = 0.0f;
        m_view(3, 3) = 1.0f;

        m_viewDirty = false;
    }
}    

void Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
    XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
    XMVECTOR U = XMVector3Cross(L, R);
    XMStoreFloat3(&m_position, pos);
    XMStoreFloat3(&m_look, L);
    XMStoreFloat3(&m_right, R);
    XMStoreFloat3(&m_up, U);
    m_viewDirty = true;
}

void Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up)
{
    XMVECTOR P = XMLoadFloat3(&pos);
    XMVECTOR T = XMLoadFloat3(&target);
    XMVECTOR U = XMLoadFloat3(&up);
    LookAt(P, T, U);
    m_viewDirty = true;
}

XMFLOAT4X4 Camera::GetProj4x4f()
{
    return m_proj;
}

#endif // CAMERA_H