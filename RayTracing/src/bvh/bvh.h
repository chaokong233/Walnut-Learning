#pragma once
#include "bvh_include.h"

class Hittable_List;
class Ray;
class HitResult;
class PreTriangle;

class BVHAccel
{
public:
	BVHAccel();

	void Build(std::shared_ptr<Hittable_List> hittable_list);

	bool hit(const Ray& ray, double t_min, double t_max, HitResult& result);


private:
	std::shared_ptr<Hittable_List> hittable_list_;
	//
	std::shared_ptr<Bvh> bvh_;
	//std::shared_ptr<std::vector<Hittable>> accel_List;
	std::vector<PreTriangle> accel_List_;
	//
	std::shared_ptr<bvh::v2::ThreadPool> thread_pool_;
	std::shared_ptr<bvh::v2::ParallelExecutor> executor_;
};

