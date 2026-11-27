#include "render/Mesh.h"
#include "Graphics/GraphicsAPI.h"
#include "Engine.h"

#include <cmath>

namespace eng
{
    Mesh::Mesh(const VertexLayout& layout, const std::vector<float>& vertices, const std::vector<uint32_t>& indices)
    {
        m_vertexLayout = layout;

        auto& graphicsAPI = Engine::GetInstance().GetGraphicsAPI();

        m_VBO = graphicsAPI.CreateVertexBuffer(vertices);
        m_EBO = graphicsAPI.CreateIndexBuffer(indices);

        glGenVertexArrays(1, &m_VAO);
        glBindVertexArray(m_VAO);

        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);

        for (auto& element : m_vertexLayout.elements)
        {
            glVertexAttribPointer(element.index, element.size, element.type, GL_FALSE,
                m_vertexLayout.stride, (void*)(uintptr_t)element.offset);
            glEnableVertexAttribArray(element.index);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        m_vertexCout = (vertices.size() * sizeof(float)) / m_vertexLayout.stride;
        m_indexCount = indices.size();
    }

    Mesh::Mesh(const VertexLayout& layout, const std::vector<float>& vertices)
    {
        m_vertexLayout = layout;

        auto& graphicsAPI = Engine::GetInstance().GetGraphicsAPI();

        m_VBO = graphicsAPI.CreateVertexBuffer(vertices);

        glGenVertexArrays(1, &m_VAO);
        glBindVertexArray(m_VAO);

        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);

