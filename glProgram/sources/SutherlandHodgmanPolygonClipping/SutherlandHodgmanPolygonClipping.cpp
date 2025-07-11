//=============================================================================================
// Sutherland-Hodgman polygon clipping algorithm
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

class SutherlandHodgman : public glApp
{
	std::vector<std::pair<Geometry<vec2>*, Geometry<vec2>*>> shapes;
	Geometry<vec2>* window;
	GPUProgram* gpuProgram;

	const float epsilon = 1e-5f;

public:
	SutherlandHodgman() : glApp("Sutherland-Hodgman Polygon Clipping") {}
	~SutherlandHodgman()
	{
		for (const auto [s, sc] : shapes)
		{
			delete s;
			delete sc;
		}

		delete window;
		delete gpuProgram;
	}

	// Inicializáció,
	void onInitialization()
	{
		auto* quad = new Geometry<vec2>;
		quad->Vtx() = { vec2(-0.5f, -0.5f), vec2(-0.7f, -0.8f), vec2(-0.3f, -0.7f), vec2(-0.2f, -0.3f), vec2(-0.8f, -0.1f), vec2(-0.7f, -0.8f) };
		quad->updateGPU();

		auto* triangle = new Geometry<vec2>;
		triangle->Vtx() = { vec2(0.5f, 0.0f), vec2(0.2f, -0.5f), vec2(0.8f, -0.3f), vec2(0.6f, 0.4f), vec2(0.2f, -0.5f) };
		triangle->updateGPU();

		auto* star = new Geometry<vec2>;
		star->Vtx() = GenerateStarVertices(vec2(-0.2f, 0.5f), 0.4f);
		star->updateGPU();

		shapes.push_back({ quad, new Geometry<vec2> });
		shapes.push_back({ triangle, new Geometry<vec2> });
		shapes.push_back({ star, new Geometry<vec2> });

		window = new Geometry<vec2>;
		RecalculateWindow(0, 0);
		RecalculateClippedShapes();

		gpuProgram = new GPUProgram(vertSource, fragSource);

		glLineWidth(2.0f);
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);

		// draw the shape and the clipped shape (darker and brighter color)
		for (auto [s, sc] : shapes)
		{
			s->Draw(gpuProgram, GL_TRIANGLE_FAN, vec3(0.2f, 0.2f, 0.0f));
			sc->Draw(gpuProgram, GL_TRIANGLES, vec3(1.0f, 1.0f, 0.0f));
		}

