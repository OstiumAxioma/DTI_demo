#ifndef GLSHADERPROGRAM_H
#define GLSHADERPROGRAM_H

#include <string>
#include <glad/glad.h>

namespace DTIFiberLib {

/**
 * OpenGL Shader Program Manager
 * Handles shader compilation, linking, and uniform variable management
 */
class GLShaderProgram {
public:
    GLShaderProgram();
    ~GLShaderProgram();

    // Load and compile shaders from source strings
    bool loadFromString(const char* vertexSource, const char* fragmentSource);

    // Use this shader program
    void use();

    // Uniform setters
    void setUniformMatrix4fv(const char* name, const float* value);
    void setUniform1i(const char* name, int value);
    void setUniform1f(const char* name, float value);
    void setUniform3f(const char* name, float v0, float v1, float v2);

    // Get program ID
    GLuint getProgramID() const { return m_programID; }

    // Check if program is valid
    bool isValid() const { return m_programID != 0; }

private:
    GLuint compileShader(const char* source, GLenum shaderType);
    bool linkProgram(GLuint vertexShader, GLuint fragmentShader);
    void checkCompileErrors(GLuint shader, const char* type);

    GLuint m_programID;
};

} // namespace DTIFiberLib

#endif // GLSHADERPROGRAM_H
