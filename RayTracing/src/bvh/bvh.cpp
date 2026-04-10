#include "bvh.h"
#include "src/Ray_Hittable.h"
#include "src/Mesh.h"
#include <execution>

BVHAccel::BVHAccel()
{
	thread_pool_ = std::make_shared<bvh::v2::ThreadPool>();
    executor_ = std::make_shared<bvh::v2::ParallelExecutor>(*thread_pool_);

}
#ifndef useTrianglePrimitive

void BVHAccel::Build(std::shared_ptr<Hittable_List> hittable_list)
{
    accel_List_ = hittable_list;
    auto objsPtr = hittable_list->GetObjects();
    auto& objs = *objsPtr;
    size_t obj_Num = objs.size();
    std::vector<bvh_BBox> bboxes(obj_Num);
    std::vector<bvh_Vec3> centers(obj_Num);
    executor_->for_each(0, obj_Num, [&objs, &bboxes, &centers] (size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            bboxes[i]  = objs[i]->GetBvhBBox();
            centers[i] = objs[i]->GetBvhCenter();
        }
    });

    typename bvh::v2::DefaultBuilder<bvh_Node>::Config config;
    config.quality = bvh::v2::DefaultBuilder<bvh_Node>::Quality::High;

    bvh_ = std::make_shared<Bvh>(bvh::v2::DefaultBuilder<bvh_Node>::build(*thread_pool_, bboxes, centers, config));

    // TODO:Permute
    //accel_List = std::make_shared<std::vector<Hittable>>();
}

bool BVHAccel::hit(const Ray& ray, double t_min, double t_max, HitResult& result)
{
    static constexpr size_t invalid_id = std::numeric_limits<size_t>::max();
    static constexpr size_t stack_size = 64;
    static constexpr bool use_robust_traversal = false;
    // Trans Ray Value
    auto pOri = ray.origin();
    auto pDir = glm::normalize(ray.direction());
    bvh_Vec3 ori(pOri.x, pOri.y, pOri.z);
    bvh_Vec3 dir(pDir.x, pDir.y, pDir.z);
    bvh_Ray r(ori, dir, t_min, t_max);
    // Get List
    auto objsPtr = accel_List_->GetObjects();
    auto& objs = *objsPtr;

    auto prim_id = invalid_id;
    HitResult temp_Res;

    // Traverse the BVH and get the u, v coordinates of the closest intersection.
    bvh::v2::SmallStack<Bvh::Index, stack_size> stack;
    bvh_->intersect<false, use_robust_traversal>(r, bvh_->get_root().index, stack,
        [&] (size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                // TODO:permute
                //size_t j = should_permute ? i : bvh.prim_ids[i];
                size_t j = bvh_->prim_ids[i];
                if(objs[j]->bvh_hit(r, temp_Res))
                {
                    prim_id = i;
                    r.tmax = temp_Res.t;
                }
            }
            if (prim_id != invalid_id)
            {
                result = temp_Res;
                return true;
            }
            return false;
        });

    return prim_id != invalid_id;
}

#else

void BVHAccel::Build(std::shared_ptr<Hittable_List> hittable_list)
{
    hittable_list_ = hittable_list;
    accel_List_.clear();

    //auto& tries = *hittable_list->GetTriangles();
    auto& tries = hittable_list->triangles_;
    if (tries.empty()) return;

    //auto& sums = *hittable_list->GetSums();
    uint32_t triNum = hittable_list->GetTrianglesNum();

    std::vector<bvh_BBox> bboxes(triNum);
    std::vector<bvh_Vec3> centers(triNum);

    executor_->for_each(0, triNum, [&tries, &bboxes, &centers] (size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            bboxes[i]  = tries[i].GetBvhBBox();
            centers[i] = tries[i].GetBvhCenter();
        }
    });
    //for (size_t i = 0; i < triNum; ++i) {

    //}
    //std::vector<uint32_t> modelIter(models.size());
    //for (uint32_t i = 0; i < models.size(); i++)
    //{
    //    modelIter[i] = i;
    //}

    //std::for_each(std::execution::par, modelIter.begin(), modelIter.end(), [&models, &sums, &bboxes, &centers](uint32_t iter)
    //    {
    //        auto& model = models[iter];
    //        uint32_t offset = sums[iter];
    //        uint32_t acu = 0;
    //        auto meshes = model->GetMeshes();
	   //     for (auto& mesh : *meshes)
	   //     {
		  //      auto tries = mesh->GetTriangles();
		  //      for (auto& tri : *tries)
		  //      {
			 //       bboxes[acu + offset]  = tri.GetBvhBBox();
    //                centers[acu + offset] = tri.GetBvhCenter();
    //                acu++;
		  //      }
	   //     }
    //    });

    typename bvh::v2::DefaultBuilder<bvh_Node>::Config config;
    config.quality = bvh::v2::DefaultBuilder<bvh_Node>::Quality::High;

    bvh_ = std::make_shared<Bvh>(bvh::v2::DefaultBuilder<bvh_Node>::build(*thread_pool_, bboxes, centers, config));

    // Permute
    accel_List_.resize(triNum);
    executor_->for_each(0, triNum, [&] (size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            auto j = bvh_->prim_ids[i];
            accel_List_[i] = tries[j];
        }
    });

}

bool BVHAccel::hit(const Ray& ray, double t_min, double t_max, HitResult& result)
{
    static constexpr size_t invalid_id = std::numeric_limits<size_t>::max();
    static constexpr size_t stack_size = 64;
    static constexpr bool use_robust_traversal = false;
    // Trans Ray Value
    auto pOri = ray.origin();
    auto pDir = glm::normalize(ray.direction());
    bvh_Vec3 ori(pOri.x, pOri.y, pOri.z);
    bvh_Vec3 dir(pDir.x, pDir.y, pDir.z);
    bvh_Ray r(ori, dir, t_min, t_max);
    // Get List
    auto prim_id = invalid_id;
    HitResult temp_Res;

    // Traverse the BVH and get the u, v coordinates of the closest intersection.
    bvh::v2::SmallStack<Bvh::Index, stack_size> stack;
    bvh_->intersect<false, use_robust_traversal>(r, bvh_->get_root().index, stack,
        [&] (size_t begin, size_t end) {
            for (size_t i = begin; i < end; ++i) {
                // permute
                size_t j = i;
                if(accel_List_[j].bvh_hit(r, temp_Res))
                {
                    prim_id = i;
                    r.tmax = temp_Res.t;
                }
            }
            if (prim_id != invalid_id)
            {
                result = temp_Res;
                return true;
            }
            return false;
        });

    return prim_id != invalid_id;
}

#endif