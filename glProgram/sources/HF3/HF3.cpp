#include "framework.h"
#include <iostream>

const vec3 colors[4]{
	{ 1.0f, 1.0f, 1.0f },
	{ 0.0f, 0.0f, 1.0f },
	{ 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f }
};

const std::vector<unsigned> imageCompressed {
	252, 252, 252, 252, 252, 252, 252, 252, 252, 0, 9, 80, 1, 148, 13, 72, 13, 140, 25, 60, 21, 132, 41, 12, 1, 28,
25, 128, 61, 0, 17, 4, 29, 124, 81, 8, 37, 116, 89, 0, 69, 16, 5, 48, 97, 0, 77, 0, 25, 8, 1, 8, 253, 253, 253, 253,
101, 10, 237, 14, 237, 14, 241, 10, 141, 2, 93, 14, 121, 2, 5, 6, 93, 14, 49, 6, 57, 26, 89, 18, 41, 10, 57, 26,
89, 18, 41, 14, 1, 2, 45, 26, 89, 26, 33, 18, 57, 14, 93, 26, 33, 18, 57, 10, 93, 18, 5, 2, 33, 18, 41, 2, 5, 2, 5, 6,
89, 22, 29, 2, 1, 22, 37, 2, 1, 6, 1, 2, 97, 22, 29, 38, 45, 2, 97, 10, 1, 2, 37, 42, 17, 2, 13, 2, 5, 2, 89, 10, 49,
46, 25, 10, 101, 2, 5, 6, 37, 50, 9, 30, 89, 10, 9, 2, 37, 50, 5, 38, 81, 26, 45, 22, 17, 54, 77, 30, 41, 22, 17, 58,
1, 2, 61, 38, 65, 2, 9, 58, 69, 46, 37, 6, 1, 10, 9, 62, 65, 38, 5, 2, 33, 102, 57, 54, 33, 102, 57, 30, 1, 14, 33, 2,
9, 86, 9, 2, 21, 6, 13, 26, 5, 6, 53, 94, 29, 26, 1, 22, 29, 0, 29, 98, 5, 14, 9, 46, 1, 2, 5, 6, 5, 2, 0, 13, 0, 13,
118, 1, 2, 1, 42, 1, 4, 5, 6, 5, 2, 4, 33, 78, 1, 6, 1, 6, 1, 10, 5, 34, 1, 20, 2, 9, 2, 12, 25, 14, 5, 30, 1, 54, 13, 6,
9, 2, 1, 32, 13, 8, 37, 2, 13, 2, 1, 70, 49, 28, 13, 16, 53, 2, 1, 46, 1, 2, 1, 2, 53, 28, 17, 16, 57, 14, 1, 18, 1, 14,
1, 2, 57, 24, 13, 20, 57, 0, 2, 1, 2, 17, 0, 17, 2, 61, 0, 5, 16, 1, 28, 25, 0, 41, 2, 117, 56, 25, 0, 33, 2, 1, 2, 117,
52, 201, 48, 77, 0, 121, 40, 1, 0, 205, 8, 1, 0, 1, 12, 213, 4, 13, 12, 253, 253, 253, 141
};

const char* vertSource = R"(
	#version 330				
    precision highp float;

	layout(location = 0) in vec2 vtxPos;
	layout(location = 1) in vec2 vtxUV;
	out vec2 texcoord;
	out vec2 ndc;
	
	void main() {
		gl_Position = vec4(vtxPos, 0, 1);
		ndc = vtxPos;
		texcoord = vtxUV;
	}
)";

const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform sampler2D samplerUnit;
	uniform vec3 color;
	uniform bool useColor;
	uniform vec3 sun;

	in vec2 texcoord;
	in vec2 ndc;
	out vec4 fragmentColor;
	
	const float PI = 3.14159265359;
    const float phiMax = 85.0 * PI / 180.0;
    const float yMax = log(tan(PI / 4.0 + phiMax / 2.0));

	vec2 NDCToSpherical(vec2 ndc) {
        float phi = ndc.x * PI;
        float theta = 2.0 * (atan(exp(ndc.y * yMax)) - PI / 4.0);
        return vec2(phi, theta);
	}
	vec3 SphericalToWorld(vec2 spherical) {
		return vec3(
			cos(spherical.y) * cos(spherical.x),
			cos(spherical.y) * sin(spherical.x),
			sin(spherical.y)
		);
	}
	void main() {
		vec2 spherical = NDCToSpherical(ndc);
		vec3 world = SphericalToWorld(spherical);
        float brightness = dot(world, sun) < 0.0 ? 1.0 : 0.5;
		
		if (useColor)
			fragmentColor = vec4(color, 1.0);
		else
			fragmentColor = brightness * texture(samplerUnit, texcoord);
	}
)";

const int winWidth = 600, winHeight = 600;
const float R = 40000.0f / 2.0f / M_PI;
const float phiMax = 85.0f * M_PI / 180.0f;
const float yMax = logf(tanf(M_PI / 4.0f + phiMax / 2.0f));

vec2 ScreenToNDC(const vec2& screen)
{
	return {
		screen.x * 2.0f / winWidth - 1.0f,
		1.0f - screen.y * 2.0f / winHeight
	};
}
vec2 SphericalToNDC(const vec2& spherical)
{
	return {
		spherical.x / M_PI,
		logf(tanf(M_PI / 4.0f + spherical.y / 2.0f)) / yMax
	};
}
vec2 NDCToSpherical(const vec2& ndc)	
{
	return {
		ndc.x * M_PI,
		2.0f * (atanf(expf(ndc.y * yMax)) - M_PI / 4.0f)
	};
}
vec3 SphericalToWorld(const vec2& spherical)
{
	return {
		cosf(spherical.y) * cosf(spherical.x),
		cosf(spherical.y) * sinf(spherical.x),
		sinf(spherical.y)
	};
}

