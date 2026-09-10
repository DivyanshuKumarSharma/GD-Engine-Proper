#pragma once
#include <GL/glew.h>
#include "graphics/VertexLayout.h"

#include <glm//vec3.hpp>

#include <memory>
#include <string>

namespace eng
{
    class Mesh
    {
    public:
        Mesh(const VertexLayout& layout, const std::vector<float>& vertices, const std::vector<uint32_t>& indices);
        Mesh(const VertexLayout& layout, const std::vector<float>& vertices);
        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        void Bind();
        void Unbind();
        void Draw();
        void DrawIndexedRange(uint32_t startIndex, uint32_t indexCount);
        void UpdateDynamic(const std::vector<float>& vertices);
        void UpdateDynamic(const std::vector<float>& vertices, const std::vector<uint32_t>& indices);

        static std::shared_ptr<Mesh> CreateBox(const glm::vec3& extents = glm::vec3(1.0f));
        static std::shared_ptr<Mesh> CreateSphere(float radius, int sectors, int stacks);
        static std::shared_ptr<Mesh> CreatePlane();

        // Flat shapes in the local XY plane, facing +Z, centred on the origin.
        // width/height are the full extents; UVs span [0,1] with V up.
        static std::shared_ptr<Mesh> CreateQuad(float width = 1.0f, float height = 1.0f);
        // Regular n-gon (3 = triangle) inscribed in `radius`, double-sided.
        static std::shared_ptr<Mesh> CreatePolygon(int sides, float radius = 0.5f);
        // Solid of revolution about local Y, centred on the origin.
        static std::shared_ptr<Mesh> CreateCylinder(float radius = 0.5f, float height = 1.0f, int sides = 24);

    private:
        VertexLayout m_vertexLayout;
        GLuint m_VBO = 0;
        GLuint m_EBO = 0;
        GLuint m_VAO = 0;

        size_t m_vertexCout = 0;
        size_t m_indexCount = 0;
    };
}