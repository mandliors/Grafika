// =============================================================================================
// Ray Marching
//=============================================================================================
#include "framework.h"

#include <array>
#include <iostream>
#include <unordered_map>

const int winWidth = 600, winHeight = 600;

const int MAX_INTERSECTABLES = 200;
const int MAX_LIGHTS = 10;
const int MAX_MATERIALS = 10;

class ColorlessQuadFan
{
	unsigned int vao, vbo;

public:
	ColorlessQuadFan(const vec2(&vertices)[4])
	{
		// Setup buffers
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

		// CPU -> GPU
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(vec2), vertices, GL_STATIC_DRAW);
	}
	void Bind() { glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); } // activate
	void Draw(GPUProgram* prog)
	{
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
	virtual ~ColorlessQuadFan()
	{
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}
};

// scene
struct Light
{
	vec3 Le;
	vec4 wLightPos;
};
struct Material 
{
	vec3 ka, kd, ks;
	float shininess;
};


// primitives, intersectables
enum PrimitiveType
{
	PT_PLANE = 0,
	PT_SPHERE,
	PT_BOX,
	PT_ROUNDED_BOX,
	PT_CAPSULE,
};
enum OperationType
{
	OT_UNION = 0,
	OT_DIFFERENCE,
	OT_INTERSECTION,
	OT_XOR,
	OT_SMOOTH_UNION,
	OT_SMOOTH_DIFFERENCE,
	OT_SMOOTH_INTERSECTION,
	OT_NONE = -1
};
class Primitive
{
	friend class RayMarchShader;

protected:
	PrimitiveType type;
	int id;
	int next;
	OperationType opType;
	float opSmoothing;

public:
	Primitive(PrimitiveType type, OperationType opType = OT_NONE, float opSmoothing = 0.0f)
		: type(type), id(-1), next(-1), opType(opType), opSmoothing(opSmoothing) { }
	virtual void Set(GPUProgram* gpuProgram, int primitiveId)
	{
		if (primitiveId == -1)
			return;

		if (opType == OT_NONE && next != -1)
		{
			std::cerr << "primitive with no operation type cannot have a next primitive" << std::endl;
			return;
		}

		gpuProgram->setUniform(type, "primitives[" + std::to_string(primitiveId) + "].type");
		gpuProgram->setUniform(id, "primitives[" + std::to_string(primitiveId) + "].id");
		gpuProgram->setUniform(next, "primitives[" + std::to_string(primitiveId) + "].next");
		gpuProgram->setUniform(opType, "primitives[" + std::to_string(primitiveId) + "].opType");
		gpuProgram->setUniform(opSmoothing, "primitives[" + std::to_string(primitiveId) + "].opSmoothing");
	}
};

struct Plane : public Primitive
{
	vec3 n; // normal
	float d; // offset

	Plane(const vec3& n, float d, OperationType opType = OT_NONE, float opSmoothing = 0.0f) : Primitive(PT_PLANE, opType, opSmoothing), n(n), d(d) { }
	void Set(GPUProgram* gpuProgram, int primitiveId = -1) override
	{
		Primitive::Set(gpuProgram, primitiveId);
		gpuProgram->setUniform(n, "planes[" + std::to_string(id) + "].n");
		gpuProgram->setUniform(d, "planes[" + std::to_string(id) + "].d");
	}
};
struct Sphere : public Primitive
{
	vec3 c; // center
	float r; // radius

	Sphere(const vec3& c, float r, OperationType opType = OT_NONE, float opSmoothing = 0.0f) : Primitive(PT_SPHERE, opType, opSmoothing), c(c), r(r) { }
	void Set(GPUProgram* gpuProgram, int primitiveId = -1) override
	{
		Primitive::Set(gpuProgram, primitiveId);
		gpuProgram->setUniform(c, "spheres[" + std::to_string(id) + "].c");
		gpuProgram->setUniform(r, "spheres[" + std::to_string(id) + "].r");
	}
};
struct Box : public Primitive
{
	vec3 c; // center
	vec3 b; // half dimensions

