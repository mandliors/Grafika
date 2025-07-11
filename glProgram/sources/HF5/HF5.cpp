//=============================================================================================
// HF5
//=============================================================================================
#include "framework.h"

template<class T> struct Dnum 
{
	float f;
	T d;

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
template<class T> Dnum<T> Pow(Dnum<T> g, float n) { return Dnum<T>(powf(g.f, n), n * powf(g.f, n - 1) * g.d); }

typedef Dnum<vec2> Dnum2;

const int windowWidth = 600, windowHeight = 600;

class PointTexture : public Texture
{
public:
	PointTexture(int width, int height, std::vector<vec3>& image) : Texture(width, height, image) 
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
};

struct Camera 
{
	vec3 wEye, wLookat, wVup;
	float fov, asp, fp, bp;

public:
	Camera() 
	{
		asp = (float)windowWidth / windowHeight;
		fov = 45.0f * (float)M_PI / 180.0f;
		fp = 1; bp = 20;
	}
	mat4 V() { return lookAt(wEye, wLookat, wVup); }
	mat4 P() { return perspective(fov, asp, fp, bp); }
};

struct Material
{
	vec3 kd, ks, ka;
	float shininess;
};

struct Light 
{
	vec3 La, Le;
	vec4 wLightPos; // homogeneous coordinates, can be at ideal point
};

struct RenderState
{
	mat4			  MVP, M, Minv, V, P;
	Material*		  material;
	Light*			  light;
	Texture*		  texture;
	vec3			  wEye;
	std::vector<vec3> triangles; // triangles in world space, for shadow computation
};

class Shader : public GPUProgram
{
public:
	virtual void Bind(RenderState state) = 0;

	void setUniformMaterial(const Material& material, const std::string& name) 
	{
		setUniform(material.kd, name + ".kd");
		setUniform(material.ks, name + ".ks");
		setUniform(material.ka, name + ".ka");
		setUniform(material.shininess, name + ".shininess");
	}

	void setUniformLight(const Light& light, const std::string& name) 
	{
		setUniform(light.La, name + ".La");
		setUniform(light.Le, name + ".Le");
		setUniform(light.wLightPos, name + ".wLightPos");
	}
};

class PhongShader : public Shader 
{
	// vertex shader in GLSL
	const char* vertexSource = R"(
		#version 330
		precision highp float;

		struct Light {
			vec3 La, Le;
			vec4 wLightPos;
		};

		uniform mat4  MVP, M, Minv; // MVP, Model, Model-inverse
		uniform Light light;
		uniform vec3  wEye;         // pos of eye

		layout(location = 0) in vec3  vtxPos;            // pos in modeling space
		layout(location = 1) in vec3  vtxNorm;      	 // normal in modeling space
		layout(location = 2) in vec2  vtxUV;

		out vec3 wPos;				// pos in world space
		out vec3 wNormal;		    // normal in world space
		out vec3 wView;             // view in world space
		out vec3 wLight;		    // light dir in world space
		out vec2 texcoord;

		void main() {
			gl_Position = MVP * vec4(vtxPos, 1); // to NDC

			// vectors for radiance computation
			vec4 wPos4 = M * vec4(vtxPos, 1);
			wPos = wPos4.xyz / wPos4.w; // homogeneous to world space
			wLight = light.wLightPos.xyz * wPos4.w - wPos4.xyz * light.wLightPos.w;
		    wView  = wEye - wPos4.xyz/wPos4.w;
		    wNormal = (vec4(vtxNorm, 0) * Minv).xyz;
		    texcoord = vtxUV;
		}
	)";

	// fragment shader in GLSL
	const char* fragmentSource = R"(
		#version 330
		precision highp float;

		struct Light {
			vec3 La, Le;
			vec4 wLightPos;
		};

		struct Material {
			vec3 kd, ks, ka;
			float shininess;
		};

		struct Ray {
			vec3 start;
			vec3 dir; // unit vector
		};

		uniform Material material;
		uniform Light light;
		uniform sampler2D diffuseTexture;

		uniform int nTriangles; // number of triangles
		uniform vec3 triangles[100 * 3]; // 100 triangles

		in  vec3 wPos;          // interpolated world space position
		in  vec3 wNormal;       // interpolated world sp normal
		in  vec3 wView;         // interpolated world sp view
		in  vec3 wLight;        // interpolated world sp illum dir
		in  vec2 texcoord;
		
        out vec4 fragmentColor; // output goes to frame buffer

		float epsilon = 0.001f; // small offset to avoid self-shadowing

		bool isInShadow(vec3 wPos);
		bool hitTriangle(Ray ray, vec3 v0, vec3 v1, vec3 v2);

		void main() {
			vec3 N = normalize(wNormal);
			vec3 V = normalize(wView); 
			if (dot(N, V) < 0) N = -N;	// prepare for one-sided surfaces like Mobius or Klein
			
			vec3 texColor = texture(diffuseTexture, texcoord).rgb;
			vec3 ka = material.ka * texColor;
			vec3 kd = material.kd * texColor;
			
			vec3 radiance = ka * light.La; // ambient light
			if (isInShadow(wPos + N * epsilon))
			{
				// if in shadow, no light contribution
				fragmentColor = vec4(radiance, 1);
				return;
			}

			vec3 L = normalize(wLight);
			vec3 H = normalize(L + V);
			float cost = max(dot(N,L), 0), cosd = max(dot(N,H), 0);
			
			radiance += (kd * cost + material.ks * pow(cosd, material.shininess)) * light.Le;
			
			fragmentColor = vec4(radiance, 1);
		}

		bool isInShadow(vec3 wPos) {
			Ray ray;
			ray.start = wPos;
			ray.dir = normalize(wLight);
	
			bool inShadow = false;
			for (int i = 0; i < nTriangles; i++)
			{
				if (hitTriangle(ray, triangles[i*3], triangles[i*3 + 1], triangles[i*3 + 2]))
				{
					inShadow = true;
					break;
				}
			}
			return inShadow; 
		}
		bool hitTriangle(Ray ray, vec3 v0, vec3 v1, vec3 v2) {
			vec3 n = cross(v1 - v0, v2 - v0);
			float denom = dot(ray.dir, n);
			if (abs(denom) < epsilon) return false; // ray is parallel to triangle plane
			float t = dot(v0 - ray.start, n) / denom;
			if (t < epsilon) return false; // ignore intersections too close
	
			vec3 p = ray.start + ray.dir * t;
			if (dot(cross(v1 - v0, p - v0), n) < 0) return false;
			if (dot(cross(v2 - v1, p - v1), n) < 0) return false;
			if (dot(cross(v0 - v2, p - v2), n) < 0) return false;
	
			return true;
		}
	)";

