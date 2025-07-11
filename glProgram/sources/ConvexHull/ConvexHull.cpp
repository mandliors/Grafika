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

class ConvexHullApp : public glApp {
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
	Geometry<vec2>* points;
	Geometry<vec2>* hullPoints;

	vec2* draggedVertex = nullptr;

public:
	ConvexHullApp() : glApp("Convex hull") { }
	~ConvexHullApp()
	{
		delete points;
		delete hullPoints;

		delete gpuProgram;
	}

	// Inicializáció, 
	void onInitialization()
	{
		glLineWidth(3.0f);
		glPointSize(10.0f);

		gpuProgram = new GPUProgram(vertSource, fragSource);

		points = new Geometry<vec2>();
		hullPoints = new Geometry<vec2>();

		srand(time(NULL));
		const int numPoints = 15;
		points->Vtx().reserve(numPoints);
		for (int i = 0; i < numPoints; i++)
			points->Vtx().emplace_back((rand() % 1000 / 500.0f - 1.0f) * 0.9f, (rand() % 1000 / 500.0f - 1.0f) * 0.9f);

		Recalculate();
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);

		hullPoints->updateGPU();
		if (hullPoints->Vtx().size() >= 3)
		{
			hullPoints->Draw(gpuProgram, GL_TRIANGLE_FAN, vec3(0.0f, 1.0f, 1.0f));
			hullPoints->Draw(gpuProgram, GL_LINE_LOOP, vec3(1.0f, 1.0f, 1.0f));
		}

		points->updateGPU();
		points->Draw(gpuProgram, GL_POINTS, vec3(1, 0, 0));
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
			points->Vtx().push_back(ScreenToNDC(pX, pY));
			Recalculate();
			refreshScreen();
		}
		else if (but == MOUSE_RIGHT)
		{
			for (auto& p : points->Vtx())
			{
				vec2 mousePos = ScreenToNDC(pX, pY);
				if (length(mousePos - p) < 0.05f)
				{
					draggedVertex = &p;
					return;
				}
			}
		}
	}

	void onMouseReleased(MouseButton but, int pX, int pY)
	{
		if (but == MOUSE_RIGHT)
			draggedVertex = nullptr;
	}

	void Recalculate()
	{
		vec2* pLow = &points->Vtx()[0];
		for (auto& p : points->Vtx())
			if (p.y < pLow->y) pLow = &p;

		hullPoints->Vtx().clear();
		vec2 pCur = *pLow, *pNext = nullptr, dir(1, 0);
		do 
		{ 
			// find convex hull points one by one
			float maxCos = -1;
			for (auto& p : points->Vtx())
			{
				// find minimal left turn
				float len = length(p - pCur);
				if (len > 0)
				{
					float cosPhi = dot(dir, p - pCur) / len;
					if (cosPhi > maxCos) { maxCos = cosPhi; pNext = &p; }
				}
			}
			hullPoints->Vtx().push_back(*pNext); // save as convex hull
			dir = normalize(*pNext - pCur); // prepare for next
			pCur = *pNext;
		} while (pLow != pNext);
	}

	vec2 ScreenToNDC(int px, int py)
	{
		float x = (2.0f * px / winWidth) - 1.0f;
		float y = 1.0f - (2.0f * py / winHeight);
		return { x, y };
	}
};

ConvexHullApp app;