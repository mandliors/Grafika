//=============================================================================================
// Barycentric coordinate system example
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

class Barycentric : public glApp 
{
	Geometry<vec2>* vertices;
	Geometry<vec2>* fillPoints;
	Geometry<vec2>* outlinePoints;
	GPUProgram* gpuProgram;

	vec2* draggedVertex = nullptr;

	const float dt = 0.01f;

public:
	Barycentric() : glApp("Barycentric Coordinate System") {}
	~Barycentric() { delete vertices; delete fillPoints;  delete outlinePoints; delete gpuProgram; }

	// Inicializáció, 
	void onInitialization()
	{
		vertices = new Geometry<vec2>;
		fillPoints = new Geometry<vec2>;
		outlinePoints = new Geometry<vec2>;		
		gpuProgram = new GPUProgram(vertSource, fragSource);

		vertices->Vtx() = {
			vec2(-0.7f, -0.7f),
			vec2(0.7f, -0.6f),
			vec2(0.4f, 0.4f),
			vec2(-0.4f, 0.7f),
		};
		vertices->updateGPU();
		Recalculate();
	}

	// Ablak újrarajzolás
	void onDisplay() 
	{
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);
		
		glPointSize(2);
		fillPoints->Draw(gpuProgram, GL_POINTS, vec3(0.0f, 1.0f, 1.0f));
		glPointSize(4);
		outlinePoints->Draw(gpuProgram, GL_POINTS, vec3(1.0f, 0.0f, 0.0f));
		glPointSize(10);
		vertices->Draw(gpuProgram, GL_POINTS, vec3(1.0f, 1.0f, 0.0f));
	}

	void onMouseMotion(int px, int py)
	{
		if (draggedVertex)
		{
			*draggedVertex = ScreenToNDC(px, py);
			vertices->updateGPU();
			Recalculate();
			refreshScreen();
		}
	}
	void onMousePressed(MouseButton but, int pX, int pY)
	{
		if (but == MOUSE_LEFT)
		{
			vec2 p = ScreenToNDC(pX, pY);
			for (auto& v : vertices->Vtx())
			{
				if (length(p - v) < 0.05f)
				{
					draggedVertex = &v;
					return;
				}
			}
		}
	}
	void onMouseReleased(MouseButton but, int pX, int pY)
	{
		if (but == MOUSE_LEFT)
			draggedVertex = nullptr;
	}

private:
	void Recalculate()
	{
		const auto& vtx = vertices->Vtx();

		fillPoints->Vtx() = CalculateFillPoints(vtx[0], vtx[1], vtx[2], vtx[3]);
		outlinePoints->Vtx() = CalculateOutlinePoints(vtx[0], vtx[1], vtx[2], vtx[3]);

		fillPoints->updateGPU();
		outlinePoints->updateGPU();
	}

	std::vector<vec2> CalculateFillPoints(const vec2& p1, const vec2& p2, const vec2& p3, const vec2& p4)
	{
		std::vector<vec2> points;
		for (float t1 = 0.0f; t1 <= 1.0f; t1 += dt)
		{
			for (float t2 = 0.0f; t2 <= 1.0f; t2 += dt)
			{
				if (t1 + t2 > 1.0f) break;
				for (float t3 = 0.0f; t3 <= 1.0f; t3 += dt)
				{
					if (t1 + t2 + t3 > 1.0f) break;
					
					float t4 = 1.0f - t1 - t2 - t3;
					if (t4 < 0) break;
					
					vec2 point = t1 * p1 + t2 * p2 + t3 * p3 + t4 * p4;
					points.push_back(point);
				}
			}
		}
		return points;
	}

	std::vector<vec2> CalculateOutlinePoints(const vec2& p1, const vec2& p2, const vec2& p3, const vec2& p4)
	{
		std::vector<vec2> points;
		for (float t = 0; t <= 1.0f; t += dt)
		{
			points.push_back((1 - t) * p1 + t * p2);
			points.push_back((1 - t) * p2 + t * p3);
			points.push_back((1 - t) * p3 + t * p4);
			points.push_back((1 - t) * p4 + t * p1);
		}
		return points;
	}

	vec2 ScreenToNDC(int px, int py)
	{
		float x = (2.0f * px / winWidth) - 1.0f;
		float y = 1.0f - (2.0f * py / winHeight);
		return { x, y };
	}

} app;