public:
	PhongShader() { create(vertexSource, fragmentSource); }

	void Bind(RenderState state)
	{
		Use(); 		// make this program run
		setUniform(state.MVP, "MVP");
		setUniform(state.M, "M");
		setUniform(state.Minv, "Minv");
		setUniform(state.wEye, "wEye");
		if (state.texture != nullptr) state.texture->Bind(0);
		setUniform(0, "diffuseTexture");

		// upload triangles
		setUniform((int)state.triangles.size() / 3, "nTriangles");
		for (int i = 0; i < state.triangles.size(); i++)
			setUniform(state.triangles[i], "triangles[" + std::to_string(i) + "]");

		setUniformMaterial(*state.material, "material");
		setUniformLight(*state.light, "light");
	}
};

int defaultN = 1;
int defaultM = 6;

struct VertexData
{
	vec3 position, normal;
	vec2 texcoord;
};

class ParamSurface : public Geometry<VertexData> 
{
	unsigned int nVtxPerStrip, nStrips;

public:
	ParamSurface() { nVtxPerStrip = nStrips = 0; }

	virtual void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z) = 0;

	VertexData GenVertexData(float u, float v)
	{
		VertexData vtxData;
		vtxData.texcoord = vec2(u, v);
		Dnum2 X, Y, Z;
		Dnum2 U(u, vec2(1, 0)), V(v, vec2(0, 1));
		eval(U, V, X, Y, Z);
		vtxData.position = vec3(X.f, Y.f, Z.f);
		vec3 drdU(X.d.x, Y.d.x, Z.d.x), drdV(X.d.y, Y.d.y, Z.d.y);
		vtxData.normal = cross(drdU, drdV);
		return vtxData;
	}

	void create(int N = defaultN, int M = defaultM) 
	{
		nVtxPerStrip = (M + 1) * 2;
		nStrips = N;
		for (int i = 0; i < N; i++)
		{
			for (int j = 0; j <= M; j++)
			{
				vtx.push_back(GenVertexData((float)j / M, (float)i / N));
				vtx.push_back(GenVertexData((float)j / M, (float)(i + 1) / N));
			}
		}
		glBufferData(GL_ARRAY_BUFFER, nVtxPerStrip * nStrips * sizeof(VertexData), &vtx[0], GL_STATIC_DRAW);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0 = POSITION
		glEnableVertexAttribArray(1);  // attribute array 1 = NORMAL
		glEnableVertexAttribArray(2);  // attribute array 2 = TEXCOORD0
		// attribute array, components/attribute, component type, normalize?, stride, offset
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, position));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, texcoord));
	}

	void Draw()
	{
		Bind();
		for (unsigned int i = 0; i < nStrips; i++)
			glDrawArrays(GL_TRIANGLE_STRIP, i * nVtxPerStrip, nVtxPerStrip);
	}
};

