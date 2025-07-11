//=============================================================================================
// Frenet Frame
//=============================================================================================
#define FILE_OPERATIONS
#include "framework.h"

//---------------------------
template<class T> struct Dnum 
{	// Dual numbers for automatic derivation
	//---------------------------
	float f;  // value
	T d;      // first derivative
	T dd;     // second derivative

	Dnum(float f0 = 0.0f, T d0 = T(0), T dd0 = T(0)) : f(f0), d(d0), dd(dd0) { }
	Dnum operator+(const Dnum& r) { return Dnum(f + r.f, d + r.d, dd + r.dd); }
	Dnum operator-(const Dnum& r) { return Dnum(f - r.f, d - r.d, dd - r.dd); }
	Dnum operator*(const Dnum& r) { return Dnum(f * r.f, f * r.d + d * r.f, f * r.dd + T(2) * d * r.d + dd * r.f); }
};

// Elementary functions prepared for the chain rule as well
template<class T> Dnum<T> Sin(const Dnum<T>& a)
{
	float sf = sinf(a.f), cf = cosf(a.f);
	return Dnum<T>(sf, cf * a.d, cf * a.dd - sf * (a.d * a.d));
}
template<class T> Dnum<T> Cos(const Dnum<T>& a) 
{
	float sf = sinf(a.f), cf = cosf(a.f);
	return Dnum<T>(cf, -sf * a.d, -cf * (a.d * a.d) - sf * a.dd);
}
template<class T> Dnum<T> Pow(const Dnum<T>& a, float n)
{
	float pf = powf(a.f, n);
	float pfn1 = pf / a.f;
	float pfn2 = pfn1 / a.f;
	return Dnum<T>(pf, n * pfn1 * a.d, n * (n - 1) * pfn2 * (a.d * a.d) + n * pfn1 * a.dd);
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

const int tessellationLevel = 50;
const float arrowLength = 0.4f;

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
		Dnum2 U(u, vec2(1, 0), vec2(0, 0)), V(v, vec2(0, 1), vec2(0, 0));
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

class Sphere : public ParamSurface
{
	float R;

public:
	Sphere(float R, Material material) : R(R), ParamSurface(vec3(0.0f), material) { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		U = U * 2.0f * (float)M_PI, V = V * (float)M_PI;
		X = Cos(U) * Sin(V) * R; Y = Sin(U) * Sin(V) * R; Z = Cos(V) * R;
	}
};
class Arrow : public ParamSurface
{
public:
	Arrow(Material material) : ParamSurface(vec3(0.0f), material) { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		const float cylinderHeight = 0.8f;
		const float cylinderWidth = 0.5f;
		const float r = 0.6f;

		U = U * 2.0f * M_PI;

		// cylinder
		if (V.f < cylinderHeight)
		{
			X = Cos(U) * r * cylinderWidth;
			Z = Sin(U) * r * cylinderWidth;
			Y = V * 2.0f - 1.0f;
		}
		// cone
		else
		{
			V = (V - cylinderHeight) * (1.0f / (1.0f - cylinderHeight)) * -1.0f + 1.0f;
			X = V * Cos(U) * r;
			Z = V * Sin(U) * r;
			Y = V * 2.0f * (1.0f - cylinderHeight) * -1.0f + 1.0f;
		}
	}
};

class ExtrudedSurface : public ParamSurface
{
protected:
	struct State {
		Dnum2 X, Y, Z;
		State operator+(const State& other) { return { X + other.X, Y + other.Y, Z + other.Z }; }
	};

	virtual State s(Dnum2& V) = 0;
	virtual State b(Dnum2& U) = 0;

public:
	ExtrudedSurface(vec3 position, Material material) : ParamSurface(position, material) {}
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		State R = s(V) + b(U);
		X = R.X; Y = R.Y; Z = R.Z;
	}
};
class VivianiCurve : public ExtrudedSurface
{
	float R;

	State s(Dnum2& V) // gerinc
	{
		V = V * 2.0f * M_PI;
		return { Pow(Cos(V), 2), Cos(V) * Sin(V), Sin(V) };
	}
	State b(Dnum2& U) // profil
	{
		U = U * 2.0f * M_PI;
		return { Cos(U) * R, 0.0f, Sin(U) * R };
	}

public:
	VivianiCurve(float R, Material material) : R(R), ExtrudedSurface(vec3(0.0f), material) { create(tessellationLevel * 3, tessellationLevel / 3); }

	State Get(float v) { Dnum2 V(v, vec2(0, 1), vec2(0, 0)); return s(V); }
};


