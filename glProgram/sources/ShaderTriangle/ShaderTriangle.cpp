#include "framework.h"

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 vtxPos; // pos in modeling space
	out vec2 vertexPos;

	void main() {
		gl_Position = vec4(vtxPos, 0, 1);
		vertexPos = vtxPos;
	}
)";

// pixel árnyaló (1. verzió)
const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform vec2 p1, p2, p3; // háromszög három csúcsa

	in vec2 vertexPos;
	out vec4 fragmentColor; // output goes to frame buffer
		
	void main() {
		vec3 p1p2 = vec3(p2 - p1, 0.0f);
		vec3 p1p3 = vec3(p3 - p1, 0.0f);
		vec3 n = normalize(cross(p1p2, p1p3));

		vec3 p1p = vec3(vertexPos - p1, 0.0f);
		vec3 p2p = vec3(vertexPos - p2, 0.0f);
		vec3 p3p = vec3(vertexPos - p3, 0.0f);
		
		vec3 p2p3 = vec3(p3 - p2, 0.0f);
		vec3 p3p1 = vec3(p1 - p3, 0.0f);
		
		if (dot(cross(p1p2, p1p), n) < 0.0f ||
			dot(cross(p2p3, p2p), n) < 0.0f || 
			dot(cross(p3p1, p3p), n) < 0.0f)
		{
			fragmentColor = vec4(0, 0, 1, 1);
		}
		else {
			fragmentColor = vec4(1, 0, 0, 1);
		}
	}
)";

// pixel árnyaló (2. verzió)
//const char* fragSource = R"(
//	#version 330
//    precision highp float;
//
//	uniform vec2 p1, p2, p3; // háromszög három csúcsa
//
//	in vec2 vertexPos;
//	out vec4 fragmentColor; // output goes to frame buffer
//		
//	void main() {
//		vec2 n12 = p2 - p1; n12 = vec2(-n12.y, n12.x);
//		vec2 n23 = p3 - p2; n23 = vec2(-n23.y, n23.x);
//		vec2 n31 = p1 - p3; n31 = vec2(-n31.y, n31.x);
//
//		vec2 p1p3 = p3 - p1;
//		vec2 p2p1 = p1 - p2;
//		vec2 p3p2 = p2 - p3;
//
//		vec2 p1p = vertexPos - p1;
//		vec2 p2p = vertexPos - p2;
//		vec2 p3p = vertexPos - p3;
//		
//		if (dot(p1p, n12) * dot(p1p3, n12) < 0.0f ||
//			dot(p2p, n23) * dot(p2p1, n23) < 0.0f || 
//			dot(p3p, n31) * dot(p3p2, n31) < 0.0f)
//		{
//			fragmentColor = vec4(0, 0, 1, 1);
//		}
//		else {
//			fragmentColor = vec4(1, 0, 0, 1);
//		}
//	}
//)";

const int winWidth = 600, winHeight = 600;

class ShaderTriangle : public glApp {
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
	
	unsigned vao, vbo;

public:
	ShaderTriangle() : glApp("Shader triangle") {}
	~ShaderTriangle() { delete gpuProgram; glDeleteBuffers(1, &vbo); glDeleteVertexArrays(1, &vao); }

	// Inicializáció, 
	void onInitialization() {
		gpuProgram = new GPUProgram(vertSource, fragSource);
		gpuProgram->setUniform(vec2(-0.7f, -0.6f), "p1");
		gpuProgram->setUniform(vec2( 0.8f, -0.6f), "p2");
		gpuProgram->setUniform(vec2(-0.2f,  0.7f), "p3");

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

		float vtxPos[] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
		glBufferData(GL_ARRAY_BUFFER, sizeof(vtxPos), vtxPos, GL_STATIC_DRAW);
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
		glViewport(0, 0, winWidth, winHeight);

		gpuProgram->Use();
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
};

ShaderTriangle app;

