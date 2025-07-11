//=============================================================================================
// Hullámvasút
//=============================================================================================
#include "framework.h"
#include <iostream>

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330
    precision highp float;

	uniform mat4 MVP;
	layout(location = 0) in vec2 vertexPosition;
	
	void main()
	{
		gl_Position = MVP * vec4(vertexPosition, 0, 1);
	}
)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;			// konstans szín
	out vec4 fragmentColor;		// pixel szín

	void main()
	{
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

const int winWidth = 600, winHeight = 600;
const float G = 40.0f, LAMBDA = 1.0f;

namespace math2D {
	inline mat4 translate(const vec2& v) { return translate(mat4(1.0f), vec3(v.x, v.y, 0.0f)); }
	inline mat4 scale(const vec2& v) { return scale(mat4(1.0f), vec3(v.x, v.y, 1.0f)); }
	inline mat4 rotateXY(float angle) { return rotate(mat4(1.0f), angle, vec3(0.0f, 0.0f, 1.0f)); }
}

class Camera2D {
	vec2 wCenter; // center in world coords
	vec2 wSize;   // width and height in world coords

public:
	Camera2D(const vec2& wCenter, const vec2& wSize) : wCenter(wCenter), wSize(wSize) {}

	mat4 V() { return translate(vec3(-wCenter.x, -wCenter.y, 0)); }
	mat4 P() { return scale(vec3(2 / wSize.x, 2 / wSize.y, 1)); }
	mat4 Vinv() { return translate(vec3(wCenter.x, wCenter.y, 0)); }
	mat4 Pinv() { return scale(vec3(wSize.x / 2, wSize.y / 2, 1)); }

	void Zoom(float s) { wSize = wSize * s; }
	void Pan(vec2 t) { wCenter = wCenter + t; }
};

class Object : public Geometry<vec2>
{
public:
	void Update()
	{
		vtx = GenVertexData();
		if (vtx.size() > 0)
			updateGPU();
	}
	virtual void Render(GPUProgram* gpuProgram, Camera2D* camera, vec3 color)
	{
		mat4 M = math2D::translate(position) * math2D::rotateXY(rotation) * math2D::scale(scale);
		mat4 MVP = camera->P() * camera->V() * M;
		gpuProgram->setUniform(MVP, "MVP");
		Draw(gpuProgram, GL_LINE_STRIP, color);
	}

protected:
	virtual std::vector<vec2> GenVertexData() = 0;

protected:
	vec2 position = vec2(0, 0);
	vec2 scale = vec2(1, 1);
	float rotation = 0;
};

const int nTessVertices = 500;
class Spline : public Object
{
	Geometry<vec2>* controlPoints;
	std::vector<float> knotValues;

public:
	Spline() : Object() { controlPoints = new Geometry<vec2>(); }
	~Spline() { delete controlPoints; }

	void AddControlPoint(vec2 cp)
	{
		controlPoints->Vtx().push_back(cp);
		controlPoints->updateGPU();

		knotValues.push_back(69);
		for (int i = 0; i < controlPoints->Vtx().size(); i++)
			knotValues[i] = (float)i / (controlPoints->Vtx().size() - 1);
	}
	void Render(GPUProgram* gpuProgram, Camera2D* camera, vec3 color) override
	{
		Object::Render(gpuProgram, camera, color);
		controlPoints->Draw(gpuProgram, GL_POINTS, { 1.0f, 0.0f, 0.0f });
	}

	std::vector<vec2> GenVertexData() override
	{
		if (controlPoints->Vtx().size() < 2)
			return {};

		std::vector<vec2> vertices;
		for (int i = 0; i <= nTessVertices; ++i)
		{
			float t = (float)i / nTessVertices;
			vertices.push_back(r(t));
		}
		return vertices;
	}

	bool isOnTrack(float tau) const { return glm::dot(vec2{ 0, -G }, N(tau)) + powf(v(tau), 2.0f) * kappa(tau) <= 0.0f; }
	inline float kappa(float tau) const { return glm::dot(r_prime_prime(tau), N(tau)) / powf(glm::length(r_prime(tau)), 2.0f); }
	inline float omega(float tau, float dt) const { return dphi(tau, dt) / dt; }
	inline float dtau(float tau, float dt) const { return v(tau) * dt / glm::length(r_prime(tau)); }
	inline float dphi(float tau, float dt) const { return acosf(glm::dot(T(tau), T(tau + dtau(tau, dt)))); }
	inline float v(float tau) const { return sqrtf(2 * G * (r(0).y - r(tau).y) / (1 + LAMBDA)); }
	inline vec2 T(float tau) const { return glm::normalize(r_prime(tau)); }
	mat4 M(float tau) const
	{
		vec2 t = T(tau);
		vec2 n = N(tau);

		/*
			Tekintsük a T és az N bázisvektorokból álló B bázist (B := [T|N]).
			Ekkor (bázistranszformáció): B * p_B = p => p_B = B^-1 * p
			Mivel B egy ortonormált bázis, ezért B^-1 = B^T, vagyis B^T:
		*/
		return mat4(
			t.x, t.x, 0.0f, 0.0f,
			n.y, n.y, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);

		//TODO: maybe this?
		/*return mat4(
			t.x,  n.x,  0.0f, 0.0f,
			t.y,  n.y,  0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		);*/
	}
	vec2 N(float tau) const
	{
		vec2 t = T(tau);
		return vec2(-t.y, t.x);
	}
	vec2 r(float tau) const
	{
		int i = GetIndexByTau(tau);
		const auto& cps = controlPoints->Vtx();

		vec2 v0(0, 0), v1(0, 0);
		if (i > 0)
			v0 = (cps[i + 1] - cps[i - 1]) / (knotValues[i + 1] - knotValues[i - 1]);
		if (i < cps.size() - 2)
			v1 = (cps[i + 2] - cps[i]) / (knotValues[i + 2] - knotValues[i]);

		return Hermite(
			cps[i],
			v0,
			knotValues[i],

			cps[i + 1],
			v1,
			knotValues[i + 1],

			tau
		);
	}
	vec2 r_prime(float tau) const
	{
		int i = GetIndexByTau(tau);
		const auto& cps = controlPoints->Vtx();

		vec2 v0(0, 0), v1(0, 0);
		if (i > 0)
			v0 = (cps[i + 1] - cps[i - 1]) / (knotValues[i + 1] - knotValues[i - 1]);
		if (i < cps.size() - 2)
			v1 = (cps[i + 2] - cps[i]) / (knotValues[i + 2] - knotValues[i]);

		return HermiteDerivative(
			cps[i],
			v0,
			knotValues[i],

			cps[i + 1],
			v1,
			knotValues[i + 1],

			tau
		);
	}
	vec2 r_prime_prime(float tau) const
	{
		int i = GetIndexByTau(tau);
		const auto& cps = controlPoints->Vtx();

		vec2 v0(0, 0), v1(0, 0);
		if (i > 0)
			v0 = (cps[i + 1] - cps[i - 1]) / (knotValues[i + 1] - knotValues[i - 1]);
		if (i < cps.size() - 2)
			v1 = (cps[i + 2] - cps[i]) / (knotValues[i + 2] - knotValues[i]);

		return HermiteSecondDerivative(
			cps[i],
			v0,
			knotValues[i],

			cps[i + 1],
			v1,
			knotValues[i + 1],

			tau
		);
	}
	vec2 Hermite(const vec2& p0, const vec2& v0, float t0, const vec2& p1, const vec2& v1, float t1, float t) const
	{
		float dt = t1 - t0;
		vec2 a0 = p0, a1 = v0;
		vec2 a2 = 3.0f * (p1 - p0) / dt / dt - (v1 + 2.0f * v0) / dt;
		vec2 a3 = 2.0f * (p0 - p1) / dt / dt / dt + (v1 + v0) / dt / dt;

		dt = t - t0;
		return ((a3 * dt + a2) * dt + a1) * dt + a0;
	}
	vec2 HermiteDerivative(const vec2& p0, const vec2& v0, float t0, const vec2& p1, const vec2& v1, float t1, float t) const
	{
		float dt = t1 - t0;
		vec2 a1 = v0;
		vec2 a2 = 3.0f * (p1 - p0) / powf(dt, 2) - (v1 + 2.0f * v0) / dt;
		vec2 a3 = 2.0f * (p0 - p1) / powf(dt, 3) + (v1 + v0) / powf(dt, 2);

		dt = t - t0;
		return 3.0f * a3 * powf(dt, 2) + 2.0f * a2 * dt + a1;
	}
	vec2 HermiteSecondDerivative(const vec2& p0, const vec2& v0, float t0, const vec2& p1, const vec2& v1, float t1, float t) const
	{
		float dt = t1 - t0;
		vec2 a2 = 3.0f * (p1 - p0) / dt / dt - (v1 + 2.0f * v0) / dt;
		vec2 a3 = 2.0f * (p0 - p1) / dt / dt / dt + (v1 + v0) / dt / dt;

		return 6.0f * a3 * (t - t0) + 2.0f * a2;
	}
	int GetIndexByTau(float tau) const
	{
		const auto& cps = controlPoints->Vtx();
		for (int i = 0; i < cps.size() - 1; i++)
			if (knotValues[i] <= tau && tau <= knotValues[i + 1])
				return i;
	
		return -1;
	}
};

class Gondola : public Object
{
	enum class State {
		IDLE, MOVING, FLOATING
	} state = State::IDLE;

	float tau;
	const Spline& spline;
	const float radius = 1.0f;

public:
	Gondola(const Spline& spline)
		: Object(), state(State::IDLE), tau(0.0f), spline(spline)
	{
		scale = vec2{ radius, radius };
		Update();
	}
	void Start()
	{
		tau = 0.01f;
		rotation = 0.0f;
		state = State::MOVING;
	}
	bool Animate(float dt)
	{
		if (state != State::MOVING)
			return false;

		static auto prevPos = spline.r(tau) + spline.N(tau) * radius;
		position = spline.r(tau) + spline.N(tau) * radius;
		auto posDiff = position - prevPos;
		float dphi = glm::length(posDiff) / radius;
		rotation -= dphi;

		//rotation += spline.dphi(tau, dt);
		
		/*if (!spline.isOnTrack(tau))
		{
			state = State::FLOATING;
			return false;
		}*/

		tau += spline.dtau(tau, dt);
		if (tau > 1)
		{
			Start();
			return false;
		}
		prevPos = position;

		return true;
	}
	void Render(GPUProgram* gpuProgram, Camera2D* camera, vec3 color) override
	{
		if (state == State::IDLE)
			return;

		// set transformation matrix in the vertex shader based on the position and rotation
		mat4 M = math2D::translate(position) * math2D::rotateXY(rotation) * math2D::scale(scale);
		mat4 MVP = camera->P() * camera->V() * M;
		gpuProgram->setUniform(MVP, "MVP");

		// draw blue circle
		Draw(gpuProgram, GL_TRIANGLE_FAN, color);

		// draw white border
		std::vector<vec2> borderVtx(vtx.begin() + 1, vtx.end());
		std::swap(vtx, borderVtx);
		updateGPU();
		Draw(gpuProgram, GL_LINE_LOOP, { 1.0f, 1.0f, 1.0f });
		std::swap(vtx, borderVtx);
		updateGPU();

		// draw diagonals (from center to circumference)
		std::vector<vec2> vtxDiagonals;
		for (int i = 1; i < vtx.size(); i += 6)
		{
			vtxDiagonals.push_back(vtx[0]); // Center (0, 0)
			vtxDiagonals.push_back(vtx[i]); // Circumference point
		}
		std::swap(vtx, vtxDiagonals);
		updateGPU();
		Draw(gpuProgram, GL_LINES, { 1.0f, 1.0f, 1.0f });
		std::swap(vtx, vtxDiagonals);
		updateGPU();
	}

private:
	std::vector<vec2> GenVertexData() override
	{
		static const int resolution = 36;
		static const float dangle = 2 * M_PI / resolution;

		std::vector<vec2> vertices;
		vertices.push_back({ 0.0f, 0.0f });
		for (float angle = 0.0f; angle < 2 * M_PI + dangle * 0.5f; angle += dangle)
			vertices.push_back(vec2{ cosf(angle), sinf(angle) });

		return vertices;

	}
};

class HF2 : public glApp {
	Spline* spline;
	Gondola* gondola;
	Camera2D* camera;
	GPUProgram* gpuProgram;

public:
	HF2() : glApp("Roller coaster") {}
	~HF2() { delete spline; delete gondola; delete gpuProgram; }

	void onInitialization() override
	{
		glPointSize(10);
		glLineWidth(3);

		spline = new Spline();
		gondola = new Gondola(*spline);
		camera = new Camera2D({ 0, 0 }, { 20, 20 });
		gpuProgram = new GPUProgram(vertSource, fragSource);
	}
	void onDisplay() override
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, winWidth, winHeight);

		spline->Render(gpuProgram, camera, vec3(1.0f, 1.0f, 0.0f));
		gondola->Render(gpuProgram, camera, vec3(0.0f, 0.0f, 1.0f));
	}

	void onMousePressed(MouseButton but, int pX, int pY) override
	{
		if (but != MouseButton::MOUSE_LEFT)
			return;

		spline->AddControlPoint(ScreenToWorld(pX, pY));
		spline->Update();

		refreshScreen();
	}
	void onKeyboard(int key) override
	{
		if (key == ' ')
			gondola->Start();
	}
	void onTimeElapsed(float startTime, float endTime) override
	{
		const static float dt = 0.01; // dt is ”infinitesimal”
		for (float t = startTime; t < endTime; t += dt)
		{
			float Dt = fmin(dt, endTime - t);
			if (gondola->Animate(Dt))
				refreshScreen();
			else
				break;
		}
	}

private:
	vec2 ScreenToWorld(int x, int y) const
	{
		return {
			x / (float)winWidth * 20.0f - 10.0f,
			10.0f - y / (float)winHeight * 20.0f
		};
	}
} app;