	Box(const vec3& c, const vec3& b, OperationType opType = OT_NONE, float opSmoothing = 0.0f) : Primitive(PT_BOX, opType, opSmoothing), c(c), b(b) { }
	void Set(GPUProgram* gpuProgram, int primitiveId = -1) override
	{
		Primitive::Set(gpuProgram, primitiveId);
		gpuProgram->setUniform(c, "boxes[" + std::to_string(id) + "].c");
		gpuProgram->setUniform(b, "boxes[" + std::to_string(id) + "].b");
	}
};
struct RoundedBox : public Primitive
{
	vec3 c; // center
	vec3 b; // half dimensions
	float r; // roundness

	RoundedBox(const vec3& c, const vec3& b, float r, OperationType opType = OT_NONE, float opSmoothing = 0.0f) : Primitive(PT_ROUNDED_BOX, opType, opSmoothing), c(c), b(b), r(r) { }
	void Set(GPUProgram* gpuProgram, int primitiveId = -1) override
	{
		Primitive::Set(gpuProgram, primitiveId);
		gpuProgram->setUniform(c, "roundedBoxes[" + std::to_string(id) + "].c");
		gpuProgram->setUniform(b, "roundedBoxes[" + std::to_string(id) + "].b");
		gpuProgram->setUniform(r, "roundedBoxes[" + std::to_string(id) + "].r");
	}
};
struct Capsule : public Primitive
{
	vec3 a, b; // two endpoints
	float r; // radius

	Capsule(const vec3& a, const vec3& b, float r, OperationType opType = OT_NONE, float opSmoothing = 0.0f) : Primitive(PT_CAPSULE, opType, opSmoothing), a(a), b(b), r(r) { }
	void Set(GPUProgram* gpuProgram, int primitiveId = -1) override
	{
		Primitive::Set(gpuProgram, primitiveId);
		gpuProgram->setUniform(a, "capsules[" + std::to_string(id) + "].a");
		gpuProgram->setUniform(b, "capsules[" + std::to_string(id) + "].b");
		gpuProgram->setUniform(r, "capsules[" + std::to_string(id) + "].r");
	}
};

class Intersectable 
{
	friend class RayMarchShader;

	int rootPrimitive;
	int materialId;

	std::vector<Primitive*> primitives;

public:
	Intersectable(const std::vector<Primitive*>& primitives, int materialId)
		: materialId(materialId), primitives(primitives) { }
	Primitive*& Get(int i) { return primitives[i]; }
	void Set(GPUProgram* gpuProgram, int intersectableId)
	{
		gpuProgram->setUniform(rootPrimitive, "intersectables[" + std::to_string(intersectableId) + "].rootPrimitive");
		gpuProgram->setUniform(materialId, "intersectables[" + std::to_string(intersectableId) + "].materialId");
	}
};

class RayMarchShader
{
	GPUProgram* gpuProgram;

	int nPrimitives = 0;
	std::array<Primitive*, MAX_INTERSECTABLES> primitives;
	std::unordered_map<PrimitiveType, int> primitiveTypeCount = {
		{ PT_PLANE, 0 },
		{ PT_SPHERE, 0 },
		{ PT_BOX, 0 },
		{ PT_ROUNDED_BOX, 0 },
		{ PT_CAPSULE, 0 }
	};

	int nIntersectables = 0;
	std::array<Intersectable*, MAX_INTERSECTABLES> intersectables;

	int nLights = 0;
	int nMaterials = 0;

public:
	RayMarchShader(const vec3& La, int nReflections, float coeffRefl)
	{
		gpuProgram = new GPUProgram(vertSource, fragSource);
		gpuProgram->Use();

		gpuProgram->setUniform(vec2(winWidth, winHeight), "winSize");
		gpuProgram->setUniform(nReflections, "nReflections");
		gpuProgram->setUniform(coeffRefl, "coeffRefl");
		gpuProgram->setUniform(La, "La");

		gpuProgram->setUniform(0, "nLights");
		gpuProgram->setUniform(0, "nIntersectables");
	}
	~RayMarchShader()
	{
		delete gpuProgram;
		for (auto& primitive : primitives) delete primitive;
		for (auto& intersectable : intersectables) delete intersectable;
	}
	GPUProgram* Get() { return gpuProgram; }

