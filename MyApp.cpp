#include "Headers/MyApp.h"
#include "Headers/Model.h"
#include "SDL_GLDebugMessageCallback.h"
#include "ObjParser.h"
#include "ProgramBuilder.h"

#include "Headers/Log.h"

#include <imgui.h>
#include <iostream>
#include <string>
#include <sstream>

#define SSBO_PADDING 5

CMyApp::CMyApp()
{
}

CMyApp::~CMyApp()
{
}

void CMyApp::SetupDebugCallback()
{
	// Enable and set the debug callback function if we are in debug context
	GLint context_flags;
	glGetIntegerv(GL_CONTEXT_FLAGS, &context_flags);
	if (context_flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
		glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, GL_DONT_CARE, 0, nullptr, GL_FALSE);
		glDebugMessageCallback(SDL_GLDebugMessageCallback, nullptr);
	}
}

void CMyApp::InitShaders()
{
	// Drawing objects
	m_programSimpleID = glCreateProgram();
	ProgramBuilder{ m_programSimpleID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/Vert_PosNormTex.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/Frag_Simple.frag")
		.Link();

	// Drawing models
	m_programModelID = glCreateProgram();
	ProgramBuilder{ m_programModelID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/Models/Vert_Model.vert")
		.ShaderStage(GL_GEOMETRY_SHADER, "Shaders/Models/Geom_Model_old.geom")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/Models/Frag_Improved.frag")
		.Link();

	// Drawing points
	m_programPointsID = glCreateProgram();
	ProgramBuilder{ m_programPointsID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/Debug/Vert_Points.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/Debug/Frag_Points.frag")
		.Link();

	// Drawing cones
	m_programConesID = glCreateProgram();
	ProgramBuilder{ m_programConesID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/Debug/Vert_Cones.vert")
		.ShaderStage(GL_GEOMETRY_SHADER, "Shaders/Debug/Geom_Cones.geom")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/Debug/Frag_Points.frag")
		.Link();

	// Conemap generation
	m_programConemapID = glCreateProgram();
	ProgramBuilder{ m_programConemapID }
		.ShaderStage(GL_COMPUTE_SHADER, "Shaders/Conemap/Comp_Conemap.comp")
		.Link();

	InitAxesShader();
	InitSkyboxShader();
}

void CMyApp::CleanShaders()
{
	glDeleteProgram(m_programSimpleID);
	glDeleteProgram(m_programConemapID);
	glDeleteProgram(m_programModelID);
	CleanSkyboxShader();
	CleanAxesShader();
}

void CMyApp::InitSkyboxShader() {
	m_programSkyboxID = glCreateProgram();
	ProgramBuilder{ m_programSkyboxID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/Skybox/Vert_skybox.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/Skybox/Frag_skybox_skeleton.frag")
		.Link();
}

void CMyApp::CleanSkyboxShader() {
	glDeleteProgram(m_programSkyboxID);
}

void CMyApp::InitAxesShader()
{
	m_programAxesID = glCreateProgram();
	ProgramBuilder{ m_programAxesID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/Axes/Vert_axes.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/Axes/Frag_PosCol.frag")
		.Link();
}

void CMyApp::CleanAxesShader()
{
	glDeleteProgram(m_programAxesID);
}

void CMyApp::InitGeometry()
{
	InitModels();
	InitSkyboxGeometry();
}

void CMyApp::CleanGeometry()
{
	CleanModels();
	CleanSkyboxGeometry();
}

void CMyApp::InitSkyboxGeometry() {
	// skybox geo
	MeshObject<glm::vec3> skyboxCPU =
	{
		std::vector<glm::vec3>
		{
			// back
			glm::vec3(-1, -1, -1),
			glm::vec3(1, -1, -1),
			glm::vec3(1,  1, -1),
			glm::vec3(-1,  1, -1),
			// front
			glm::vec3(-1, -1, 1),
			glm::vec3(1, -1, 1),
			glm::vec3(1,  1, 1),
			glm::vec3(-1,  1, 1),
		},

		std::vector<GLuint>
		{
		// back
		0, 1, 2,
		2, 3, 0,
			// front
			4, 6, 5,
			6, 4, 7,
			// left
			0, 3, 4,
			4, 3, 7,
			// right
			1, 5, 2,
			5, 6, 2,
			// bottom
			1, 0, 4,
			1, 4, 5,
			// top
			3, 2, 6,
			3, 6, 7,
		}
	};

	m_SkyboxGPU = CreateGLObjectFromMesh(skyboxCPU, { { 0, offsetof(glm::vec3, x), 3, GL_FLOAT } });
}

void CMyApp::CleanSkyboxGeometry()
{
	CleanOGLObject(m_SkyboxGPU);
}

void CMyApp::InitModels() {
	{
		const std::initializer_list<VertexAttributeDescriptor> vertexAttribList =
		{
			{ 0, offsetof(VertexMergedNorm, position), 4, GL_FLOAT },
			{ 1, offsetof(VertexMergedNorm, normal), 3, GL_FLOAT },
			{ 2, offsetof(VertexMergedNorm, mergedNormal), 3, GL_FLOAT },
			{ 3, offsetof(VertexMergedNorm, texcoord), 2, GL_FLOAT }
		};

		// Suzanne
		// MeshObject<VertexMergedNorm> SuzanneCPU = ObjParser::mergeNormals(ObjParser::parse("Assets/Suzanne.obj"));
		// MeshObject<VertexMergedNorm> SuzanneCPU = ObjParser::mergeNormals(ObjParser::parse("Assets/ico-sphere-3.obj"));
		// MeshObject<VertexMergedNorm> SuzanneCPU = ObjParser::mergeNormals(ObjParser::parse("Assets/ico-sphere-4.obj"));
		// MeshObject<VertexMergedNorm> SuzanneCPU = ObjParser::mergeNormals(ObjParser::parse("Assets/uv-sphere-32.obj"));
		// MeshObject<VertexMergedNorm> SuzanneCPU = ObjParser::mergeNormals(ObjParser::parse("Assets/uv-sphere-64.obj"));
		// MeshObject<VertexMergedNorm> SuzanneCPU = ObjParser::mergeNormals(ObjParser::parse("Assets/cube.obj"));
		/*
		m_models.push_back(new Model(
			m_programModelID, m_modelTextureID, m_conemapTextureID, glm::scale(glm::vec3(10.0f, 10.0f, 10.0f)),
			CreateGLObjectFromMesh(SuzanneCPU, vertexAttribList), false
		));
		*/
		// SQUARE
		MeshObject<VertexMergedNorm> ObjectCPU = {
			{
				{glm::vec4(0, 0, 0, 1.), glm::vec3(0, 1., 0), glm::vec3(0, 1., 0), glm::vec2(0, 0)},
				{glm::vec4(0, 0, 1., 1.), glm::vec3(0, 1., 0), glm::vec3(0, 1., 0), glm::vec2(0, 1.)},
				{glm::vec4(1., 0, 1., 1.), glm::vec3(0, 1., 0), glm::vec3(0, 1., 0), glm::vec2(1., 1.)},
				{glm::vec4(1., 0, 0, 1.), glm::vec3(0, 1., 0), glm::vec3(0, 1., 0), glm::vec2(1., 0)}
			},
			{
				0,1,2,
				// 0,2,3
			}
		};
		
		m_models.push_back(new Model(
			m_programModelID, m_modelTextureID, m_conemapTextureID, glm::identity<glm::mat4>(), // glm::scale(glm::vec3(10.0f, 10.0f, 10.0f))
			CreateGLObjectFromMesh(ObjectCPU, vertexAttribList), false
		));
	}
	
	{
		// inner sphere
		/*
		const std::initializer_list<VertexAttributeDescriptor> vertexAttribList =
		{
			{ 0, offsetof(VertexMergedNorm, position), 4, GL_FLOAT },
			{ 1, offsetof(VertexMergedNorm, normal), 3, GL_FLOAT },
			{ 2, offsetof(VertexMergedNorm, mergedNormal), 3, GL_FLOAT },
			{ 3, offsetof(VertexMergedNorm, texcoord), 2, GL_FLOAT }
		};
		MeshObject<VertexMergedNorm> ModelCPU = ObjParser::mergeNormals(ObjParser::parse("Assets/ico-sphere-4.obj"));
		m_models.push_back(new Model(
			m_programModelID, m_modelTextureID, m_conemapTextureID, glm::scale(glm::vec3(1.0f, 1.0f, 1.0f)),
			CreateGLObjectFromMesh(ModelCPU, vertexAttribList), false
		));
		*/
	}
}

void CMyApp::CleanModels() {
	for (int i = 0; i < m_models.size(); ++i) {
		delete(m_models[i]);
	}
	m_models.clear();
}

void CMyApp::InitTexture() {
	// Model texture
	{
		ImageRGBA image;
		image = ImageFromFile("Assets/metal.png");
		glGenTextures(1, &m_modelTextureID);
		glBindTexture(GL_TEXTURE_2D, m_modelTextureID);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	InitHeightMapTexture();
	InitConemapTexture();
	InitSkyboxTexture();

	glBindTexture(GL_TEXTURE_2D, 0);
}

void CMyApp::CleanTexture() {
	glDeleteTextures(1, &m_modelTextureID);
	CleanHeightMapTexture();
	CleanConemapTexture();
	CleanSkyboxTexture();
}

void CMyApp::InitHeightMapTexture() {
	// Heightmap texture
	{
		ImageRGBA image;
		image = ImageFromFile(m_heightMaps[m_activeHeightMap]);
		glCreateTextures(GL_TEXTURE_2D, 1, &m_heightmapTexureID);
		glTextureStorage2D(m_heightmapTexureID, 1, GL_RGBA8, image.width, image.height);
		glBindTexture(GL_TEXTURE_2D, m_heightmapTexureID);

		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTextureSubImage2D(m_heightmapTexureID, 0, 0, 0, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, image.data());

		// Setup member fields
		m_HMres.x = image.width;
		m_HMres.y = image.height;
		m_HMres_r.x = 1.f / image.width;
		m_HMres_r.y = 1.f / image.height;
	}
}

void CMyApp::CleanHeightMapTexture() {
	glDeleteTextures(1, &m_heightmapTexureID);
}

void CMyApp::InitConemapTexture() {
	if (m_conemapTextureID != 0) {
		glDeleteTextures(1, &m_conemapTextureID);
	}
	glGenTextures(1, &m_conemapTextureID);
	glBindTexture(GL_TEXTURE_2D, m_conemapTextureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, m_HMres.x, m_HMres.y, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void CMyApp::CleanConemapTexture() {
	glDeleteTextures(1, &m_conemapTextureID);
}

void CMyApp::InitSkyboxTexture() {
	// skybox texture
	static const char* skyboxFiles[6] = {
		"Assets/xpos.png",
		"Assets/xneg.png",
		"Assets/ypos.png",
		"Assets/yneg.png",
		"Assets/zpos.png",
		"Assets/zneg.png",
	};

	ImageRGBA images[6];
	for (int i = 0; i < 6; ++i)
	{
		images[i] = ImageFromFile(skyboxFiles[i], false);
	}

	glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_skyboxTextureID);
	glTextureStorage2D(m_skyboxTextureID, 1, GL_RGBA8, images[0].width, images[0].height);

	for (int face = 0; face < 6; ++face)
	{
		glTextureSubImage3D(m_skyboxTextureID, 0, 0, 0, face, images[face].width, images[face].height, 1, GL_RGBA, GL_UNSIGNED_BYTE, images[face].data());
	}

	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

void CMyApp::CleanSkyboxTexture() {
	glDeleteTextures(1, &m_skyboxTextureID);
}

void CMyApp::InitSSBOs() {
	// visual debug
	glGenBuffers(1, &m_pointsSSBO);
	Log::logToConsole("Visual debug buffer generated (ID: ", m_pointsSSBO, ")");
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_pointsSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec4) * 128, nullptr, GL_DYNAMIC_DRAW);
	glm::vec4 attr{ 2.f, 0, 0, 0 };
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::vec4), &attr);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_pointsSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	
	// numerical debug
	glGenBuffers(1, &m_debugSSBO);
	Log::logToConsole("Numerical debug buffer generated (ID: ", m_debugSSBO, ")");
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_debugSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec4) * 128, nullptr, GL_DYNAMIC_DRAW);
	attr = { 0, 0, 0, 0 };
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::vec4), &attr);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_debugSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	SetPointsBase();
}

void CMyApp::SetPointsBase() {
	std::vector<glm::vec4> Points{
		glm::vec4{m_pointsBase[0], m_pointsBase[1], m_pointsBase[2], 1},
		glm::vec4{m_pointsBase[0] + m_pointsDir[0], m_pointsBase[1] + m_pointsDir[1], m_pointsBase[2] + m_pointsDir[2], 1}
	};
	glNamedBufferSubData(m_pointsSSBO, sizeof(glm::vec4) * SSBO_PADDING, sizeof(glm::vec4) * 2, Points.data());
	glNamedBufferSubData(m_debugSSBO, sizeof(glm::vec4), sizeof(glm::vec4) * 2, Points.data());
}

void CMyApp::CleanSSBOs() {
	glDeleteBuffers(1, &m_pointsSSBO);
}

bool CMyApp::Init()
{
	SetupDebugCallback();

	// Set a bluish clear color
	// glClear() will use this for clearing the color buffer.
	// glClearColor(0.125f, 0.25f, 0.5f, 1.0f);
	glClearColor(0, 0, 0, 1.0f);

	glGenQueries(1, &m_timeQueryID);

	InitShaders();
	InitTexture();
	InitGeometry();
	InitSSBOs();

	//
	// Other
	//
	// glEnable(GL_CULL_FACE);	 // Enable discarding the back-facing faces.
	glCullFace(GL_BACK);     // GL_BACK: facets facing away from camera, GL_FRONT: facets facing towards the camera
	glEnable(GL_DEPTH_TEST); // Enable depth testing. (for overlapping geometry)
	glDepthFunc(GL_LESS);

	// Camera
	m_camera.SetView(
		glm::vec3(0, 20, 20),	// From where we look at the scene - eye
		glm::vec3(0, 4, 0),		// Which point of the scene we are looking at - at
		glm::vec3(0, 1, 0)		// Upwards direction - up
	);
	m_cameraManipulator.SetCamera(&m_camera);

	RenderConemap();

	return true;
}

void CMyApp::RenderConemap() {

	glBindTexture(GL_TEXTURE_2D, m_conemapTextureID);
	if (m_interpolation) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_heightmapTexureID);

	int localSizeX = 16;
	int localSizeY = 16;
	GLuint numWorkgroupsX = (m_HMres.x + localSizeX - 1) / localSizeX;
	GLuint numWorkgroupsY = (m_HMres.y + localSizeY - 1) / localSizeY;

	glUseProgram(m_programConemapID);
	glBindImageTexture(0, m_conemapTextureID, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG8);

	glUniform1i(ul(m_programConemapID, "width"), m_HMres.x);
	glUniform1i(ul(m_programConemapID, "height"), m_HMres.y);
	glUniform1i(ul(m_programConemapID, "inputImage"), 0);

	GLuint timeElapsed;		// elapsed time in nano seconds
	glBeginQuery(GL_TIME_ELAPSED, m_timeQueryID);

	glDispatchCompute(numWorkgroupsX, numWorkgroupsY, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	glEndQuery(GL_TIME_ELAPSED);
	glGetQueryObjectuiv(m_timeQueryID, GL_QUERY_RESULT, &timeElapsed);

	Log::logToConsole("Conemap generated in ", timeElapsed / 1000., "ms");

	glBindTexture(GL_TEXTURE_2D, 0);
}

void CMyApp::Clean()
{
	CleanShaders();
	CleanGeometry();
	CleanTexture();
	CleanSSBOs();
}

void CMyApp::Update(const SUpdateInfo& updateInfo)
{
	m_cameraManipulator.Update(updateInfo.DeltaTimeInSec);
	m_ElapsedTimeInSec = updateInfo.ElapsedTimeInSec;
}

void CMyApp::DrawAxes()
{
	glUseProgram(m_programAxesID);

	glm::mat4 axisWorld = glm::translate(m_camera.GetAt());
	glProgramUniformMatrix4fv( m_programAxesID, ul(m_programAxesID, "viewProj"), 1, GL_FALSE, glm::value_ptr(m_camera.GetViewProj()));
	glProgramUniformMatrix4fv( m_programAxesID, ul(m_programAxesID, "world"), 1, GL_FALSE, glm::value_ptr(axisWorld));

	// We always want to see it, regardless of whether there is an object in front of it
	glDisable(GL_DEPTH_TEST);
	
	glDrawArrays(GL_LINES, 0, 6);
	glUseProgram(0);
	glEnable(GL_DEPTH_TEST);
}

void CMyApp::RenderDebug() {
	int n = 0;
	void* ptr = glMapNamedBuffer(m_pointsSSBO, GL_READ_ONLY);
	if (ptr) {
		glm::vec4* data = static_cast<glm::vec4*>(ptr);
		n = (int) data[0].x;
		glUnmapNamedBuffer(m_pointsSSBO);
	} else {
		Log::errorToConsole("Unable to map SSBO");
		return;
	}
	Log::logToConsole("Points count: ", n);
	// std::cout << "n = " << n << std::endl;

	// We always want to see it, regardless of whether there is an object in front of it
	glDisable(GL_DEPTH_TEST);

	//
	// Points
	//
	if (n > 0) {
		glUseProgram(m_programPointsID);

		glUniform1i(ul(m_programPointsID, "SSBOPadding"), SSBO_PADDING);
		glUniformMatrix4fv(ul(m_programPointsID, "viewProj"), 1, GL_FALSE, glm::value_ptr(m_camera.GetViewProj()));

		GLfloat pointSize;
		glGetFloatv(GL_POINT_SIZE, &pointSize);
		glPointSize(10.f);

		glDrawArrays(GL_POINTS, 0, n);

		glPointSize(pointSize);
	}

	//
	// Cones
	//
	if (n > 4 && m_showCones && m_activeTechnique == 1) {
		glUseProgram(m_programConesID);

		glUniform1i(ul(m_programConesID, "SSBOPadding"), SSBO_PADDING);
		glUniformMatrix4fv(ul(m_programConesID, "viewProj"), 1, GL_FALSE, glm::value_ptr(m_camera.GetViewProj()));

		// cone map
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m_conemapTextureID);
		if (m_interpolation) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		glUniform1i(ul(m_programConesID, "coneMap"), 1);

		GLfloat lineWidth;
		glGetFloatv(GL_LINE_WIDTH, &lineWidth);
		glLineWidth(2.f);

		glDrawArrays(GL_POINTS, 0, n - 3);

		glLineWidth(lineWidth);
	}
	
	glUseProgram(0);
	glEnable(GL_DEPTH_TEST);
}

