#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <algorithm>
#include <memory>
#include <vector>
#include <set>

#include "Ray.h"
#include "bvh/bvh.h"

#define useTrianglePrimitive 1

struct HitResult;
class Ray;
class Color;
class Material;
class Rect;
class Light;
class Model;

bvh_Vec3 trans_Vec3_to_bvhVec3(glm::vec3 v);
bvh_Vec3 trans_Vec3_to_bvhVec3(glm::vec4 v);

class Hittable
{
public:
	virtual bool hit(const Ray& ray, double t_min, double t_max, HitResult& result) = 0;

	bool bvh_hit(const bvh_Ray& ray, HitResult& result)
	{
		double t_min = ray.tmin;
		double t_max = ray.tmax;
		auto& o = ray.org;
		auto& d = ray.dir;
		Ray r(glm::vec3(o[0], o[1], o[2]), glm::vec3(d[0], d[1], d[2]));
		return hit(r, t_min, t_max, result);
	}

	// bvh
	virtual bvh_Vec3 GetBvhCenter() = 0;
	virtual bvh_BBox GetBvhBBox() = 0;

};

class RenderedNode
{
public:
	std::shared_ptr<Material> material;
};

class Node
{
public:
	Node() = default;

	Node(glm::mat4 localTrans, std::weak_ptr<Node> parent)
        : localTransform(localTrans), parent_(parent)
    {
        glm::mat4 parMat = glm::mat4(1.0);
        if (auto it = parent.lock()) parMat = it->worldTransform;
        refreshTransform(parMat);
    }

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refreshTransform(worldTransform);
        }
    }

	glm::mat4 worldTransform{glm::mat4(1)};
	glm::mat4 localTransform{glm::mat4(1)};
	std::weak_ptr<Node> parent_;
private:
	// 
	std::vector<std::shared_ptr<Node>> children;

};

// 顶点
struct Vertex
{
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 normal;
    alignas(16) glm::vec3 tangent;
    alignas(16) glm::vec2 texcoord;
};

class Triangle : public Hittable, public RenderedNode
{
public:
    friend class PreTriangle;

    Triangle(std::array<Vertex, 3> _verteies, std::shared_ptr<Material> material);

    Triangle(const Triangle& tri) = default;
    Triangle& operator=(const Triangle& tri) = default;

    virtual bool hit(const Ray& ray, double t_min, double t_max, HitResult& result) override { return false; }

	// bvh
	virtual bvh_Vec3 GetBvhCenter() override;
	virtual bvh_BBox GetBvhBBox() override;

private:
    std::array<Vertex, 3> verteies;
};


class MovableNode : public Node
{
public:
	MovableNode() = default;

	inline void SetPosition(glm::vec3 pos) { node_position = pos; }
	inline void SetRotate(glm::vec3 rot) { node_rotate = rot; }
	inline void SetScale(glm::vec3 sca) { node_scale = sca; }

	void CalculateTrans();

	glm::vec3 node_position	{0,0,0};
	glm::vec3 node_rotate	{0,0,0};
	glm::vec3 node_scale	{1,1,1};
};

// =====================Light=============================
struct LightSample
{
	Light* light;
	Color sampleColor;
	glm::vec3 samplePosition;
	glm::vec3 rayDirection;
};

class Light
{
public:
	friend class LightComparer;

	virtual LightSample sample() const = 0;

	inline void SetPower(float v) { this->power_ = v; }
	inline float GetPower() const { return power_; }

	float power_;
protected:
};

class AreaLight : public Light, public MovableNode
{
public:
	AreaLight();

	AreaLight(const Rect& rect, Color col, float pow = 1.0f);

	void Precompute();
	virtual LightSample sample() const override;

private:
	Color color_;

	glm::vec4 v1{ -.5f,0,-.5f, 1.0f };
	glm::vec4 v2{ -.5f,0,.5f, 1.0f };
	glm::vec4 v3{ .5f,0,.5f, 1.0f };
	glm::vec4 v4{ .5f,0,-.5f, 1.0f };
	glm::vec3 edge1_;
	glm::vec3 edge2_;
	glm::vec3 normal_ { 0,1,0 };

};

struct LightComparer
{
	bool operator()(const std::shared_ptr<Light>& l1, const std::shared_ptr<Light>& l2) const
	{
		return l1->power_ > l2->power_;
	}
};

class SceneLightList
{
public:
	SceneLightList() = default;

