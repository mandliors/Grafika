//=============================================================================================
// Rasterization
//=============================================================================================
#include "framework.h"

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

class Rasterization : public glApp
{
	Framebuffer* fb;
	GPUProgram* gpuProgram;

	vec3 image[winWidth * winHeight];

public:
	Rasterization() : glApp("Rasterization") { }
	~Rasterization() { delete fb; delete gpuProgram; }

	// Inicializáció, 
	void onInitialization() 
	{
		memset(image, 0, sizeof(image));
		
		DrawLine(image, 250, 100, 100, 500, vec3(1.0f, 1.0f, 0.0f));
		DrawLine(image, 100, 250, 250, 250, vec3(0.0f, 1.0f, 1.0f));
		DrawLine(image, 150, 200, 150, 450, vec3(1.0f, 0.0f, 1.0f));

		DrawTriangle(image, 480, 560, 520, 400, 350, 500, vec3(1.0f, 0.0f, 0.0f));
		DrawTriangle(image, 550, 200, 580, 100, 500, 120, vec3(0.0f, 1.0f, 0.0f));
		DrawTriangle(image, 400, 550, 550, 450, 390, 300, vec3(0.0f, 0.0f, 1.0f));
		DrawTriangle(image, 310, 310, 580, 350, 500, 200, vec3(1.0f, 1.0f, 0.0f));
		DrawTriangle(image, 470, 480, 490, 430, 520, 520, vec3(1.0f, 0.0f, 1.0f));
		DrawTriangle(image, 360, 100, 580, 200, 400, 300, vec3(0.0f, 1.0f, 1.0f));


		fb = new Framebuffer(winWidth, winHeight, image);
		fb->Update(winWidth, winHeight, image);

		gpuProgram = new GPUProgram(vertSource, fragSource);
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		fb->Draw(gpuProgram);
	}

	void DrawLine(vec3* imageData, short x1, short y1, short x2, short y2, const vec3& color)
	{
		const int T = 12;

		// vertical line
		if (x1 == x2)
		{
			if (y2 < y1) std::swap(y1, y2);
			for (short y = y1; y <= y2; y++)
				imageData[y * winWidth + x1] = color;
			
			return;
		}

		if (x1 > x2)
		{
			std::swap(x1, x2);
			std::swap(y1, y2);
		}

		int m = ((y2 - y1) << T) / (x2 - x1);
		int y = (y1 << T) + (1 << (T - 1)); // +0.5
		for (short x = x1; x <= x2; x++) 
		{
			short Y = y >> T; // trunc
			imageData[Y * winWidth + x] = color;
			y = y + m;
		}
	}

	void DrawTriangle(vec3* imageData, short x1, short y1, short x2, short y2, short x3, short y3, const vec3& color)
	{
		// sort vertices by y coordinate (y1 <= y2 <= y3)
		if (y1 > y2) std::swap(y1, y2), std::swap(x1, x2);
		if (y1 > y3) std::swap(y1, y3), std::swap(x1, x3);
		if (y2 > y3) std::swap(y2, y3), std::swap(x2, x3);

		if (y1 == y3) return; // horizontal line (y1 = y2 = y3)

		// top triangle
		if (y2 > y1)
		{
			// mm is the reciprocal of the slope
			float mm12 = (x2 - x1) / (float)(y2 - y1);
			float mm13 = (x3 - x1) / (float)(y3 - y1);
			
			float xStart = x1, xEnd = x1;

			if (x2 < x1) std::swap(mm12, mm13);
			
			for (int y = y1; y <= y2; y++)
			{
				if (xStart > xEnd) std::swap(xStart, xEnd);
				for (int x = xStart; x <= xEnd; ++x)
					imageData[y * winWidth + x] = color;

				xStart += mm13;
				xEnd += mm12;
			}
		}

		// bottom triangle
		if (y3 > y2)
		{
			float mm13 = (x3 - x1) / (float)(y3 - y1);
			float mm23 = (x3 - x2) / (float)(y3 - y2);

			float xStart = x1 + mm13 * (y2 - y1);
			float xEnd = x2;
			
			if (x2 < x1) std::swap(mm13, mm23);

			for (int y = y2; y <= y3; y++)
			{
				if (xStart > xEnd) std::swap(xStart, xEnd);
				for (int x = xStart; x <= xEnd; ++x)
					imageData[y * winWidth + x] = color;

				xStart += mm13;
				xEnd += mm23;
			}
		}
	}

} app;

