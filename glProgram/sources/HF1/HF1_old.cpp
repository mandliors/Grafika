////=============================================================================================
//// Házi feladat 1
////=============================================================================================
//#include "framework.h"
//#include <iostream>
//
//// csúcspont árnyaló
//const char* vertSource = R"(
//	#version 330				
//    precision highp float;
//
//	layout(location = 0) in vec3 cP;	// 0. bemeneti regiszter
//
//	void main() {
//		gl_Position = vec4(cP.x, cP.y, cP.z, 1); 	// bemenet már normalizált eszközkoordinátákban
//	}
//)";
//
//// pixel árnyaló
//const char* fragSource = R"(
//	#version 330
//    precision highp float;
//
//	uniform vec3 color;			// konstans szín
//	out vec4 fragmentColor;		// pixel szín
//
//	void main() {
//		fragmentColor = vec4(color, 1); // RGB -> RGBA
//	}
//)";
//
//const int winWidth = 600, winHeight = 600;
//enum class State {
//	POINT = 0, LINE_P1, LINE_P2, TRANSLATION_START, TRANSLATION_MOVING, INTERSECTION_L1, INTERSECTION_L2
//};
//
//
//struct MyVec3
//{
//	float x, y, z;
//
//	MyVec3 operator+(const MyVec3& other) const { return { x + other.x, y + other.y, z + other.z }; }
//	MyVec3 operator-(const MyVec3& other) const { return { x - other.x, y - other.y, z - other.z }; }
//	MyVec3 operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar }; }
//	MyVec3 operator/(float scalar) const { return { x / scalar, y / scalar, z / scalar }; }
//	MyVec3 normalize() const { return *this / sqrt(x * x + y * y + z * z); }
//};
//
//class MyGPUProgram
//{
//	GLuint shaderProgramId = 0;
//	bool waitError = true;
//
//	bool checkShader(unsigned int shader, std::string message) { // shader fordítási hibák kezelése
//		GLint infoLogLength = 0, result = 0;
//		glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
//		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
//		if (!result) {
//			std::string errorMessage(infoLogLength, '\0');
//			glGetShaderInfoLog(shader, infoLogLength, NULL, (GLchar*)errorMessage.data());
//			printf("%s! \n Log: \n%s\n", message.c_str(), errorMessage.c_str());
//			if (waitError) getchar();
//			return false;
//		}
//		return true;
//	}
//
//	bool checkLinking(unsigned int program) { 	// shader szerkesztési hibák kezelése
//		GLint infoLogLength = 0, result = 0;
//		glGetProgramiv(program, GL_LINK_STATUS, &result);
//		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
//		if (!result) {
//			std::string errorMessage(infoLogLength, '\0');
//			glGetProgramInfoLog(program, infoLogLength, nullptr, (GLchar*)errorMessage.data());
//			printf("Failed to link shader program! \n Log: \n%s\n", errorMessage.c_str());
//			if (waitError) getchar();
//			return false;
//		}
//		return true;
//	}
//
//	int getLocation(const std::string& name) {	// uniform változó címének lekérdezése
//		int location = glGetUniformLocation(shaderProgramId, name.c_str());
//		if (location < 0) printf("uniform %s cannot be set\n", name.c_str());
//		return location;
//	}
//
//	std::string shaderType2string(GLenum shadeType) {
//		switch (shadeType)
//		{
//		case GL_VERTEX_SHADER:			return "Vertex"; break;
//		case GL_FRAGMENT_SHADER:		return "Fragment"; break;
//		case GL_GEOMETRY_SHADER:		return "Geometry"; break;
//		case GL_TESS_CONTROL_SHADER:	return "Tessellation control"; break;
//		case GL_TESS_EVALUATION_SHADER:	return "Tessellation evaluation"; break;
//		case GL_COMPUTE_SHADER:			return "Compute"; break;
//		default:						return "Unknown [shader type]"; break;
//		}
//	}
//
//public:
//	MyGPUProgram() { }
//	MyGPUProgram(const char* const vertexShaderSource, const char* const fragmentShaderSource, const char* const geometryShaderSource = nullptr) {
//		create(vertexShaderSource, fragmentShaderSource, geometryShaderSource);
//	}
//
//	void create(const char* const vertexShaderSource, const char* const fragmentShaderSource, const char* const geometryShaderSource = nullptr) {
//		// Program létrehozása a forrás sztringbõl
//		GLuint  vertexShader = glCreateShader(GL_VERTEX_SHADER);
//		if (!vertexShader) {
//			printf("Error in vertex shader creation\n");
//			exit(1);
//		}
//		glShaderSource(vertexShader, 1, (const GLchar**)&vertexShaderSource, NULL);
//		glCompileShader(vertexShader);
//		if (!checkShader(vertexShader, "Vertex shader error")) return;
//
//		// Program létrehozása a forrás sztringbõl, ha van geometria árnyaló
//		GLuint geometryShader = 0;
//		if (geometryShaderSource != nullptr) {
//			geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
//			if (!geometryShader) {
//				printf("Error in geometry shader creation\n");
//				exit(1);
//			}
//			glShaderSource(geometryShader, 1, (const GLchar**)&geometryShaderSource, NULL);
//			glCompileShader(geometryShader);
//			if (!checkShader(geometryShader, "Geometry shader error")) return;
//		}
//
//		// Program létrehozása a forrás sztringbõl
//		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
//		if (!fragmentShader) {
//			printf("Error in fragment shader creation\n");
//			exit(1);
//		}
//
//		glShaderSource(fragmentShader, 1, (const GLchar**)&fragmentShaderSource, NULL);
//		glCompileShader(fragmentShader);
//		if (!checkShader(fragmentShader, "Fragment shader error")) return;
//
//		shaderProgramId = glCreateProgram();
//		if (!shaderProgramId) {
//			printf("Error in shader program creation\n");
//			exit(-1);
//		}
//		glAttachShader(shaderProgramId, vertexShader);
//		glAttachShader(shaderProgramId, fragmentShader);
//		if (geometryShader > 0) glAttachShader(shaderProgramId, geometryShader);
//
//		// Szerkesztés
//		if (!link()) return;
//
//		// Ez fusson
//		glUseProgram(shaderProgramId);
//	}
//
//	bool link() {
//		glLinkProgram(shaderProgramId);
//		return checkLinking(shaderProgramId);
//	}
//
//	void Use() { glUseProgram(shaderProgramId); } 		// make this program run
//
//	void setUniform(int i, const std::string& name) {
//		int location = getLocation(name);
//		if (location >= 0) glUniform1i(location, i);
//	}
//
//	void setUniform(float f, const std::string& name) {
//		int location = getLocation(name);
//		if (location >= 0) glUniform1f(location, f);
//	}
//
//	void setUniformf3(float* v, const std::string& name) {
//		int location = getLocation(name);
//		if (location >= 0) glUniform3fv(location, 1, v);
//	}
//
//	~MyGPUProgram() { if (shaderProgramId > 0) glDeleteProgram(shaderProgramId); }
//};
//
//class Object
//{
//	unsigned int vao, vbo;	// GPU
//protected:
//	std::vector<MyVec3> vtx;	// CPU
//public:
//	Object() {
//		glGenVertexArrays(1, &vao);
//		glBindVertexArray(vao);
//		glGenBuffers(1, &vbo);
//		glBindBuffer(GL_ARRAY_BUFFER, vbo);
//		glEnableVertexAttribArray(0);
//		int nf = std::min((int)(sizeof(MyVec3) / sizeof(float)), 4);
//		glVertexAttribPointer(0, nf, GL_FLOAT, GL_FALSE, 0, NULL);
//	}
//	std::vector<MyVec3>& Vtx() { return vtx; }
//	void updateGPU() {	// CPU -> GPU
//		glBindBuffer(GL_ARRAY_BUFFER, vbo);
//		glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(MyVec3), &vtx[0], GL_DYNAMIC_DRAW);
//	}
//	void Bind() { glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); } // aktiválás
//	void Draw(MyGPUProgram* prog, int type, MyVec3 color)
//	{
//		if (vtx.size() > 0) 
//		{
//			prog->setUniformf3(&color.x, "color");
//			glBindVertexArray(vao);
//			glDrawArrays(type, 0, (int)vtx.size());
//		}
//	}
//	virtual ~Object() {
//		glDeleteBuffers(1, &vbo);
//		glDeleteVertexArrays(1, &vao);
//	}
//};
//
//class PointCollection
//{
//	Object object;
//
//public:
//	void Add(const MyVec3& point)
//	{
//		std::cout << "Point " << point.x << ", " << point.y << " added" << std::endl;
//
//		object.Vtx().push_back(point);
//		object.updateGPU();
//	}
//	MyVec3* GetClosestPoint(const MyVec3& point)
//	{
//		for (auto& p : object.Vtx())
//			if (GetDistance(p, point) < 0.01f)
//				return &p;
//		
//		return nullptr;
//	}
//	void Draw(MyGPUProgram* program, const MyVec3& color) { object.Draw(program, GL_POINTS, color); }
//	
//private:
//	float GetDistance(const MyVec3& p1, const MyVec3& p2) const
//	{
//		return (float)sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
//	}
//};
//
//class Line
//{
//	MyVec3 p0, normal;
//
//public:
//	Line(const MyVec3& p1, const MyVec3& p2) : p0(p1)
//	{
//		MyVec3 dir = (p2 - p1).normalize();
//		normal = MyVec3{ -dir.y, dir.x, 0.0f };
//
//		// implicit
//		std::cout << "\t  Implicit: " << normal.x << "x + " << normal.y << "y + " << -normal.x * p1.x - normal.y * p1.y << " = 0" << std::endl;
//
//		// paraméteres
//		std::cout << "\tParametric: r(t) = (" << p1.x << ", " << p1.y << ") + (" << p2.x << ", " << p2.y << ")t" << std::endl;
//	}
//
//	MyVec3 GetIntersection(const Line& other) const
//	{
//		// két egyenes metszéspontjának kiszámítása
//		float A1 = normal.x, B1 = normal.y, C1 = -normal.x * p0.x - normal.y * p0.y;
//		float A2 = other.normal.x, B2 = other.normal.y, C2 = -other.normal.x * other.p0.x - other.normal.y * other.p0.y;
//
//		if (A1 == 0.0f)
//		{
//			float y = -C1 / B1;
//			float x = A2 == 0.0f ? 0.0f : (-B2 * y - C2) / A2;
//
//			return MyVec3{ x, y, 1.0f };
//		}
//		else if (A2 == 0.0f)
//		{
//			float y = -C2 / B2;
//			float x = (-B1 * y - C1) / A1;
//
//			return MyVec3{ x, y, 1.0f };
//		}
//		else
//		{
//			float y = (C2 * A1 - C1 * A2) / (B1 * A2 - B2 * A1);
//			float x = (-B1 * y - C1) / A1;
//
//			return MyVec3{ x, y, 1.0f };
//		}
//	}
//
//	bool ContainsPoint(const MyVec3& point) const
//	{
//		return GetPointDistance(point) < 0.01f;
//	}
//
//	std::pair<MyVec3, MyVec3> GetDrawPoints() const
//	{
//		MyVec3 dir{ normal.y, -normal.x, 0.0f };
//		MyVec3 p1 = p0 + dir * 3.0f;
//		MyVec3 p2 = p0 - dir * 3.0f;
//
//		return { p1, p2 };
//	}
//
//	void TranslateToPoint(const MyVec3& point)
//	{
//		p0 = point;
//	}
//
//
//private:
//	float GetPointDistance(const MyVec3& point) const
//	{
//		float A = normal.x, B = normal.y, C = -normal.x * p0.x - normal.y * p0.y;
//
//		return abs(A * point.x + B * point.y + C) / sqrt(A * A + B * B);
//	}
//};
//
//class LineCollection
//{
//	std::vector<Line> lines;
//	Object object;
//public:
//	void Add(const MyVec3& p1, const MyVec3& p2)
//	{
//		std::cout << "Line added" << std::endl;
//
//		lines.emplace_back(p1, p2);
//		object.Vtx().resize(lines.size() * 2);
//	}
//
//	Line* GetLine(const MyVec3& point)
//	{
//		for (auto& line : lines)
//			if (line.ContainsPoint(point))
//				return &line;
//
//		return nullptr;
//	}
//
//	void Draw(MyGPUProgram* program, const MyVec3& color)
//	{
//		if (lines.empty())
//			return;
//
//		// frissíteni kell a vonalak VBO-ját
//		for (size_t i = 0; i < lines.size() * 2; i += 2)
//		{
//			auto [p1, p2] = lines[i / 2].GetDrawPoints();
//			object.Vtx()[i] = p1;
//			object.Vtx()[i + 1] = p2;
//		}
//		object.updateGPU();
//		object.Draw(program, GL_LINES, color);
//	}
//
//private:
//	float GetDistance(const MyVec3& p1, const MyVec3& p2)
//	{
//		return (float)sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
//	}
//};
//
//class HW1App : public glApp {
//	MyGPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
//	State state; // állapot (milyen müveletet végzünk)
//	MyVec3* selectedPoint; // elsönek kijelölt pont
//	Line* selectedLine; // elsönek kijelölt vonal
//
//	PointCollection* points;    // pontok
//	LineCollection* lines;    // vonalak
//public:
//	HW1App() : glApp("HF1"), state(State::POINT), selectedPoint(nullptr) { }
//	~HW1App() { delete gpuProgram; delete points; delete lines; }
//
//	void onInitialization()
//	{
//		glPointSize(10); // pontméret
//		glLineWidth(3);  // vonalvastagság
//
//		gpuProgram = new MyGPUProgram(vertSource, fragSource);
//		points = new PointCollection();
//		lines = new LineCollection();
//	}
//
//	void onDisplay()
//	{
//		glClearColor(0.16f, 0.16f, 0.16f, 1.0f); // háttér szín
//		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
//		glViewport(0, 0, winWidth, winHeight);
//
//		lines->Draw(gpuProgram, { 0, 1, 1 }); // cyan
//		points->Draw(gpuProgram, { 1, 0, 0 }); // piros
//	}
//
//	void onKeyboard(int key) override
//	{
//		switch (key)
//		{
//		case 'p':
//			std::cout << "Add points" << std::endl;
//			state = State::POINT;
//			break;
//		case 'l':
//			std::cout << "Define lines" << std::endl;
//			state = State::LINE_P1;
//			break;
//		case 'm':
//			std::cout << "Move" << std::endl;
//			state = State::TRANSLATION_START;
//			break;
//		case 'i':
//			std::cout << "Intersect" << std::endl;
//			state = State::INTERSECTION_L1;
//			break;
//		}
//	}
//
//	void onMousePressed(MouseButton but, int pX, int pY) override
//	{
//		if (but != MouseButton::MOUSE_LEFT)
//			return;
//
//		const MyVec3 mousePos = ScreenToNDC(pX, pY);
//
//		switch (state)
//		{
//		case State::POINT:
//			points->Add(ScreenToNDC(pX, pY));
//			break;
//		case State::LINE_P1:
//			if ((selectedPoint = points->GetClosestPoint(mousePos)) != nullptr)
//				state = State::LINE_P2;
//			break;
//		case State::LINE_P2:
//			{
//				const MyVec3* point = points->GetClosestPoint(mousePos);
//				if (point != nullptr && point != selectedPoint)
//				{
//					lines->Add(*selectedPoint, *point);
//					state = State::LINE_P1;
//				}
//			}
//			break;
//		case State::TRANSLATION_START:
//			if ((selectedLine = lines->GetLine(mousePos)) != nullptr)
//				state = State::TRANSLATION_MOVING;
//			break;
//		case State::INTERSECTION_L1:
//			if ((selectedLine = lines->GetLine(mousePos)) != nullptr)
//				state = State::INTERSECTION_L2;
//			break;
//		case State::INTERSECTION_L2:
//			{
//				Line* line = lines->GetLine(mousePos);
//				if (line != nullptr && line != selectedLine)
//				{
//					MyVec3 intersection = selectedLine->GetIntersection(*line);
//					points->Add(intersection);
//					state = State::INTERSECTION_L1;
//				}
//			}
//			break;
//		default:
//			break;
//		}
//
//		refreshScreen();
//	}
//
//	void onMouseMotion(int pX, int pY) override
//	{
//		const MyVec3 mousePos = ScreenToNDC(pX, pY);
//
//		switch (state)
//		{
//			case State::TRANSLATION_MOVING:
//				selectedLine->TranslateToPoint(mousePos);
//				break;
//		default:
//			break;
//		}
//
//		refreshScreen();
//	}
//
//	void onMouseReleased(MouseButton but, int pX, int pY) override
//	{
//		if (but != MouseButton::MOUSE_LEFT)
//			return;
//
//		switch (state)
//		{
//		case State::TRANSLATION_MOVING:
//			state = State::TRANSLATION_START;
//			break;
//		default:
//			break;
//		}
//	}
//
//private:
//	MyVec3 ScreenToNDC(int x, int y) const
//	{
//		return { x / 300.0f - 1.0f, 1.0f - y / 300.0f, 1.0f };
//	}
//} app;