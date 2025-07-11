//=============================================================================================
// Quaternion
//=============================================================================================
#define FILE_OPERATIONS
#include "framework.h"

const float eps = 0.0001f;
const float lineLength = 1.4f;
const float sensitivity = 0.01f;

// Quaternions
vec4 quaternion(float ang, vec3 axis) // konstruálás
{
	vec3 d = normalize(axis) * sinf(ang / 2);
	return vec4(d.x, d.y, d.z, cosf(ang / 2));
}
vec4 qinverse(vec4 quaternion) // kvaternió inverze
{
	return vec4(-quaternion.x, -quaternion.y, -quaternion.z, quaternion.w) / powf(length(quaternion), 2.0f);
}
vec4 qmul(vec4 q1, vec4 q2) // kvaternió szorzás
{
	vec3 d1(q1.x, q1.y, q1.z), d2(q2.x, q2.y, q2.z);
	return vec4(d2 * q1.w + d1 * q2.w + cross(d1, d2),
		q1.w * q2.w - dot(d1, d2));
}
mat4 qrotate(vec4 quaternion) // forgatás mátrix kvaternióból
{
	float angle = acosf(quaternion.w) * 2.0f;
	if (abs(angle) < eps) return rotate(0.0f, vec3(1.0f, 0.0f, 0.0f));

	vec3 axis = vec3(quaternion) / sinf(angle / 2.0f);

	return rotate(angle, axis);
}

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
	layout(location = 2) in vec2 vtxUV;// normal in model sp
	
	out vec2 texcoords;
	out vec3 wNormal; // normal in world space
	out vec3 wView; // view in world space
	out vec3 wLight; // light dir in world space
	
	void main() {
		gl_Position = MVP * vec4(vtxPos, 1); // to NDC
		vec4 wPos = M * vec4(vtxPos, 1);
		wLight = wLiPos.xyz * wPos.w - wPos.xyz * wLiPos.w;
		wView = wEye - wPos.xyz / wPos.w;
		wNormal = (vec4(vtxNorm, 0) * Minv).xyz;
		texcoords = vtxUV;
	}
)";
const char* fragSourcePhong = R"(
	#version 330
    precision highp float;

	uniform vec3 kd, ks, ka;// diffuse, specular, ambient ref
	uniform float shine; // shininess for specular ref
	uniform vec3 La, Le; // ambient and dir/point source rad

	uniform sampler2D diffuseTexture;

	in vec2 texcoords;
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
		vec3 color = ka * La + (kd * cost + ks * pow(cosd,shine)) * Le;
		vec4 texColor = texture(diffuseTexture, texcoords);
		fragmentColor = vec4(texColor.rgb * color, 1);
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
	vec2 texcoords;
};

//---------------------------
class ParamSurface : public Geometry<VertexData> {

	int nVtxPerStrip, nStrips;

	vec3 translation, scaling;
	vec4 quaternion;

public:
	Material* material;
	Texture* texture;

	ParamSurface(vec3 position, Material* material, Texture* texture)
		: translation(position), scaling(1.0f), quaternion(vec4(0.0f, 0.0f, 0.0f, 1.0f)), material(material), texture(texture) {
		nVtxPerStrip = nStrips = 0;
	}

	void Rotate(vec4 delta) { quaternion = normalize(qmul(delta, quaternion)); }

	virtual void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z) = 0;

	VertexData GenVertexData(float u, float v) {
		VertexData vtxData;
		Dnum2 X, Y, Z;
		Dnum2 U(u, vec2(1, 0)), V(v, vec2(0, 1));
		eval(U, V, X, Y, Z);
		vtxData.position = vec3(X.f, Y.f, Z.f);
		vec3 drdU(X.d.x, Y.d.x, Z.d.x), drdV(X.d.y, Y.d.y, Z.d.y);
		vtxData.normal = cross(drdU, drdV);
		vtxData.texcoords = vec2(u, v);
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
		glEnableVertexAttribArray(2);  // attribute array 2 = TEXCOORDS
		// attribute array, components/attribute, component type, normalize?, stride, offset
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, position));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, texcoords));

		updateGPU();
	}

	void Draw(Camera* camera, GPUProgram* gpuProgram)
	{
		// transformation matrices
		mat4 M = translate(translation) * qrotate(quaternion) * scale(scaling);
		gpuProgram->setUniform(M, "M");

		mat4 Minv = scale(1.0f / scaling) * qrotate(qinverse(quaternion)) * translate(-translation);
		gpuProgram->setUniform(Minv, "Minv");

		gpuProgram->setUniform(camera->P() * camera->V() * M, "MVP");

		// material properties
		gpuProgram->setUniform(material->kd, "kd");
		gpuProgram->setUniform(material->ks, "ks");
		gpuProgram->setUniform(material->ka, "ka");
		gpuProgram->setUniform(material->shine, "shine");

		gpuProgram->setUniform(0, "diffuseTexture");
		texture->Bind(0);

		Bind();
		for (unsigned int i = 0; i < nStrips; i++)
			glDrawArrays(GL_TRIANGLE_STRIP, i * nVtxPerStrip, nVtxPerStrip);
	}
};

