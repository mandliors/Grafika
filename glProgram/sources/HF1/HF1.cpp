//=============================================================================================
// Házi feladat 1
//=============================================================================================
#include "framework.h"
#include <iostream>

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec3 cP;	// 0. bemeneti regiszter

	void main() {
		gl_Position = vec4(cP.x, cP.y, cP.z, 1); 	// bemenet már normalizált eszközkoordinátákban
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
enum class State {
	POINT = 0, LINE_P1, LINE_P2, TRANSLATION_START, TRANSLATION_MOVING, INTERSECTION_L1, INTERSECTION_L2
};

class Object
{
	unsigned int vao, vbo;	// GPU
protected:
	std::vector<vec3> vtx;	// CPU

public:
	Object()
	{
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(0);
		int nf = std::min((int)(sizeof(vec3) / sizeof(float)), 4);
		glVertexAttribPointer(0, nf, GL_FLOAT, GL_FALSE, 0, NULL);
	}
	std::vector<vec3>& Vtx() { return vtx; }
	void updateGPU() {	// CPU -> GPU
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(vec3), &vtx[0], GL_DYNAMIC_DRAW);
	}
	void Bind() { glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); } // aktiválás
	void Draw(GPUProgram* prog, int type, vec3 color)
	{
		if (vtx.size() > 0)
		{
			prog->setUniform(color, "color");
			glBindVertexArray(vao);
			glDrawArrays(type, 0, (int)vtx.size());
		}
	}
	virtual ~Object() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}
};

class PointCollection
{
	Object object;

public:
	void Add(const vec3& point)
	{
		std::cout << "Point " << point.x << ", " << point.y << " added" << std::endl;

		object.Vtx().push_back(point);
		object.updateGPU();
	}
	vec3* GetClosestPoint(const vec3& point)
	{
		for (auto& p : object.Vtx())
			if (length(p - point) < 0.05f)
				return &p;

		return nullptr;
	}
	void Draw(GPUProgram* program, const vec3& color) { object.Draw(program, GL_POINTS, color); }
};

class Line
{
	vec3 line;

public:
	Line(const vec3& p1, const vec3& p2) : line(cross(p1, p2))
	{
		vec3 dir = normalize(p2 - p1);
		vec3 normal = vec3{ -dir.y, dir.x, 0.0f };

		// implicit
		std::cout << "\t  Implicit: " << normal.x << "x + " << normal.y << "y + " << -normal.x * p1.x - normal.y * p1.y << " = 0" << std::endl;

		// paraméteres
		std::cout << "\tParametric: r(t) = (" << p1.x << ", " << p1.y << ") + (" << p2.x << ", " << p2.y << ")t" << std::endl;
	}

	vec3 GetIntersection(const Line& other) const
	{
		vec3 intersection = cross(line, other.line);
		return intersection / intersection.z;
	}

	bool ContainsPoint(const vec3& point) const
	{
		return dot(line, point) < 0.1f;
	}

	std::pair<vec2, vec2> GetDrawPoints() const
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

	void TranslateToPoint(const vec3& point)
	{
		line.z = -dot(vec2(line.x, line.y), vec2(point.x, point.y));
	}
};

class LineCollection
{
	std::vector<Line> lines;
	Object object;

public:
	void Add(const vec3& p1, const vec3& p2)
	{
		std::cout << "Line added" << std::endl;

		lines.emplace_back(p1, p2);
		object.Vtx().resize(lines.size() * 2);
	}

	Line* GetLine(const vec3& point)
	{
		for (auto& line : lines)
			if (line.ContainsPoint(point))
				return &line;

		return nullptr;
	}

	void Draw(GPUProgram* program, const vec3& color)
	{
		if (lines.empty())
			return;

		// frissíteni kell a vonalak VBO-ját
		for (size_t i = 0; i < lines.size() * 2; i += 2)
		{
			auto [p1, p2] = lines[i / 2].GetDrawPoints();
			object.Vtx()[i] = vec3(p1.x, p1.y, 1.0f);
			object.Vtx()[i + 1] = vec3(p2.x, p2.y, 1.0f);
		}
		object.updateGPU();
		object.Draw(program, GL_LINES, color);
	}
};