	void Add(std::shared_ptr<Light> l) { lightList_.push_back(l); }
	void Build();
	std::shared_ptr<Light> randomLight(float& pickProbablity);

	uint32_t size() const { return lightList_.size(); }

private:
	std::vector<std::shared_ptr<Light>> lightList_;
	float totalPower_{ 0 };
};

// ===================== Material ====================
class Material
{
public:
	virtual bool scatter(const Ray& r_in, const HitResult& rec, Color& attenuation, Ray& scattered) const = 0;
	// 对特定方向采样时的Attenuation
	virtual Color getAttenuation(const Ray& r_in, const HitResult& rec, const Ray& r_out) const = 0;
	virtual Color emitted(double u, double v, const glm::vec3& p) const { return Color(0, 0, 0, 1); }
	bool isTracingDirectLight = true;
};

class Lambertian : public Material
{
public:
	Lambertian(const Color& a)
		:albedo_(a)
	{
		Material::isTracingDirectLight = true;
	}

	// Getter
	Color GetAlbedo() const { return albedo_; }

	// Setter
	void SetAlbedo(const Color& a) { albedo_ = a; }

	virtual bool scatter(const Ray& r_in, const HitResult& rec, Color& attenuation, Ray& scattered) const override;
	virtual Color getAttenuation(const Ray& r_in, const HitResult& rec, const Ray& r_out) const;

private:
	Color albedo_;
};

class Metal : public Material
{
public:
	Metal()
	{
		Material::isTracingDirectLight = false;
	};

	Metal(const Color& a)
		:albedo_(a)
	{
		Material::isTracingDirectLight = false;
	}

	// Getter
	Color GetAlbedo() const { return albedo_; }
	// Setter
	void SetAlbedo(const Color& a) { albedo_ = a; }
	void SetRoughness(float v) { roughness_ = std::clamp(v, 0.0f, 1.0f); }

	virtual bool scatter(const Ray& r_in, const HitResult& rec, Color& attenuation, Ray& scattered) const override;
	virtual Color getAttenuation(const Ray& r_in, const HitResult& rec, const Ray& r_out) const override { return Color(0); };

private:
	Color albedo_ {1,1,1,1};
	float roughness_ = 0;
};

class Refraction : public Material
{
public:
	Refraction() = default;

	Refraction(const Color& a)
		:albedo_(a) {}

	// Getter
	Color GetAlbedo() const { return albedo_; }

	// Setter
	void SetAlbedo(const Color& a) { albedo_ = a; }
	void SetRoughness(float v) { roughness_ = std::clamp(v, 0.0f, 1.0f); }
	void SetRefractive(float v) { refractive_ = v; }

	virtual bool scatter(const Ray& r_in, const HitResult& rec, Color& attenuation, Ray& scattered) const override;
	static double schlick(double cosine, double ref_idx);


private:
	Color albedo_ {1,1,1,1};
	float roughness_ = 0;
	float refractive_ = 1.3f;
};

class PureRefraction : public Material
{
public:
	PureRefraction()
	{
		Material::isTracingDirectLight = false;
	};

	PureRefraction(const Color& a)
		:albedo_(a)
	{
		Material::isTracingDirectLight = false;
	}

	// Getter
	Color GetAlbedo() const { return albedo_; }

	// Setter
	void SetAlbedo(const Color& a) { albedo_ = a; }
	void SetRoughness(float v) { roughness_ = std::clamp(v, 0.0f, 1.0f); }
	void SetRefractive(float v) { refractive_ = v; }

	virtual bool scatter(const Ray& r_in, const HitResult& rec, Color& attenuation, Ray& scattered) const override;

private:
	Color albedo_ {1,1,1,1};
	float roughness_ = 0;
	float refractive_ = 1.3f;
};

//======================Light============================
class Diffuse_Emissive : public Material
{
public:
	Diffuse_Emissive() = default;

	Diffuse_Emissive(Color a)
		:color_(a) {}

	Diffuse_Emissive(Color a, float in)
		:color_(a), intensity_(in) {}

	// Getter
	Color GetEmissiveColor() const { return color_; }
	float GetIntensity() const { return intensity_; }

	// Setter
	void SetEmissiveColor(Color a) { color_ = a; }
	void SetIntensity(float v) { intensity_ = v; }

	virtual bool scatter(const Ray& r_in, const HitResult& rec, Color& attenuation, Ray& scattered) const override { return false; }
	virtual Color getAttenuation(const Ray& r_in, const HitResult& rec, const Ray& r_out) const override;
	virtual Color emitted(double u, double v, const glm::vec3& p) const { return color_.MulWithoutAlpha(intensity_); };

private:
	Color color_{1,1,1,1};
	float intensity_{1.0f};
};