	Intersectable* AddIntersectable(Intersectable* intersectable)
	{
		intersectable->rootPrimitive = nPrimitives;
		intersectable->Set(gpuProgram, nIntersectables);
		if (nIntersectables >= intersectables.size())
		{
			std::cerr << "maximum number of intersectables exceeded" << std::endl;
			return intersectable;
		}
		intersectables[nIntersectables++] = intersectable;
		gpuProgram->setUniform(nIntersectables, "nIntersectables");

		for (int i = 0; i < intersectable->primitives.size(); i++)
		{
			if (nPrimitives >= primitives.size())
			{
				std::cerr << "maximum number of primitives exceeded" << std::endl;
				return intersectable;
			}

			auto& primitive = intersectable->primitives[i];
			primitive->id = primitiveTypeCount[primitive->type]++;
			primitive->next = (i < intersectable->primitives.size() - 1) ? nPrimitives + 1 : -1;
			primitive->Set(gpuProgram, nPrimitives);
			primitives[nPrimitives++] = primitive;
		}

		return intersectable;
	}
	Light AddLight(const Light& light)
	{
		if (nLights >= MAX_LIGHTS)
		{
			std::cerr << "maximum number of lights exceeded" << std::endl;
			return light;
		}
		gpuProgram->setUniform(light.Le, "lights[" + std::to_string(nLights) + "].Le");
		gpuProgram->setUniform(light.wLightPos, "lights[" + std::to_string(nLights) + "].wLightPos");
		gpuProgram->setUniform(++nLights, "nLights");

		return light;
	}
	Material AddMaterial(const Material& material)
	{
		if (nMaterials >= MAX_MATERIALS)
		{
			std::cerr << "maximum number of materials exceeded" << std::endl;
			return material;
		}
		gpuProgram->setUniform(material.ka, "materials[" + std::to_string(nMaterials) + "].ka");
		gpuProgram->setUniform(material.kd, "materials[" + std::to_string(nMaterials) + "].kd");
		gpuProgram->setUniform(material.ks, "materials[" + std::to_string(nMaterials) + "].ks");
		gpuProgram->setUniform(material.shininess, "materials[" + std::to_string(nMaterials) + "].shininess");
		nMaterials++;

		return material;
	}

private:
	// csúcspont árnyaló
	const char* vertSource = R"(
		#version 330				
		precision highp float;

		layout(location = 0) in vec2 cP;

