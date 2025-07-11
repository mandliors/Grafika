//=============================================================================================
// 3D Flag
//=============================================================================================
#define FILE_OPERATIONS
#include "framework.h"

// Gouraud shader (per-vertex shading)
const char* vertSourceGouraud = R"(
	#version 330				
    precision highp float;

	uniform mat4 MVP, M, Minv; // MVP, Model, Model-inverse
	uniform vec3 kd, ks, ka; // diffuse, specular, ambient ref
	uniform float shine; // shininess for specular ref
	uniform vec3 La, Le; // ambient and point sources
	uniform vec4 wLiPos; // pos of light source in world
	uniform vec3 wEye; // pos of eye in world
	
	
	layout(location = 0) in vec3 vtxPos; // pos in modeling space
	layout(location = 1) in vec3 vtxNorm; // normal in modeling space
	layout(location = 2) in vec2 vtxUV; // UV coords
	
	out vec3 color; // computed vertex color
	out vec2 texcoord; // UV coords to fragment shader
	
	void main() {
		gl_Position = MVP * vec4(vtxPos, 1); // to NDC
		vec4 wPos = M * vec4(vtxPos, 1);
		vec3 L = normalize( wLiPos.xyz * wPos.w - wPos.xyz * wLiPos.w);
		vec3 V = normalize(wEye - wPos.xyz/wPos.w);
		vec4 wNormal = vec4(vtxNorm, 0) * Minv;
		vec3 N = normalize(wNormal.xyz);
		vec3 H = normalize(L + V);
		float cost = max(dot(N, L), 0), cosd = max(dot(N, H), 0);
		color = ka * La + (kd * cost + ks * pow(cosd, shine)) * Le;
		texcoord = vtxUV; // pass UV coords to fragment shader
	}
)";
const char* fragSourceGouraud = R"(
	#version 330
    precision highp float;

	uniform sampler2D samplerUnit; // texture sampler

	in vec3 color; // interpolated color of vertex shader
	in vec2 texcoord; // UV coords from vertex shader

	out vec4 fragmentColor; // output goes to frame buffer

	void main() {
		vec4 texColor = texture(samplerUnit, texcoord);
		fragmentColor = vec4(color * texColor.rgb, 1); // modulate with texture color
	}
)";

// Phong shader (per-pixel shading)
const char* vertSourcePhong = R"(
	#version 330				
    precision highp float;

	uniform mat4 MVP, M, Minv; // MVP, Model, Model-inverse
	uniform vec4 wLiPos; // pos of light source
	uniform vec3 wEye; // pos of eye
	
	layout(location = 0) in vec3 vtxPos; // pos in model sp
	layout(location = 1) in vec3 vtxNorm;// normal in model sp
	layout(location = 2) in vec2 vtxUV; // UV coords
	
	out vec3 wNormal; // normal in world space
	out vec3 wView; // view in world space
	out vec3 wLight; // light dir in world space
	out vec2 texcoord; // UV coords in world space
	
	void main() {
		gl_Position = MVP * vec4(vtxPos, 1); // to NDC
		vec4 wPos = M * vec4(vtxPos, 1);
		wLight = wLiPos.xyz * wPos.w - wPos.xyz * wLiPos.w;
		wView = wEye - wPos.xyz/wPos.w;
		wNormal = (vec4(vtxNorm, 0) * Minv).xyz;
		texcoord = vtxUV; // pass UV coords to fragment shader
	}
)";
const char* fragSourcePhong = R"(
	#version 330
    precision highp float;

	uniform vec3 kd, ks, ka;// diffuse, specular, ambient ref
	uniform float shine; // shininess for specular ref
	uniform vec3 La, Le; // ambient and dir/point source rad

	uniform sampler2D samplerUnit;
	
	in vec2 texcoord; // UV coords from vertex shader
	in vec3 wNormal; // interpolated world sp normal
	in vec3 wView; // interpolated world sp view
	in vec3 wLight; // interpolated world sp illum dir
	
	out vec4 fragmentColor; // output goes to frame buffer
	
	void main() {
		vec3 N = normalize(wNormal);
		vec3 V = normalize(wView);
		vec3 L = normalize(wLight);
		vec3 H = normalize(L + V);
		float cost = max(dot(N,L), 0), cosd = max(dot(N,H), 0);
		vec3 color = ka * La + (kd * cost + ks * pow(cosd,shine)) * Le;

		vec4 texColor = texture(samplerUnit, texcoord);
		fragmentColor = vec4(color * texColor.rgb, 1); // modulate with texture color
	}
)";

