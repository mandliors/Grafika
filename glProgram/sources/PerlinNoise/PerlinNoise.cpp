//=============================================================================================
// Perlin Noise
//=============================================================================================
#include "framework.h"

#include <random>

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
		float v = texture(samplerUnit, texcoord).r;
		fragmentColor = vec4(v, v, v, 1);
	}
)";

const int winWidth = 600, winHeight = 600;

class OneComponentTexture
{
	unsigned int vao, vbo[2];
	unsigned int textureId = 0;

public:
	OneComponentTexture(int width, int height, float* data)
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
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	void Update(int width, int height, float* data)
	{
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, data);
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
	~OneComponentTexture()
	{
		if (textureId > 0)
			glDeleteTextures(1, &textureId);
	}
};

class PerlinNoise 
{
	int gridSize;
	std::vector<vec2> gradients;

public:
	PerlinNoise(int gridSize)
		: gridSize(gridSize)
	{
		int gradCount = gridSize + 1;
		gradients.resize(gradCount * gradCount);
		for (int j = 0; j < gradCount; ++j)
			for (int i = 0; i < gradCount; ++i)
				gradients[j * gradCount + i] = RandomUnitVector2D();
	}
	float Get(float x, float y)
	{
		int x0 = x / gridSize, y0 = y / gridSize; // top left corner of the grid cell
		float dx = x / (float)gridSize - x0, dy = y / (float)gridSize - y0; // offset in the grid cell
		
		// retrieve the gradients in the corners of the grid cell
		int gradCount = gridSize + 1;
		vec2& g00 = gradients[y0 * gradCount + x0];
		vec2& g10 = gradients[y0 * gradCount + (x0 + 1)];
		vec2& g01 = gradients[(y0 + 1) * gradCount + x0];
		vec2& g11 = gradients[(y0 + 1) * gradCount + (x0 + 1)];

		// calculate u and v for the interpolation
		float u = Fade(dx), v = Fade(dy);

		// calculate the dot products for the interpolation
		float dot00 = dot(g00, vec2(dx, dy)), dot10 = dot(g10, vec2(dx - 1.0f, dy));
		float dot01 = dot(g01, vec2(dx, dy - 1.0f)), dot11 = dot(g11, vec2(dx - 1.0f, dy - 1.0f));

		// interpolate between the dot products based on u and v
		return Lerp(Lerp(dot00, dot10, u), Lerp(dot01, dot11, u), v);
	}

private:
	vec2 RandomUnitVector2D() { float angle = RandFloat() * 2 * M_PI; return { cosf(angle), sinf(angle) }; }
	float RandFloat() { return rand() / (float)RAND_MAX; }
	float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
	float Lerp(float a, float b, float t) { return a + (b - a) * t; }
};

class PerlinNoiseApp : public glApp
{
	PerlinNoise* perlin;
	float* mapData = new float[winWidth * winHeight];
	OneComponentTexture* map;

	GPUProgram* gpuProgram;

public:
	PerlinNoiseApp() : glApp("Perlin Noise") {}
	~PerlinNoiseApp() { delete perlin; delete[] mapData; delete map; delete gpuProgram; }

	// Inicializáció, 
	void onInitialization()
	{
		srand(time(NULL));
		perlin = new PerlinNoise(40);

		// calculate values
		float min = 1.0f, max = -1.0f;
		for (int y = 0; y < winHeight; y++)
		{
			for (int x = 0; x < winWidth; x++)
			{
				float v = perlin->Get(x, y);
				if (v < min) min = v;
				else if (v > max) max = v;
				mapData[y * winWidth + x] = v;
			}
		}
		//normalize values
		float d = max - min;
		for (int i = 0; i < winWidth * winHeight; i++)
			mapData[i] = (mapData[i] - min) / d;

		map = new OneComponentTexture(winWidth,  winHeight, mapData);

		gpuProgram = new GPUProgram(vertSource, fragSource);
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		map->Draw(gpuProgram);
	}

} app;

