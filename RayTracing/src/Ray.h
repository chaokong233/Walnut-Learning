#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <algorithm>
#include <memory>
#include <vector>

class Material;

uint32_t uint_Power(uint32_t src, float para);

struct HitResult
{
	bool isHit {false};
	glm::vec3 hitPosition;
	glm::vec3 hitNormal;
	double t;
	float u;
	float v;
	bool isFrontFace {false};
	std::shared_ptr<Material> material;
};

class Color
{
public:
	Color() = default;

	Color(float r, float g, float b, float a)
		:r(r), b(b), g(g), a(a)	{}

	Color(float v)
		:r(v), b(v), g(v), a(1)	{}

	Color(const Color& other)
	{
		this->r = other.r;
		this->g = other.g;
		this->b = other.b;
		this->a = other.a;
	}

	Color(Color&& other) noexcept
	{
		this->r = other.r;
		this->g = other.g;
		this->b = other.b;
		this->a = other.a;
	}

	Color& operator=(const Color& other)
	{
		this->r = other.r;
		this->g = other.g;
		this->b = other.b;
		this->a = other.a;
		return *this;
	}

	Color(glm::vec3 vector)
	{
		r = vector.x;
		g = vector.y;
		b = vector.z;
		a = 1;
	}

	Color(glm::vec4& vector)
	{
		r = vector.x;
		g = vector.y;
		b = vector.z;
		a = vector.w;
	}


	uint32_t GetColorData()
	{
		uint32_t color = 0;
		// Check
		r = std::clamp(r, 0.0f, 1.0f);
		g = std::clamp(g, 0.0f, 1.0f);
		b = std::clamp(b, 0.0f, 1.0f);
		a = std::clamp(a, 0.0f, 1.0f);

		color |= (uint32_t)(r * 255) << 0;
		color |= (uint32_t)(g * 255) << 8;
		color |= (uint32_t)(b * 255) << 16;
		color |= (uint32_t)(a * 255) << 24;

		return  color;
	}

	Color MulWithoutAlpha(Color para) const
	{
		Color res(*this);
		res.r *= para.r;
		res.g *= para.g;
		res.b *= para.b;
		return res;
	}

	Color MulWithoutAlpha(float para) const
	{
		Color res(*this);
		res.r *= para;
		res.g *= para;
		res.b *= para;
		return res;
	}

	static Color Lerp(Color col1, Color col2, float alpha);
	static Color Pow(Color& col, float para, bool isAffectAlpha = false);

	// Operator
	template<typename T>
	Color operator/(T para)
	{
		Color res(*this);
		res.r /= para;
		res.g /= para;
		res.b /= para;
		res.a /= para;
		return res;
	}

	template<typename T>
	Color operator*(T para)
	{
		Color res(*this);
		res.r *= para;
		res.g *= para;
		res.b *= para;
		res.a *= para;
		return res;
	}

	Color operator+(Color& para)
	{
		Color res(*this);
		res.r += para.r;
		res.g += para.g;
		res.b += para.b;
		res.a += para.a;
		return res;
	}

	Color operator-(Color& para)
	{
		Color res(*this);
		res.r -= para.r;
		res.g -= para.g;
		res.b -= para.b;
		res.a -= para.a;
		return res;
	}

	double Norm()
	{
		return std::sqrt(r * r + b * b + g * g);
	}

	double NormSqr()
	{
		return r * r + b * b + g * g;
	}

	float r;
	float b;
	float g;
	float a;
private:
};

class Ray
{
public:
	Ray() = default;

 	Ray(glm::vec3 origin, glm::vec3 dir)
		:origin_(origin), direction_(dir) {}

	glm::vec3 at(float t) const
	{
		return origin_ + t * direction_;
	}

	glm::vec3 origin() const { return origin_; }
	glm::vec3 direction() const { return direction_; }

private:
	glm::vec3 origin_;
	glm::vec3 direction_;

};