        for (auto& element : m_vertexLayout.elements)
        {
            glVertexAttribPointer(element.index, element.size, element.type, GL_FALSE,
                m_vertexLayout.stride, (void*)(uintptr_t)element.offset);
            glEnableVertexAttribArray(element.index);
        }

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        m_vertexCout = (vertices.size() * sizeof(float)) / m_vertexLayout.stride;
    }

    void Mesh::Bind()
    {
        glBindVertexArray(m_VAO);
    }

    void Mesh::Unbind()
    {
        glBindVertexArray(0);
    }

    void Mesh::Draw()
    {
        if (m_indexCount > 0)
        {
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indexCount), GL_UNSIGNED_INT, 0);
        }
        else
        {
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertexCout));
        }
    }

    void Mesh::DrawIndexedRange(uint32_t startIndex, uint32_t indexCount)
    {
        if (indexCount == 0)
        {
            return;
        }
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), 
            GL_UNSIGNED_INT, 
            reinterpret_cast<void*>(static_cast<size_t>(startIndex) * sizeof(uint32_t)));
    }

    void Mesh::UpdateDynamic(const std::vector<float>& vertices)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_vertexCout = (vertices.size() * sizeof(float)) / m_vertexLayout.stride;
    }

    void Mesh::UpdateDynamic(const std::vector<float>& vertices, const std::vector<uint32_t>& indices)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_vertexCout = (vertices.size() * sizeof(float)) / m_vertexLayout.stride;

        if (m_EBO == 0)
        {
            Engine::GetInstance().GetGraphicsAPI().CreateIndexBuffer(indices);
        }
        else
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                indices.size() * sizeof(uint32_t), indices.data(), GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }
        m_indexCount = indices.size();
    }

    std::shared_ptr<Mesh> Mesh::CreateBox(const glm::vec3& extents)
    {
        const glm::vec3 half = extents * 0.5f;
        std::vector<float> vertices =
        {
            // Front face
            half.x, half.y, half.z, 1.0f, 0.0f, 0.0f, extents.x, extents.y, 0.0f, 0.0f, 1.0f,
            -half.x, half.y, half.z, 0.0f, 1.0f, 0.0f, 0.0f, extents.y, 0.0f, 0.0f, 1.0f,
            -half.x, -half.y, half.z, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            half.x, -half.y, half.z, 1.0f, 1.0f, 0.0f, extents.x, 0.0f, 0.0f, 0.0f, 1.0f,

            // Top face 
            half.x, half.y, -half.z, 1.0f, 0.0f, 0.0f, extents.x, extents.z, 0.0f, 1.0f, 0.0f,
            -half.x, half.y, -half.z, 0.0f, 1.0f, 0.0f, 0.0f, extents.z, 0.0f, 1.0f, 0.0f,
            -half.x, half.y, half.z, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            half.x, half.y, half.z, 1.0f, 1.0f, 0.0f, extents.x, 0.0f, 0.0f, 1.0f, 0.0f,

            // Right face
            half.x, half.y, -half.z, 1.0f, 0.0f, 0.0f, extents.z, extents.y, 1.0f, 0.0f, 0.0f,
            half.x, half.y, half.z, 0.0f, 1.0f, 0.0f, 0.0f, extents.y, 1.0f, 0.0f, 0.0f,
            half.x, -half.y, half.z, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            half.x, -half.y, -half.z, 1.0f, 1.0f, 0.0f, extents.z, 0.0f, 1.0f, 0.0f, 0.0f,

            // Left face
            -half.x, half.y, half.z, 1.0f, 0.0f, 0.0f, extents.z, extents.y, -1.0f, 0.0f, 0.0f,
            -half.x, half.y, -half.z, 0.0f, 1.0f, 0.0f, 0.0f, extents.y, -1.0f, 0.0f, 0.0f,
            -half.x, -half.y, -half.z, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
            -half.x, -half.y, half.z, 1.0f, 1.0f, 0.0f, extents.z, 0.0f, -1.0f, 0.0f, 0.0f,

            // Bottom face
            half.x, -half.y, half.z, 1.0f, 0.0f, 0.0f, extents.x, extents.z, 0.0f, -1.0f, 0.0f,
            -half.x, -half.y, half.z, 0.0f, 1.0f, 0.0f, 0.0f, extents.z, 0.0f, -1.0f, 0.0f,
            -half.x, -half.y, -half.z, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,
            half.x, -half.y, -half.z, 1.0f, 1.0f, 0.0f, extents.x, 0.0f, 0.0f, -1.0f, 0.0f,

            // Back face
            -half.x, half.y, -half.z, 1.0f, 0.0f, 0.0f, extents.x, extents.y, 0.0f, 0.0f, -1.0f,
            half.x, half.y, -half.z, 0.0f, 1.0f, 0.0f, 0.0f, extents.y, 0.0f, 0.0f, -1.0f,
            half.x, -half.y, -half.z, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
            -half.x, -half.y, -half.z, 1.0f, 1.0f, 0.0f, extents.x, 0.0f, 0.0f, 0.0f, -1.0f
        };

        std::vector<unsigned int> indices =
        {
            // front face
            0, 1, 2,
            0, 2, 3,
            // top face
            4, 5, 6,
            4, 6, 7,
            // right face
            8, 9, 10,
            8, 10, 11,
            // left face
            12, 13, 14,
            12, 14, 15,
            // bottom face
            16, 17, 18,
            16, 18, 19,
            // back face
            20, 21, 22,
            20, 22, 23
        };

        eng::VertexLayout vertexLayout;

        // Postion
        vertexLayout.elements.push_back({
            VertexElement::PositionIndex,
            3,
            GL_FLOAT,
            0
            });
        // Color
        vertexLayout.elements.push_back({
            VertexElement::ColorIndex,
            3,
            GL_FLOAT,
            sizeof(float) * 3
            });
        // UV
        vertexLayout.elements.push_back({
            VertexElement::UVIndex,
            2,
            GL_FLOAT,
            sizeof(float) * 6
            });
        // Normal
        vertexLayout.elements.push_back({
            VertexElement::NormalIndex,
            3,
            GL_FLOAT,
            sizeof(float) * 8
            });
        vertexLayout.stride = sizeof(float) * 11;

        auto result = std::make_shared<eng::Mesh>(vertexLayout, vertices, indices);

        return result;
    }

    std::shared_ptr<Mesh> Mesh::CreateSphere(float radius, int sectors, int stacks)
    {
        const float PI = 3.14159265358979323846f;

        std::vector<float> vertices((stacks + 1) * (sectors + 1) * 8);
        for (int i = 0; i <= stacks; ++i)
        {
            float stackAngle = PI / 2.0f - static_cast<float>(i) * (PI / static_cast<float>(stacks)); // From -π/2 to π/2
            float xy = radius * cosf(stackAngle); // x-y plane radius at this stack
            float z = radius * sinf(stackAngle);  // z coordinate

            for (int j = 0; j <= sectors; ++j) {
                float sectorAngle = static_cast<float>(j) * (2.0f * PI / static_cast<float>(sectors)); // From 0 to 2π

                float x = xy * cosf(sectorAngle);
                float y = xy * sinf(sectorAngle);

                size_t vertexStart = (i * (sectors + 1) + j) * 8;
                // Position
                vertices[vertexStart] = x;
                vertices[vertexStart + 1] = y;
                vertices[vertexStart + 2] = z;

                // Normal (normalized position vector)
                float length = sqrtf(x * x + y * y + z * z);
                vertices[vertexStart + 3] = x / length;
                vertices[vertexStart + 4] = y / length;
                vertices[vertexStart + 5] = z / length;

                // UV coordinates
                vertices[vertexStart + 6] = static_cast<float>(j) / static_cast<float>(sectors);
                vertices[vertexStart + 7] = static_cast<float>(i) / static_cast<float>(stacks);
            }
        }

        // Generate indices
        std::vector<unsigned int> indices;
        for (int i = 0; i < stacks; ++i)
        {
            int k1 = i * (sectors + 1);
            int k2 = k1 + sectors + 1;

            for (int j = 0; j < sectors; ++j, ++k1, ++k2)
            {
                if (i != 0)
                {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }

                if (i != (stacks - 1))
                {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }

        eng::VertexLayout vertexLayout;

        // Postion
        vertexLayout.elements.push_back({
            VertexElement::PositionIndex,
            3,
            GL_FLOAT,
            0
            });
        // Normal
        vertexLayout.elements.push_back({
            VertexElement::NormalIndex,
            3,
            GL_FLOAT,
            sizeof(float) * 3
            });
        // UV
        vertexLayout.elements.push_back({
            VertexElement::UVIndex,
            2,
            GL_FLOAT,
            sizeof(float) * 6
            });
        vertexLayout.stride = sizeof(float) * 8;

        auto result = std::make_shared<eng::Mesh>(vertexLayout, vertices, indices);

        return result;
    }

    namespace
    {
        // pos(3) normal(3) uv(2), matching the CreateSphere layout the default
        // 3D shader expects (the colour attribute at location 1 is unused).
        eng::VertexLayout PosNormalUvLayout()
        {
            eng::VertexLayout layout;
            layout.elements.push_back({ VertexElement::PositionIndex, 3, GL_FLOAT, 0 });
            layout.elements.push_back({ VertexElement::NormalIndex, 3, GL_FLOAT, sizeof(float) * 3 });
            layout.elements.push_back({ VertexElement::UVIndex, 2, GL_FLOAT, sizeof(float) * 6 });
            layout.stride = sizeof(float) * 8;
            return layout;
        }
    }

    std::shared_ptr<Mesh> Mesh::CreateQuad(float width, float height)
    {
        const float hw = width * 0.5f;
        const float hh = height * 0.5f;
        const std::vector<float> vertices =
        {
            -hw, -hh, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,
             hw, -hh, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
             hw,  hh, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
            -hw,  hh, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,
        };
        const std::vector<uint32_t> indices = { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 };
        return std::make_shared<Mesh>(PosNormalUvLayout(), vertices, indices);
    }

    std::shared_ptr<Mesh> Mesh::CreatePolygon(int sides, float radius)
    {
        sides = sides < 3 ? 3 : sides;
        const float PI = 3.14159265358979323846f;

        std::vector<float> vertices;
        vertices.insert(vertices.end(), { 0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.5f, 0.5f }); // centre
        for (int i = 0; i <= sides; ++i)
        {
            const float a = PI * 0.5f + static_cast<float>(i) * (2.0f * PI / static_cast<float>(sides));
            const float x = std::cos(a) * radius;
            const float y = std::sin(a) * radius;
            vertices.insert(vertices.end(), { x, y, 0.0f,  0.0f, 0.0f, 1.0f,  0.5f + x / (2.0f * radius), 0.5f + y / (2.0f * radius) });
        }

        std::vector<uint32_t> indices;
        for (int i = 1; i <= sides; ++i)
        {
            indices.push_back(0);
            indices.push_back(static_cast<uint32_t>(i));
            indices.push_back(static_cast<uint32_t>(i + 1));
            // back face
            indices.push_back(0);
            indices.push_back(static_cast<uint32_t>(i + 1));
            indices.push_back(static_cast<uint32_t>(i));
        }
        return std::make_shared<Mesh>(PosNormalUvLayout(), vertices, indices);
    }

    std::shared_ptr<Mesh> Mesh::CreateCylinder(float radius, float height, int sides)
    {
        sides = sides < 3 ? 3 : sides;
        const float PI = 3.14159265358979323846f;
        const float hy = height * 0.5f;

        std::vector<float> vertices;
        std::vector<uint32_t> indices;

        // Side wall.
        for (int i = 0; i <= sides; ++i)
        {
            const float a = static_cast<float>(i) * (2.0f * PI / static_cast<float>(sides));
            const float nx = std::cos(a);
            const float nz = std::sin(a);
            const float u = static_cast<float>(i) / static_cast<float>(sides);
            vertices.insert(vertices.end(), { nx * radius, -hy, nz * radius,  nx, 0.0f, nz,  u, 0.0f });
            vertices.insert(vertices.end(), { nx * radius,  hy, nz * radius,  nx, 0.0f, nz,  u, 1.0f });
        }
        for (int i = 0; i < sides; ++i)
        {
            const uint32_t base = static_cast<uint32_t>(i * 2);
            indices.insert(indices.end(), { base, base + 1, base + 2, base + 1, base + 3, base + 2 });
        }

        // Caps.
        const auto addCap = [&](float y, float ny)
        {
            const uint32_t centre = static_cast<uint32_t>(vertices.size() / 8);
            vertices.insert(vertices.end(), { 0.0f, y, 0.0f,  0.0f, ny, 0.0f,  0.5f, 0.5f });
            for (int i = 0; i <= sides; ++i)
            {
                const float a = static_cast<float>(i) * (2.0f * PI / static_cast<float>(sides));
                const float x = std::cos(a) * radius;
                const float z = std::sin(a) * radius;
                vertices.insert(vertices.end(), { x, y, z,  0.0f, ny, 0.0f,  0.5f + x, 0.5f + z });
            }
            for (int i = 1; i <= sides; ++i)
            {
                if (ny > 0.0f)
                {
                    indices.insert(indices.end(), { centre, centre + static_cast<uint32_t>(i), centre + static_cast<uint32_t>(i + 1) });
                }
                else
                {
                    indices.insert(indices.end(), { centre, centre + static_cast<uint32_t>(i + 1), centre + static_cast<uint32_t>(i) });
                }
            }
        };
        addCap(hy, 1.0f);
        addCap(-hy, -1.0f);

        return std::make_shared<Mesh>(PosNormalUvLayout(), vertices, indices);
    }

    std::shared_ptr<Mesh> Mesh::CreatePlane()
    {
        std::vector<float> vertices =
        {
            1.0f, 1.0f,
            0.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f
        };

        std::vector<uint32_t> indices =
        {
            0, 1, 2,
            0, 2, 3
        };

        eng::VertexLayout vertexLayout;

        // Position
        vertexLayout.elements.push_back({
            VertexElement::PositionIndex,
            2,
            GL_FLOAT,
            0
            });
        vertexLayout.stride = sizeof(float) * 2;

        auto result = std::make_shared<eng::Mesh>(vertexLayout, vertices, indices);

        return result;
    }
}