void CMyApp::RenderModels() {
	//
	// models
	//
	for (auto m : m_models) {
		if (m->GetWireFrame()) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		else {
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}

		GLuint progID = m->GetProgram();
		glUseProgram(progID);

		{	// SET UNIFORMS FOR PARALLAX MAPPING
			// uniform vec2 HMres;     // height map resolution (w, h)
			glUniform2f(ul(progID, "HMres"), m_HMres.x, m_HMres.y);
			// uniform vec2 HMres_r;   // reciprical of the height map resolution (1/w, 1/h)
			glUniform2f(ul(progID, "HMres_r"), m_HMres_r.x, m_HMres_r.y);
			// uniform float relax = 1.;
			glUniform1i(ul(progID, "maxSteps"), m_maxSteps);
			// uniform vec3 camPos;
			glm::vec3 camPos = m_camera.GetEye();
			glUniform3f(ul(progID, "camPos"), camPos.x, camPos.y, camPos.z);
			glUniform1i(ul(progID, "discardFragments"), m_discardFragments);
			// uniform vec3 lightDir;
			glUniform3f(ul(progID, "lightDir"), m_lightPos.x, m_lightPos.y, m_lightPos.z);
			// uniform float lightIntensity = 1.;
			glUniform1i(ul(progID, "displayNonConverged"), m_displayNonConverged);
			glUniform1f(ul(progID, "epsilon"), m_epsilon);

			glUniform1f(ul(progID, "modelNormalMult"), m_modelNormalMult);
			glUniform1i(ul(progID, "rayMarchingTechnique"), m_activeTechnique);

			glUniform1i(ul(progID, "SSBOPadding"), SSBO_PADDING);
			glUniform1i(ul(progID, "showSteps"), m_showSteps ? 1 : 0);
			glUniform1i(ul(progID, "showEnterExit"), m_showEnterExit ? 1 : 0);
			glUniform1i(ul(progID, "showFlags"), m_showFlags ? 1 : 0);
		}

		// texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m->GetTexture());
		glUniform1i(ul(progID, "texImage"), 0);

		// cone map
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, m->GetConemap());
		if (m_interpolation) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		glUniform1i(ul(progID, "coneMap"), 1);

		glUniformMatrix4fv(ul(progID, "viewProj"), 1, GL_FALSE, glm::value_ptr(m_camera.GetViewProj()));
		glUniformMatrix4fv(ul(progID, "world"), 1, GL_FALSE, glm::value_ptr(m->GetTransform()));
		glUniformMatrix4fv(ul(progID, "worldIT"), 1, GL_FALSE, glm::value_ptr(m->GetTransform()));

		glBindVertexArray(m->GetVAO());

		glDrawElements(m->GetDrawMode(), m->GetVertexCount(), GL_UNSIGNED_INT, nullptr);
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glBindTexture(GL_TEXTURE_2D, 0);

	glUseProgram(0);
	glBindVertexArray(0);
}

