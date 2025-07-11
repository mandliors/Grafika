//=============================================================================================
// Parametric Surfaces
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
		: translation(position), rotAxis(1.0f), rotAngle(0.0f), scaling(1.0f), material(material) { nVtxPerStrip = nStrips = 0; }

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

class PlaneXZ : public ParamSurface
{
public:
	PlaneXZ(Material material) : ParamSurface(vec3(0.0f), material) { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		X = U * 2.0f - 1.0f; Z = V * 2.0f - 1.0f; Y = 0.0f;
	}
};
class Sphere : public ParamSurface
{
public:
	Sphere(Material material) : ParamSurface(vec3(0.0f), material) { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		U = U * 2.0f * (float)M_PI, V = V * (float)M_PI;
		X = Cos(U) * Sin(V); Y = Sin(U) * Sin(V); Z = Cos(V);
	}
};
class Cylinder : public ParamSurface
{
public:
	Cylinder(Material material) : ParamSurface(vec3(0.0f), material) { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		U = U * 2.0f * M_PI;
		X = Cos(U); Z = Sin(U); Y = V * 2.0f - 1.0f;
	}
};
class Cone : public ParamSurface
{
	float alpha;
public:
	Cone(float alpha, Material material) : alpha(alpha), ParamSurface(vec3(0.0f), material) { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		U = U * 2.0f * M_PI;
		X = V * tanf(alpha * 0.5f) * Cos(U); Z = V * tanf(alpha * 0.5f) * Sin(U); Y = V * -1.0f + 0.5f;
	}
};
class Hyperboloid : public ParamSurface
{
public:
	Hyperboloid(Material material) : ParamSurface(vec3(0.0f), material) { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		U = U * 2.0f - 1.0f; V = V * 2.0f * M_PI;
		X = Cosh(U) * Cos(V); Z = Cosh(U) * Sin(V); Y = Sinh(U);
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
	ExtrudedSurface(vec3 position, Material material) : ParamSurface(position, material) { }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		State R = s(V) + b(U);
		X = R.X; Y = R.Y; Z = R.Z;
	}
};
class Churros : public ExtrudedSurface
{
	float R;

	State s(Dnum2& V) // gerinc
	{
		V = ((V - 0.5f) * M_PI) * 0.7f;
		return { V / 0.7f / M_PI * 2.0f, Tan(V) / 2.0f, 0.0f };
	}
	State b(Dnum2& U) // profil
	{
		U = U * 2.0f * M_PI;
		Dnum2 M = Cos(U * 6.0f) * 0.1f + 1.0f;
		return { Cos(U) * R * M, 0.0f, Sin(U) * R * M };
	}

public:
	Churros(float R, Material material) : R(R), ExtrudedSurface(vec3(0.0f), material) { create(); }
};

class RotatedSurface : public ParamSurface
{
protected:
	virtual Dnum2 px(Dnum2& U) = 0;
	virtual Dnum2 py(Dnum2& U) = 0;

public:
	RotatedSurface(vec3 position, Material material) : ParamSurface(position, material) {}
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		V = V * 2.0f * M_PI;
		X = px(U) * Cos(V);
		Z = px(U) * Sin(V);
		Y = py(U);
	}
};
class Heart : public RotatedSurface
{
	Dnum2 px(Dnum2& U) { return Pow(Sin(U * 2.0f * M_PI), 3); }
	Dnum2 py(Dnum2& U)
	{
		U = U * 2.0f * M_PI;
		return (Cos(U) * 13.0f - Cos(U * 2.0f) * 5.0f - Cos(U * 3.0f) * 2.0f - Cos(U * 4.0f)) / 12.0f;
	}

public:
	Heart(Material material) : RotatedSurface(vec3(0.0f), material) { create(); }
};

class ParamSurfaces : public glApp
{
	Camera* camera;
	Material material;
	Light light;

	int surfaceIdx = 0;
	static const int nSurfaces = 8;
	ParamSurface* surfaces[nSurfaces];

	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók

	bool wireframe = false;

public:
	ParamSurfaces() : glApp("Parametric Surfaces") {}
	~ParamSurfaces() { delete camera; delete gpuProgram; for (int i = 0; i < nSurfaces; i++) delete surfaces[i]; }

	// Inicializáció
	void onInitialization()
	{
		glEnable(GL_DEPTH_TEST);

		material = Material{ vec3(0.15f, 0.6f, 0.9f), vec3(0.05f, 0.2f, 0.3f), vec3(0.6f, 0.6f, 0.6f), 20.0f };
		light = Light{ vec3(0.4f), vec3(1.0f), vec4(1.0f, 1.0f, 1.0f, 0.0f)};

		camera = new Camera(vec3(0.0f, 1.0f, 5.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
		
		surfaces[0] = new PlaneXZ(material); surfaces[0]->translation = vec3(0.0f, -1.0f, 0.0f);
		surfaces[1] = new Sphere(material);
		surfaces[2] = new Cylinder(material); surfaces[2]->translation = vec3(0.0f, -0.8f, 0.0f); surfaces[2]->scaling = vec3(0.4f, 0.8f, 0.4f);
		surfaces[3] = new Cone(0.5f, material); surfaces[3]->scaling = vec3(2.0f);
		surfaces[4] = new Hyperboloid(material); surfaces[4]->scaling = vec3(0.6f);
		surfaces[5] = new Torus(1.0f, 0.25f, material); surfaces[5]->translation = vec3(0.0f, -0.8f, 0.0f);
		surfaces[6] = new Churros(0.4f, material);
		surfaces[7] = new Heart(material); surfaces[7]->scaling = vec3(0.6f); surfaces[7]->translation = vec3(0.0f, -0.8f, 0.0f);

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

		surfaces[surfaceIdx]->Draw(camera, gpuProgram);
	}

	void onMousePressed(MouseButton but, int pX, int pY)
	{
		if (but == MOUSE_LEFT) 
			surfaceIdx = (surfaceIdx + 1) % nSurfaces;
		else if (but == MOUSE_RIGHT)
		{
			wireframe = !wireframe;
			glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
		}
		
		refreshScreen();
	}

} app;
