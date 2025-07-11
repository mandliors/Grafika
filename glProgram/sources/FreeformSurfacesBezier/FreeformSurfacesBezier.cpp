//=============================================================================================
// Freeform Surfaces - Bézier
//=============================================================================================
#include "framework.h"

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

// Basic shader
const char* vertSourceBasic = R"(
	#version 330				
    precision highp float;

	uniform mat4 MVP; // MVP, Model

	layout(location = 0) in vec3 vtxPos; // pos in model sp
	
	void main() {
		gl_Position = MVP * vec4(vtxPos, 1); // to NDC
	}
)";
const char* fragSourceBasic = R"(
	#version 330
    precision highp float;

	uniform vec3 color;

	out vec4 fragmentColor; // output goes to frame buffer
	
	void main() {
		fragmentColor = vec4(color, 1);
	}
)";

const int tessellationLevel = 20;
const float eps = 0.0001f;
const float pickDelta = 0.1f;

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
template<unsigned R, unsigned C>
class BezierSurface : public Geometry<VertexData>
{
	int nVtxPerStrip, nStrips;
	Geometry<vec3>* controlPoints;

public:
	float rotAngle;
	vec3 translation, rotAxis, scaling;

	Material material;

	BezierSurface(vec3 position, Material material)
		: translation(position), rotAxis(1.0f), rotAngle(0.0f), scaling(1.0f), material(material)
	{
		nVtxPerStrip = nStrips = 0;

		controlPoints = new Geometry<vec3>;
		auto& cps = controlPoints->Vtx();
		cps.reserve(R * C);

		float dx = 2.0f / (C - 1), dz = 2.0f / (R - 1);
		for (int i = 0; i < R; i++)
			for (int j = 0; j < C; j++)
				cps.emplace_back(-1.0f + j * dx, (rand() % 11 - 5) / 8.0f, -1.0f + i * dz);

		Update();
	}
	~BezierSurface() { delete controlPoints; }

	std::vector<vec3>& Cps() { return controlPoints->Vtx(); };

	vec3 evalPoint(float u, float v)
	{
		auto& cps = controlPoints->Vtx();
		vec3 point(0.0f);
		for (int i = 0; i < R; i++)
		{
			float Bu = B(i, R - 1, u);
			for (int j = 0; j < C; j++)
			{
				float Bv = B(j, C - 1, v);
				point += Bu * Bv * cps[i * C + j];
			}
		}
		return point;
	}
	vec3 evalNormal(float u, float v)
	{
		vec3 du = evalPoint(u + eps, v) - evalPoint(u - eps, v);
		vec3 dv = evalPoint(u, v + eps) - evalPoint(u, v - eps);
		return normalize(cross(du, dv));
	}

	VertexData GenVertexData(float u, float v) { return { evalPoint(u, v), evalNormal(u, v) }; }
	void Update(int N = tessellationLevel, int M = tessellationLevel)
	{
		controlPoints->updateGPU();

		nVtxPerStrip = (M + 1) * 2;
		nStrips = N;
		vtx.clear();
		for (int i = 0; i < N; i++) {
			for (int j = 0; j <= M; j++) {
				vtx.push_back(GenVertexData((float)j / M, (float)i / N));
				vtx.push_back(GenVertexData((float)j / M, (float)(i + 1) / N));
			}
		}
		updateGPU();
	}

	mat4 M() { return translate(translation) * rotate(rotAngle, rotAxis) * scale(scaling); }

	void Draw(Camera* camera, GPUProgram* surfaceShader, GPUProgram* pointShader)
	{
		// transformation matrices
		mat4 M = translate(translation) * rotate(rotAngle, rotAxis) * scale(scaling);
		mat4 Minv = scale(1.0f / scaling) * rotate(-rotAngle, rotAxis) * translate(-translation);
		mat4 MVP = camera->P() * camera->V() * M;

		surfaceShader->setUniform(M, "M");
		surfaceShader->setUniform(Minv, "Minv");
		surfaceShader->setUniform(MVP, "MVP");

		// material properties
		surfaceShader->setUniform(material.kd, "kd");
		surfaceShader->setUniform(material.ks, "ks");
		surfaceShader->setUniform(material.ka, "ka");
		surfaceShader->setUniform(material.shine, "shine");

		Bind();
		glEnableVertexAttribArray(0);  // attribute array 0 = POSITION
		glEnableVertexAttribArray(1);  // attribute array 1 = NORMAL
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, position));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));
		for (unsigned int i = 0; i < nStrips; i++)
			glDrawArrays(GL_TRIANGLE_STRIP, i * nVtxPerStrip, nVtxPerStrip);

		pointShader->Use();
		pointShader->setUniform(MVP, "MVP");

		controlPoints->Bind();
		glEnableVertexAttribArray(0);  // attribute array 0 = POSITION
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);

		glDisable(GL_DEPTH_TEST);
		controlPoints->Draw(pointShader, GL_POINTS, vec3(1.0f, 1.0f, 0.0f));
		glEnable(GL_DEPTH_TEST);
	}

