//=============================================================================================
// Perlin Noise 3D
//=============================================================================================
#include "framework.h"

#include <random>

// Phong shader (per-pixel shading)
const char* vertSourcePhong = R"(
	#version 330				
    precision highp float;

	uniform mat4 MVP, M, Minv; // MVP, Model, Model-inverse
	uniform vec4 wLiPos; // pos of light source
	uniform vec3 wEye; // pos of eye
	
	layout(location = 0) in vec3 vtxPos; // pos in model sp
	layout(location = 1) in vec3 vtxNorm;// normal in model sp
	
	out vec3 wNormal; // normal in world space
	out vec3 wView; // view in world space
	out vec3 wLight; // light dir in world space
	
	void main() {
		gl_Position = MVP * vec4(vtxPos, 1); // to NDC
		vec4 wPos = M * vec4(vtxPos, 1);
		wLight = wLiPos.xyz * wPos.w - wPos.xyz * wLiPos.w;
		wView = wEye - wPos.xyz / wPos.w;
		wNormal = (vec4(vtxNorm, 0) * Minv).xyz;
	}
)";
const char* fragSourcePhong = R"(
	#version 330
    precision highp float;

	uniform vec3 kd, ks, ka;// diffuse, specular, ambient ref
	uniform float shine; // shininess for specular ref
	uniform vec3 La, Le; // ambient and dir/point source rad

	in vec3 wNormal; // interpolated world sp normal
	in vec3 wView; // interpolated world sp view
	in vec3 wLight; // interpolated world sp illum dir
	
	out vec4 fragmentColor; // output goes to frame buffer
	
	void main() {
		vec3 N = normalize(wNormal);
		vec3 V = normalize(wView);
		if (dot(N, V) < 0) N = -N;

		vec3 L = normalize(wLight);
		vec3 H = normalize(L + V);
		float cost = max(dot(N,L), 0), cosd = max(dot(N,H), 0);
		fragmentColor = vec4(ka * La + (kd * cost + ks * pow(cosd,shine)) * Le, 1);
	}
)";

// Texture shader
const char* vertSourceTexture = R"(
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

const char* fragSourceTexture = R"(
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

const int tessellationLevel = 50;
const float eps = 0.001f;

const int winWidth = 600, winHeight = 600;

struct Camera
{
	vec3 wEye, wLookat, wVup; // külsö paraméterek
	float fov, asp, fp, bp; // belsö paraméterek
public:
	Camera(vec3 wEye, vec3 wLookat, vec3 wVup)
		: wEye(wEye), wLookat(wLookat), wVup(wVup)
	{
		asp = (float)winWidth / winHeight; // aspektus arány
		fov = 40.0f * (float)M_PI / 180.0f; // függöleges látószög
		fp = 1; bp = 20; // elsö és hátsó vágósík távolság
	}
	// nézeti transzformáció
	mat4 V() { return lookAt(wEye, wLookat, wVup); }
	// perspektív transzformáció
	mat4 P() { return perspective(fov, asp, fp, bp); }
};

struct Light {
	vec3 La, Le;
	vec4 wLightPos;
};
struct Material
{
	vec3 ka, kd, ks;
	float shine;
};

struct VertexData
{
	vec3 position, normal;
};

//---------------------------
class Terrain : public Geometry<VertexData>
{
	int nVtxPerStrip, nStrips;

public:
	float rotAngle;
	vec3 translation, rotAxis, scaling;

	Material material;

	Terrain(vec3 position, Material material)
		: translation(position), rotAxis(1.0f), rotAngle(0.0f), scaling(1.0f), material(material)
	{
		nVtxPerStrip = nStrips = 0;
	}
	~Terrain() { }

	vec3 evalPoint(float u, float v, float* heightMap, float w, float h)
	{
		int x = u * (w - 1);
		int y = v * (h - 1);
		return {
			u * 2.0f - 1.0f,
			heightMap[(int)(y * w) + x] * 0.2f,
			v * 2.0f - 1.0f
		};
	}
	vec3 evalNormal(float u, float v, float* heightMap, float w, float h)
	{
		vec3 du = evalPoint(u + eps, v, heightMap, w, h) - evalPoint(u - eps, v, heightMap, w, h);
		vec3 dv = evalPoint(u, v + eps, heightMap, w, h) - evalPoint(u, v - eps, heightMap, w, h);
		return normalize(cross(du, dv));
	}