// NPR shading (non-photorealistic shading)
const char* vertSourceNPR = R"(
	#version 330				
    precision highp float;

	uniform mat4 MVP, M, Minv; // MVP, Model, Model-inverse
	uniform vec4 wLiPos; // pos of light source
	uniform vec3 wEye; // pos of eye
	
	layout(location = 0) in vec3 vtxPos; // pos in model sp
	layout(location = 1) in vec3 vtxNorm;// normal in model sp
	layout(location = 2) in vec2 vtxUV; // UV coords
	
	out vec3 wNormal; // normal in world space
	out vec3 wView; // view in world space
	out vec3 wLight; // light dir in world space
	out vec2 texcoord; // UV coords in world space
	
	void main() {
		gl_Position = MVP * vec4(vtxPos, 1); // to NDC
		vec4 wPos = M * vec4(vtxPos, 1);
		wLight = wLiPos.xyz * wPos.w - wPos.xyz * wLiPos.w;
		wView = wEye - wPos.xyz/wPos.w;
		wNormal = (vec4(vtxNorm, 0) * Minv).xyz;
		texcoord = vtxUV; // pass UV coords to fragment shader
	}
)";
const char* fragSourceNPR = R"(
	#version 330
    precision highp float;

	uniform vec3 kd, ks, ka;// diffuse, specular, ambient ref
	uniform float shine; // shininess for specular ref
	uniform vec3 La, Le; // ambient and dir/point source rad

	uniform sampler2D samplerUnit;
	
	in vec3 wNormal; // interpolated world sp normal
	in vec3 wView; // interpolated world sp view
	in vec3 wLight; // interpolated world sp illum dir
	in vec2 texcoord; // UV coords from vertex shader
	
	out vec4 fragmentColor; // output goes to frame buffer
	
	void main() {
		vec3 N = normalize(wNormal);
		vec3 V = normalize(wView);
		vec3 L = normalize(wLight);
		float y = (dot(N, L) > 0.5) ? 1 : 0.5;
		vec3 color = (abs(dot(N, V)) < 0.2) ? vec3(0) : y * kd;

		vec4 texColor = texture(samplerUnit, texcoord);
		fragmentColor = vec4(color * texColor.rgb, 1); // modulate with texture color

		// to avoid unused variable warning
		vec3 dummy = kd * ks * ka * La * Le * shine;
	}
)";

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

struct VtxData 
{
	vec3 pos, normal;
	vec2 texcoord;
};

class ParamSurface : public Geometry<VtxData>
{
	int nVtxInStrip, nStrips;
	
	float rotAngle;
	vec3 translation, rotAxis, scaling;

	Material material;

public:
	ParamSurface(vec3 position, Material material)
		: translation(position), rotAxis(1.0f), rotAngle(0.0f), scaling(1.0f), material(material) { }
	virtual VtxData GenVtxData(float u, float v) = 0;
	void create(int M, int N)
	{
		nVtxInStrip = (M + 1) * 2;
		nStrips = N;
		vtx.clear();
		for (int i = 0; i < N; i++) 
		{
			for (int j = 0; j <= M; j++) 
			{
				vtx.push_back(GenVtxData((float)j / M, (float)i / N));
				vtx.push_back(GenVtxData((float)j / M, (float)(i + 1) / N));
			}
		}
		glEnableVertexAttribArray(0); // 0. regiszter = pozíció
		glEnableVertexAttribArray(1); // 1. regiszter = normál vektor
		glEnableVertexAttribArray(2); // 2. regiszter = textúra koordináta
		int nb = sizeof(VtxData);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, nb, (void*)offsetof(VtxData, pos));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, nb, (void*)offsetof(VtxData, normal));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, nb, (void*)offsetof(VtxData, texcoord));

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
			glDrawArrays(GL_TRIANGLE_STRIP, i * nVtxInStrip, nVtxInStrip);
	}
};

class Flag : public ParamSurface
{
public:
	float W, H, D, K, phase;

	Flag(float w, float h, float d, float k, float phase, Material material)
		: W(w), H(h), D(d), K(k), phase(phase), ParamSurface(vec3(-w * 0.5f, -h * 0.5f, 0.0f), material) { }

	VtxData GenVtxData(float u, float v) override
	{
		VtxData vd;
		vd.texcoord = vec2(u, 1.0f - v);
		float angle = u * K + phase;
		vd.pos = vec3(u * W, v * H, D * sin(angle));
		vd.normal = vec3(-K * D * cos(angle), 0, W);
		return vd;
	}
};

class Flag3D : public glApp 
{
	Camera* camera;
	Material material;
	Texture* texture;
	Light light;

	float flagPhase = 0.0f;
	Flag* flag;

	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók

public:
	Flag3D() : glApp("3D Flag") {}
	~Flag3D() { delete camera; delete texture; delete flag; delete gpuProgram; }

	// Inicializáció
	void onInitialization() 
	{
		glEnable(GL_DEPTH_TEST);

		material = Material { vec3(0.15f, 0.6f, 0.9f), vec3(0.05f, 0.2f, 0.3f), vec3(0.6f, 0.6f, 0.6f), 50.0f };
		texture = new Texture("image.png");
		light = Light{ vec3(0.4f), vec3(2.0f), vec4(1.0f, 1.0f, 1.0f, 0.0f) };

		camera = new Camera(vec3(0.0f, 0.0f, 5.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
		flag = new Flag(2.0f, 1.5f, 0.5f, 3.0f, 0.0f, material);

		//gpuProgram = new GPUProgram(vertSourceGouraud, fragSourceGouraud);
		gpuProgram = new GPUProgram(vertSourcePhong, fragSourcePhong);
		//gpuProgram = new GPUProgram(vertSourceNPR, fragSourceNPR);
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
		
		gpuProgram->setUniform(0, "samplerUnit");
		
		flag->Draw(camera, gpuProgram);
	}

	void onTimeElapsed(float startTime, float endTime)
	{
		flagPhase = fmodf(flagPhase + (endTime - startTime) * 5.0f, 2 * M_PI);
		flag->phase = flagPhase;
		flag->create(20, 20);
		refreshScreen();
	}

} app;
