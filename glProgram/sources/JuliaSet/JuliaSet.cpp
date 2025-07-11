//=============================================================================================
// Julia Set
//=============================================================================================
#include "framework.h"

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 cVertex;
	
	uniform vec2 cameraCenter, cameraSize;
	
	out vec2 z0;
	
	void main() {
		gl_Position = vec4(cVertex, 0, 1);
		z0 = cVertex * cameraSize/2 + cameraCenter;
	}

)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform vec2 c;
	uniform vec3 color; // NOT USED!
	
	in vec2 z0;
	out vec4 fragCol;

	void main() {
		vec2 z = z0;
		for (int i = 0; i < 1000; i++)
			z = vec2(z.x*z.x - z.y*z.y, 2*z.x*z.y) + c;
		
		fragCol = (dot(z,z) < 100) ? vec4(0.1, 1, 0.1, 1) : vec4(0.1, 0.1, 0.1, 1);

		// to avoid warnings
		vec3 _dummy = color;
	}
)";

const int winWidth = 600, winHeight = 600;

class JuliaSet : public glApp
{
	Geometry<vec2>* quad;
	GPUProgram* gpuProgram;

	vec2 cameraCenter = vec2(0.0f);
	vec2 cameraSize = vec2(2.0f);

	bool bMouseHeld = false;

public:
	JuliaSet() : glApp("Julia Set") {}
	~JuliaSet() { delete quad; delete gpuProgram; }

	// Inicializáció
	void onInitialization()
	{
		quad = new Geometry<vec2>;
		quad->Vtx() = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
		quad->updateGPU();

		gpuProgram = new GPUProgram(vertSource, fragSource);
		gpuProgram->setUniform({ -0.9f, -0.2f }, "c");
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

	void onMouseMotion(int pX, int pY)
	{
		if (!bMouseHeld) return;

		vec2 mouse = ViewportWindow(pX, pY);
		gpuProgram->setUniform(mouse, "c");
		refreshScreen();
	}

	void onMousePressed(MouseButton but, int pX, int pY)
	{
		if (but == MOUSE_RIGHT)
			bMouseHeld = true;
		else if (but == MOUSE_LEFT)
		{
			cameraCenter += ViewportWindow(pX, pY) * cameraSize / 2.0f;
			cameraSize *= 0.5f;
			gpuProgram->setUniform(cameraCenter, "cameraCenter");
			refreshScreen();
		}
	}
	void onMouseReleased(MouseButton but, int pX, int pY)
	{
		if (but == MOUSE_RIGHT)
			bMouseHeld = false;
	}

	vec2 ViewportWindow(int pX, int pY)
	{
		return {
			pX * 2.0f / winWidth - 1.0f,
			1.0f - pY * 2.0f / winHeight
		};
	}
} app;