void CMyApp::RenderSkybox() {
	glUseProgram(m_programSkyboxID);

	glProgramUniform1i(m_programSkyboxID, ul(m_programSkyboxID, "skyboxTexture"), 1);
	glProgramUniformMatrix4fv(m_programSkyboxID, ul(m_programSkyboxID, "viewProj"), 1, GL_FALSE, glm::value_ptr(m_camera.GetViewProj()));
	glProgramUniformMatrix4fv(m_programSkyboxID, ul(m_programSkyboxID, "world"), 1, GL_FALSE, glm::value_ptr(glm::translate(m_camera.GetEye())));

	// Save the last Z-test, namely the relation by which we update the pixel.
	GLint prevDepthFnc;
	glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFnc);

	// Now we use less-then-or-equal, because we push everything to the far clipping plane
	glDepthFunc(GL_LEQUAL);

	glBindTextureUnit(1, m_skyboxTextureID);
	glBindVertexArray(m_SkyboxGPU.vaoID);

	glDrawElements(GL_TRIANGLES, m_SkyboxGPU.count, GL_UNSIGNED_INT, nullptr);

	glDepthFunc(prevDepthFnc);

	glUseProgram(0);
	glBindVertexArray(0);
	glBindTextureUnit(0, 0);
}

void CMyApp::Render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	RenderModels();
	// RenderSkybox();
	if (m_showAxes) {
		DrawAxes();
	}
	if (m_showPoints) {
		RenderDebug();
	}
}

