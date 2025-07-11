//=============================================================================================
// Lagrange Curve
//=============================================================================================
#include "framework.h"

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330
    precision highp float;

	layout(location = 0) in vec2 vertexPosition;
	
	void main()
	{
		gl_Position = vec4(vertexPosition, 0, 1);
	}
)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;			// konstans szín
	out vec4 fragmentColor;		// pixel szín

	void main()
	{
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

const int winWidth = 600, winHeight = 600;

const int nTessVertices = 500;
class LagrangeCurve
{
	Geometry<vec2>* controlPoints;
	std::vector<float> knotValues;
	Geometry<vec2>* curvePoints;

	vec2* selectedPoint = nullptr;

public:
	LagrangeCurve() { controlPoints = new Geometry<vec2>(); curvePoints = new Geometry<vec2>(); }
	~LagrangeCurve() { delete controlPoints; delete curvePoints; }

	void AddControlPoint(vec2 cp)
	{
		controlPoints->Vtx().push_back(cp);
		controlPoints->updateGPU();
		Recalculate();
	}
	void Render(GPUProgram* gpuProgram)
	{
		if (controlPoints->Vtx().size() >= 2)
			curvePoints->Draw(gpuProgram, GL_LINE_STRIP, { 1.0f, 1.0f, 0.0f });

		if (controlPoints->Vtx().size() == 0)
			return;

		controlPoints->Draw(gpuProgram, GL_POINTS, { 1.0f, 0.0f, 0.0f });
	}

	bool onMousePressed(MouseButton but, const vec2& p)
	{
		if (but == MouseButton::MOUSE_LEFT)
		{
			AddControlPoint(p);
			return true;
		}
		else if (but == MouseButton::MOUSE_RIGHT)
		{
			auto& cps = controlPoints->Vtx();
			const vec2 mousePos = p;

			for (auto& p : cps)
			{
				if (length(p - mousePos) < 0.05f)
				{
					selectedPoint = &p;
					break;
				}
			}
		}
		return false;
	}
	void onMouseReleased(MouseButton but, int pX, int pY) { selectedPoint = nullptr; }
	bool onMouseMotion(const vec2& p)
	{
		if (selectedPoint)
		{
			*selectedPoint = p;
			controlPoints->updateGPU();
			Recalculate();
			return true;
		}
		return false;
	}

private:
	void Recalculate()
	{
		knotValues.push_back(69);
		for (int i = 0; i < controlPoints->Vtx().size(); i++)
			knotValues[i] = (float)i / (controlPoints->Vtx().size() - 1);

		curvePoints->Vtx() = GenerateVertices();
		if (curvePoints->Vtx().size() >= 2)
			curvePoints->updateGPU();
	}

	std::vector<vec2> GenerateVertices()
	{
		std::vector<vec2> vertices;
		for (int i = 0; i <= nTessVertices; ++i)
		{
			float t = (float)i / nTessVertices;
			vertices.push_back(r(t));
		}
		return vertices;
	}
	vec2 r(float t)
	{
		vec2 rt(0, 0);
		const auto& cps = controlPoints->Vtx();
		for (int i = 0; i < cps.size(); i++)
			rt += cps[i] * L(i, t);

		return rt;
	}
	float L(int i, float t)
	{
		float Li = 1.0f;
		for (int j = 0; j < controlPoints->Vtx().size(); j++)
			if (j != i) Li *= (t - knotValues[j]) / (knotValues[i] - knotValues[j]);
		return Li;
	}
};

class LagrangeApp : public glApp {
	LagrangeCurve* curve;
	GPUProgram* gpuProgram;

public:
	LagrangeApp() : glApp("Lagrange Curve") {}
	~LagrangeApp() { delete gpuProgram; delete curve; }

	void onInitialization() override
	{
		glPointSize(10);
		glLineWidth(3);

		gpuProgram = new GPUProgram(vertSource, fragSource);
		curve = new LagrangeCurve;
	}

	void onDisplay() override
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		curve->Render(gpuProgram);
	}

	void onMousePressed(MouseButton but, int pX, int pY) override
	{
		if (curve->onMousePressed(but, ScreenToNDC(pX, pY)))
			refreshScreen();
	}
	void onMouseReleased(MouseButton but, int pX, int pY) override { curve->onMouseReleased(but, pX, pY); }
	void onMouseMotion(int pX, int pY) override
	{
		if (curve->onMouseMotion(ScreenToNDC(pX, pY)))
			refreshScreen();
	}

	vec2 ScreenToNDC(int x, int y)
	{
		return {
			x / (float)winWidth * 2.0f - 1.0f,
			1.0f - y / (float)winHeight * 2.0f
		};
	}
} app;