class PlaneXZ : public ParamSurface
{
public:
	PlaneXZ() { create(1, 1); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		X = U * 2.0f - 1.0f; Z = V * 2.0f - 1.0f; Y = 0.0f;
	}
};
class Cylinder : public ParamSurface 
{
public:
	Cylinder() { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z) 
	{
		U = U * 2.0f * M_PI;
		X = Cos(U); Z = Sin(U); Y = V;
	}
};
class Cone : public ParamSurface
{
	float alpha;

public:
	Cone(float alpha) : alpha(alpha) { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z)
	{
		U = U * 2.0f * M_PI, V = V * M_PI;
		X = V / M_PI * tanf(alpha) * Cos(U); Z = V / M_PI * tanf(alpha) * Sin(U); Y = V / -M_PI;
	}
};

struct Object
{
	Shader* shader;
	Material* material;
	Texture* texture;
	ParamSurface* geometry;
	vec3 scaling, translation, rotationAxis;
	float rotationAngle;

public:
	Object(Shader* _shader, Material* _material, Texture* _texture, ParamSurface* _geometry) :
		scaling(vec3(1, 1, 1)), translation(vec3(0, 0, 0)), rotationAxis(0, 0, 1), rotationAngle(0) 
	{
		shader = _shader;
		material = _material;
		texture = _texture;
		geometry = _geometry;
	}

	virtual void SetModelingTransform(mat4& M, mat4& Minv)
	{
		M = translate(translation) * rotate(rotationAngle, rotationAxis) * scale(scaling);
		Minv = scale(vec3(1 / scaling.x, 1 / scaling.y, 1 / scaling.z)) * rotate(-rotationAngle, rotationAxis) * translate(-translation);
	}

	void Draw(RenderState state) 
	{
		mat4 M, Minv;
		SetModelingTransform(M, Minv);
		state.M = M;
		state.Minv = Minv;
		state.MVP = state.P * state.V * state.M;
		state.material = material;
		state.texture = texture;
		shader->Bind(state);
		geometry->Draw();
	}

