//=============================================================================================
// Shader Texture Magic
//=============================================================================================
#include "framework.h"

const char* vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 vp;
	out vec2 uv;
	
	void main() {
		uv = (vp + vec2(1, 1)) / 2;
		gl_Position = vec4(vp, 0, 1);
	}
)";

const char* fragMagicLens = R"(
	#version 330
    precision highp float;

	uniform sampler2D samplerUnit;
	uniform vec2 uvc;
	uniform float time;

	in vec2 uv;
	out vec4 fragmentColor;
	
	void main() {
		const float r2 = 0.05f;
		float d2 = dot(uv - uvc, uv - uvc);
		vec2 tuv = (d2 < r2) ? (uv - uvc) * d2 / r2 + uvc : uv;
		tuv.y *= -1;

		fragmentColor = texture(samplerUnit, tuv);
		float dummy = time * 0.0;
	}
)";

const char* fragSwirl = R"(
	#version 330
    precision highp float;

	uniform sampler2D samplerUnit;
	uniform vec2 uvc;
	uniform float time;

	in vec2 uv;
	out vec4 fragmentColor;
	
	void main() {
		const float a = 8, alpha = 15;
		float ang = a * exp( -alpha * length(uv - uvc) );
		mat2 rotMat = mat2( cos(ang), sin(ang),
		-sin(ang), cos(ang) );
		vec2 tuv = (uv - uvc) * rotMat + uvc;
		tuv.y *= -1;

		fragmentColor = texture(samplerUnit, tuv);
		float dummy = time * 0.0;
	}
)";

const char* fragBlackHole = R"(
	#version 330
    precision highp float;

	uniform sampler2D samplerUnit;
	uniform vec2 uvc;
	uniform float time;

	in vec2 uv;
	out vec4 fragmentColor;
	
	void main() {
		const float r0 = 0.09f, ds = 0.001f;
		vec3 p = vec3(uv,0), dir = vec3(0,0,1), blackhole = vec3(uvc,0.5f);
		float r2 = dot(blackhole - p, blackhole - p);
		while (p.z < 1 && r2 > r0 * r0) {
			p += dir * ds;
			r2 = dot(blackhole - p, blackhole - p);
			vec3 gDir = (blackhole - p)/sqrt(r2); // gravity direction
			dir = normalize(dir * ds + gDir * r0 / r2 / 4 * ds * ds);
		}
		if (p.z >= 1) fragmentColor = texture(samplerUnit, vec2(p.x,-p.y));
		else fragmentColor = vec4(0, 0, 0, 1);
		float dummy = time * 0.0;
	}
)";

const char* fragWave = R"(
	#version 330
    precision highp float;

	uniform sampler2D samplerUnit;
	uniform vec2 uvc;
	uniform float time;

	const float PI = 3.14159265, n = 1.33, c = 0.1, aMax = 0.1, waveWidth = 0.03, waterDepth = 1; 

	in vec2 uv;
	out vec4 fragmentColor;
	
	void main() {
		float d = length(uv - uvc), waveDist = c * time;
		if (abs(d - waveDist) < waveWidth) {
			float angIn = aMax/waveDist * sin((waveDist-d)/waveWidth*PI);
			float angRefr = asin(sin(angIn)/n);
			vec2 dir = (uv - uvc)/d;
			vec2 tuv = uv + dir * tan(angIn - angRefr) * waterDepth;
			fragmentColor = texture(samplerUnit, tuv * vec2(1, -1));
		} else {
			fragmentColor = texture(samplerUnit, uv * vec2(1, -1));
		}
	}
)";

const int winWidth = 600, winHeight = 600;

class Texture2D
{
	unsigned int vao, vbo;
	unsigned int textureId = 0;

public:
	Texture2D(const fs::path pathname, bool transparent = false, int sampling = GL_LINEAR) {
		if (textureId == 0) glGenTextures(1, &textureId);  				// azonosító generálás
		glBindTexture(GL_TEXTURE_2D, textureId);    // kötés
		unsigned int width, height;
		unsigned char* pixels;
		if (transparent) {
			lodepng_decode32_file(&pixels, &width, &height, pathname.string().c_str());
			for (int y = 0; y < height; ++y) {
				for (int x = 0; x < width; ++x) {
					float sum = 0;
					for (int c = 0; c < 3; ++c) {
						sum += pixels[4 * (x + y * width) + c];
					}
					pixels[4 * (x + y * width) + 3] = sum / 6;
				}
			}

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels); // GPU-ra
		}
		else {
			lodepng_decode24_file(&pixels, &width, &height, pathname.string().c_str());
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels); // GPU-ra
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampling); // szûrés
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampling);
		printf("%s, w: %d, h: %d\n", pathname.string().c_str(), width, height);

		// setup VAO
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// setup VBOs
		glGenBuffers(1, &vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		float vtxs[] = { -1, -1, 1, -1, 1, 1, -1, 1 };
		glBufferData(GL_ARRAY_BUFFER, sizeof(vtxs), vtxs, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
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
	~Texture2D()
	{
		if (textureId > 0)
			glDeleteTextures(1, &textureId);
	}
};

class ShaderTextureMagic : public glApp
{
	Texture2D* texture;

	static const int programCount = 4;
	int programIdx = 0;
	GPUProgram* gpuPrograms[programCount];

	const float maxTime = 3.0f;
	float time = -1.0f;

public:
	ShaderTextureMagic() : glApp("Shader Texture Magic") {}
	~ShaderTextureMagic() { delete texture; for (const auto* p : gpuPrograms) delete p; }

	// Inicializáció, 
	void onInitialization()
	{
		texture = new Texture2D("image.png");

		gpuPrograms[0] = new GPUProgram(vertSource, fragMagicLens);
		gpuPrograms[1] = new GPUProgram(vertSource, fragSwirl);
		gpuPrograms[2] = new GPUProgram(vertSource, fragBlackHole);
		gpuPrograms[3] = new GPUProgram(vertSource, fragWave);
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		gpuPrograms[programIdx]->Use();
		texture->Draw(gpuPrograms[programIdx]);
	}

	void onMouseMotion(int pX, int pY)
	{
		gpuPrograms[programIdx]->setUniform(ScreenToUV(pX, pY), "uvc");
	}

	void onMousePressed(MouseButton but, int pX, int pY)
	{
		if (but == MOUSE_LEFT)
			programIdx = (programIdx + 1) % programCount;
		else
		{
			time = 0.0f;
			gpuPrograms[programIdx]->setUniform(time, "time");
		}
	}

	void onTimeElapsed(float startTime, float endTime)
	{
		if (-0.5f < time && time < maxTime)
			time += endTime - startTime;
		else
			time = -1.0f;
		
		gpuPrograms[programIdx]->setUniform(time, "time");
		refreshScreen();
	}

	vec2 ScreenToUV(int x, int y)
	{
		return {
			x / (float)winWidth,
			1.0f - y / (float)winHeight
		};
	}

} app;