		void main() { gl_Position = vec4(cP.x, cP.y, 0, 1); }
	)";

	// pixel árnyaló
	const char* fragSource = R"(
		#version 330
		precision highp float;

		// ray casting
		struct Ray {
			vec3 start;
			vec3 dir;
		};
		struct Hit {
			float t;
			int materialId;
		};


		// scene
		struct Light {
			vec3 Le;
			vec4 wLightPos;
		};
		struct Material {
			vec3 ka, kd, ks;
			float shininess;
		};


		// primitives, intersectables
		struct Primitive {
			int type, id, next;
			int opType;
			float opSmoothing;
		};
		struct Plane {
			vec3 n; // normal
			float d; // offset
		};
		struct Sphere {
			vec3 c; // center
			float r; // radius
		};
		struct Box {
			vec3 c; // center
			vec3 b; // half dimensions
		};
		struct RoundedBox {
			vec3 c; // center
			vec3 b; // half dimensions
			float r; // roundness
		};
		struct Capsule {
			vec3 a, b; // two endpoints
			float r; // radius
		};

		struct Intersectable {
			int rootPrimitive;
			int materialId;
		};

		// primitive types
		const int PT_PLANE = 0;
		const int PT_SPHERE = 1;
		const int PT_BOX = 2;
		const int PT_ROUNDED_BOX = 3;
		const int PT_CAPSULE = 4;


		// camera
		uniform vec2 winSize;
		uniform vec3 camPos, camDir, camRight, camUp;
		uniform float fov, asp;


		// scene
		uniform int nReflections;
		uniform float coeffRefl;
	
		uniform vec3 La;
		const int maxLights = 10;
		uniform int nLights;
		uniform Light lights[maxLights];

		const int maxMaterials = 10;
		uniform Material materials[maxMaterials];

		const int maxPlanes = 100;
		uniform Plane planes[maxPlanes];

		const int maxSpheres = 100;
		uniform Sphere spheres[maxSpheres];

		const int maxBoxes = 100;
		uniform Box boxes[maxBoxes];

		const int maxRoundedBoxes = 100;
		uniform RoundedBox roundedBoxes[maxRoundedBoxes];
	
		const int maxCapsules = 100;
		uniform Capsule capsules[maxCapsules];

		const int maxPrimitives = 200;
		uniform Primitive primitives[maxPrimitives];

		const int maxIntersectables = 200;
		uniform int nIntersectables;
		uniform Intersectable intersectables[maxIntersectables];
	
	
		// rendering parameters
		const int nSteps = 128;
		const float minHitDist = 0.0001f;
		const float maxTraceDist = 100000.0f;
		const float gamma = 1.1;
	
		const float eps = 0.001f;

		uniform float time;

		out vec4 fragmentColor;


		// SDFs
		float sdfPlane(Plane pl, vec3 p) { return dot(p, pl.n) + pl.d; }
		float sdfSphere(Sphere s, vec3 p) { return length(p - s.c) - s.r; }
		float sdfBox(Box b, vec3 p)
		{
			vec3 q = abs(p - b.c) - b.b;
			return length(max(q, 0.0f)) + min(max(q.x, max(q.y, q.z)), 0.0f);
		}
		float sdfRoundedBox(RoundedBox rb, vec3 p)
		{
			vec3 q = abs(p - rb.c) - rb.b + rb.r;
			return length(max(q, 0.0f)) + min(max(q.x, max(q.y, q.z)), 0.0f) - rb.r;
		}
		float sdfCapsule(Capsule c, vec3 p)
		{
			vec3 pa = p - c.a, ba = c.b - c.a;
			float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);
			return length(pa - ba * h) - c.r;
		}

		float sdf(int type, int id, vec3 p)
		{
			if (type == PT_PLANE) return sdfPlane(planes[id], p);
			if (type == PT_SPHERE) return sdfSphere(spheres[id], p);
			if (type == PT_BOX) return sdfBox(boxes[id], p);
			if (type == PT_ROUNDED_BOX) return sdfRoundedBox(roundedBoxes[id], p);
			if (type == PT_CAPSULE) return sdfCapsule(capsules[id], p);
		}


		// shape operation functions
		const int OT_UNION = 0;
		const int OT_DIFFERENCE = 1;
		const int OT_INTERSECTION = 2;
		const int OT_XOR = 3;
		const int OT_SMOOTH_UNION = 4;
		const int OT_SMOOTH_DIFFERENCE = 5;
		const int OT_SMOOTH_INTERSECTION = 6;

		float opUnion(float a, float b) { return min(a, b); }
		float opDifference(float a, float b) { return max(a, -b); }
		float opIntersection(float a, float b) { return max(a, b); }
		float opXor(float a, float b) { return max(min(a, b),-max(a, b)); }
		float opSmoothUnion(float a, float b, float k)
		{
			float h = clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
			return mix(b, a, h) - k * h * (1.0f - h);
		}
		float opSmoothDifference(float a, float b, float k)
		{
			float h = clamp(0.5f - 0.5f * (a + b) / k, 0.0f, 1.0f);
			return mix(b, -a, h) +  k * h * (1.0f - h);
		}
		float opSmoothIntersection(float a, float b, float k)
		{
			float h = clamp(0.5f - 0.5f * (b - a) / k, 0.0f, 1.0f);
			return mix(b, a, h) + k * h * (1.0f - h);
		}
		float operation(float a, float b, float k, int type)
		{
			if (type == OT_UNION) return opUnion(a, b);
			if (type == OT_DIFFERENCE) return opDifference(a, b);
			if (type == OT_INTERSECTION) return opIntersection(a, b);
			if (type == OT_XOR) return opXor(a, b);
			if (type == OT_SMOOTH_UNION) return opSmoothUnion(a, b, k);
			if (type == OT_SMOOTH_DIFFERENCE) return opSmoothDifference(a, b, k);
			if (type == OT_SMOOTH_INTERSECTION) return opSmoothIntersection(a, b, k);
		}

		// scene functions
		float intersectable(int intersectableIdx, vec3 p)
		{
			int current = intersectables[intersectableIdx].rootPrimitive;
			float minT = sdf(primitives[current].type, primitives[current].id, p);
			int next = primitives[current].next;

			while(next != -1)
			{
				float currT = sdf(primitives[next].type, primitives[next].id, p);
				minT = operation(minT, currT, primitives[current].opSmoothing, primitives[current].opType);
				current = next;
				next = primitives[next].next;
			}

			return minT;
		}
		Hit map(vec3 p)
		{
			Hit bestHit = Hit(maxTraceDist, -1);
			for (int i = 0; i < nIntersectables; i++)
			{
				float t = intersectable(i, p);
				if (t < bestHit.t)
					bestHit = Hit(t, intersectables[i].materialId);
			}
			return bestHit;
		}
		vec3 normal(vec3 p)
		{	
			float gradX = map(p + vec3(eps, 0.0f, 0.0f)).t - map(p - vec3(eps, 0.0f, 0.0f)).t;
			float gradY = map(p + vec3(0.0f, eps, 0.0f)).t - map(p - vec3(0.0f, eps, 0.0f)).t;
			float gradZ = map(p + vec3(0.0f, 0.0f, eps)).t - map(p - vec3(0.0f, 0.0f, eps)).t;

			return normalize(vec3(gradX, gradY, gradZ));
		}
		Hit shoot(Ray ray)
		{
			float dist = 0.0f;
			for (int i = 0; i < nSteps; i++)
			{
				vec3 pos = ray.start + dist * ray.dir;

				Hit hit = map(pos);
				if (hit.t <= minHitDist)
					return Hit(dist, hit.materialId);

				dist += hit.t;

				if (dist > maxTraceDist)
					break;
			}
			return Hit(-1, -1);
		}
		vec3 directLight(vec3 pos, vec3 N, int materialId)
		{
			vec3 radiance = materials[materialId].ka * La;

			vec3 V = normalize(camPos - pos);
			for (int i = 0; i < nLights; i++)
			{
				vec3 L = normalize(lights[i].wLightPos.xyz - pos.xyz * lights[i].wLightPos.w);
				vec3 H = normalize(L + V);

				float cost = max(dot(N, L), 0), cosd = max(dot(N, H), 0);

				radiance += (materials[materialId].kd * cost + materials[materialId].ks * pow(cosd, materials[materialId].shininess)) * lights[i].Le;
			}

			return radiance;
		}
		vec3 march(Ray ray)
		{
			Ray currentRay = ray;
			vec3 mask = vec3(1.0f);
			vec3 radiance = vec3(0.0f);
			for (int i = 0; i <= nReflections; i++)
			{
				Hit hit = shoot(currentRay);
				if (hit.t >= 0.0f)
				{
					vec3 pos = currentRay.start + hit.t * currentRay.dir;
					vec3 N = normal(pos);
					vec3 rad = directLight(pos, N, hit.materialId);
					radiance += mask * rad;

					mask *= coeffRefl;
					currentRay = Ray(pos + N * eps, reflect(currentRay.dir, N));
				}
				else if (i == 0)
				{
					radiance = La;
					break;
				}
			}

			// HDR
			radiance = radiance / (radiance + vec3(1.0f));
		
			// Gamma correction
			radiance= pow(radiance, vec3(1.0f / gamma));

			return radiance;
		}
		Ray getRay(vec2 fragCoord)
		{
			float px = (2.0f * fragCoord.x / winSize.x - 1.0f) * asp;
			float py = 2.0f * fragCoord.y / winSize.y - 1.0f;

			float scale = tan(fov * 0.5f);
			vec3 rayDir = normalize(px * scale * camRight + py * scale * camUp + camDir);
		
			return Ray(camPos, rayDir);
		}

		// entry point
		void main()
		{
			Ray ray = getRay(gl_FragCoord.xy);		
			vec3 color = march(ray);
		
			fragmentColor = vec4(color, 1.0f);

			// to avoid warnings
			float _dummy = time;
		}
	)";
};