		window->Draw(gpuProgram, GL_LINE_LOOP, vec3(1.0f, 1.0f, 1.0f));
	}

	void onMouseMotion(int pX, int pY)
	{
		RecalculateWindow(pX, pY);
		RecalculateClippedShapes();
		refreshScreen();
	}

	void RecalculateWindow(int pX, int pY)
	{
		static const float wHalf = 0.4f;
		static const float hHalf = 0.3f;

		vec2 p = ScreenToNDC(pX, pY);
		window->Vtx() = {
			vec2(p.x - wHalf, p.y - hHalf),
			vec2(p.x + wHalf, p.y - hHalf),
			vec2(p.x + wHalf, p.y + hHalf),
			vec2(p.x - wHalf, p.y + hHalf),
		};
		window->updateGPU();
	}

	void RecalculateClippedShapes()
	{
		for (auto& [shape, clippedShape] : shapes)
		{
			// copy the vertices, but skip the center and the first (duplicate) vertex
			clippedShape->Vtx().assign(shape->Vtx().begin() + 2, shape->Vtx().end());

			// clip with the window sides
			clippedShape->Vtx() = SutherlandHodgmanPolygonClipping(clippedShape->Vtx(), window->Vtx()[0], vec2(0.0f, 1.0f));
			clippedShape->Vtx() = SutherlandHodgmanPolygonClipping(clippedShape->Vtx(), window->Vtx()[1], vec2(-1.0f, 0.0f));
			clippedShape->Vtx() = SutherlandHodgmanPolygonClipping(clippedShape->Vtx(), window->Vtx()[2], vec2(0.0f, -1.0f));
			clippedShape->Vtx() = SutherlandHodgmanPolygonClipping(clippedShape->Vtx(), window->Vtx()[3], vec2(1.0f, 0.0f));

			// triangulate the clipped shape
			if (!clippedShape->Vtx().empty())
			{
				clippedShape->Vtx() = Triangulate(clippedShape->Vtx());
				if (!clippedShape->Vtx().empty())
					clippedShape->updateGPU();
			}
		}
	}

	std::vector<vec2> SutherlandHodgmanPolygonClipping(const std::vector<vec2>& p, const vec2& p0, const vec2& n) // vertices, clip line's start point and normal vector
	{
		std::vector<vec2> q;
		for (int i = 0; i < p.size(); i++)
		{
			int nI = (i + 1) % p.size();
			if (dot(p[i] - p0, n) >= 0.0f) // vertex is in the polygon
			{
				q.push_back(p[i]);
				if (dot(p[nI] - p0, n) < 0.0f) // next vertex is outside the polygon
					q.push_back(Intersect(p[i], p[nI], p0, n));
			}
			else if (dot(p[nI] - p0, n) >= 0.0f) // next vertex is in the polygon
				q.push_back(Intersect(p[i], p[nI], p0, n));
		}
		return q;
	}

	vec2 Intersect(const vec2& p1, const vec2& p2, const vec2& p0, const vec2& n)
	{
		vec2 d = p2 - p1;
		float t = dot(p0 - p1, n) / dot(d, n);
		return p1 + t * d;
	}

	std::vector<vec2> GenerateStarVertices(const vec2& c, float r)
	{
		const int N = 5 * 2;
		std::vector<vec2> vertices;
		float angleStep = 2 * M_PI / N;
		vertices.push_back(c);
		for (int i = 0; i <= N; ++i)
		{
			float angle = i * angleStep;
			float R = i % 2 == 0 ? r : r * 0.5f;
			float x = c.x + R * cos(angle);
			float y = c.y + R * sin(angle);
			vertices.push_back(vec2(x, y));
		}

		return vertices;
	}

	std::vector<vec2> Triangulate(const std::vector<vec2>& v)
	{
		std::vector<vec2> vertices = v;
		std::vector<vec2> triangles;
		triangles.reserve((vertices.size() - 2) * 3);

		while (vertices.size() >= 3)
		{
			int vIdx = FindEar(vertices);
			if (vIdx == -1) break;

			// add new triangle
			int prevIdx = (vIdx - 1 + vertices.size()) % vertices.size();
			int nextIdx = (vIdx + 1) % vertices.size();
			triangles.push_back(vertices[prevIdx]);
			triangles.push_back(vertices[vIdx]);
			triangles.push_back(vertices[nextIdx]);

			// remove the ear vertex
			vertices.erase(vertices.begin() + vIdx);
		}

		return triangles;
	}

	int FindEar(const std::vector<vec2>& v)
	{
		if (v.size() < 3) return -1;

		int diagIdx = -1;
		for (int i = 0; i < v.size(); i++)
		{
			diagIdx = i;
			int prevIdx = (i - 1 + v.size()) % v.size();
			int nextIdx = (i + 1) % v.size();

			// search for edge intersections (enough vertices are needed)
			if (v.size() > 4)
			{
				for (int j = 0; j < v.size() - 4; j++)
				{
					// edge indices
					int i1 = (i - 2 - j + v.size()) % v.size();
					int i2 = (i - 3 - j + v.size()) % v.size();
					if (LineIntersect(v[prevIdx], v[nextIdx], v[i1], v[i2]))
					{
						diagIdx = -1; // edge intersection found
						break;
					}
				}
			}

			// not a diagonal
			if (diagIdx == -1) continue;

			// triangle is always an ear
			if (v.size() == 3) return diagIdx;

			// calculate the maximum x coord for the half line
			float xMax = v[0].x;
			for (int j = 1; j < v.size(); j++)
				if (v[j].x > xMax)
					xMax = v[j].x;
			xMax += 0.01f; // to avoid precision issues

			// see if it goes fully outside of the polygon
			vec2 halfPoint = (v[prevIdx] + v[nextIdx]) * 0.5f;
			int c = 0;
			for (int j = 0; j < v.size(); j++)
				if (LineIntersect(halfPoint, { xMax, halfPoint.y }, v[j], v[(j + 1) % v.size()]))
					c++;

			if (c % 2 == 1) return diagIdx;
		}

		return -1;
	}

	bool LineIntersect(const vec2& l1, const vec2& l2, const vec2& r1, const vec2& r2)
	{
		vec2 ln = l2 - l1; ln = vec2(-ln.y, ln.x);
		if (dot(ln, (r1 - l1)) * dot(ln, (r2 - l1)) > 0) return false;

		vec2 rn = r2 - r1; rn = vec2(-rn.y, rn.x);
		if (dot(rn, (l1 - r1)) * dot(rn, (l2 - r1)) > 0) return false;

		return true;
	}

	vec2 ScreenToNDC(int pX, int pY)
	{
		float x = (2.0f * pX / winWidth) - 1.0f;
		float y = 1.0f - (2.0f * pY / winHeight);
		return vec2(x, y);
	}

} app;

