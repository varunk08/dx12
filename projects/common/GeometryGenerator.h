#pragma once

#include <cstdint>
#include <DirectXMath.h>
#include <vector>

using uint16 = std::uint16_t;
using uint32 = std::uint32_t;

// Helper class for vertices
class Vertex
{
public:
    Vertex(){}
    Vertex(
        const DirectX::XMFLOAT3& p,
        const DirectX::XMFLOAT3& n,
        const DirectX::XMFLOAT3& t,
        const DirectX::XMFLOAT2& uv)
        :
        m_position(p),
        m_normal(n),
        m_tangentU(t),
        m_texC(uv)
        {}

    Vertex(
        float px, float py, float pz,
        float nx, float ny, float nz,
        float tx, float ty, float tz,
        float u, float v)
        :
        m_position(px,py,pz),
        m_normal(nx,ny,nz),
        m_tangentU(tx, ty, tz),
        m_texC(u,v)
        {}

    DirectX::XMFLOAT3 m_position;
    DirectX::XMFLOAT3 m_normal;
    DirectX::XMFLOAT3 m_tangentU;
    DirectX::XMFLOAT2 m_texC;
};


class MeshData
{
public:

    MeshData() {}
    MeshData(const MeshData& other)
    {
        m_vertices.resize(other.m_vertices.size());
        m_indices32.resize(other.m_indices32.size());

        m_vertices.assign(other.m_vertices.cbegin(), other.m_vertices.cend());
        m_indices32.assign(other.m_indices32.cbegin(), other.m_indices32.cend());
    }

    std::vector<uint16>& GetIndices16()
    {
        if (m_indices16.empty())
        {
            m_indices16.resize(m_indices32.size());
            for (uint32 i = 0; i < m_indices16.size(); i++)
            {
                m_indices16[i] = static_cast<uint16>(m_indices32[i]);
            }

        }

        return m_indices16;
    }
    
    std::vector<Vertex> m_vertices;
    std::vector<uint32> m_indices32;

private:
    std::vector<uint16> m_indices16;
};

// Generates simple geometry
class GeometryGenerator
{
public:
    MeshData CreateBox(float width, float height, float depth, uint32 numSubdivisions);
    MeshData CreateSphere(float radius, uint32 sliceCount, uint32 stackCount);
    MeshData CreateGeoSphere(float radius, uint32 numSubdivisions);
    MeshData CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);
    MeshData CreateGrid(float width, float depth, uint32 m, uint32 n);
    MeshData CreateQuad(float x, float y, float w, float h, float depth);

private:
    void SubDivide(MeshData& meshData);

    Vertex MidPoint(const Vertex& v0, const Vertex& v1);

    void BuildCylinderTopCap(float bottomRadius,
                             float topRadius,
                             float height,
                             uint32 sliceCount,
                             uint32 stackCount,
                             MeshData& meshData);

    void BuildCylinderBottomCap(float bottomRadius,
                                float topRadius,
                                float height,
                                uint32 sliceCount,
                                uint32 stackCount,
                                MeshData& meshData);
};