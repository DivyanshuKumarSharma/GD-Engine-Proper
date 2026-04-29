#pragma once
#include <GL/glew.h>
#include <string>
#include <unordered_map>

namespace eng
{
    class ShaderProgram
    {
    public:
        ShaderProgram() = delete; // Delete default constructor
        ShaderProgram(const ShaderProgram&) = delete; // Delete copy constructor
        ShaderProgram& operator=(const ShaderProgram&) = delete; // Delete copy assignment operator
        explicit ShaderProgram(GLuint shaderProgramID);
        void Bind();
        GLint getUniformLocation(const std::string& name);
        void setUniform(const std::string& name, float value);

    private:

        GLuint m_shaderProgramID = 0;
        std::unordered_map<std::string, GLint> m_uniformLocationCache;
    };
}