class Sphere : public ParamSurface
{
public:
	Sphere(Material* material, Texture* texture) : ParamSurface(vec3(0.0f), material, texture) { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		U = U * 2.0f * (float)M_PI, V = V * (float)M_PI;
		X = Cos(U) * Sin(V); Y = Sin(U) * Sin(V); Z = Cos(V);
	}
};

class Line3D : public Geometry<vec3>
{
public:
	Line3D() {}

	void Update(const vec3& p1, const vec3& p2) { vtx = { p1, p2 }; updateGPU(); }
	void Draw(Camera* camera, GPUProgram* gpuProgram)
	{
		gpuProgram->setUniform(camera->P() * camera->V(), "MVP");
		Geometry::Draw(gpuProgram, GL_LINES, vec3(1.0f));
	}
};

class Quaternion : public glApp
{
	Camera* camera;
	Material material;
	Texture* texture;
	Light light;

	Sphere* sphere;
	Line3D* axis;

	enum Rotation { NONE = 0, AXIAL, DRAG } rotation = NONE;
	vec3 arcballStart;
	vec3 prevArcball;
	int prevPX;

	GPUProgram* phongShader;
	GPUProgram* basicShader;

public:
	Quaternion() : glApp("Quaternion") {}
	~Quaternion() { delete camera; delete sphere; delete axis; delete texture; delete phongShader; delete basicShader; }

	// Inicializáció
	void onInitialization()
	{
		glEnable(GL_DEPTH_TEST);

		material = Material{ vec3(1.0f), vec3(1.0f), vec3(1.0f), 20.0f };
		texture = new Texture("earth.png");
		light = Light{ vec3(0.4f), vec3(1.0f), vec4(1.0f, 1.0f, 1.0f, 0.0f) };

		camera = new Camera(vec3(0.0f, 0.0f, 5.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));

		sphere = new Sphere(&material, texture);
		sphere->Rotate(quaternion(-M_PI / 2, vec3(1.0f, 0.0f, 0.0f)));
		sphere->Rotate(quaternion(M_PI / 2, vec3(0.0f, 1.0f, 0.0f)));

		axis = new Line3D;

		phongShader = new GPUProgram(vertSourcePhong, fragSourcePhong);
		basicShader = new GPUProgram(vertSourceBasic, fragSourceBasic);
	}

	// Ablak újrarajzolás
	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);     // háttér szín
		glLineWidth(5.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		phongShader->Use();
		phongShader->setUniform(light.La, "La");
		phongShader->setUniform(light.Le, "Le");
		phongShader->setUniform(light.wLightPos, "wLiPos");
		phongShader->setUniform(camera->wEye, "wEye");

		sphere->Draw(camera, phongShader);

		if (rotation)
		{
			basicShader->Use();
			axis->Draw(camera, basicShader);
		}
	}

	void onMousePressed(MouseButton but, int pX, int pY)
	{
		rotation = but == MOUSE_LEFT ? AXIAL : DRAG;
		arcballStart = prevArcball = ScreenToArcball(pX, pY);
		prevPX = pX;

		axis->Update({ 0.0f, 0.0f, 0.0f }, arcballStart * lineLength);

		refreshScreen();
	}
	void onMouseReleased(MouseButton but, int pX, int pY)
	{
		rotation = NONE;

		refreshScreen();
	}
	void onMouseMotion(int pX, int pY)
	{
		if (!rotation) return;

		vec3 axis, currArcball = ScreenToArcball(pX, pY);
		float angle = 0.0f;
		
		if (rotation == AXIAL)
		{
			axis = arcballStart;
			angle = (pX - prevPX) * sensitivity;
			prevPX = pX;
		}
		else if (rotation == DRAG)
		{
			axis = cross(prevArcball, currArcball);
			angle = acos(clamp(dot(prevArcball, currArcball), -1.0f, 1.0f));
		}

		float axisLength = length(axis);
		if (axisLength > eps)
		{
			axis /= axisLength;
			vec4 delta = quaternion(angle, axis);
			sphere->Rotate(delta);
		}
		prevArcball = currArcball;

		if (rotation == DRAG)
			this->axis->Update({ 0.0f, 0.0f, 0.0f }, currArcball * lineLength);

		refreshScreen();
	}

private:
	vec3 ScreenToArcball(int pX, int pY) // arcball projection (project screen pos to unit arcball sphere screen space)
	{
		vec3 ndc = {
			2.0f * pX / winWidth - 1.0f,
			1.0f - 2.0f * pY / winHeight,
			0.0f
		};

		float len2 = ndc.x * ndc.x + ndc.y * ndc.y;

		if (len2 <= 1.0f) // on the sphere
			ndc.z = sqrt(1.0f - len2);
		else // outside the sphere, project to edge
			ndc = normalize(ndc);

		return ndc;
	}

} app;
