#include "Ray_Hittable.h"
#include "Ray.h"
#include "Walnut/Random.h"
#include "Mesh.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <memory>
#include <utility>

#include "Mesh.h"

void Hittable_List::Add(std::shared_ptr<Hittable> object)
{	
	objects_.push_back(object);
}

void Hittable_List::AddModel(std::shared_ptr<Model> model)
{
	uint32_t triNum = model->GetTrianglesNum();
	trianglesNum_ += triNum;
	// Store Triangle Ptr
	//models_.push_back(model);

	//sums_.push_back(sums_.back() + triNum);

	auto meshes = model->GetMeshes();
	for (auto& mesh : *meshes)
	{
		auto tries = mesh->GetTriangles();
		for (auto& tri : *tries)
		{
			triangles_.push_back(tri);
		}
	}
}

bool Hittable_List::hit(const Ray& ray, double t_min, double t_max, HitResult& result)
{
	HitResult temp_Res;
	double closest = t_max;
	bool hit_any = false;

	for (auto& obj : objects_)
	{
		obj->hit(ray, t_min, closest, temp_Res);
		if (temp_Res.isHit)
		{
			closest = temp_Res.t;
			hit_any = true;
		}
	}
	if(hit_any)	result = temp_Res;

	return hit_any;
}

// 重载运算符
static glm::vec3& operator/(glm::vec3&& vector, double para)
{
	vector.x /= para;
	vector.y /= para;
	vector.z /= para;
	return vector;
}

bvh_Vec3 trans_Vec3_to_bvhVec3(glm::vec3 v)
{
	return bvh_Vec3(v.x, v.y, v.z);
}

bvh_Vec3 trans_Vec3_to_bvhVec3(glm::vec4 v)
{
	return bvh_Vec3(v.x, v.y, v.z);
}

Sphere::Sphere(glm::vec3 center, double radius)
	:center_(center), radius_(radius)
{
	material = std::make_shared<Lambertian>(Color(.4,.4,.4,1));
}

bool Sphere::hit(const Ray& ray, double t_min, double t_max, HitResult& result)
{
	glm::vec3 oc = ray.origin() - center_;
	auto a = glm::dot(ray.direction(), ray.direction());
	auto half_b = dot(oc, ray.direction());
	auto c = glm::dot(oc, oc) - radius_*radius_;
	auto discriminant = half_b*half_b - a*c;

    if (discriminant > 0) {
        auto root = sqrt(discriminant);
        auto temp = (-half_b - root)/a;
        if (temp < t_max && temp > t_min) {
			result.isHit = true;
            result.t = temp;
            result.hitPosition = ray.at(result.t);
            result.hitNormal = (result.hitPosition - center_) / radius_;
			result.isFrontFace = glm::dot(result.hitNormal, ray.direction()) < 0;
			result.material = material;
            return true;
        }
        temp = (-half_b + root) / a;
        if (temp < t_max && temp > t_min) {
			result.isHit = true;
            result.t = temp;
            result.hitPosition = ray.at(result.t);
            result.hitNormal = (result.hitPosition - center_) / radius_;
			result.isFrontFace = glm::dot(result.hitNormal, ray.direction()) < 0;
			result.material = material;
            return true;
        }
    }
	result.isHit = false;
    return false;
}

bvh_Vec3 Sphere::GetBvhCenter()
{
	return trans_Vec3_to_bvhVec3(center_);
}

bvh_BBox Sphere::GetBvhBBox()
{
	float r = std::abs(radius_);
	bvh_Vec3 min(center_.x - r, center_.y - r, center_.z - r);
	bvh_Vec3 max(center_.x + radius_, center_.y + r, center_.z + r);
	return bvh_BBox(min, max);
}

Color Color::Lerp(Color col1, Color col2, float alpha)
{
	Color res(0,0,0,0);

	res.r = col1.r * (1.0f - alpha) + col2.r * alpha;
	res.g = col1.g * (1.0f - alpha) + col2.g * alpha;
	res.b = col1.b * (1.0f - alpha) + col2.b * alpha;
	res.a = col1.a * (1.0f - alpha) + col2.a * alpha;

	return res;
}

Color Color::Pow(Color& col, float para, bool isAffectAlpha)
{
	Color res(0,0,0, col.a);

	res.r = std::pow(col.r, para);
	res.g = std::pow(col.g, para);
	res.b = std::pow(col.b, para);
	if(isAffectAlpha) col.a = std::pow(col.a, para);
	return res;
}

