//=============================================================================================
// Geometry Shader Normals
//=============================================================================================
#define FILE_OPERATIONS
#include "framework.h"

//---------------------------
template<class T> struct Dnum { // Dual numbers for automatic derivation
	//---------------------------
	float f; // function value
	T d;  // derivatives
	Dnum(float f0 = 0, T d0 = T(0)) { f = f0, d = d0; }
	Dnum operator+(Dnum r) { return Dnum(f + r.f, d + r.d); }
	Dnum operator-(Dnum r) { return Dnum(f - r.f, d - r.d); }
	Dnum operator*(Dnum r) {
		return Dnum(f * r.f, f * r.d + d * r.f);
	}
	Dnum operator/(Dnum r) {
		return Dnum(f / r.f, (r.f * d - r.d * f) / r.f / r.f);
	}
};

// Elementary functions prepared for the chain rule as well
template<class T> Dnum<T> Exp(Dnum<T> g) { return Dnum<T>(expf(g.f), expf(g.f) * g.d); }
template<class T> Dnum<T> Sin(Dnum<T> g) { return  Dnum<T>(sinf(g.f), cosf(g.f) * g.d); }
template<class T> Dnum<T> Cos(Dnum<T>  g) { return  Dnum<T>(cosf(g.f), -sinf(g.f) * g.d); }
template<class T> Dnum<T> Tan(Dnum<T>  g) { return Sin(g) / Cos(g); }
template<class T> Dnum<T> Sinh(Dnum<T> g) { return  Dnum<T>(sinh(g.f), cosh(g.f) * g.d); }
template<class T> Dnum<T> Cosh(Dnum<T> g) { return  Dnum<T>(cosh(g.f), sinh(g.f) * g.d); }
template<class T> Dnum<T> Tanh(Dnum<T> g) { return Sinh(g) / Cosh(g); }
template<class T> Dnum<T> Log(Dnum<T> g) { return  Dnum<T>(logf(g.f), g.d / g.f); }
template<class T> Dnum<T> Pow(Dnum<T> g, float n) {
	return  Dnum<T>(powf(g.f, n), n * powf(g.f, n - 1) * g.d);
}

typedef Dnum<vec2> Dnum2;

// Phong shader (per-pixel shading)
const char* vertSourcePhong = R"(
	#version 330				
    precision highp float;

	uniform mat4 MVP, M, Minv; // MVP, Model, Model-inverse
	uniform vec4 wLiPos; // pos of light source
	uniform vec3 wEye; // pos of eye
	
	layout(location = 0) in vec3 vtxPos; // pos in model sp
	layout(location = 1) in vec3 vtxNorm;// normal in model sp
	
	out vec3 wPos; // position in world space
	out vec3 wNormal; // normal in world space
	out vec3 wView; // view in world space
	out vec3 wLight; // light dir in world space
	
	void main() {
		gl_Position = MVP * vec4(vtxPos, 1); // to NDC
		vec4 wPos4 = M * vec4(vtxPos, 1);
		wPos = wPos4.xyz / wPos4.w;
		wLight = wLiPos.xyz * wPos4.w - wPos4.xyz * wLiPos.w;
		wView = wEye - wPos4.xyz / wPos4.w;
		wNormal = (vec4(vtxNorm, 0) * Minv).xyz;
	}
)";
const char* fragSourcePhong = R"(
	#version 330
    precision highp float;

	uniform vec3 kd, ks, ka;// diffuse, specular, ambient ref
	uniform float shine; // shininess for specular ref
	uniform vec3 La, Le; // ambient and dir/point source rad

	in vec3 wPos;
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

// Normal shader
const char* vertSourceNormals = R"(
	#version 330 core

	layout(location = 0) in vec3 vtxPos;
	layout(location = 1) in vec3 vtxNorm;

	uniform mat4 M;
	uniform mat4 Minv;
	uniform mat4 MVP;

	out vec3 wPos;
	out vec3 wNormal;

	void main() {
		vec4 worldPos = M * vec4(vtxPos, 1.0);
		wPos = worldPos.xyz / worldPos.w;
		wNormal = (vec4(vtxNorm, 0) * Minv).xyz;
		gl_Position = MVP * vec4(vtxPos, 1.0);
	}
)";

const char* geomSourceNormals = R"(
	#version 330
	precision highp float;

	layout(triangles) in;
	layout(line_strip, max_vertices = 6) out;

	uniform mat4 MVP;
	uniform mat4 Minv;

	in vec3 wPos[3];
	in vec3 wNormal[3];

	const float normalLength = 0.15f; // scale of the normal line

	void main() {
		mat4 VP = MVP * Minv; // calculate view-projection matrix from MVP and Minv
		for (int i = 0; i < 3; i++) {
			vec3 start = wPos[i];
			vec3 end = start + normalize(wNormal[i]) * normalLength;

			gl_Position = VP * vec4(start, 1.0);
			EmitVertex();

			gl_Position = VP * vec4(end, 1.0);
			EmitVertex();

			EndPrimitive();
		}
	}
)";
const char* fragSourceNormals = R"(
	#version 330 core
	out vec4 fragmentColor;

	uniform vec3 kd, ks, ka;// diffuse, specular, ambient ref
	uniform float shine; // shininess for specular ref

	void main() {
		fragmentColor = vec4(1.0, 1.0, 0.0, 1.0);

		// just to avoid shader uniform setting errors
		vec3 _dummy = kd;
		_dummy = ks;
		_dummy = ka;
		float _dummy2 = shine;
	}
)";