void CMyApp::RenderGUI()
{
	ImGui::Begin("Visual debug window");
	{

		float pb[3]{ m_pointsBase[0], m_pointsBase[1], m_pointsBase[2] };
		float pd[3]{ m_pointsDir[0], m_pointsDir[1], m_pointsDir[2] };
		
		ImGui::Checkbox("Show debug", &m_showPoints);
		ImGui::Checkbox("Show steps", &m_showSteps);
		ImGui::Checkbox("Show enter/exit", &m_showEnterExit);
		ImGui::Checkbox("Show cones", &m_showCones);
		ImGui::SliderFloat3("Base", m_pointsBase, -10.0f, 10.0f);
		ImGui::SliderFloat3("Direction", m_pointsDir, -1.0f, 1.0f);
		if (ImGui::Button("To camera")) {
			glm::vec3 e = m_camera.GetEye();
			m_pointsBase[0] = e.x;
			m_pointsBase[1] = e.y;
			m_pointsBase[2] = e.z;

			glm::vec3 a = m_camera.GetAt() - e;
			m_pointsDir[0] = a.x;
			m_pointsDir[1] = a.y;
			m_pointsDir[2] = a.z;
		}

		if (
			pb[0] != m_pointsBase[0] || pb[1] != m_pointsBase[1] || pb[2] != m_pointsBase[2] ||
			pd[0] != m_pointsDir[0] || pd[1] != m_pointsDir[1] || pd[2] != m_pointsDir[2]
		) {	// save values to buffer if changed
			if (m_pointsDir[0] == 0 && m_pointsDir[1] == 0 && m_pointsDir[2] == 0) {
				m_pointsDir[1] = 1.f;
			}
			else {
				float l = sqrt(pow(m_pointsDir[0], 2) + pow(m_pointsDir[1], 2) + pow(m_pointsDir[2], 2));
				m_pointsDir[0] = m_pointsDir[0] / l;
				m_pointsDir[1] = m_pointsDir[1] / l;
				m_pointsDir[2] = m_pointsDir[2] / l;
			}

			SetPointsBase();
		}

	}
	ImGui::End();

	ImGui::Begin("Numerical debug window");
	{

		float pb[3]{ m_pointsBase[0], m_pointsBase[1], m_pointsBase[2] };
		float pd[3]{ m_pointsDir[0], m_pointsDir[1], m_pointsDir[2] };

		ImGui::SliderFloat3("Base", m_pointsBase, -10.0f, 10.0f);
		ImGui::SliderFloat3("Direction", m_pointsDir, -1.0f, 1.0f);
		if (ImGui::Button("To camera")) {
			glm::vec3 e = m_camera.GetEye();
			m_pointsBase[0] = e.x;
			m_pointsBase[1] = e.y;
			m_pointsBase[2] = e.z;

			glm::vec3 a = m_camera.GetAt() - e;
			m_pointsDir[0] = a.x;
			m_pointsDir[1] = a.y;
			m_pointsDir[2] = a.z;
		}

		if (
			pb[0] != m_pointsBase[0] || pb[1] != m_pointsBase[1] || pb[2] != m_pointsBase[2] ||
			pd[0] != m_pointsDir[0] || pd[1] != m_pointsDir[1] || pd[2] != m_pointsDir[2]
			) {	// save values to buffer if changed
			if (m_pointsDir[0] == 0 && m_pointsDir[1] == 0 && m_pointsDir[2] == 0) {
				m_pointsDir[1] = 1.f;
			}
			else {
				float l = sqrt(pow(m_pointsDir[0], 2) + pow(m_pointsDir[1], 2) + pow(m_pointsDir[2], 2));
				m_pointsDir[0] = m_pointsDir[0] / l;
				m_pointsDir[1] = m_pointsDir[1] / l;
				m_pointsDir[2] = m_pointsDir[2] / l;
			}

			SetPointsBase();
		}

		{
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_debugSSBO);
			GLint size = 0;
			glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &size);
			glm::vec4* data = (glm::vec4*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
			if (data) {
				ImGui::Text("Step count: %.4f", data[0].x);
				ImGui::Text("Eye (scene space): %.4f; %.4f; %.4f", data[1].x, data[1].y, data[1].z);
				ImGui::Text("Direction (scene space): %.4f; %.4f; %.4f", data[2].x, data[2].y, data[2].z);

				if (ImGui::BeginTable("Verteces (scene space)", 4)) {

					ImGui::TableSetupColumn("pos");
					ImGui::TableSetupColumn("norm");
					ImGui::TableSetupColumn("merged");
					ImGui::TableSetupColumn("tex");
					ImGui::TableHeadersRow();

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("0; 0; 0");
					ImGui::TableSetColumnIndex(1); ImGui::Text("0; 1; 0");
					ImGui::TableSetColumnIndex(2); ImGui::Text("0; 1; 0");
					ImGui::TableSetColumnIndex(3); ImGui::Text("0; 0");
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("0; 0; 1");
					ImGui::TableSetColumnIndex(1); ImGui::Text("0; 1; 0");
					ImGui::TableSetColumnIndex(2); ImGui::Text("0; 1; 0");
					ImGui::TableSetColumnIndex(3); ImGui::Text("0; 1");
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("1; 0; 1");
					ImGui::TableSetColumnIndex(1); ImGui::Text("0; 1; 0");
					ImGui::TableSetColumnIndex(2); ImGui::Text("0; 1; 0");
					ImGui::TableSetColumnIndex(3); ImGui::Text("1; 1");

					ImGui::EndTable();
				}

				ImGui::Text("scene2texture space:");
				if (ImGui::BeginTable("scene2texture space", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("%.4f", data[3].x);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", data[3].y);
					ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", data[3].z);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", data[3].w);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("%.4f", data[4].x);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", data[4].y);
					ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", data[4].z);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", data[4].w);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("%.4f", data[5].x);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", data[5].y);
					ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", data[5].z);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", data[5].w);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("%.4f", data[6].x);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", data[6].y);
					ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", data[6].z);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", data[6].w);

					ImGui::EndTable();
				}
				ImGui::Text("Eye (texture space): %.4f; %.4f; %.4f; %.4f", data[7].x, data[7].y, data[7].z, data[7].w);

				ImGui::Text("scene2unit space:");
				if (ImGui::BeginTable("scene2unit space", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("%.4f", data[8].x);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", data[8].y);
					ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", data[8].z);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", data[8].w);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("%.4f", data[9].x);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", data[9].y);
					ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", data[9].z);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", data[9].w);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("%.4f", data[10].x);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", data[10].y);
					ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", data[10].z);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", data[10].w);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::Text("%.4f", data[11].x);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", data[11].y);
					ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", data[11].z);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", data[11].w);

					ImGui::EndTable();
				}
				ImGui::Text("Eye (unit space): %.4f; %.4f; %.4f; %.4f", data[12].x, data[12].y, data[12].z, data[12].w);

				ImGui::Text("In (texture space): %.4f; %.4f; %.4f", data[13].x, data[13].y, data[13].z);
				ImGui::Text("Out (texture space): %.4f; %.4f; %.4f", data[14].x, data[14].y, data[14].z);
				ImGui::Text("v (texture space): %.4f; %.4f; %.4f", data[15].x, data[15].y, data[15].z);

				if (ImGui::BeginTable("Steps", 5)) {
					ImGui::TableSetupColumn("ti");
					ImGui::TableSetupColumn("t");
					ImGui::TableSetupColumn("height");
					ImGui::TableSetupColumn("tan");
					ImGui::TableSetupColumn("ui");
					ImGui::TableHeadersRow();

					for (int i = 0; i < (int)floor(data[0].x); ++i) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0); ImGui::Text("%.4f", data[16 + i * 2].x);
						ImGui::TableSetColumnIndex(1); ImGui::Text("%.4f", data[16 + i * 2].y);
						ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f", data[16 + i * 2].z);
						ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f", data[16 + i * 2].w);
						ImGui::TableSetColumnIndex(4); ImGui::Text("%.4f; %.4f; %.4f", data[16 + i * 2 + 1].x, data[16 + i * 2 + 1].y, data[16 + i * 2 + 1].z);
					}

					ImGui::EndTable();

				}

				ImGui::Text("Flags: %d", data[0].y);
			}
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}

	}
	ImGui::End();

	ImGui::Begin("Options window");
	{
		bool regenConeMap = false;

		ImGui::Text("Render resolution %dx%d", m_width, m_height);
		ImGui::Checkbox("Show axes", &m_showAxes);

		// ray marching technique
		if (ImGui::BeginCombo("Ray marching technique", m_rayMarchingTechniques[m_activeTechnique].c_str()))
		{
			for (int i = 0; i < m_rayMarchingTechniques.size(); ++i) {
				if (ImGui::Selectable(m_rayMarchingTechniques[i].c_str(), m_activeTechnique == i)) {
					m_activeTechnique = i;
				}
			}
			ImGui::EndCombo();
		}
		bool interpolation = m_interpolation;
		ImGui::Checkbox("Texture interpolation", &interpolation);
		regenConeMap = regenConeMap || interpolation != m_interpolation;

		// heightmap
		int hmapID = m_activeHeightMap;
		if (ImGui::BeginCombo("Heightmap", m_heightMaps[hmapID].c_str()))
		{
			for (int i = 0; i < m_heightMaps.size(); ++i) {
				if (ImGui::Selectable(m_heightMaps[i].c_str(), hmapID == i)) {
					hmapID = i;
				}
			}
			ImGui::EndCombo();
		}
		regenConeMap = regenConeMap || hmapID != m_activeHeightMap;

		ImGui::SliderFloat3("light_dir", &m_lightPos.x, -10.f, 10.f);
		ImGui::SliderFloat3("light_col", &m_lightCol.x, 0.f, 1.f);
		ImGui::Image((ImTextureID) m_conemapTextureID, ImVec2(256, 256));

		for (int i = 0; i < m_models.size(); ++i) {
			std::stringstream label{};
			label << "wireframe " << i;
			
			bool wireframe = m_models[i]->GetWireFrame();
			ImGui::Checkbox(label.str().c_str(), &wireframe);
			m_models[i]->SetWireFrame(wireframe);
		}

		ImGui::SliderInt("max steps", &m_maxSteps, 1, 100);
		ImGui::SliderFloat("epsilon", &m_epsilon, 0.0, 1.0, "%.3f");
		ImGui::SliderFloat("modelNormalMult", &m_modelNormalMult, 0.1, 2.0, "%.3f");
		ImGui::Checkbox("show non-converge", &m_displayNonConverged);
		ImGui::Checkbox("discard fragments", &m_discardFragments);
		ImGui::Checkbox("show con-step flags", &m_showFlags);

		if (regenConeMap) {
			// regenerate conemap
			CleanConemapTexture();
			CleanHeightMapTexture();
			m_activeHeightMap = hmapID;
			m_interpolation = interpolation;
			InitHeightMapTexture();
			InitConemapTexture();
			RenderConemap();
		}
	}
	ImGui::End();
}

