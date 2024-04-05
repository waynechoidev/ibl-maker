#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define GLM_ENABLE_EXPERIMENTAL

#include <string>
#include <filesystem>
#include "stb_image.h"
#include "stb_image_write.h"

#include <glm/gtc/type_ptr.hpp>

#include "window.h"
#include "program.h"
#include "cubemap.h"
#include "cube.h"
#include "hdr-texture.h"
#include "quad.h"

std::string baseName = "air_museum_playground";
std::string fileName = "air_museum_playground_4k.hdr";

std::filesystem::path currentDir = std::filesystem::path(__FILE__).parent_path();
std::filesystem::path rootDir = std::filesystem::path(__FILE__).parent_path().parent_path();

const GLfloat SPHERE_SCALE = 1.0;
const GLint WIDTH = 1920;
const GLint HEIGHT = 1080;

GLint envCubeSize = 2048;
GLint irradianceSize = 128;
GLint brdfSize = 512;

bool useDirectLight = true;
bool useEnvLight = true;
float heightScale = 0.03;
glm::vec3 lightPos = glm::vec3(0.0f, 0.0f, 2.0f);
bool dragStartFlag = false;
glm::vec3 prevMouseRayVector = glm::vec3(0.0f, 1.0f, 0.0f);

void saveImage(const char *filename, int width, int height, unsigned char *data)
{
	stbi_write_jpg(filename, width, height, 4, data, 100);
}

void saveFramebuffer(std::string type, int index, int size)
{
	unsigned char *imageData = new unsigned char[size * size * 4];

	glReadPixels(0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, imageData);

	std::string orientation;
	switch (index)
	{
	case 0:
		orientation = "_px";
		break;
	case 1:
		orientation = "_nx";
		break;
	case 2:
		orientation = "_py";
		break;
	case 3:
		orientation = "_ny";
		break;
	case 4:
		orientation = "_pz";
		break;
	case 5:
		orientation = "_nz";
		break;
	case 100:
	default:
		orientation = "";
		break;
	}

	std::filesystem::path dirPath = rootDir / "output";

	if (!std::filesystem::exists(dirPath))
	{
		std::filesystem::create_directories(dirPath);
	}

	std::string fileName = dirPath / (baseName + "_" + type + orientation + ".jpg");

	saveImage(fileName.c_str(), size, size, imageData);

	delete[] imageData;
}
int main()
{
	Window mainWindow = Window(300, 200);
	mainWindow.initialise();

	Cube cube = Cube();
	Quad quad = Quad();

	Program equirectangularToCubemapProgram = Program();
	equirectangularToCubemapProgram.createFromFiles(currentDir / "shaders/cubemap.vert", currentDir / "shaders/equirectangular-to-cubemap.frag");
	GLuint equirectangularToCubemapProgramId = equirectangularToCubemapProgram.getId();
	GLuint equirectangularToCubemapViewLoc = glGetUniformLocation(equirectangularToCubemapProgramId, "view");
	GLuint equirectangularToCubemapProjectionLoc = glGetUniformLocation(equirectangularToCubemapProgramId, "projection");

	Program irradianceProgram = Program();
	irradianceProgram.createFromFiles(currentDir / "shaders/cubemap.vert", currentDir / "shaders/irradiance.frag");
	GLuint irradianceProgramId = irradianceProgram.getId();
	GLuint irradianceViewLoc = glGetUniformLocation(irradianceProgramId, "view");
	GLuint irradianceProjectionLoc = glGetUniformLocation(irradianceProgramId, "projection");

	Program brdfProgram = Program();
	brdfProgram.createFromFiles(currentDir / "shaders/brdf.vert", currentDir / "shaders/brdf.frag");

	unsigned int captureFBO;
	unsigned int captureRBO;
	glGenFramebuffers(1, &captureFBO);
	glGenRenderbuffers(1, &captureRBO);

	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	glm::mat4 captureViews[] =
		{
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

	HDRTexture hdrTexture = HDRTexture();
	std::string inputSrc = rootDir / ("input/" + fileName);
	hdrTexture.initialise("hdrTexture", inputSrc);

	// Create Env Cubemap
	Cubemap envCubemap = Cubemap();
	envCubemap.initialize(envCubeSize, "envCubemap");

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, envCubeSize, envCubeSize);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

	equirectangularToCubemapProgram.use();
	glUniformMatrix4fv(equirectangularToCubemapProjectionLoc, 1, GL_FALSE, glm::value_ptr(captureProjection));
	hdrTexture.use(equirectangularToCubemapProgramId, 0);

	glViewport(0, 0, envCubeSize, envCubeSize);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glUniformMatrix4fv(equirectangularToCubemapViewLoc, 1, GL_FALSE, glm::value_ptr(captureViews[i]));
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap.getId(), 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		cube.draw();

		saveFramebuffer("env", i, envCubeSize);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Create Irradiance Cubemap
	Cubemap irradianceCubemap = Cubemap();
	irradianceCubemap.initialize(irradianceSize, "irradianceCubemap");

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, irradianceSize, irradianceSize);

	irradianceProgram.use();
	glUniformMatrix4fv(irradianceProjectionLoc, 1, GL_FALSE, glm::value_ptr(captureProjection));
	envCubemap.use(irradianceProgramId, 0);

	glViewport(0, 0, irradianceSize, irradianceSize);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glUniformMatrix4fv(irradianceViewLoc, 1, GL_FALSE, glm::value_ptr(captureViews[i]));
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceCubemap.getId(), 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		cube.draw();

		saveFramebuffer("irradiance", i, irradianceSize);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Create BRDF texture
	HDRTexture brdfLUT = HDRTexture();
	brdfLUT.initialise("brdfLUT", brdfSize);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUT.getId(), 0);

	glViewport(0, 0, brdfSize, brdfSize);
	brdfProgram.use();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	quad.draw();

	saveFramebuffer("brdf", 100, brdfSize);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return 0;
}