	VertexData GenVertexData(float u, float v, float* heightMap, float w, float h) { return { evalPoint(u, v, heightMap, w, h), evalNormal(u, v, heightMap, w, h) }; }
	void Update(float* heightMap, int w, int h, int N = tessellationLevel, int M = tessellationLevel)
	{
		nVtxPerStrip = (M + 1) * 2;
		nStrips = N;
		vtx.clear();
		for (int i = 0; i < N; i++) {
			for (int j = 0; j <= M; j++) {
				vtx.push_back(GenVertexData((float)j / M, (float)i / N, heightMap, w, h));
				vtx.push_back(GenVertexData((float)j / M, (float)(i + 1) / N, heightMap, w, h));
			}
		}
		updateGPU();
	}

	mat4 M() { return translate(translation) * rotate(rotAngle, rotAxis) * scale(scaling); }

	void Draw(Camera* camera, GPUProgram* shader)
	{
		// transformation matrices
		mat4 M = translate(translation) * rotate(rotAngle, rotAxis) * scale(scaling);
		mat4 Minv = scale(1.0f / scaling) * rotate(-rotAngle, rotAxis) * translate(-translation);
		mat4 MVP = camera->P() * camera->V() * M;

		shader->setUniform(M, "M");
		shader->setUniform(Minv, "Minv");
		shader->setUniform(MVP, "MVP");

		// material properties
		shader->setUniform(material.kd, "kd");
		shader->setUniform(material.ks, "ks");
		shader->setUniform(material.ka, "ka");
		shader->setUniform(material.shine, "shine");

		Bind();
		glEnableVertexAttribArray(0);  // attribute array 0 = POSITION
		glEnableVertexAttribArray(1);  // attribute array 1 = NORMAL
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, position));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));
		for (unsigned int i = 0; i < nStrips; i++)
			glDrawArrays(GL_TRIANGLE_STRIP, i * nVtxPerStrip, nVtxPerStrip);
	}
};