const int tessellationLevel = 30;

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
class ParamSurface : public Geometry<VertexData> {

	int nVtxPerStrip, nStrips;

public:
	float rotAngle;
	vec3 translation, rotAxis, scaling;

	Material material;

	ParamSurface(vec3 position, Material material)
		: translation(position), rotAxis(1.0f), rotAngle(0.0f), scaling(1.0f), material(material) {
		nVtxPerStrip = nStrips = 0;
	}

	virtual void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z) = 0;

	VertexData GenVertexData(float u, float v) {
		VertexData vtxData;
		Dnum2 X, Y, Z;
		Dnum2 U(u, vec2(1, 0)), V(v, vec2(0, 1));
		eval(U, V, X, Y, Z);
		vtxData.position = vec3(X.f, Y.f, Z.f);
		vec3 drdU(X.d.x, Y.d.x, Z.d.x), drdV(X.d.y, Y.d.y, Z.d.y);
		vtxData.normal = cross(drdU, drdV);
		return vtxData;
	}

	void create(int N = tessellationLevel, int M = tessellationLevel) {
		nVtxPerStrip = (M + 1) * 2;
		nStrips = N;
		vtx.clear();
		for (int i = 0; i < N; i++) {
			for (int j = 0; j <= M; j++) {
				vtx.push_back(GenVertexData((float)j / M, (float)i / N));
				vtx.push_back(GenVertexData((float)j / M, (float)(i + 1) / N));
			}
		}
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0 = POSITION
		glEnableVertexAttribArray(1);  // attribute array 1 = NORMAL
		// attribute array, components/attribute, component type, normalize?, stride, offset
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, position));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));

		updateGPU();
	}

	void Draw(Camera* camera, GPUProgram* gpuProgram)
	{
		// transformation matrices
		mat4 M = translate(translation) * rotate(rotAngle, rotAxis) * scale(scaling);
		gpuProgram->setUniform(M, "M");

		mat4 Minv = scale(1.0f / scaling) * rotate(-rotAngle, rotAxis) * translate(-translation);
		gpuProgram->setUniform(Minv, "Minv");

		gpuProgram->setUniform(camera->P() * camera->V() * M, "MVP");

		// material properties
		gpuProgram->setUniform(material.kd, "kd");
		gpuProgram->setUniform(material.ks, "ks");
		gpuProgram->setUniform(material.ka, "ka");
		gpuProgram->setUniform(material.shine, "shine");

		Bind();
		for (unsigned int i = 0; i < nStrips; i++)
			glDrawArrays(GL_TRIANGLE_STRIP, i * nVtxPerStrip, nVtxPerStrip);
	}
};

class Torus : public ParamSurface
{
	float R, r;
public:
	Torus(float R, float r, Material material) : R(R), r(r), ParamSurface(vec3(0.0f), material) { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		U = U * 2.0f * M_PI; V = V * 2.0f * M_PI;
		X = (Cos(U) * r + R) * Cos(V); Z = (Cos(U) * r + R) * Sin(V); Y = Sin(U) * r;
	}
};

class GeometryShaderNormals : public glApp
{
	Camera* camera;
	Material material;
	Light light;

	Torus* donut;

	GPUProgram* phongShader;
	GPUProgram* normalShader;

	bool wireframe = false;

public:
	GeometryShaderNormals() : glApp("Geometry Shader Normals") {}
	~GeometryShaderNormals() { delete camera; delete donut; delete phongShader; delete normalShader; }

	// Inicializáció
	void onInitialization()
	{
		glEnable(GL_DEPTH_TEST);

		material = Material{ vec3(0.15f, 0.6f, 0.9f), vec3(0.05f, 0.2f, 0.3f), vec3(0.6f), 20.0f };
		light = Light{ vec3(0.4f), vec3(1.0f), vec4(1.0f, 1.0f, 1.0f, 0.0f) };

		camera = new Camera(vec3(0.0f, 1.0f, 5.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));

		donut = new Torus(1.0f, 0.4f, material);
		donut->rotAxis = vec3(1.0f, 0.5f, 0.0f);

		phongShader = new GPUProgram(vertSourcePhong, fragSourcePhong);
		normalShader = new GPUProgram(vertSourceNormals, fragSourceNormals, geomSourceNormals);
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
		donut->Draw(camera, phongShader);

		normalShader->Use();
		donut->Draw(camera, normalShader);
	}

	void onTimeElapsed(float ts, float te)
	{
		donut->rotAngle = fmodf(donut->rotAngle + (te - ts), 2.0f * M_PI);
		refreshScreen();
	}

	void onMousePressed(MouseButton but, int pX, int pY)
	{
		wireframe = !wireframe;
		glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
	}

} app;