	std::vector<vec3> GetVerticesInWorldSpace()
	{
		std::vector<vec3> vertices;
		const auto& vtx = geometry->Vtx();
		if (vtx.size() < 3)
			return vertices;

		mat4 M = translate(translation) * rotate(rotationAngle, rotationAxis) * scale(scaling);

		std::vector<vec3> worldVertices;
		for (const auto& vertex : vtx)
			worldVertices.push_back(Vertex2World(M, vertex.position));

		for (size_t i = 0; i < worldVertices.size() - 2; i++)
		{
			vertices.push_back(worldVertices[i]);
			vertices.push_back(worldVertices[i + 1]);
			vertices.push_back(worldVertices[i + 2]);
		}

		return vertices;
	}

private:
	vec3 Vertex2World(const mat4& M, const vec3& vtx)
	{
		vec4 pos = vec4(vtx, 1.0f);
		pos = M * pos; // transform to world space
		return vec3(pos.x, pos.y, pos.z) / pos.w; // homogeneous to world space
	}
};

class Scene
{
	Camera camera; // 3D camera
	Light light;
	std::vector<Object*> objects;

	Shader* shader = nullptr;
	std::vector<ParamSurface*> surfaces;
	std::vector<Material*> materials;
	std::vector<Texture*> textures;

	float cameraAngle;

public:
	~Scene()
	{
		delete shader;
		for (const auto* o : objects) delete o;
		for (const auto* s : surfaces) delete s;
		for (const auto* m : materials) delete m;
		for (const auto* t : textures) delete t;
	}

