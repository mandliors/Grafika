//=============================================================================================
// Mandelbrot Set
//=============================================================================================
#include "framework.h"

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 cVertex;
	
	uniform vec2 cameraCenter, cameraSize;
	
	out vec2 c;
	
	void main() {
		gl_Position = vec4(cVertex, 0, 1);
		c = cVertex * cameraSize/2 + cameraCenter;
	}

)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color; // NOT USED!

	const int nIteration = 1000;	

	in vec2 c;
	out vec4 fragCol;

	void main() {
		vec2 z = c;
		int i;
		for (i = 0; i < nIteration; i++) {
			z = vec2(z.x * z.x - z.y * z.y, 2 * z.x * z.y) + c;
			if (dot(z, z) > 4) break;
		}
		fragCol = (i == nIteration) ? vec4(1, 0.1, 0.1, 1) : vec4(0.1, 0.1, 0.1, 1);

		// to avoid warnings
		vec3 _dummy = color;
	}
)";

const int winWidth = 600, winHeight = 600;

class MandelbrotSet : public glApp
{
	Geometry<vec2>* quad;
	GPUProgram* gpuProgram;

	vec2 cameraCenter = vec2(-0.5f, 0.0f);
	vec2 cameraSize = vec2(3.0f);

public:
	MandelbrotSet() : glApp("Mandelbrot Set") {}
	~MandelbrotSet() { delete quad; delete gpuProgram; }

	// Inicializáció
	void onInitialization()
	{
		quad = new Geometry<vec2>;
		quad->Vtx() = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
		quad->updateGPU();

		gpuProgram = new GPUProgram(vertSource, fragSource);
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		gpuProgram->setUniform(cameraCenter, "cameraCenter");
		gpuProgram->setUniform(cameraSize, "cameraSize");

		quad->Draw(gpuProgram, GL_TRIANGLE_FAN, vec3(0.0f, 1.0f, 0.0f));
	}

	void onMousePressed(MouseButton but, int pX, int pY)
	{
		cameraCenter += ViewportWindow(pX, pY) * cameraSize / 2.0f;
		cameraSize *= 0.5f;
		gpuProgram->setUniform(cameraCenter, "cameraCenter");
		refreshScreen();
	}

	vec2 ViewportWindow(int pX, int pY)
	{
		return {
			pX * 2.0f / winWidth - 1.0f,
			1.0f - pY * 2.0f / winHeight
		};
	}
} app;