void Rect::Pricompute()
{
	v1 = worldTransform * v1;
	v2 = worldTransform * v2;
	v3 = worldTransform * v3;
	v4 = worldTransform * v4;
	edge1_ = v4 - v1;
	edge2_ = v2 - v1;
	normal_ = glm::normalize(glm::cross(edge2_, edge1_));
}

Rect::Rect(std::shared_ptr<Material> mat, glm::vec3 pos /*= glm::vec3(0, 0, 0)*/, glm::vec3 rot /*= glm::vec3(0, 0, 0)*/, glm::vec3 sca /*= glm::vec3(1, 1, 1)*/)
{
	material = mat;
	node_position = pos;
	node_rotate = rot;
	node_scale = sca;
	CalculateTrans();
	refreshTransform(glm::mat4(1));
	Pricompute();
}

bool Rect::hit(const Ray& ray, double t_min, double t_max, HitResult& result)
{
	glm::vec3 b = ray.origin() - glm::vec3(v1);
	glm::vec3 e0 = ray.direction();
	float orig_detemenate = glm::dot(glm::cross(e0, edge2_), edge1_);
	if (orig_detemenate == 0)
	{
		result.isHit = false;
		return false;
	}
	float t = glm::dot(glm::cross(b, edge1_), edge2_) / orig_detemenate;
	float u = glm::dot(glm::cross(e0, edge2_), b) / orig_detemenate;
	float v = glm::dot(glm::cross(b, edge1_), e0) / orig_detemenate;
	if (u > 0 && v > 0 && u < 1 && v < 1 && t > t_min && t < t_max)
	{
		bool isFront = glm::dot(e0, normal_) < 0;
		if (useBackFace || isFront)
		{	
			result.hitNormal = isFront ? normal_ : normal_ * -1.0f;
			result.isFrontFace = isFront;
			result.isHit = true;
			result.t = t;
			result.hitPosition = ray.at(t);
			result.material = material;
			return true;
		}
	}
	result.isHit = false;
	return false;
}

bvh_Vec3 Rect::GetBvhCenter()
{
	return trans_Vec3_to_bvhVec3((v1 + v2 + v3 + v4) / 4.0f);
}

bvh_BBox Rect::GetBvhBBox()
{
	bvh_Vec3 p1 = trans_Vec3_to_bvhVec3(v1);
	bvh_Vec3 p2 = trans_Vec3_to_bvhVec3(glm::vec3(v1) + edge1_);
	bvh_Vec3 p3 = trans_Vec3_to_bvhVec3(glm::vec3(v1) + edge2_);
	bvh_Vec3 p4 = trans_Vec3_to_bvhVec3(glm::vec3(v1) + edge1_ + edge2_);
	return bvh_BBox(p1).extend(p2).extend(p3).extend(p4).reserveSafeExtent(0.01f);
}

bool Lambertian::scatter(const Ray& r_in, const HitResult& rec, Color& attenuation, Ray& scattered) const
{
	glm::vec3 target = rec.hitPosition + rec.hitNormal + Walnut::Random::InUnitSphere();
	scattered = Ray(rec.hitPosition, target - rec.hitPosition);
	attenuation = albedo_;
	return true;
}

Color Lambertian::getAttenuation(const Ray& r_in, const HitResult& rec, const Ray& r_out) const
{
	return albedo_.MulWithoutAlpha(glm::dot(rec.hitNormal, glm::normalize(r_out.direction())));
}

bool Metal::scatter(const Ray& r_in, const HitResult& rec, Color& attenuation, Ray& scattered) const
{
	glm::vec3 reflectDir = glm::reflect(glm::normalize(r_in.direction()), rec.hitNormal) + Walnut::Random::InUnitSphere() * roughness_;
	scattered = Ray(rec.hitPosition, reflectDir);
	attenuation = albedo_;
	return true;
}

RenderScene::RenderScene()
{
	hittable_list_ = std::make_shared<Hittable_List>();
	bvh_ = std::make_shared<BVHAccel>();
	lights_list_ = std::make_shared<SceneLightList>();
}

void RenderScene::AddObject(std::shared_ptr<Hittable> object)
{
	hittable_list_->Add(object);
}

void RenderScene::AddModel(std::shared_ptr<Model> model)
{
	hittable_list_->AddModel(model);
}

void RenderScene::AddLight(std::shared_ptr<Light> object)
{
	lights_list_->Add(object);
}

bool RenderScene::hit(const Ray& ray, double t_min, double t_max, HitResult& result)
{
	if (isUseBvhAccel_)
	{
		return bvh_->hit(ray, t_min, t_max, result);
	}
	else
	{
		return hittable_list_->hit(ray, t_min, t_max, result);
	}
}

