#pragma once
#include <string>
#include <GL/glew.h>
#include <memory>  

namespace eng
{
    class ShaderProgram;
    class GraphicsAPI
    {
    public:
        GraphicsAPI() = default; // Default constructor
        GraphicsAPI(const GraphicsAPI&) = delete; // Delete copy constructor
        GraphicsAPI& operator=(const GraphicsAPI&) = delete; // Delete copy assignment operator

        std::unique_ptr<ShaderProgram> CreateShaderProgram(const std::string& vertexSource, const std::string& fragmentSource);

        void bindShaderProgram(ShaderProgram& shaderProgram);
    };
}