#pragma once

#include <vector>
#include <string>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

// GLEW
#include <GL/glew.h>

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

// Utils
#include "Camera.h"
#include "CameraManipulator.h"
#include "GLUtils.hpp"

// Headers
#include "Model.h"

struct SUpdateInfo
{
	float ElapsedTimeInSec = 0.0f;	// Elapsed time since start of the program
	float DeltaTimeInSec = 0.0f;	// Elapsed time since last update
};

class CMyApp
{
public:
	CMyApp();
	~CMyApp();

	bool Init();
	void Clean();

	void Update(const SUpdateInfo&);
	void Render();
	void RenderGUI();

	void KeyboardDown(const SDL_KeyboardEvent&);
	void KeyboardUp(const SDL_KeyboardEvent&);
	void MouseMove(const SDL_MouseMotionEvent&);
	void MouseDown(const SDL_MouseButtonEvent&);
	void MouseUp(const SDL_MouseButtonEvent&);
	void MouseWheel(const SDL_MouseWheelEvent&);
	void Resize(int, int);

	void OtherEvent(const SDL_Event&);
protected:
	void SetupDebugCallback();

	//
	// Variables
	//
	float m_ElapsedTimeInSec = 0.0f;
	int m_width = 640, m_height = 480;
	glm::vec2 m_HMres{};
	glm::vec2 m_HMres_r{};
	
	// Camera
	Camera m_camera;
	CameraManipulator m_cameraManipulator;

	// Shader variables
	GLuint m_programAxesID = 0;			// Program showing X,Y,Z directions
	GLuint m_programSimpleID = 0;		// Incrementally rendering pipeline
	GLuint m_programSkyboxID = 0;		// Skybox shaders
	GLuint m_programConemapID = 0;		// Conemap generation
	GLuint m_programModelID = 0;		// Drawing models
	GLuint m_programPointsID = 0;		// Drawing points

	// Light source
	glm::vec3 m_lightPos = glm::vec3(0, 1., 0);
	glm::vec3 m_lightCol = glm::vec3(1., 1., 1.);

	// Shader initialization and termination
	void InitShaders();
	void CleanShaders();
	void InitSkyboxShader();
	void CleanSkyboxShader();
	void InitAxesShader();
	void CleanAxesShader();

	// Geometry variables
	OGLObject m_SkyboxGPU = {};
	std::vector<Model*> m_models{};

	// Geometry initialization and termination
	void InitGeometry();
	void CleanGeometry();
	void InitSkyboxGeometry();
	void CleanSkyboxGeometry();
	void InitModels();
	void CleanModels();

	// Textures
	GLuint m_skyboxTextureID = 0;
	GLuint m_heightmapTexureID = 0;
	GLuint m_conemapTextureID = 0;
	GLuint m_modelTextureID = 0;

	// Texture initialization
	void InitTexture();
	void CleanTexture();
	void InitHeightMapTexture();
	void CleanHeightMapTexture();
	void InitSkyboxTexture();
	void CleanSkyboxTexture();
	void InitConemapTexture();
	void CleanConemapTexture();

	// rendering methods
	void RenderConemap();
	void DrawAxes();
	void DrawPoints();
	void RenderModels();
	void RenderSkybox();

	// buffers
	GLuint m_pointsSSBO = 0;
	void InitSSBOs();
	void CleanSSBOs();
	void SetPointsBase();

	// ImGui stuff
	bool m_showPoints = true;
	float m_pointsBase[3]{ 2.2f, .5f, 0 };
	float m_pointsDir[3]{ 0, 0, 1.f };
	bool m_displayNonConverged = false;
	bool m_discardFragments = true;
	float m_epsilon = 0.01;
	int m_activeHeightMap = 0;
	float m_modelNormalMult = 0.1;
	int m_maxSteps = 50;
	std::vector<std::string> m_heightMaps{
		"Assets/heightmap_dot.png",
		"Assets/spikes.png",
		"Assets/hemisphere.png",
		"Assets/cone.jpg",
		"Assets/Earth-heightmap-small.png",
		"Assets/circles-height-map.jpg",
		"Assets/rocks-heightmap.jpg"
	};
};