std::shared_ptr<Light> RenderScene::pickLightWeighted(float& probablity)
{
	if (lights_list_->size() == 0) return nullptr;
	return lights_list_->randomLight(probablity);
}

void RenderScene::Update(double ts)
{
	// Bvh Accel
	bvh_->Build(hittable_list_);
	lights_list_->Build();
}

bool Refraction::scatter(const Ray& r_in, const HitResult& rec, Color& attenuation, Ray& scattered) const
{
	attenuation = albedo_;
	float eta = rec.isFrontFace ? 1.0f / refractive_ : refractive_;
	glm::vec3 n = rec.isFrontFace ? rec.hitNormal : -rec.hitNormal;

	glm::vec3 reflectDir = glm::refract(glm::normalize(r_in.direction()), n, eta);// +Walnut::Random::InUnitSphere() * roughness_;
	// 全反射
	if (reflectDir == glm::vec3(0))
	{
		glm::vec3 reflectDir = glm::reflect(glm::normalize(r_in.direction()), rec.hitNormal);// +Walnut::Random::InUnitSphere() * roughness_;
		scattered = Ray(rec.hitPosition, reflectDir);
		return true;
	}

	float cos_theta = glm::dot(-n, r_in.direction());
	float reflect_prob = schlick(cos_theta, eta);
    if (Walnut::Random::Float() < reflect_prob)
    {
		glm::vec3 reflectDir = glm::reflect(glm::normalize(r_in.direction()), rec.hitNormal);// +Walnut::Random::InUnitSphere() * roughness_;
		scattered = Ray(rec.hitPosition, reflectDir);
		return true;
    }

	scattered = Ray(rec.hitPosition, reflectDir);
	return true;
}

double Refraction::schlick(double cosine, double ref_idx)
{
	auto r0 = (1 - ref_idx) / (1 + ref_idx);
	r0 = r0 * r0;
	return r0 + (1 - r0) * pow((1 - cosine), 5);
}

bool PureRefraction::scatter(const Ray& r_in, const HitResult& rec, Color& attenuation, Ray& scattered) const
{
	attenuation = albedo_;
	float eta = rec.isFrontFace ? 1.0f / refractive_ : refractive_;
	glm::vec3 n = rec.isFrontFace ? rec.hitNormal : -rec.hitNormal;

	glm::vec3 reflectDir = glm::refract(glm::normalize(r_in.direction()), n, eta);// +Walnut::Random::InUnitSphere() * roughness_;
	// 全反射
	if (reflectDir == glm::vec3(0))
	{
		glm::vec3 reflectDir = glm::reflect(glm::normalize(r_in.direction()), rec.hitNormal);// +Walnut::Random::InUnitSphere() * roughness_;
		scattered = Ray(rec.hitPosition, reflectDir);
		return true;
	}

	scattered = Ray(rec.hitPosition, reflectDir);
	return true;
}

void MovableNode::CalculateTrans()
{
	localTransform = glm::scale(glm::mat4(1), node_scale);
	localTransform = glm::eulerAngleXYZ(glm::radians(node_rotate.x), glm::radians(node_rotate.y), glm::radians(node_rotate.z)) * localTransform;
	localTransform = glm::translate(glm::mat4(1), node_position) * localTransform;

	glm::mat4 parMat = glm::mat4(1.0);
    if (auto it = parent_.lock()) parMat = it->worldTransform;
    refreshTransform(parMat);
}

Box::Box(std::shared_ptr<Material> material)
{
	this->material = material;
	SetFaces();
	for (auto& f : faces_)
	{
		f.CalculateTrans();
		f.material = material;
	}
}

Box::Box(std::shared_ptr<Material> mat, glm::vec3 pos /*= glm::vec3(0, 0, 0)*/, glm::vec3 rot /*= glm::vec3(0, 0, 0)*/, glm::vec3 sca /*= glm::vec3(1, 1, 1)*/)
{
	material = mat;
	node_position = pos;
	node_rotate = rot;
	node_scale = sca;
	CalculateTrans();
	refreshTransform(glm::mat4(1));
	SetFaces();

	for (auto& f : faces_)
	{
		f.CalculateTrans();
		f.refreshTransform(worldTransform);
		f.Pricompute();
		f.material = material;
	}
}

