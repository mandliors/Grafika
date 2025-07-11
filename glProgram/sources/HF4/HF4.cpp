//=============================================================================================
// HF4
//=============================================================================================
#include "framework.h"

const char* vertSource = R"(
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

const char* fragSource = R"(
	#version 330
   	precision highp float;

	uniform sampler2D samplerUnit;

	in vec2 texcoord;
	out vec4 fragmentColor;
	
	void main() {
		fragmentColor = texture(samplerUnit, texcoord);
	}
)";

const int winWidth = 600, winHeight = 600;
const int maxDepth = 5;

vec3 operator/(vec3 a, vec3 b) { return vec3(a.x / b.x, a.y / b.y, a.z / b.z); }

class Texture2D
{
	unsigned int vao, vbo[2];
	unsigned int textureId = 0;

public:
	Texture2D(int width, int height, vec3* data)
	{
		// setup VAO
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		// setup VBOs
		glGenBuffers(2, vbo);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		float vtxs[] = { -1, -1, 1, -1, 1, 1, -1, 1 };
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
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
			GL_RGB, GL_FLOAT, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	void UpdateData(int width, int height, vec3* data)
	{
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
			GL_RGB, GL_FLOAT, data);
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

struct Material
{
	vec3 n, kappa;
	vec3 ka, kd, ks;
	float shine;
	bool reflective, refractive, rough;
};

struct Ray
{
	vec3 start;
	vec3 dir; // egységvektor
	bool out; // kívül? vagy ior
};
struct Hit
{
	float t = -1.0f;
	vec3 position;
	vec3 normal;
	Material* material;
};

struct Intersectable
{
	Material* material;
	virtual Hit intersect(const Ray& ray) = 0;
	virtual ~Intersectable() { delete material; }
};

class Sphere : public Intersectable
{
	vec3 center;
	float radius;

public:
	Sphere(const vec3& center, float radius, Material* material)
		: center(center), radius(radius) {
		this->material = material;
	}

	Hit intersect(const Ray& ray) override
	{
		Hit hit;
		vec3 dist = ray.start - center;
		float a = dot(ray.dir, ray.dir);
		float b = dot(dist, ray.dir) * 2;
		float c = dot(dist, dist) - radius * radius;
		float discr = b * b - 4 * a * c;
		if (discr < 0) return hit; else discr = sqrtf(discr);
		float t1 = (-b + discr) / 2 / a, t2 = (-b - discr) / 2 / a;
		if (t1 <= 0) return hit; // t1 >= t2 for sure
		hit.t = (t2 > 0) ? t2 : t1;
		hit.position = ray.start + ray.dir * hit.t;
		hit.normal = (hit.position - center) / radius; // Only for sphere!!!
		hit.material = material;
		return hit;
	}
};
class Cylinder : public Intersectable
{
	vec3 s;  // talppont
	vec3 d0; // irány (normalizált)
	float r; // sugár
	float h; // magasság

public:
	Cylinder(const vec3& s, const vec3& d0, float r, float h, Material* material) : s(s), d0(normalize(d0)), r(r), h(h) { this->material = material; }

	Hit intersect(const Ray& ray) override
	{
		vec3 p = ray.start - s;
		float a = 1.0f - powf(dot(ray.dir, d0), 2);
		float b = 2.0f * (dot(p, ray.dir) - dot(p, d0) * dot(ray.dir, d0));
		float c = dot(p, p) - powf(dot(p, d0), 2) - r * r;

		Hit hit;
		float discr = b * b - 4 * a * c;
		if (discr < 0) return Hit(); else discr = sqrtf(discr);
		float t1 = (-b + discr) / 2 / a, t2 = (-b - discr) / 2 / a;
		if (t1 <= 0) return hit; // t1 >= t2 for sure
		hit.t = (t2 > 0) ? t2 : t1;
		hit.position = ray.start + ray.dir * hit.t;
		hit.normal = (hit.position - s - dot(hit.position - s, d0) * d0) / r;
		hit.material = material;

		// wrong direction or too far
		float d = dot(hit.position - s, d0);
		if (d < 0 || d > h) return Hit();

		return hit;
	}
};
class Cone : public Intersectable
{
	vec3 p;     // talppont
	vec3 d0;	// irány (normalizált)
	float alfa; // nyílásszög (radian)
	float h;	// magasság

public:
	Cone(const vec3& p, const vec3& d0, float alfa, float h, Material* material) : p(p), d0(normalize(d0)), alfa(alfa), h(h) { this->material = material; }

	Hit intersect(const Ray& ray) override
	{
		vec3 q = ray.start - p;

		float a = dot(ray.dir, d0);
		float b = dot(q, d0);
		float c = 1.0f;
		float d = dot(q, ray.dir);
		float e = dot(q, q);

		float A = a * a - c * powf(cos(alfa), 2);
		float B = 2.0f * (a * b - powf(cos(alfa), 2) * d);
		float C = b * b - powf(cos(alfa), 2) * e;

		Hit hit;
		float discr = B * B - 4 * A * C;
		if (discr < 0) return Hit(); else discr = sqrtf(discr);
		float t1 = (-B + discr) / 2 / A, t2 = (-B - discr) / 2 / A;
		hit.t = (t2 > 0 && t2 < t1) ? t2 : t1;
		if (hit.t > 0)
		{
			hit.position = ray.start + hit.t * ray.dir;

			vec3 x_p = hit.position - p;
			hit.normal = normalize(powf(cos(alfa), 2.0f) * x_p - dot(x_p, d0) * d0);
			hit.material = material;
		}

		// wrong direction or too far
		d = dot(hit.position - p, d0);
		if (d < 0 || d > h) return Hit();

		return hit;
	}
};
class PlaneXZ : public Intersectable
{
	vec3 p; // pont
	vec3 n; // normálvektor
	float w, h;

	Material* material2;

public:
	PlaneXZ(const vec3& p, const vec3& n, float w, float h, Material* material1, Material* material2)
		: p(p), n(normalize(n)), w(w), h(h), material2(material2) {
		this->material = material1;
	}

	~PlaneXZ() { delete material2; }

	Hit intersect(const Ray& ray) override
	{
		Hit hit;
		float t = (dot(n, p) - dot(n, ray.start)) / dot(n, ray.dir);
		if (t < 0) return hit;
		hit.t = t;
		hit.position = ray.start + ray.dir * hit.t;
		hit.normal = n;
		vec3 q = hit.position - p;
		if (abs(q.x) > w / 2 || abs(q.z) > h / 2) return Hit();
		q += vec3(w / 2, 0, h / 2);
		hit.material = (int(q.x) + int(q.z)) % 2 == 0 ? material : material2;

		return hit;
	}
};

class Camera
{
	vec3 eye, lookat;
	vec3 up, right;
	vec2 size;
	float fov;

public:
	Camera(const vec3& eye, const vec3& lookat, const vec3& up, const vec2& size, float fov)
		: eye(eye), lookat(lookat), up(up), right(normalize(cross(lookat - eye, up))), size(size), fov(fov* M_PI / 180.0f) {
	}

	Ray getRay(int x, int y)
	{
		vec3 forward = normalize(lookat - eye);
		float aspect = size.x / size.y;
		float fovScale = tanf(fov * 0.5f);

		float px = (2.0f * (x + 0.5f) / size.x - 1.0f) * aspect * fovScale;
		float py = (1.0f - 2.0f * (y + 0.5f) / size.y) * fovScale;
		vec3 dir = normalize(forward + px * right + py * up);

		return Ray{ eye, dir, true };
	}
};

struct Light
{
	vec3 direction;
	vec3 Le;
};

class Scene
{
	vec3 La;			// ambient light
	float Ne = 0.001f;  // epsilon

	vec3 image[winWidth * winHeight];
	Texture2D* texture = nullptr;

	std::vector<Light*> lights;
	std::vector<Intersectable*> objects;

	Camera* camera;
	float cameraAngle;

public:
	Scene(vec3 La) : La(La), cameraAngle(0.0f)
	{
		camera = new Camera({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { winWidth, winHeight }, 45.0f);
		updateCameraPosition();

		texture = new Texture2D(winWidth, winHeight, image);

		Material* checkerBoardMaterial1 = new Material{ vec3(0.0f), vec3(0.0f), vec3(0.9f, 0.9f, 0.9f), vec3(0.3f, 0.3f, 0.3f), vec3(0.0f), 0.0f, false, false, true };
		Material* checkerBoardMaterial2 = new Material{ vec3(0.0f), vec3(0.0f), vec3(0.0f, 0.3f, 0.9f), vec3(0.0f, 0.1f, 0.3f), vec3(0.0f), 0.0f, false, false, true };
		Material* goldenCylinderMaterial = new Material{ vec3(0.17f, 0.35f, 1.5f), vec3(3.1f, 2.7f, 1.9f), vec3(0.0f), vec3(0.0f), vec3(0.0f), 0.0f, true, false, false };
		Material* waterCylinderMaterial = new Material{ vec3(1.3f), vec3(0.0f), vec3(0.0f), vec3(0.0f), vec3(0.0f), 0.0f, true, true, false };
		Material* plasticCylinderMaterial = new Material{ vec3(0.0f), vec3(0.0f), vec3(0.9f, 0.6f, 0.3f), vec3(0.3f, 0.2f, 0.1f), vec3(2.0f), 50.0f, false, false, true };
		Material* cyanConeMaterial = new Material{ vec3(0.0f), vec3(0.0f), vec3(0.3f, 0.6f, 0.9f), vec3(0.1f, 0.2f, 0.3f), vec3(2.0f), 100.0f, false, false, true };
		Material* magentaConeMaterial = new Material{ vec3(0.0f), vec3(0.0f), vec3(0.9f, 0.0f, 0.6f), vec3(0.3f, 0.0f, 0.2f), vec3(2.0f), 20.0f, false, false, true };

		objects.push_back(new PlaneXZ(vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f), 20.0f, 20.0f, checkerBoardMaterial1, checkerBoardMaterial2));
		objects.push_back(new Cylinder(vec3(1.0f, -1.0f, 0.0f), vec3(0.1f, 1.0f, 0.0f), 0.3f, 2.0f, goldenCylinderMaterial));
		objects.push_back(new Cylinder(vec3(0.0f, -1.0f, -0.8f), vec3(-0.2f, 1.0f, -0.1f), 0.3f, 2.0f, waterCylinderMaterial));
		objects.push_back(new Cylinder(vec3(-1.0f, -1.0f, 0.0f), vec3(0.0f, 1.0f, 0.1f), 0.3f, 2.0f, plasticCylinderMaterial));
		objects.push_back(new Cone(vec3(0.0f, 1.0f, 0.0f), vec3(-0.1f, -1.0f, -0.05f), 0.2f, 2.0f, cyanConeMaterial));
		objects.push_back(new Cone(vec3(0.0f, 1.0f, 0.8f), vec3(0.2f, -1.0f, 0.0f), 0.2f, 2.0f, magentaConeMaterial));

		lights.push_back(new Light{ normalize(vec3{ 1.0f, 1.0f, 1.0f }), vec3{ 2.0f } });
	}
	~Scene()
	{
		delete camera;
		for (Intersectable* obj : objects) delete obj;
		for (Light* light : lights) delete light;
		delete texture;
	}

	void build()
	{
		for (int i = 0; i < winHeight; i++)
		{
			for (int j = 0; j < winWidth; j++)
			{
				Ray ray = camera->getRay(j, i);
				vec3 color = trace(ray, objects, maxDepth);
				image[i * winWidth + j] = color;
			}
		}
		texture->UpdateData(winWidth, winHeight, image);
	}
	void render(GPUProgram* gpuProgram)
	{
		texture->Draw(gpuProgram);
	}
	void rotate()
	{
		cameraAngle -= M_PI / 4.0f;
		updateCameraPosition();
	}
	void updateCameraPosition()
	{
		vec3 newPos = vec3(4.0f * sin(cameraAngle), 1.0f, 4.0f * cos(cameraAngle));

		delete camera;
		camera = new Camera(newPos, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { winWidth, winHeight }, 45.0f);
	}
	Hit firstIntersect(Ray ray, const std::vector<Intersectable*>& objects)
	{
		Hit bestHit;
		for (Intersectable* obj : objects)
		{
			Hit hit = obj->intersect(ray);
			if (hit.t > 0 && (bestHit.t < 0 || hit.t < bestHit.t))
				bestHit = hit;
		}
		if (dot(ray.dir, bestHit.normal) > 0)
			bestHit.normal = bestHit.normal * -1.0f;

		return bestHit;
	}
	vec3 trace(Ray ray, const std::vector<Intersectable*>& objects, int maxDepth)
	{
		// recursion limit
		if (maxDepth <= 0)
			return La;

		Hit hit = firstIntersect(ray, objects);
		// no hit
		if (hit.t < 0)
			return La;

		vec3 outRad(0, 0, 0);

		// rough
		if (hit.material->rough)
			outRad = DirectLight(hit, ray, lights);

		// reflective
		if (hit.material->reflective)
		{
			Ray reflectRay{ hit.position + Ne * hit.normal, reflect(ray.dir, hit.normal), ray.out };
			outRad += trace(reflectRay, objects, maxDepth - 1) * Fresnel(ray.dir, hit.normal, hit.material->n, hit.material->kappa);
		}

		// refractive
		if (hit.material->refractive)
		{
			float ior = ray.out ? hit.material->n.x : 1.0f / hit.material->n.x;
			vec3 refractionDir = refract(ray.dir, hit.normal, ior);
			if (length(refractionDir) > 0)
			{
				Ray refractRay{ hit.position - Ne * hit.normal, refractionDir, !ray.out };
				outRad += trace(refractRay, objects, maxDepth - 1) * (vec3(1, 1, 1) - Fresnel(ray.dir, hit.normal, hit.material->n, hit.material->kappa));
			}
		}

		return outRad;
	}

private:
	vec3 reflect(vec3 V, vec3 N)
	{
		return V - N * dot(N, V) * 2.0f;
	};
	vec3 refract(vec3 V, vec3 N, float ns)
	{
		float cosa = -dot(V, N);
		float disc = 1 - (1 - cosa * cosa) / ns / ns;
		if (disc < 0) return vec3(0, 0, 0);
		return V / ns + N * (cosa / ns - sqrt(disc));
	}
	vec3 Fresnel(vec3 V, vec3 N, vec3 n, vec3 kappa)
	{
		static const vec3 one(1, 1, 1);
		float cosa = -dot(V, N);
		vec3 F0 = ((n - one) * (n - one) + kappa * kappa) /
			((n + one) * (n + one) + kappa * kappa);
		return F0 + (one - F0) * powf(1 - cosa, 5);
	}
	vec3 shade(vec3 N, vec3 V, vec3 L, vec3 inRad, vec3 kd, vec3 ks, float shine)
	{
		float cosTheta = dot(N, L); // unit vecs
		if (cosTheta < 0) return vec3(0, 0, 0); // self shadow
		vec3 diffuseRad = inRad * kd * cosTheta; // diffuse
		vec3 H = normalize(V + L);
		float cosDelta = dot(N, H);
		if (cosDelta < 0) return diffuseRad;
		return diffuseRad + inRad * ks * powf(cosDelta, shine);
	}
	vec3 DirectLight(Hit hit, Ray ray, const std::vector<Light*> lights)
	{
		vec3 outRad = hit.material->ka * La;
		for (const auto& light : lights)
		{
			Ray shadowRay{ hit.position + Ne * hit.normal, light->direction };
			Hit shadowHit = firstIntersect(shadowRay, objects);
			if (shadowHit.t < 0)
			{
				vec3 L = normalize(light->direction);
				vec3 V = -ray.dir;
				vec3 inRad = light->Le;
				outRad += shade(hit.normal, V, L, inRad, hit.material->kd, hit.material->ks, hit.material->shine);
			}
		}
		return outRad;
	}
};


class HF4 : public glApp
{
	GPUProgram* gpuProgram;
	Scene* scene;

public:
	HF4() : glApp("HF4") {}
	~HF4() { delete gpuProgram; delete scene; }

	void onInitialization()
	{
		gpuProgram = new GPUProgram(vertSource, fragSource);
		scene = new Scene(vec3(1.0f) * 0.4f);

		scene->build();
		refreshScreen();
	}

	void onDisplay()
	{
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		scene->render(gpuProgram);
	}

	void onKeyboard(int key)
	{
		if (key == 'a')
			scene->rotate();

		scene->build();
		refreshScreen();
	}
};

HF4 app;

