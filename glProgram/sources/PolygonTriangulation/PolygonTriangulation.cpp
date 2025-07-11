//=============================================================================================
// Polygon Triangulation
//=============================================================================================
#include "framework.h"

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 cP;	// 0. bemeneti regiszter
	layout(location = 1) in vec3 col;	// 1. bemeneti regiszter

	out vec3 color;		// kimeneti regiszter

	void main() {
		gl_Position = vec4(cP.x, cP.y, 0, 1); 	// bemenet már normalizált eszközkoordinátákban
		color = col;	// szín kimenet
	}
)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
    precision highp float;

	in vec3 color;			// bemeneti szín
	out vec4 fragmentColor;		// pixel szín

	void main() {
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

const int winWidth = 600, winHeight = 600;

template<class T>
class ColoredGeometry
{
	//---------------------------
	unsigned int vao, vbo;	// GPU

protected:
	std::vector<T> vtx;	// CPU
	std::vector<vec3> col;	// CPU
	int nf = min((int)(sizeof(T) / sizeof(float)), 4); // számított attribútumok száma

public:
	ColoredGeometry()
	{
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, nf, GL_FLOAT, GL_FALSE, (nf + 3) * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(nf * sizeof(float)));
	}
	std::vector<T>& Vtx() { return vtx; }
	std::vector<vec3>& Col() { return col; }
	void updateGPU()
	{
		std::vector<float> data;
		data.reserve(vtx.size() * (nf + 3));
		for (int i = 0; i < vtx.size(); i++)
		{
			// add vertex data
			for (int j = 0; j < nf; j++)
				data.push_back(((float*)&vtx[i])[j]);

			// add color data
			data.push_back(col[i].r);
			data.push_back(col[i].g);
			data.push_back(col[i].b);
		}

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_DYNAMIC_DRAW);
	}
	void Bind() { glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); } // aktiválás
	void Draw(GPUProgram* prog, int type)
	{
		if (vtx.size() > 0)
		{
			glBindVertexArray(vao);
			glDrawArrays(type, 0, (int)vtx.size());
		}
	}
	virtual ~ColoredGeometry()
	{
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}
};

class PolygonTriangulation : public glApp 
{
	ColoredGeometry<vec2>* polyLines;
	ColoredGeometry<vec2>* polyTriangles;
	GPUProgram* gpuProgram;

public:
	PolygonTriangulation() : glApp("Polygon Triangulation") {}
	~PolygonTriangulation() { delete polyLines; delete polyTriangles; delete gpuProgram; }

	// Inicializáció, 
	void onInitialization() 
	{
		srand(time(NULL));

		polyLines = new ColoredGeometry<vec2>;
		polyTriangles = new ColoredGeometry<vec2>;

		gpuProgram = new GPUProgram(vertSource, fragSource);

		Recalculate();

		glLineWidth(3.0f);
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);

		polyTriangles->Draw(gpuProgram, GL_TRIANGLES);
		polyLines->Draw(gpuProgram, GL_LINE_LOOP);
	}

	void onMousePressed(MouseButton but, int pX, int pY)
	{
		Recalculate();
		refreshScreen();
	}

	void Recalculate()
	{
		polyLines->Vtx() = GenerateVertices(10, 0.7f, 0.25f);
		polyLines->Col().resize(polyLines->Vtx().size(), vec3(1.0f, 1.0f, 1.0f));
		polyLines->updateGPU();

		polyTriangles->Vtx() = Triangulate(polyLines->Vtx());
		polyTriangles->Col().clear();
		for (int i = 0; i < polyLines->Vtx().size(); i++)
		{
			vec3 color{ (rand() % 100) / 100.0f, (rand() % 100) / 100.0f, (rand() % 100) / 100.0f };
			polyTriangles->Col().push_back(color);
			polyTriangles->Col().push_back(color);
			polyTriangles->Col().push_back(color);
		}
		polyTriangles->updateGPU();
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

	std::vector<vec2> GenerateVertices(int count, float r, float noise)
	{
		/*{ vec2(-0.8f, -0.8f), vec2(-0.7f, 0.8f), vec2(-0.1f, 0.1f), vec2(0.8f, 0.4f), vec2(0.9f, -0.4f), vec2(0.3f, -0.85f) }*/

		std::vector<vec2> vertices;
		vertices.reserve(count);

		float da = 2 * M_PI / count;
		for (float a = 0; a < 2 * M_PI; a += da)
		{
			float x = r * cos(a) + (rand() % 200 - 100) / 100.0f * noise;
			float y = r * sin(a) + (rand() % 200 - 100) / 100.0f * noise;
			vertices.push_back(vec2(x, y));
		}

		return vertices;
	}

} app;