bool Box::hit(const Ray& ray, double t_min, double t_max, HitResult& result)
{
	HitResult temp_Res;
	double closest = t_max;
	bool hit_any = false;

	for (auto& f : faces_)
	{
		f.hit(ray, t_min, closest, temp_Res);
		if (temp_Res.isHit)
		{
			closest = temp_Res.t;
			hit_any = true;
		}
	}
	if(hit_any)	result = temp_Res;

	return hit_any;
}

bvh_Vec3 Box::GetBvhCenter()
{
	return trans_Vec3_to_bvhVec3(node_position);
}

bvh_BBox Box::GetBvhBBox()
{
	glm::vec4 v[8] =  { { -.5f, -.5f, -.5f, 1.0f },
						{ -.5f,  .5f,  .5f, 1.0f },
						{  .5f, -.5f,  .5f, 1.0f },
						{  .5f,  .5f, -.5f, 1.0f },
						{ -.5f,  .5f, -.5f, 1.0f },
						{  .5f, -.5f, -.5f, 1.0f },
						{ -.5f, -.5f,  .5f, 1.0f },
						{ -.5f, -.5f, -.5f, 1.0f } };

	for (int i = 0; i < 8; i++)
	{
		v[i] = worldTransform * v[i];
	}

	bvh_BBox b(trans_Vec3_to_bvhVec3(v[0]));
	for (int i = 1; i < 8; i++)
	{
		b = b.extend(trans_Vec3_to_bvhVec3(v[i]));
	}
	return b;
}

void Box::Update()
{
	for (auto& f : faces_)
	{
		f.refreshTransform(worldTransform);
		f.Pricompute();
		f.material = material;
	}
}

void Box::SetFaces()
{
	faces_.resize(6);
	faces_[0].node_position = glm::vec3(0, -.5f, 0);
	faces_[0].node_rotate = glm::vec3(180, 0, 0);
	faces_[1].node_position = glm::vec3(0, .5f, 0);

	faces_[2].node_rotate = glm::vec3(90, 0, 0);
	faces_[2].node_position = glm::vec3(0, 0, .5f);

	faces_[3].node_rotate = glm::vec3(-90, 0, 0);
	faces_[3].node_position = glm::vec3(0, 0, -.5f);

	faces_[4].node_rotate = glm::vec3(0, 0, 90);
	faces_[4].node_position = glm::vec3(-.5f, 0, 0);

	faces_[5].node_rotate = glm::vec3(0, 0, -90);
	faces_[5].node_position = glm::vec3(.5f, 0, 0);
}

void SceneLightList::Build()
{
	std::sort(lightList_.begin(), lightList_.end(), LightComparer());

	totalPower_ = 0;
	for (auto& it : lightList_)
	{
		totalPower_ += it->GetPower();
	}
}

std::shared_ptr<Light> SceneLightList::randomLight(float& pickProbablity)
{
	float r = Walnut::Random::Float(0, totalPower_);
	float p = 0;
	for (auto& l : lightList_)
	{
		float nowP = l->GetPower();
		p += nowP;
		if (r <= p)
		{
			pickProbablity = nowP * lightList_.size() / totalPower_;
			return l;
		}
	}
	return nullptr;
}

AreaLight::AreaLight(const Rect& rect, Color col, float pow)
	:color_(col)
{
	SetPower(pow);

	node_position = rect.node_position +0.01f * rect.normal_;
	node_rotate = rect.node_rotate;
	node_scale = rect.node_scale;
	CalculateTrans();

	v1 = rect.v1;
	v2 = rect.v2;
	v3 = rect.v3;
	v4 = rect.v4;
	edge1_ = rect.edge1_;
	edge2_ = rect.edge2_;
	normal_ = rect.normal_;
}

AreaLight::AreaLight()
{
	SetPower(5);
}

void AreaLight::Precompute()
{
	v1 = worldTransform * v1;
	v2 = worldTransform * v2;
	v3 = worldTransform * v3;
	v4 = worldTransform * v4;
	edge1_ = v4 - v1;
	edge2_ = v2 - v1;
	normal_ = glm::normalize(glm::cross(edge2_, edge1_));
}

LightSample AreaLight::sample() const
{
	float u = Walnut::Random::Float();
	float v = Walnut::Random::Float();
	glm::vec3 pos = glm::vec3(v1) + u * edge1_ + v * edge2_;
	LightSample sample{};
	sample.sampleColor = color_.MulWithoutAlpha(power_);
	sample.samplePosition = pos;
	sample.rayDirection = normal_;
	return sample;
}

Color Diffuse_Emissive::getAttenuation(const Ray& r_in, const HitResult& rec, const Ray& r_out) const
{
	return Color(1);
}