class FrenetFrame : public glApp
{
	Camera* camera;
	Light light;
	
	Material pathMat;
	Material cartMat;
	Material tangentMat, normalMat, binormalMat;

	VivianiCurve* path;
	Sphere* cart;
	Arrow* tangent, *normal, *binormal;

	GPUProgram* gpuProgram;

public:
	FrenetFrame() : glApp("Frenet Frame") {}
	~FrenetFrame() { delete camera; delete gpuProgram; delete path; delete cart; delete tangent; delete normal; delete binormal; }

	// Inicializáció
	void onInitialization()
	{
		glEnable(GL_DEPTH_TEST);

		// camera
		camera = new Camera(vec3(4.0f, 0.5f, 0.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));

		// light
		light = Light{ vec3(0.4f), vec3(1.0f), vec4(1.0f, 1.0f, 1.0f, 0.0f) };
		
		// materials
		pathMat = Material{ 
			.ka = vec3(0.27f),
			.kd = vec3(0.81f),
			.ks = vec3(0.6f),
			.shine = 20.0f
		};
		cartMat = Material{
			.ka = vec3(0.3f, 0.0f, 0.0f),
			.kd = vec3(0.9f, 0.0f, 0.0f),
			.ks = vec3(0.6f),
			.shine = 20.0f
		};
		tangentMat = Material{
			.ka = vec3(0.3f, 0.3f, 0.0f),
			.kd = vec3(0.9f, 0.9f, 0.0f),
			.ks = vec3(0.6f),
			.shine = 20.0f
		};
		normalMat = Material{
			.ka = vec3(0.0f, 0.3f, 0.0f),
			.kd = vec3(0.0f, 0.9f, 0.0f),
			.ks = vec3(0.6f),
			.shine = 20.0f
		};
		binormalMat = Material{
			.ka = vec3(0.0f, 0.1f, 0.3f),
			.kd = vec3(0.0f, 0.3f, 0.9f),
			.ks = vec3(0.6f),
			.shine = 20.0f
		};

		// objects
		path = new VivianiCurve(0.02f, pathMat);
		cart = new Sphere(0.08f, cartMat);

		tangent = new Arrow(tangentMat);
		tangent->scaling = vec3(0.1f, arrowLength * 0.5f, 0.1f);
		normal = new Arrow(normalMat);
		normal->scaling = vec3(0.1f, arrowLength * 0.5f, 0.1f);
		binormal = new Arrow(binormalMat);
		binormal->scaling = vec3(0.1f, arrowLength * 0.5f, 0.1f);

		gpuProgram = new GPUProgram(vertSourcePhong, fragSourcePhong);
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		gpuProgram->setUniform(light.La, "La");
		gpuProgram->setUniform(light.Le, "Le");
		gpuProgram->setUniform(light.wLightPos, "wLiPos");

		gpuProgram->setUniform(camera->wEye, "wEye");

		path->Draw(camera, gpuProgram);
		cart->Draw(camera, gpuProgram);

		tangent->Draw(camera, gpuProgram);
		normal->Draw(camera, gpuProgram);
		binormal->Draw(camera, gpuProgram);
	}

	void onTimeElapsed(float ts, float te)
	{
		static float t = 0.0f;

		t += (te - ts) * 0.2f;
		auto state = path->Get(t);

		vec3 pos(state.X.f, state.Y.f, state.Z.f);
		vec3 tang = normalize(vec3(state.X.d.y, state.Y.d.y, state.Z.d.y));
		vec3 norm = normalize(vec3(state.X.dd.y, state.Y.dd.y, state.Z.dd.y));
		vec3 binorm = normalize(cross(tang, norm));
		
		cart->translation = pos;
		
		vec3 axis, up(0.0f, 1.0f, 0.0f);
		float angle;

		// tangent
		axis = normalize(cross(up, tang));
		angle = acos(dot(up, tang));
		tangent->translation = pos + tang * arrowLength * 0.5f;
		tangent->rotAxis = axis;
		tangent->rotAngle = angle;

		// normal
		axis = normalize(cross(up, norm));
		angle = acos(dot(up, norm));
		normal->translation = pos + norm * arrowLength * 0.5f;
		normal->rotAxis = axis;
		normal->rotAngle = angle;

		// binormal
		axis = normalize(cross(up, binorm));
		angle = acos(dot(up, binorm));
		binormal->translation = pos + binorm * arrowLength * 0.5f;
		binormal->rotAxis = axis;
		binormal->rotAngle = angle;

		refreshScreen();
	}

} app;
