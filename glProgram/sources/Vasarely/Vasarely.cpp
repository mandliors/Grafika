//=============================================================================================
// Vasarely
//=============================================================================================
#include "framework.h"

const char * const vertSource = R"(
	#version 330

	uniform float scaling, up;
	layout(location = 0) in vec2 vp; 

	void main() { gl_Position = vec4(vp.x * scaling, vp.y * scaling + up, 0, 1); }
)";

const char * const fragSource = R"(
	#version 330

	uniform vec3 color;
	out vec4 outColor;

	void main() { outColor = vec4(color, 1); }
)";

class Circle : public Geometry<vec2> {
public:
	Circle() {
		const int nVertices = 100; // sokszög csúcsainak száma
		for (int i = 0; i < nVertices; i++) {
			float phi = i * 2.0f * (float)M_PI / nVertices;
			vtx.push_back(vec2(cosf(phi), sinf(phi))); // kör egyenlete
		}
		updateGPU(); // CPU->GPU szinkronizálás
	}
};

const int winWidth = 600, winHeight = 600;

class VasarelyApp : public glApp {
	GPUProgram* gpuProgram;  // árnyaló programok
	Circle* circle;          // egység sugarú kör
	float amplitude = 0.8f;  // betüremkedés illuzió nagysága
	bool animate = false;    // mozgassuk?
public:
	VasarelyApp() : glApp(3, 3, winWidth, winHeight, "Vasarely") { }
	void onInitialization() {
		circle = new Circle();
		gpuProgram = new GPUProgram(vertSource, fragSource);
	}
	void onDisplay() {
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // alkalmazói ablak törlése
		glViewport(0, 0, winWidth, winHeight); // keletkezõ fénykép mérete

		const int nCircles = 20;
		const float r0 = 1.0f, r1 = 1.0f / nCircles; // sugár tartománya
		vec3 ce0(1, 0, 0), ce1(0, 0, 0), co0(0, 0, 0.5f), co1(0, 1, 1);

		for (int i = 0; i < nCircles; i++) {
			float t = (float)i / (nCircles - 1.0f); // interpolációs változó
			gpuProgram->setUniform(r0 * (1 - t) + r1 * t, "scaling");
			gpuProgram->setUniform(t * (1 - t) * amplitude, "up");
			vec3 color = (i % 2 == 0) ? ce0 * (1 - t) + ce1 * t :
				                        co0 * (1 - t) + co1 * t;
			circle->Draw(gpuProgram, GL_TRIANGLE_FAN, color);
		}
	}

	void onKeyboard(int key) { if (key == 'a') animate = !animate; }
	void onTimeElapsed(float startTime, float endTime) {
		if (animate) {
			amplitude = sinf(3 * endTime); // hullámzás
			refreshScreen(); // újrarajzolás
		}
	}
} app;

