//=============================================================================================
// Catmull-Rom Spline
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
class Spline
{
	Geometry<vec2>* controlPoints;
	std::vector<float> knotValues;
	Geometry<vec2>* curvePoints;

	vec2* selectedPoint = nullptr;

public:
	Spline() { controlPoints = new Geometry<vec2>(); curvePoints = new Geometry<vec2>(); }
	~Spline() { delete controlPoints; delete curvePoints; }

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
		if (controlPoints->Vtx().size() < 2)
			return {};

		std::vector<vec2> vertices;
		for (int i = 0; i <= nTessVertices; ++i)
		{
			float t = (float)i / nTessVertices;
			vertices.push_back(r(t));
		}
		return vertices;
	}
	vec2 r(float tau)
	{
		const auto& cps = controlPoints->Vtx();
		for (int i = 0; i < cps.size() - 1; i++)
		{
			if (knotValues[i] <= tau && tau <= knotValues[i + 1])
			{
				vec2 v0(0, 0), v1(0, 0);
				if (i > 0)
					v0 = (cps[i + 1] - cps[i - 1]) / (knotValues[i + 1] - knotValues[i - 1]);
				if (i < cps.size() - 2)
					v1 = (cps[i + 2] - cps[i]) / (knotValues[i + 2] - knotValues[i]);

				return Hermite(
					cps[i],
					v0,
					knotValues[i],

					cps[i + 1],
					v1,
					knotValues[i + 1],

					tau
				);
			}
		}
	}
	vec2 Hermite(const vec2& p0, const vec2& v0, float t0, const vec2& p1, const vec2& v1, float t1, float t)
	{
		t -= t0;
		float dt = t1 - t0;
		vec2 a0 = p0, a1 = v0;
		vec2 a2 = 3.0f * (p1 - p0) / dt / dt - (v1 + 2.0f * v0) / dt;
		vec2 a3 = 2.0f * (p0 - p1) / dt / dt / dt + (v1 + v0) / dt / dt;

		return ((a3 * t + a2) * t + a1) * t + a0;
	}
};

class CatmullRomApp : public glApp {
	Spline* spline;
	GPUProgram* gpuProgram;

public:
	CatmullRomApp() : glApp("Catmull-Rom Spline") {}
	~CatmullRomApp() { delete gpuProgram; delete spline; }

	void onInitialization() override
	{
		glPointSize(10);
		glLineWidth(3);

		gpuProgram = new GPUProgram(vertSource, fragSource);
		spline = new Spline;
	}

	void onDisplay() override
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		spline->Render(gpuProgram);
	}

	void onMousePressed(MouseButton but, int pX, int pY) override
	{
		if (spline->onMousePressed(but, ScreenToNDC(pX, pY)))
			refreshScreen();
	}
	void onMouseReleased(MouseButton but, int pX, int pY) override { spline->onMouseReleased(but, pX, pY); }
	void onMouseMotion(int pX, int pY) override
	{
		if (spline->onMouseMotion(ScreenToNDC(pX, pY)))
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