// ==================== Hittable ==========================

class Sphere : public Hittable, public MovableNode, public RenderedNode
{
public:
	Sphere(glm::vec3 center, double radius);

	virtual bool hit(const Ray& ray, double t_min, double t_max, HitResult& result) override; 
	// bvh
	virtual bvh_Vec3 GetBvhCenter() override;
	virtual bvh_BBox GetBvhBBox() override;

private:
	glm::vec3 center_;
	double radius_;
};

class Rect : public Hittable, public MovableNode, public RenderedNode
{
public:
	friend class AreaLight;

	Rect() = default;

	Rect(std::shared_ptr<Material> mat, glm::vec3 pos = glm::vec3(0, 0, 0), glm::vec3 rot = glm::vec3(0, 0, 0), glm::vec3 sca = glm::vec3(1, 1, 1));

	virtual bool hit(const Ray& ray, double t_min, double t_max, HitResult& result) override; 
	// bvh
	virtual bvh_Vec3 GetBvhCenter() override;
	virtual bvh_BBox GetBvhBBox() override;

	void Pricompute();

	inline void SetUseBackFace(bool use) { useBackFace = use; }

private:
	glm::vec4 v1{ -.5f,0,-.5f, 1.0f };
	glm::vec4 v2{ -.5f,0,.5f, 1.0f };
	glm::vec4 v3{ .5f,0,.5f, 1.0f };
	glm::vec4 v4{ .5f,0,-.5f, 1.0f };
	glm::vec3 edge1_;
	glm::vec3 edge2_;
	glm::vec3 normal_ { 0,1,0 };
	bool useBackFace = true;

};

class Box : public Hittable, public MovableNode, public RenderedNode
{
public:
	Box(std::shared_ptr<Material> material);
	Box(std::shared_ptr<Material> mat, glm::vec3 pos = glm::vec3(0, 0, 0), glm::vec3 rot = glm::vec3(0, 0, 0), glm::vec3 sca = glm::vec3(1, 1, 1));

	virtual bool hit(const Ray& ray, double t_min, double t_max, HitResult& result) override; 
	// bvh
	virtual bvh_Vec3 GetBvhCenter() override;
	virtual bvh_BBox GetBvhBBox() override;

	void Update();

private:
	std::vector<Rect> faces_;

	void SetFaces();
};

class Hittable_List
{
public:
	Hittable_List() = default;
	~Hittable_List()
	{
		objects_.clear();
	}

	void Add(std::shared_ptr<Hittable> object);
	void AddModel(std::shared_ptr<Model> model);

	bool hit(const Ray& ray, double t_min, double t_max, HitResult& result); 
	
	inline std::vector<std::shared_ptr<Hittable>>* GetObjects() { return &objects_; }
	// 
	inline std::shared_ptr<Hittable> GetObject(size_t index)
	{
		if (index < 0 || index > objects_.size()) return nullptr;
		return objects_[index];
	}

	inline uint32_t GetTrianglesNum() const { return trianglesNum_; }
	//inline std::vector<Triangle>* GetTriangles() { return &triangles_; }
	//inline std::vector<uint32_t>* GetSums() { return &sums_; }

	std::vector<Triangle> triangles_ {};
private:
	std::vector<std::shared_ptr<Hittable>> objects_;
	//std::vector<uint32_t> sums_{0};
	uint32_t trianglesNum_{0};

};

// Render Scene
class RenderScene
{
public:
	RenderScene();


	void AddObject(std::shared_ptr<Hittable> object);
	void AddModel(std::shared_ptr<Model> model);
	void AddLight(std::shared_ptr<Light> object);

	// 
	inline std::shared_ptr<Hittable> GetObject(int index)
	{
		return hittable_list_->GetObject(index);
	}

	bool hit(const Ray& ray, double t_min, double t_max, HitResult& result);
	std::shared_ptr<Light> pickLightWeighted(float& probablity);
	void Update(double ts);

	inline void SetUseBvh(bool use) { isUseBvhAccel_ = use; }

private:
	std::shared_ptr<Hittable_List> hittable_list_;
	std::shared_ptr<BVHAccel> bvh_;
	std::shared_ptr<SceneLightList> lights_list_;
	bool isUseBvhAccel_ = true;
};
