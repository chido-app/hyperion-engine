#include "TerrainMeshBuilder.hpp"
#include <Threads.hpp>
#include <math/MathUtil.hpp>
#include <scene/terrain/TerrainErosion.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

TerrainMeshBuilder::TerrainMeshBuilder(const PatchInfo &patch_info)
    : m_height_data(patch_info)
{
}

void TerrainMeshBuilder::GenerateHeights(const NoiseCombinator &noise_combinator)
{
    Threads::AssertOnThread(THREAD_TASK);

    DebugLog(
        LogType::Debug,
        "Generate Terrain mesh at coord [%f, %f]\n",
        m_height_data.patch_info.coord.x,
        m_height_data.patch_info.coord.y
    );

    for (UInt z = 0; z < m_height_data.patch_info.extent.depth; z++) {
        for (UInt x = 0; x < m_height_data.patch_info.extent.width; x++) {
            const Float x_offset = Float(x + (m_height_data.patch_info.coord.x * (m_height_data.patch_info.extent.width - 1))) / Float(m_height_data.patch_info.extent.width);
            const Float z_offset = Float(z + (m_height_data.patch_info.coord.y * (m_height_data.patch_info.extent.depth - 1))) / Float(m_height_data.patch_info.extent.depth);

            const UInt index = m_height_data.GetHeightIndex(x, z);

            m_height_data.heights[index] = TerrainHeight {
                .height = noise_combinator.GetNoise(Vector(x_offset, z_offset)),
                .erosion = 0.0f,
                .sediment = 0.0f,
                .water = 1.0f,
                .new_water = 0.0f,
                .displacement = 0.0f
            };
        }
    }

    TerrainErosion::Erode(m_height_data);
}

Handle<Mesh> TerrainMeshBuilder::BuildMesh() const
{
    Threads::AssertOnThread(THREAD_TASK);

    Array<Vertex> vertices = BuildVertices();
    Array<Mesh::Index> indices = BuildIndices();

    auto mesh = CreateObject<Mesh>(
        vertices,
        indices,
        Topology::TRIANGLES,
        renderer::static_mesh_vertex_attributes
    );

    mesh->CalculateNormals();
    mesh->CalculateTangents();

    return mesh;
}

Array<Vertex> TerrainMeshBuilder::BuildVertices() const
{
    Array<Vertex> vertices;
    vertices.Resize(m_height_data.patch_info.extent.width * m_height_data.patch_info.extent.depth);

    UInt i = 0;

    for (UInt z = 0; z < m_height_data.patch_info.extent.depth; z++) {
        for (UInt x = 0; x < m_height_data.patch_info.extent.width; x++) {
            Vector3 position(x, m_height_data.heights[i].height, z);
            position *= m_height_data.patch_info.scale;

            Vector2 texcoord(
               Float(x) / Float(m_height_data.patch_info.extent.width),
               Float(z) / Float(m_height_data.patch_info.extent.depth)
            );

            vertices[i++] = Vertex(position, texcoord);
        }
    }

    return vertices;
}

Array<Mesh::Index> TerrainMeshBuilder::BuildIndices() const
{
    Array<Mesh::Index> indices;
    indices.Resize(6 * (m_height_data.patch_info.extent.width - 1) * (m_height_data.patch_info.extent.depth - 1));

    UInt pitch = m_height_data.patch_info.extent.width;
    UInt row = 0;

    UInt i0 = row;
    UInt i1 = row + 1;
    UInt i2 = pitch + i1;
    UInt i3 = pitch + row;

    UInt i = 0;

    for (UInt z = 0; z < m_height_data.patch_info.extent.depth - 1; z++) {
        for (UInt x = 0; x < m_height_data.patch_info.extent.width - 1; x++) {
            indices[i++] = i0;
            indices[i++] = i2;
            indices[i++] = i1;
            indices[i++] = i2;
            indices[i++] = i0;
            indices[i++] = i3;

            i0++;
            i1++;
            i2++;
            i3++;
        }

        row += pitch;

        i0 = row;
        i1 = row + 1;
        i2 = pitch + i1;
        i3 = pitch + row;
    }

    return indices;
}

} // namespace hyperion::v2