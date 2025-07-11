#include "framework.h"

#include <iostream>
#include <list>

const char* vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 vtxPos;
	layout(location = 1) in vec2 vtxUV;
	out vec2 texcoord;
	
	void main() {
		gl_Position = vec4(vtxPos, 0, 1);
		texcoord = vtxUV;
	}
)";

const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform sampler2D samplerUnit;

	in vec2 texcoord;
	out vec4 fragmentColor;
	
	void main() {
		fragmentColor = texture(samplerUnit, texcoord);
	}
)";

const int winWidth = 600, winHeight = 600;

class Framebuffer
{
	unsigned int vao, vbo[2];
	unsigned int textureId = 0;

public:
	Framebuffer(int width, int height, vec3* data)
	{
		// setup VAO
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// setup VBOs
		glGenBuffers(2, vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		float vtxs[] = { -1, -1, 1, -1, 1, 1, -1, 1 };
		glBufferData(GL_ARRAY_BUFFER, sizeof(vtxs), vtxs, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		float uvs[] = { 0, 1, 1, 1, 1, 0, 0, 0 };
		glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);

		// setup texture
		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_FLOAT, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	void Update(int width, int height, vec3* data)
	{
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_FLOAT, data);
	}
	void Bind(int textureUnit)
	{
		glActiveTexture(GL_TEXTURE0 + textureUnit);
		glBindTexture(GL_TEXTURE_2D, textureId);
	}
	void Draw(GPUProgram* gpuProgram)
	{
		static const int sampler = 0;

		gpuProgram->setUniform(sampler, "samplerUnit");
		glActiveTexture(GL_TEXTURE0 + sampler);

		glBindTexture(GL_TEXTURE_2D, textureId);
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
	~Framebuffer()
	{
		if (textureId > 0)
			glDeleteTextures(1, &textureId);
	}
};

struct Object {
	vec3 color;
	Object(vec3 c) : color(c) { }
	virtual bool In(vec2 r) = 0;
};
struct Circle : Object {
	vec2 center;
	float R;
	Circle(vec3 c, vec2 cen, float r) : Object(c), center(cen), R(r) {}
	bool In(vec2 r) { return (dot(r - center, r - center) < R * R); }
};
struct HalfPlane : Object {
	vec2 r0, n;
	HalfPlane(vec3 c, vec2 r, vec2 n) : Object(c), r0(r), n(normalize(n)) {}
	bool In(vec2 r) { return (dot(r - r0, n) < 0); }
};
struct GeneralEllipse : Object {
	vec2 f1, f2;
	float C;
	GeneralEllipse(vec3 c, vec2 f1, vec2 f2, float C) : Object(c), f1(f1), f2(f2), C(C) {}
	bool In(vec2 r) { return (length(r - f1) + length(r - f2) < C); }
};
struct Parabola : Object {
	vec2 f, r0, n;
	Parabola(vec3 c, vec2 f, vec2 r0, vec2 n) : Object(c), f(f), r0(r0), n(normalize(n)) {}
	bool In(vec2 r) { return (fabs(dot(r - r0, n)) > length(r - f)); }
};


class PixelDrivenRendering : public glApp
{
	Framebuffer* fb;
	GPUProgram* gpuProgram;

	vec3 image[winHeight * winWidth];

	std::list<Object*> objs;
	Object* picked = nullptr;

public:
	PixelDrivenRendering() : glApp("Pixel Driven Rendering") {}
	~PixelDrivenRendering() { delete fb; delete gpuProgram; for (Object* o : objs) delete o; }

	void onInitialization()
	{
		memset(image, 0, sizeof(image));

		fb = new Framebuffer(winWidth, winHeight, image);
		gpuProgram = new GPUProgram(vertSource, fragSource);

		objs.push_back(new HalfPlane{ {0, 1, 0}, {-0.2f, -0.5f}, {3, 1} });
		objs.push_back(new Parabola{ {1, 1, 0}, {0.5f, -0.5f}, {-0.5f, -0.5f}, {1, 0} });
		objs.push_back(new Circle{ {1, 0, 0}, {0.5f, 0.5f}, 0.3f });
		objs.push_back(new GeneralEllipse{ {0, 0, 1}, {-0.6f, -0.4f}, {0.2f, -0.1f}, 1.1f });

		Render();
		refreshScreen();
	}

	void onDisplay() override
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		fb->Draw(gpuProgram);
	}

	void Add(Object* o)
	{
		objs.push_front(o);
		picked = o;
	}
	void Pick(int pX, int pY)
	{
		vec2 wPoint = Viewport2Window(pX, pY);
		picked = nullptr;
		for (auto o : objs)
		{
			if (o->In(wPoint))
			{
				picked = o;
				return;
			}
		}
	}
	void BringToFront()
	{
		if (picked)
		{
			objs.erase(find(objs.begin(), objs.end(), picked));
			objs.push_front(picked);
		}
	}
	void Render()
	{
		for (int pX = 0; pX < winWidth; pX++) for (int pY = 0; pY < winHeight; pY++)
		{
			vec2 wPoint = Viewport2Window(pX, pY);
			for (auto o : objs)
			{
				if (o->In(wPoint))
				{
					image[pY * winWidth + pX] = o->color;
					break;
				}
			}
		}
		fb->Update(winWidth, winHeight, image);
	}

	void onMousePressed(MouseButton but, int pX, int pY)
	{
		Pick(pX, pY);
		BringToFront();
		Render();
		refreshScreen();
	}


	vec2 Viewport2Window(int x, int y)
	{
		return {
			x / (float)winWidth * 2.0f - 1.0f,
			1.0f - y / (float)winHeight * 2.0f
		};
	}

} app;