struct Camera
{
	vec3 wEye, wLookat, wVup;
	float fov, asp;

	Camera(const vec3& eye, const vec3& lookAt, const vec3& vup)
		: wEye(eye), wLookat(lookAt), wVup(vup), fov(45.0f * M_PI / 180.0f), asp((float)winWidth / winHeight) { }

	void Bind(GPUProgram* shader)
	{
		vec3 dir = normalize(wLookat - wEye);
		vec3 right = normalize(cross(dir, wVup));
		vec3 up = normalize(cross(right, dir));

		shader->setUniform(wEye, "camPos");
		shader->setUniform(dir, "camDir");
		shader->setUniform(right, "camRight");
		shader->setUniform(up, "camUp");

		shader->setUniform(fov, "fov");
		shader->setUniform(asp, "asp");
	}
};

class RayMarching : public glApp
{
	RayMarchShader* shader;

	Camera* camera;
	ColorlessQuadFan* quad;

	Intersectable* shapes;

public:
	RayMarching() : glApp("Ray Marching") {}
	~RayMarching() { delete quad; delete camera; delete shader; }

	void onInitialization()
	{
		shader = new RayMarchShader(vec3(0.6f, 0.82f, 0.9f), 2, 0.3f);

		// lights
		shader->AddLight(Light{
			.Le = vec3(2.0f),
			.wLightPos = vec4(1.0f, 1.0f, 1.0f, 0.0f),
		});

		// materials
		shader->AddMaterial(Material{ // checkerboard 1
			.ka = vec3(0.9f),
			.kd = vec3(0.3f),
			.ks = vec3(0.0f),
			.shininess = 0
		});
		shader->AddMaterial(Material{ // checkerboard 2
			.ka = vec3(0.45f),
			.kd = vec3(0.15f),
			.ks = vec3(0.0f),
			.shininess = 0
		});
		shader->AddMaterial(Material{ // silver
			.ka = vec3(0.19225f),
			.kd = vec3(0.50754f),
			.ks = vec3(0.508273f),
			.shininess = 50
		});

		// shapes
		std::vector<Primitive*> checkerboard1;
		std::vector<Primitive*> checkerboard2;
		const int N = 3;
		for (int z = -N; z < N; z++)
		{
			for (int x = -N; x < N; x++)
			{
				if ((x + z) % 2 == 0)
					checkerboard1.push_back(new Box(vec3(x + 0.5f, -0.5f, z + 0.5f), vec3(0.5f), OT_UNION));
				else
					checkerboard2.push_back(new Box(vec3(x + 0.5f, -0.5f, z + 0.5f), vec3(0.5f), OT_UNION));
			}
		}
		shader->AddIntersectable(new Intersectable(checkerboard1, 0));
		shader->AddIntersectable(new Intersectable(checkerboard2, 1));

		shapes = shader->AddIntersectable(new Intersectable({
			new RoundedBox(vec3(0.5f, 1.5f, 0.0f), vec3(0.35f), 0.05f, OT_SMOOTH_DIFFERENCE, 0.2f),
			new Capsule(vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, 2.0f, 0.0f), 0.3f, OT_SMOOTH_UNION, 0.3f),
			new Sphere(vec3(-0.5f, 1.5f, 0.0f), 0.4f),
			}, 2));

		camera = new Camera(vec3(0.7f, 2.5f, 4.2f), vec3(-0.3f, 1.3f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
		quad = new ColorlessQuadFan({ { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } });
	}

	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		camera->Bind(shader->Get());
		quad->Draw(shader->Get());
	}

	void onTimeElapsed(float ts, float te)
	{
		Sphere* s = (Sphere*)shapes->Get(2);
		s->c = vec3(-0.75f - cosf(te) * 0.4f, 1.5f, 0.0f);
		s->Set(shader->Get());

		refreshScreen();
	}

} app;