	void Build()
	{
		// camera
		camera.wEye = vec3(0, 1, 4);
		camera.wLookat = vec3(0, 0, 0);
		camera.wVup = vec3(0, 1, 0);

		// light
		light.wLightPos = vec4(1.0f, 1.0f, 1.0f, 0);	// ideal point -> directional light source
		light.Le = vec3(2.0f);
		light.La = vec3(0.4f);

		// shader
		shader = new PhongShader();

		// surfaces
		surfaces.push_back(new PlaneXZ());
		surfaces.push_back(new Cylinder());
		surfaces.push_back(new Cone(0.2f));

		// materials
		materials.push_back(new Material{ // checkerboard
			.kd = vec3(1.0f),
			.ks = vec3(0.0f),
			.ka = vec3(3.0f),
			.shininess = 0.0f,
			});
		materials.push_back(new Material{ // blue cylinder
			.kd = vec3(0.17f, 0.35f, 1.5f),
			.ks = vec3(3.1f, 2.7f, 1.9f),
			.ka = vec3(0.51f, 1.05f, 4.5f),
			.shininess = 100.0f,
			});
		materials.push_back(new Material{ // white cylinder
			.kd = vec3(1.3f),
			.ks = vec3(0.0f),
			.ka = vec3(3.9f),
			.shininess = 50.0f,
			});
		materials.push_back(new Material{ // plastic cylinder
			.kd = vec3(0.3f, 0.2f, 0.1f),
			.ks = vec3(2.0f),
			.ka = vec3(0.9f, 0.6f, 0.3f),
			.shininess = 100.0f,
			});
		materials.push_back(new Material{ // cyan cone
			.kd = vec3(0.1f, 0.2f, 0.3f),
			.ks = vec3(2.0f),
			.ka = vec3(0.3f, 0.6f, 0.9f),
			.shininess = 100.0f,
			});
		materials.push_back(new Material{ // magenta cone
			.kd = vec3(0.3f, 0.0f, 0.2f),
			.ks = vec3(2.0f),
			.ka = vec3(0.9f, 0.0f, 0.6f),
			.shininess = 20.0f,
			});

		// textures
		std::vector<vec3> whitePixelData{ vec3(1.0f) };
		textures.push_back(new PointTexture(1, 1, whitePixelData));

		std::vector<vec3> checkerBoardData; checkerBoardData.reserve(40 * 40);
		for (int i = 0; i < 40; i++) for (int j = 0; j < 40; j++)
		{
			if ((i + j) % 2 == 0)
				checkerBoardData.push_back(vec3(0.3f, 0.3f, 0.3f));
			else
				checkerBoardData.push_back(vec3(0.0f, 0.1f, 0.3f));
		}
		textures.push_back(new PointTexture(40, 40, checkerBoardData));

		// objects
		vec3 up(0.0f, 1.0f, 0.0f);

		Object* checkerBoard = new Object(shader, materials[0], textures[1], surfaces[0]);
		checkerBoard->translation = vec3(0.0f, -1.0f, 0.0f);
		checkerBoard->scaling = vec3(20.0f, 1.0f, 20.0f);
		objects.push_back(checkerBoard);

		Object* blueCylinder = new Object(shader, materials[1], textures[0], surfaces[1]);
		blueCylinder->translation = vec3(1.0f, -1.0f, 0.0f);
		blueCylinder->scaling = vec3(0.3f, 2.0f, 0.3f);
		blueCylinder->rotationAxis = vec3(0.0f, 0.0f, 1.0f);
		blueCylinder->rotationAngle = -atanf(0.1f);
		objects.push_back(blueCylinder);

		Object* whiteCylinder = new Object(shader, materials[2], textures[0], surfaces[1]);
		whiteCylinder->translation = vec3(0.0f, -1.0f, -0.8f);
		whiteCylinder->scaling = vec3(0.3f, 2.0f, 0.3f);
		whiteCylinder->rotationAxis = cross(vec3(-0.2f, 1.0f, -0.1f), up);
		whiteCylinder->rotationAngle = -acosf(dot(normalize(vec3(-0.2f, 1.0f, -0.1f)), up));
		objects.push_back(whiteCylinder);

		Object* plasticCylinder = new Object(shader, materials[3], textures[0], surfaces[1]);
		plasticCylinder->translation = vec3(-1.0f, -1.0f, 0.0f);
		plasticCylinder->scaling = vec3(0.3f, 2.0f, 0.3f);
		plasticCylinder->rotationAxis = vec3(1.0f, 0.0f, 0.0f);
		plasticCylinder->rotationAngle = atanf(0.1f);
		objects.push_back(plasticCylinder);

		Object* cyanCone = new Object(shader, materials[4], textures[0], surfaces[2]);
		cyanCone->translation = vec3(0.0f, 1.0f, 0.0f);
		cyanCone->scaling = vec3(2.0f);
		cyanCone->rotationAxis = cross(vec3(-0.1f, -1.0f, -0.05f), up);
		cyanCone->rotationAngle = acosf(dot(normalize(vec3(-0.1f, -1.0f, -0.05f)), -up));
		objects.push_back(cyanCone);

		Object* magentaCone = new Object(shader, materials[5], textures[0], surfaces[2]);
		magentaCone->translation = vec3(0.0f, 1.0f, 0.8f);
		magentaCone->scaling = vec3(2.0f);
		magentaCone->rotationAxis = cross(vec3(0.2f, -1.0f, 0.0f), up);
		magentaCone->rotationAngle = acosf(dot(normalize(vec3(0.2f, -1.0f, 0.0f)), -up));
		objects.push_back(magentaCone);
	}

	void Render()
	{
		RenderState state;
		state.wEye = camera.wEye;
		state.V = camera.V();
		state.P = camera.P();
		state.light = &light;

		for (Object* obj : objects)
		{
			state.triangles.clear();
			for (Object* o : objects)
			{
				if (o == obj) continue; // skip the current object to avoid self shadowing
				std::vector<vec3> vertices = o->GetVerticesInWorldSpace();
				state.triangles.insert(state.triangles.end(), vertices.begin(), vertices.end());

			}
			obj->Draw(state);
		}
	}

	void Rotate()
	{
		cameraAngle -= M_PI / 4.0f;
		camera.wEye = vec3(4.0f * sin(cameraAngle), 1.0f, 4.0f * cos(cameraAngle));
	}
};

class HF5 : public glApp 
{
	Scene scene;

public:
	HF5() : glApp("HF5") {}

	void onInitialization()
	{
		glViewport(0, 0, windowWidth, windowHeight);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		scene.Build();
	}
	void onDisplay()
	{
		glClearColor(0.4f, 0.4f, 0.4f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen

		scene.Render();
	}

	void onKeyboard(int key)
	{
		if (key == 'a')
			scene.Rotate();

		scene.Render();
		refreshScreen();
	}
} app;