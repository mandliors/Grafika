//=============================================================================================
// Zöld háromszög: A framework.h osztályait felhasználó megoldás
//=============================================================================================
#include "framework.h"

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330
	uniform float cosa, sina, cosb, sinb;
	layout(location = 0) in vec4 vtx;
	out float depthCue;

	void main() {
		vec4 vr1, vr2;
		// Forgatás a zw síkban alfa szöggel
		vr1 = vec4(vtx.xy, vtx.z*cosa-vtx.w*sina, vtx.z*sina+vtx.w*cosa);
		// Forgatás a xw síkban beta szöggel
		vr2 = vec4(vr1.x*cosb-vr1.w*sinb, vr1.yz, vr1.x*sinb+vr1.w*cosb);
		// Skálázás és eltolás a kamera elé
		vec4 p4d = vr2 * 0.4f + vec4(0, 0, 1, 1);
		// Intenzitás számítás a távolságból
		depthCue = 1 / (dot(p4d, p4d) - 0.4f);
		// Perspektív transzformáció
		gl_Position = vec4(p4d.xyz, p4d.z);
	} 
)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
    uniform vec3 color;
	in float depthCue;
	out vec4 outColor;

	void main() {
		outColor = vec4(color * depthCue, 1);
	}
)";

class Tesseract : public Geometry<vec4> {
	float alpha, beta;
	vec3 color;
	void addPoint(int c) {
		vec4 p(c & 1 ? 1 : -1, c & 2 ? 1 : -1, c & 4 ? 1 : -1, c & 8 ? 1 : -1);
		vtx.push_back(p);
	}
public:
	Tesseract() {
		const int maxcode = 15;
		for (int code = 0; code <= maxcode; code++) {
			for (int bit = 1; bit <= maxcode; bit <<= 1) {
				if ((code & bit) == 0) {
					addPoint(code);
					addPoint(code + bit);
				}
			}
		}
		updateGPU();
	}
	void Animate(float t) {
		alpha = t / 2; beta = t / 3;
		float s = 2 * M_PI / 3;
		color = vec3(sin(t) + 0.5, sin(t + s) + 0.5, sin(t + 2 * s) + 0.5);
	}
	void Draw(GPUProgram* gpuProgram) {
		gpuProgram->setUniform(cosf(alpha), "cosa");
		gpuProgram->setUniform(sinf(alpha), "sina");
		gpuProgram->setUniform(cosf(beta), "cosb");
		gpuProgram->setUniform(sinf(beta), "sinb");
		Bind();
		glDrawArrays(GL_LINES, 0, (int)vtx.size());
	}
};


const int winWidth = 600, winHeight = 600;

class TesseractApp : public glApp {
	Tesseract* tesseract;
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
public:
	TesseractApp() : glApp("Tesseract") { }
	~TesseractApp() { delete tesseract; delete gpuProgram; }

	// Inicializáció
	void onInitialization() {
		tesseract = new Tesseract();
		gpuProgram = new GPUProgram(vertSource, fragSource);
		gpuProgram->setUniform(vec3(0, 1, 1), "color");
	}

	// Idöléptetés
	void onTimeElapsed(float startTime, float endTime) {
		tesseract->Animate(endTime);
		refreshScreen();
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);
		tesseract->Draw(gpuProgram);
	}
};

TesseractApp app;