class MercatorMap
{
	unsigned int vao, vbo[2];
	unsigned int textureId = 0;

public:
	MercatorMap(int width, int height, std::vector<vec3>& data)
	{
		// setup VAO
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// setup VBOs
		glGenBuffers(2, vbo);
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		float vtxs[] = { 
			-1, 1, 1, 1, -1, -1, // left triangle
			-1, -1, 1, 1, 1, -1  // right triangle
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(vtxs), vtxs, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		float uvs[] = {
			0, 1, 1, 1, 0, 0, // left triangle
			0, 0, 1, 1, 1, 0  // right triangle
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);

		// setup texture
		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
			GL_RGB, GL_FLOAT, &data[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	void Bind(int textureUnit)
	{
		glActiveTexture(GL_TEXTURE0 + textureUnit);
		glBindTexture(GL_TEXTURE_2D, textureId);
	}
	void Draw(GPUProgram* gpuProgram)
	{
		static const int sampler = 0;

		gpuProgram->setUniform(false, "useColor");
		gpuProgram->setUniform(sampler, "samplerUnit");
		glActiveTexture(GL_TEXTURE0 + sampler);
		
		glBindTexture(GL_TEXTURE_2D, textureId);
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
	~MercatorMap()
	{
		if (textureId > 0)
			glDeleteTextures(1, &textureId);
	}
};

class Route
{
	Geometry<vec2>* stations;
	Geometry<vec2>* routePoints;

public:
	Route()
		: stations(new Geometry<vec2>()), routePoints(new Geometry<vec2>()) {
	}

	void AddStation(vec2 station)
	{
		static const int routeSegmentPoints = 100;

		if (!stations->Vtx().empty())
		{
			vec2 previousStationSpherical = NDCToSpherical(stations->Vtx().back());
			vec2 currentStationSpherical = NDCToSpherical(station);

			// 3D Descartes coordinates
			vec3 p1 = SphericalToWorld(previousStationSpherical);
			vec3 p2 = SphericalToWorld(currentStationSpherical);

			// calculate angle + print distance
			float dot = p1.x * p2.x + p1.y * p2.y + p1.z * p2.z;
			float sigma = acosf(dot);
			std::cout << "Distance: " <<  std::roundf(R * sigma) << " km" << std::endl;

			// slerp + convert back to spherical coordinates
			for (int i = 1; i <= routeSegmentPoints; i++)
			{
				float t = (float)i / routeSegmentPoints;

				float a = sinf((1.0f - t) * sigma) / sinf(sigma);
				float b = sinf(t * sigma) / sinf(sigma);
				vec3 p = a * p1 + b * p2;

				float theta = asinf(p.z);
				float phi = atan2f(p.y, p.x);

				vec2 pointSpherical = { phi, theta };
				routePoints->Vtx().push_back(SphericalToNDC(pointSpherical));
			}
		}
		else
			routePoints->Vtx().push_back(station);

		stations->Vtx().push_back(station);

		routePoints->updateGPU();
		stations->updateGPU();
	}
	void Render(GPUProgram* gpuProgram)
	{
		gpuProgram->setUniform(true, "useColor");

		routePoints->Draw(gpuProgram, GL_LINE_STRIP, vec3{ 1.0f, 1.0f, 0.0f });
		stations->Draw(gpuProgram, GL_POINTS, vec3{ 1.0f, 0.0f, 0.0f });
	}

	~Route() { delete stations; delete routePoints; }
};

class HF3 : public glApp {
	MercatorMap* map;
	Route* route;
	GPUProgram* gpuProgram;
	float time;

public:
	HF3() : glApp("HF3"), time(0.0f) {}
	~HF3() { delete map; delete gpuProgram; }

	void onInitialization()
	{
		glPointSize(10);
		glLineWidth(3);

		std::vector<vec3> imageUncompressed;
		imageUncompressed.reserve(winWidth * winHeight);
		_DecompressImage(imageCompressed, imageUncompressed);

		map = new MercatorMap(64, 64, imageUncompressed);
		route = new Route();
		gpuProgram = new GPUProgram(vertSource, fragSource);

		refreshScreen();
	}

	void onDisplay() override
	{
		glClear(GL_COLOR_BUFFER_BIT);

		float delta = -23.0f * M_PI / 180.0f;
		float sunLon = time * 15.0f * M_PI / 180.0f;
		vec3 sun = { cosf(delta) * cosf(sunLon), cosf(delta) * sinf(sunLon), sinf(delta) };
		gpuProgram->setUniform(sun, "sun");

		map->Draw(gpuProgram);
		route->Render(gpuProgram);
	}

	void onKeyboard(int key) override
	{
		if (key == 'n') {
			time += 1.0f;
			refreshScreen();
		}
	}

	virtual void onMousePressed(MouseButton but, int pX, int pY)
	{
		if (but != MouseButton::MOUSE_LEFT)
			return;

		vec2 world = ScreenToNDC({ (float)pX, (float)pY });
		route->AddStation(world);

		refreshScreen();
	}

private:
	void _DecompressImage(const std::vector<unsigned>& in, std::vector<vec3>& out)
	{
		for (auto b : in)
		{
			short I = b & 0x03;
			short H = (b & 0xFC) >> 2;
			
			for (int i = 0; i < H + 1; i++)
				out.push_back(colors[I]);
		}
	}
};

HF3 app;