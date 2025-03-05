#pragma once

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform2.hpp>

// GLEW
#include <GL/glew.h>

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

// Utils
#include "GLUtils.hpp"

class Model {
private:
	GLuint m_programID;
	GLuint m_textureID;
	GLuint m_conemapID;
	glm::mat4 m_transform;
	glm::mat4 m_defaultTransform;
	OGLObject m_GPU;
	bool m_wireFrame;
	int m_drawMode;

	void CleanGeometry() {
		CleanOGLObject(m_GPU);
	}

public:
	Model(GLuint programID, GLuint textureID, GLuint conemapID, glm::mat4& transform, OGLObject& GPU, bool wireFrame = true, int drawMode = GL_TRIANGLES) {
		m_programID = programID;
		m_textureID = textureID;
		m_conemapID = conemapID;
		m_transform = transform;
		m_defaultTransform = transform;
		m_GPU = GPU;
		m_wireFrame = wireFrame;
		m_drawMode = drawMode;
	}

	GLuint GetProgram() {
		return m_programID;
	}
	void SetProgramID(GLuint programID) {
		m_programID = programID;
	}

	GLuint GetTexture() {
		return m_textureID;
	}
	void SetTexture(GLuint textureID) {
		m_textureID = textureID;
	}

	GLuint GetConemap() {
		return m_conemapID;
	}
	void SetConemap(GLuint conemapID) {
		m_conemapID = conemapID;
	}

	glm::mat4 GetTransform() {
		return m_transform;
	}
	void ResetTransform() {
		m_transform = m_defaultTransform;
	}
	void UpdateTransformL(glm::mat4 transform) {
		m_transform = transform * m_transform;
	}
	void UpdateTransformR(glm::mat4 transform) {
		m_transform *= transform;
	}

	GLuint GetVAO() {
		return m_GPU.vaoID;
	}
	GLsizei GetVertexCount() {
		return m_GPU.count;
	}
	void SetGeometry(OGLObject& GPU) {
		CleanGeometry();
		m_GPU = GPU;
	}

	void SetWireFrame(bool wireFrame) {
		m_wireFrame = wireFrame;
	}
	bool GetWireFrame() {
		return m_wireFrame;
	}

	void SetDrawMode(int drawMode) {
		m_drawMode = drawMode;
	}
	int GetDrawMode() {
		return m_drawMode;
	}

	~Model() {
		CleanGeometry();
	}

};