// https://wiki.libsdl.org/SDL2/SDL_KeyboardEvent
// https://wiki.libsdl.org/SDL2/SDL_Keysym
// https://wiki.libsdl.org/SDL2/SDL_Keycode
// https://wiki.libsdl.org/SDL2/SDL_Keymod

void CMyApp::KeyboardDown(const SDL_KeyboardEvent& key)
{
	if (key.repeat == 0) // Triggers only once when held
	{
		if (key.keysym.sym == SDLK_F5 && key.keysym.mod & KMOD_CTRL) // CTRL + F5
		{
			CleanShaders();
			InitShaders();
		}
		if (key.keysym.sym == SDLK_F1) // F1
		{
			GLint polygonModeFrontAndBack[2] = {};
			// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGet.xhtml
			glGetIntegerv(GL_POLYGON_MODE, polygonModeFrontAndBack); // Query the current polygon mode. It gives the front and back modes separately.
			GLenum polygonMode = (polygonModeFrontAndBack[0] != GL_FILL ? GL_FILL : GL_LINE); // Switch between FILL and LINE
			// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glPolygonMode.xhtml
			glPolygonMode(GL_FRONT_AND_BACK, polygonMode); // Set the new polygon mode
		}
	}
	m_cameraManipulator.KeyboardDown(key);
}

void CMyApp::KeyboardUp(const SDL_KeyboardEvent& key)
{
	m_cameraManipulator.KeyboardUp(key);
}

// https://wiki.libsdl.org/SDL2/SDL_MouseMotionEvent

void CMyApp::MouseMove(const SDL_MouseMotionEvent& mouse)
{
	m_cameraManipulator.MouseMove(mouse);
}

// https://wiki.libsdl.org/SDL2/SDL_MouseButtonEvent

void CMyApp::MouseDown(const SDL_MouseButtonEvent& mouse)
{
}

void CMyApp::MouseUp(const SDL_MouseButtonEvent& mouse)
{
}

// https://wiki.libsdl.org/SDL2/SDL_MouseWheelEvent

void CMyApp::MouseWheel(const SDL_MouseWheelEvent& wheel)
{
	m_cameraManipulator.MouseWheel(wheel);
}

// New window size
void CMyApp::Resize(int _w, int _h)
{
	glViewport(0, 0, _w, _h);
	m_camera.SetAspect(static_cast<float>(_w) / _h);
	m_width = _w;
	m_height = _h;
}

// Other SDL events
// https://wiki.libsdl.org/SDL2/SDL_Event

void CMyApp::OtherEvent(const SDL_Event& ev)
{
}