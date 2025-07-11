//=============================================================================================
// Classic Curves
//=============================================================================================
#include "framework.h"

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330
    precision highp float;

	layout(location = 0) in vec2 cP;	// 0. bemeneti regiszter

	void main() {
		gl_Position = vec4(cP.x, cP.y, 0, 1); 	// bemenet már normalizált eszközkoordinátákban
	}
)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;			// konstans szín
	out vec4 fragmentColor;		// pixel szín

	void main() {
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

const int winWidth = 600, winHeight = 600;

class Curve
{
	Geometry<vec2>* vertices;

public:
	Curve() { vertices = new Geometry<vec2>(); }
	~Curve() { delete vertices; }

	void Draw(GPUProgram* gpuProgram)
	{
		if (vertices->Vtx().size() >= 2)
			vertices->Draw(gpuProgram, GL_LINE_STRIP, { 1.0f, 1.0f, 0.0f });
	}
	void UpdateVertices(vec2(*f)(float))
	{
		static const int nTessVertices = 100;
		
		vec2 p;
		float dt = 2.0f / nTessVertices;
		vertices->Vtx().clear(); vertices->Vtx().reserve(nTessVertices);
		for (float t = -1.0f; t < 1.0f + dt; t += dt)
		{
			p = f(t);
			vertices->Vtx().push_back(p);
		}
		vertices->updateGPU();
	}
};

class ClassicCurvesApp : public glApp
{
	GPUProgram* gpuProgram;
	Curve* curve;
	
	const static unsigned nFunctions = 7;
	vec2(*fs[nFunctions])(float);
	unsigned fIdx = 0;

public:
	ClassicCurvesApp() : glApp("Classic Curves") { }
	~ClassicCurvesApp() { delete gpuProgram; delete curve; }

	// Inicializáció,
	void onInitialization()
	{
		glLineWidth(3.0f);

		gpuProgram = new GPUProgram(vertSource, fragSource);

		fs[0] = [](float t) -> vec2 { return { t, 0.5f * t + 0.1f }; }; // line
		fs[1] = [](float t) -> vec2 { return { t, 3.0f * t * t - 0.8f }; }; // parabola
		fs[2] = [](float t) -> vec2 { return { 0.7f * cos(2.0f * M_PI * t), 0.7f * sin(2.0f * M_PI * t) }; }; // circle
		fs[3] = [](float t) -> vec2 { return { 0.8f * cos(2.0f * M_PI * t), 0.5f * sin(2.0f * M_PI * t) }; }; // ellipse
		fs[4] = [](float t) -> vec2 { return { 0.4f * (t - sin(2.0f * M_PI * t)), 0.4f * (1.0f - cos(2.0f * M_PI * t)) }; }; // cycloid (bad?)
		fs[5] = [](float t) -> vec2 { return { 0.5f * 1.0f / cosh(2.0f * M_PI * t), 0.5f * (t - tanh(2.0f * M_PI * t)) }; }; // tractrix (bad?)
		fs[6] = [](float t) -> vec2 { return { 0.4f * (1.0f - cos(2.0f * M_PI * t)) * cos(2.0f * M_PI * t), 0.4f * (1.0f - cos(2.0f * M_PI * t)) * sin(2.0f * M_PI * t) }; }; // cardioid

		curve = new Curve;
		curve->UpdateVertices(fs[fIdx]);
	}

	void onKeyboard(int key)
	{
		if (key == 'a')
		{
			fIdx = (fIdx + nFunctions - 1) % nFunctions;
			curve->UpdateVertices(fs[fIdx]);
			refreshScreen();
		}
		else if (key == 'd')
		{
			fIdx = (fIdx + 1) % nFunctions;
			curve->UpdateVertices(fs[fIdx]);
			refreshScreen();
		}
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);

		curve->Draw(gpuProgram);
	}
} app;