private:
	float B(int i, int n, float t)
	{
		float choose = 1;
		for (int j = 1; j <= i; j++)
			choose *= (float)(n - (i - j)) / j;

		return choose * powf(t, i) * powf(1 - t, n - i);
	}
};

class FreeformSurfacesBezier : public glApp
{
	Camera* camera;
	Material material;
	Light light;

	BezierSurface<4, 4>* surface;

	GPUProgram* phongShader;
	GPUProgram* basicShader;

	vec3* draggedVertex = nullptr;
	float prevDepth;

	bool wireframe = false;

public:
	FreeformSurfacesBezier() : glApp("Freeform Surfaces - Bezier") {}
	~FreeformSurfacesBezier() { delete camera; delete phongShader; delete basicShader; delete surface; }

	// Inicializáció
	void onInitialization()
	{
		srand(time(NULL));

		glEnable(GL_DEPTH_TEST);
		glPointSize(10.0f);

		material = Material{ vec3(0.15f, 0.6f, 0.9f), vec3(0.05f, 0.2f, 0.3f), vec3(0.6f, 0.6f, 0.6f), 20.0f };
		light = Light{ vec3(0.4f), vec3(1.0f), vec4(1.0f, 1.0f, 1.0f, 0.0f) };

		camera = new Camera(vec3(0.0f, 2.0f, 4.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));

		surface = new BezierSurface<4, 4>(vec3(0.0f), material);

		phongShader = new GPUProgram(vertSourcePhong, fragSourcePhong);
		basicShader = new GPUProgram(vertSourceBasic, fragSourceBasic);
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		phongShader->Use();
		phongShader->setUniform(light.La, "La");
		phongShader->setUniform(light.Le, "Le");
		phongShader->setUniform(light.wLightPos, "wLiPos");
		phongShader->setUniform(camera->wEye, "wEye");

		surface->Draw(camera, phongShader, basicShader);
	}

	void onMousePressed(MouseButton but, int pX, int pY)
	{
		if (but == MOUSE_LEFT)
		{
			vec2 mousePos = Viewport2Window(pX, pY);
			mat4 MVP = camera->P() * camera->V() * surface->M();
			for (auto& v : surface->Cps())
			{
				vec4 vT = MVP * vec4(v, 1.0f);
				vec2 vScreen(vT.x / vT.w, vT.y / vT.w);
				if (length(vScreen - mousePos) < pickDelta)
				{
					draggedVertex = &v;
					prevDepth = vT.z / vT.w;
				}
			}
		}
		else if (but == MOUSE_RIGHT)
		{
			wireframe = !wireframe;
			glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
			refreshScreen();
		}
	}
	void onMouseReleased(MouseButton but, int pX, int pY)
	{
		draggedVertex = nullptr;
	}
	void onMouseMotion(int pX, int pY)
	{
		if (draggedVertex == nullptr) return;

		vec2 mousePos = Viewport2Window(pX, pY);
		vec3 mouseNDC(mousePos.x, mousePos.y, prevDepth);
		vec4 mouseWorld = inverse(camera->P() * camera->V()) * vec4(mouseNDC, 1.0f);
		mouseWorld /= mouseWorld.w;

		*draggedVertex = vec3(mouseWorld);

		surface->Update();
		refreshScreen();
	}

	vec2 Viewport2Window(int pX, int pY)
	{
		return {
			pX / (float)winWidth * 2.0f - 1.0f,
			1.0f - pY / (float)winHeight * 2.0f
		};
	}

} app;