class HF1 : public glApp {
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
	State state; // állapot (milyen müveletet végzünk)
	vec3* selectedPoint; // elsönek kijelölt pont
	Line* selectedLine; // elsönek kijelölt vonal

	PointCollection* points;    // pontok
	LineCollection* lines;    // vonalak
public:
	HF1() : glApp("HF1"), state(State::POINT), selectedPoint(nullptr) {}
	~HF1() { delete gpuProgram; delete points; delete lines; }

	void onInitialization()
	{
		glPointSize(10); // pontméret
		glLineWidth(3);  // vonalvastagság

		gpuProgram = new GPUProgram(vertSource, fragSource);
		points = new PointCollection();
		lines = new LineCollection();
	}

	void onDisplay()
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);

		lines->Draw(gpuProgram, { 0, 1, 1 }); // cyan
		points->Draw(gpuProgram, { 1, 0, 0 }); // piros
	}

	void onKeyboard(int key) override
	{
		switch (key)
		{
		case 'p':
			std::cout << "Add points" << std::endl;
			state = State::POINT;
			break;
		case 'l':
			std::cout << "Define lines" << std::endl;
			state = State::LINE_P1;
			break;
		case 'm':
			std::cout << "Move" << std::endl;
			state = State::TRANSLATION_START;
			break;
		case 'i':
			std::cout << "Intersect" << std::endl;
			state = State::INTERSECTION_L1;
			break;
		}
	}

	void onMousePressed(MouseButton but, int pX, int pY) override
	{
		if (but != MouseButton::MOUSE_LEFT)
			return;

		const vec3 mousePos = ScreenToNDC(pX, pY);

		switch (state)
		{
		case State::POINT:
			points->Add(ScreenToNDC(pX, pY));
			break;
		case State::LINE_P1:
			if ((selectedPoint = points->GetClosestPoint(mousePos)) != nullptr)
				state = State::LINE_P2;
			break;
		case State::LINE_P2:
		{
			const vec3* point = points->GetClosestPoint(mousePos);
			if (point != nullptr && point != selectedPoint)
			{
				lines->Add(*selectedPoint, *point);
				state = State::LINE_P1;
			}
		}
		break;
		case State::TRANSLATION_START:
			if ((selectedLine = lines->GetLine(mousePos)) != nullptr)
				state = State::TRANSLATION_MOVING;
			break;
		case State::INTERSECTION_L1:
			if ((selectedLine = lines->GetLine(mousePos)) != nullptr)
				state = State::INTERSECTION_L2;
			break;
		case State::INTERSECTION_L2:
		{
			Line* line = lines->GetLine(mousePos);
			if (line != nullptr && line != selectedLine)
			{
				vec3 intersection = selectedLine->GetIntersection(*line);
				points->Add(intersection);
				state = State::INTERSECTION_L1;
			}
		}
		break;
		default:
			break;
		}

		refreshScreen();
	}

	void onMouseMotion(int pX, int pY) override
	{
		const vec3 mousePos = ScreenToNDC(pX, pY);

		switch (state)
		{
		case State::TRANSLATION_MOVING:
			selectedLine->TranslateToPoint(mousePos);
			break;
		default:
			break;
		}

		refreshScreen();
	}

	void onMouseReleased(MouseButton but, int pX, int pY) override
	{
		if (but != MouseButton::MOUSE_LEFT)
			return;

		switch (state)
		{
		case State::TRANSLATION_MOVING:
			state = State::TRANSLATION_START;
			break;
		default:
			break;
		}
	}

	vec3 ScreenToNDC(int x, int y) const
	{
		return { x / 300.0f - 1.0f, 1.0f - y / 300.0f, 1.0f };
	}
} app;