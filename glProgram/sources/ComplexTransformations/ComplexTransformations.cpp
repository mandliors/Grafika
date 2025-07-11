//=============================================================================================
// Complex Transformations
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

struct Complex {
	float x, y;
	Complex(float x0 = 0.0f, float y0 = 0.0f) { x = x0, y = y0; }
	Complex operator+(Complex r) { return Complex(x + r.x, y + r.y); }
	Complex operator-(Complex r) { return Complex(x - r.x, y - r.y); }
	Complex operator*(Complex r) {
		return Complex(x * r.x - y * r.y, x * r.y + y * r.x);
	}
	Complex operator/(Complex r) {
		float l = r.x * r.x + r.y * r.y;
		return (*this) * Complex(r.x / l, -r.y / l); // conjugate
	}
	operator vec2() { return { x, y }; }
};
Complex Polar(float r, float phi) { // Constructor
	return Complex(r * cosf(phi), r * sinf(phi));
}

class ComplexTransformations : public glApp {
	Geometry<vec2>* triangle;  // geometria
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók

	Complex vertices[3];
	Complex center;

public:
	ComplexTransformations() : glApp("Complex Transformations") {}
	~ComplexTransformations() { delete triangle; delete gpuProgram; }

	// Inicializáció
	void onInitialization() {
		vertices[0] = { -0.6f, -0.2f };
		vertices[1] = { -0.3f, -0.1f };
		vertices[2] = { -0.5f,  0.2f };
		center = (vertices[0] + vertices[1] + vertices[2]) / 3.0f;

		triangle = new Geometry<vec2>;

		gpuProgram = new GPUProgram(vertSource, fragSource);

		Update();
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);

		triangle->Draw(gpuProgram, GL_TRIANGLES, vec3(0.0f, 1.0f, 0.0f));
	}

	void onKeyboard(int key)
	{
		Complex translation, rotation = { 1.0f, 0.0f };

		if (key == 'a')
			translation = { -0.1f,  0.0f };
		else if (key == 'd')
			translation = { 0.1f,  0.0f };
		else if (key == 'w')
			translation = { 0.0f,  0.1f };
		else if (key == 's')
			translation = { 0.0f, -0.1f };

		if (key == 'q')
			rotation = Polar(1.0f, M_PI * 0.25f);
		else if (key == 'e')
			rotation = Polar(1.0f, -M_PI * 0.25f);
	
		for (int i = 0; i < 3; i++)
			vertices[i] = (vertices[i] - center) * rotation + center + translation;
		
		center = (vertices[0] + vertices[1] + vertices[2]) / 3.0f;
		
		Update();
	}

	void Update()
	{
		triangle->Vtx() = { vertices[0], vertices[1], vertices[2] };
		triangle->updateGPU();
		refreshScreen();
	}

} app;
