#include "../header/GLShaderProgram.h"
#include <iostream>
#include <vector>

namespace DTIFiberLib {

GLShaderProgram::GLShaderProgram()
    : m_programID(0)
{
}

GLShaderProgram::~GLShaderProgram()
{
    if (m_programID != 0) {
        glDeleteProgram(m_programID);
        m_programID = 0;
    }
}

bool GLShaderProgram::loadFromString(const char* vertexSource, const char* fragmentSource)
{
    // Compile vertex shader
    GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    if (vertexShader == 0) {
        std::cerr << "Failed to compile vertex shader" << std::endl;
        return false;
    }

    // Compile fragment shader
    GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    if (fragmentShader == 0) {
        std::cerr << "Failed to compile fragment shader" << std::endl;
        glDeleteShader(vertexShader);
        return false;
    }

    // Link shaders into program
    if (!linkProgram(vertexShader, fragmentShader)) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    // Clean up shaders (they're linked into program now)
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

void GLShaderProgram::use()
{
    if (m_programID != 0) {
        glUseProgram(m_programID);
    }
}

void GLShaderProgram::setUniformMatrix4fv(const char* name, const float* value)
{
    GLint location = glGetUniformLocation(m_programID, name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, value);
    }
}

void GLShaderProgram::setUniform1i(const char* name, int value)
{
    GLint location = glGetUniformLocation(m_programID, name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void GLShaderProgram::setUniform1f(const char* name, float value)
{
    GLint location = glGetUniformLocation(m_programID, name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void GLShaderProgram::setUniform3f(const char* name, float v0, float v1, float v2)
{
    GLint location = glGetUniformLocation(m_programID, name);
    if (location != -1) {
        glUniform3f(location, v0, v1, v2);
    }
}

GLuint GLShaderProgram::compileShader(const char* source, GLenum shaderType)
{
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // Check compilation errors
    checkCompileErrors(shader, shaderType == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT");

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool GLShaderProgram::linkProgram(GLuint vertexShader, GLuint fragmentShader)
{
    m_programID = glCreateProgram();
    glAttachShader(m_programID, vertexShader);
    glAttachShader(m_programID, fragmentShader);
    glLinkProgram(m_programID);

    // Check linking errors
    checkCompileErrors(m_programID, "PROGRAM");

    GLint success;
    glGetProgramiv(m_programID, GL_LINK_STATUS, &success);
    if (!success) {
        glDeleteProgram(m_programID);
        m_programID = 0;
        return false;
    }

    return true;
}

void GLShaderProgram::checkCompileErrors(GLuint shader, const char* type)
{
    GLint success;
    std::vector<GLchar> infoLog(1024);

    if (strcmp(type, "PROGRAM") != 0) {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog.data());
            std::cerr << "Shader compilation error (" << type << "):\n" << infoLog.data() << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog.data());
            std::cerr << "Shader program linking error:\n" << infoLog.data() << std::endl;
        }
    }
}

} // namespace DTIFiberLib