class OneComponentTexture
{
	unsigned int vao, vbo[2];
	unsigned int textureId = 0;

public:
	OneComponentTexture(int width, int height)
	{
		// setup VAO
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// setup VBOs
		glGenBuffers(2, vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		float s = 0.25f, dx = 0.7f, dy = dx;
		float vtxs[] = { dx - s, dy - s, dx + s, dy - s, dx + s, dy + s, dx - s, dy + s };
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

class PerlinNoise3D
{
	int gridSize;
	std::vector<vec3> gradients;

public:
	PerlinNoise3D(int gridSize)
		: gridSize(gridSize)
	{
		int gradCount = gridSize + 1;
		int gradDim = gridSize + 1;
		gradients.resize(gradDim * gradDim * gradDim);
		for (int z = 0; z < gradDim; z++)
			for (int y = 0; y < gradDim; y++)
				for (int x = 0; x < gradDim; x++)
					gradients[(z * gradDim + y) * gradDim + x] = RandomUnitVector3D();
	}
	float Get(float x, float y, float z)
	{
		int x0 = x / gridSize, y0 = y / gridSize, z0 = z / gridSize; // top left corner of the grid cell
		float dx = x / (float)gridSize - x0, dy = y / (float)gridSize - y0, dz = z / (float)gridSize - z0; // offset in the grid cell

		// calculate u, v and w for the interpolation
		float u = Fade(dx), v = Fade(dy), w = Fade(dz);

		// helper function to retrieve gradients
		auto gradient = [&](int i, int j, int k) -> vec3& {
			int gradDim = gridSize + 1;
			return gradients[((k % gradDim) * gradDim + (j % gradDim)) * gradDim + (i % gradDim)];
		};

		// retrieve the gradients in the corners of the grid cell
		vec3& g000 = gradient(x0, y0, z0);
		vec3& g100 = gradient(x0 + 1, y0, z0);
		vec3& g010 = gradient(x0, y0 + 1, z0);
		vec3& g110 = gradient(x0 + 1, y0 + 1, z0);

		vec3& g001 = gradient(x0, y0, z0 + 1);
		vec3& g101 = gradient(x0 + 1, y0, z0 + 1);
		vec3& g011 = gradient(x0, y0 + 1, z0 + 1);
		vec3& g111 = gradient(x0 + 1, y0 + 1, z0 + 1);

		// calculate the dot products for the interpolation
		float dot000 = dot(g000, vec3(dx, dy, dz));
		float dot100 = dot(g100, vec3(dx - 1, dy, dz));
		float dot010 = dot(g010, vec3(dx, dy - 1, dz));
		float dot110 = dot(g110, vec3(dx - 1, dy - 1, dz));

		float dot001 = dot(g001, vec3(dx, dy, dz - 1));
		float dot101 = dot(g101, vec3(dx - 1, dy, dz - 1));
		float dot011 = dot(g011, vec3(dx, dy - 1, dz - 1));
		float dot111 = dot(g111, vec3(dx - 1, dy - 1, dz - 1));
		
		// interpolate between the dot products based on u and v
		float x00i = Lerp(dot000, dot100, u);
		float x10i = Lerp(dot010, dot110, u);
		float x01i = Lerp(dot001, dot101, u);
		float x11i = Lerp(dot011, dot111, u);

		float y0i = Lerp(x00i, x10i, v);
		float y1i = Lerp(x01i, x11i, v);

		// interpolate based on w
		return Lerp(y0i, y1i, w);
	}

private:
	vec3 RandomUnitVector3D()
	{
		float theta = RandFloat() * 2 * M_PI;
		float phi = acosf(2 * RandFloat() - 1);
		return vec3(
			sin(phi) * cos(theta),
			sin(phi) * sin(theta),
			cos(phi)
		);
	}
	float RandFloat() { return rand() / (float)RAND_MAX; }
	float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
	float Lerp(float a, float b, float t) { return a + (b - a) * t; }
};

class PerlinNoiseApp : public glApp
{
	Camera* camera;
	Material material;
	Light light;

	Terrain* terrain;

	PerlinNoise3D* perlin;
	float* mapData = new float[winWidth * winHeight];
	float t = 0.0f;
	OneComponentTexture* map;

	bool wireframe = false;

	GPUProgram* phongShader;
	GPUProgram* textureShader;

public:
	PerlinNoiseApp() : glApp("Perlin Noise 3D") {}
	~PerlinNoiseApp() { delete camera; delete terrain; delete perlin; delete[] mapData; delete map;  delete phongShader; delete textureShader; }

	// Inicializáció, 
	void onInitialization()
	{
		material = Material{ vec3(0.15f, 0.6f, 0.9f), vec3(0.05f, 0.2f, 0.3f), vec3(0.8f), 60.0f };
		light = Light{ vec3(0.4f), vec3(1.0f), vec4(1.0f, 1.0f, 1.0f, 0.0f) };

		camera = new Camera(vec3(0.0f, 2.0f, 3.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));

		terrain = new Terrain(vec3(0.0f), material);

		srand(time(NULL));
		perlin = new PerlinNoise3D(80);

		map = new OneComponentTexture(winWidth, winHeight);
		UpdateMapData();

		phongShader = new GPUProgram(vertSourcePhong, fragSourcePhong);
		textureShader = new GPUProgram(vertSourceTexture, fragSourceTexture);
		
		glEnable(GL_DEPTH_TEST);
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		phongShader->Use();
		phongShader->setUniform(light.La, "La");
		phongShader->setUniform(light.Le, "Le");
		phongShader->setUniform(light.wLightPos, "wLiPos");
		phongShader->setUniform(camera->wEye, "wEye");
		terrain->Draw(camera, phongShader);

		textureShader->Use();
		map->Draw(textureShader);
	}

	void onTimeElapsed(float ts, float te)
	{
		const float timeSpeed = 50.0f;
		t += (te - ts) * timeSpeed;
		
		UpdateMapData();
		terrain->Update(mapData, winWidth, winHeight);

		refreshScreen();
	}

	void onMousePressed(MouseButton but, int pX, int pY)
	{
		wireframe = !wireframe;
		glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
		refreshScreen();
	}

private:
	void UpdateMapData()
	{
		// calculate values
		float min = 1.0f, max = -1.0f;
		for (int y = 0; y < winHeight; y++)
		{
			for (int x = 0; x < winWidth; x++)
			{
				float v = perlin->Get(x, y, t);
				if (v < min) min = v;
				else if (v > max) max = v;
				mapData[y * winWidth + x] = v;
			}
		}
		//normalize values
		float d = max - min;
		for (int i = 0; i < winWidth * winHeight; i++)
			mapData[i] = (mapData[i] - min) / d;

		map->Update(winWidth, winHeight, mapData);
	}

} app;

