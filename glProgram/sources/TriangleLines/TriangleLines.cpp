//=============================================================================================
// Zöld háromszög: A framework.h osztályait felhasználó megoldás
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

class TriangleLinesApp : public glApp {
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
	Geometry<vec2>* sides[3];
	Geometry<vec2>* eulerLine;
	Geometry<vec2>* vertices;
	Geometry<vec2>* points;

	vec3 v[3], m[3], e[3]; // csúcspontok, felezöpontok, élek
	vec3 medians[3], altitudes[3], bisectors[3]; // mediánok, magasságok, szögfelezök
	vec3 centroid, orthocenter, circumcenter, euler;

	vec3* draggedVertex = nullptr;

public:
	TriangleLinesApp(float x0, float y0, float x1, float y1, float x2, float y2) : glApp("Triangle lines")
	{
		// csúcspontok
		v[0] = vec3(x0, y0, 1);
		v[1] = vec3(x1, y1, 1);
		v[2] = vec3(x2, y2, 1);
	}
	~TriangleLinesApp()
	{
		for (int i = 0; i < 3; i++)
			delete sides[i];
		delete eulerLine;
		delete vertices;
		delete points;

		delete gpuProgram;
	}

	// Inicializáció, 
	void onInitialization()
	{
		gpuProgram = new GPUProgram(vertSource, fragSource);

		for (int i = 0; i < 3; i++)
			sides[i] = new Geometry<vec2>();
		eulerLine = new Geometry<vec2>();
		vertices = new Geometry<vec2>();
		points = new Geometry<vec2>();

		Recalculate();
	}

	// Ablak újrarajzolás
	void onDisplay() 
	{
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);

		// sides
		glLineWidth(4.0f);
		for (int i = 0; i < 3; i++)
		{
			sides[i]->updateGPU();
			sides[i]->Draw(gpuProgram, GL_LINES, vec3(1.0f, 1.0f, 1.0f));
		}

		glLineWidth(3.0f);
		DrawLines(medians, vec3(0.25f, 0.4f, 0.75f));
		DrawLines(altitudes, vec3(0.8f, 0.5f, 0.0f));
		DrawLines(bisectors, vec3(0.1f, 0.7f, 0.0f));

		// draw Euler's line
		glLineWidth(5.0f);		
		eulerLine->updateGPU();
		eulerLine->Draw(gpuProgram, GL_LINES, vec3(1.0f, 0.0f, 1.0f));

		// draw vertices
		glPointSize(10.0f);
		vertices->updateGPU();
		vertices->Draw(gpuProgram, GL_POINTS, vec3(1.0f, 1.0f, 0.0f));

		// draw special points
		points->updateGPU();
		points->Draw(gpuProgram, GL_POINTS, vec3(1.0f, 0.0f, 0.0f));
	}

	void onMouseMotion(int px, int py)
	{
		if (draggedVertex)
		{
			vec2 p = ScreenToNDC(px, py);
			*draggedVertex = vec3(p.x, p.y, 1.0f);
			Recalculate();
			refreshScreen();
		}
	}

	void onMousePressed(MouseButton but, int pX, int pY)
	{
		if (but == MOUSE_LEFT)
		{
			for (int i = 0; i < 3; i++)
			{
				vec2 p = ScreenToNDC(pX, pY);
				if (length(p - vec2(v[i].x, v[i].y)) < 0.05f)
				{
					draggedVertex = &v[i];
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

	void Recalculate()
	{
		// vertices
		vertices->Vtx() = { vec2(v[0].x, v[0].y), vec2(v[1].x, v[1].y), vec2(v[2].x, v[2].y) };

		// midpoints
		m[2] = v[0] + v[1];
		m[0] = v[1] + v[2];
		m[1] = v[2] + v[0];

		// edges
		e[0] = cross(v[1], v[2]);
		e[1] = cross(v[2], v[0]);
		e[2] = cross(v[0], v[1]);

		// lines
		for (int i = 0; i < 3; i++)
		{
			medians[i] = cross(v[i], m[i]);
			bisectors[i] = cross(m[i], vec3(e[i].x, e[i].y, 0));
			altitudes[i] = cross(v[i], vec3(e[i].x, e[i].y, 0));
		}

		// special points
		centroid = cross(medians[0], medians[1]);
		orthocenter = cross(altitudes[0], altitudes[1]);
		circumcenter = cross(bisectors[0], bisectors[1]);
		points->Vtx() = {
			vec2(centroid.x / centroid.z, centroid.y / centroid.z),
			vec2(orthocenter.x / orthocenter.z, orthocenter.y / orthocenter.z),
			vec2(circumcenter.x / circumcenter.z, circumcenter.y / circumcenter.z),
		};

		// Euler's line
		euler = cross(centroid, circumcenter);
		eulerLine->Vtx() = { CalculateLineEndpoints(euler) };

		// sides
		sides[0]->Vtx() = { vec2(v[0].x, v[0].y), vec2(v[1].x, v[1].y) };
		sides[1]->Vtx() = { vec2(v[1].x, v[1].y), vec2(v[2].x, v[2].y) };
		sides[2]->Vtx() = { vec2(v[2].x, v[2].y), vec2(v[0].x, v[0].y) };
	}

	void DrawLines(vec3* lines, const vec3& color)
	{
		Geometry<vec2> linesGeom[3];
		linesGeom[0].Vtx() = { CalculateLineEndpoints(lines[0]) };
		linesGeom[1].Vtx() = { CalculateLineEndpoints(lines[1]) };
		linesGeom[2].Vtx() = { CalculateLineEndpoints(lines[2]) };

		for (int i = 0; i < 3; i++)
		{
			linesGeom[i].updateGPU();
			linesGeom[i].Draw(gpuProgram, GL_LINES, color);
		}
	}

	std::vector<vec2> CalculateLineEndpoints(const vec3& line)
	{
		static const vec3 top = cross(vec3{ -1.0f, 1.0f, 1.0f }, vec3{ 1.0f, 1.0f, 1.0f });
		static const vec3 bottom = cross(vec3{ -1.0f, -1.0f, 1.0f }, vec3{ 1.0f, -1.0f, 1.0f });
	
		vec3 start = cross(top, line);
		vec3 end = cross(bottom, line);

		return {
			vec2(start.x / start.z, start.y / start.z),
			vec2(end.x / start.z, end.y / start.z)
		};
	}

	vec2 ScreenToNDC(int px, int py)
	{
		float x = (2.0f * px / winWidth) - 1.0f;
		float y = 1.0f - (2.0f * py / winHeight);
		return { x, y };
	}
} app(-0.8f, -0.6f, 0.8f, -0.6f, 0.5f, 0.7f);