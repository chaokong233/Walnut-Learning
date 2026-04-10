#pragma once
#include <bvh/v2/bvh.h>
#include <bvh/v2/vec.h>
#include <bvh/v2/ray.h>
#include <bvh/v2/node.h>
#include <bvh/v2/default_builder.h>
#include <bvh/v2/thread_pool.h>
#include <bvh/v2/executor.h>
#include <bvh/v2/stack.h>
#include <bvh/v2/tri.h>
#include <span>

#include <iostream>

using Scalar = float;
using bvh_BBox    = bvh::v2::BBox<Scalar, 3>;
using bvh_Vec3    = bvh::v2::Vec<Scalar, 3>;
using bvh_Tri     = bvh::v2::Tri<Scalar, 3>;
using bvh_Ray     = bvh::v2::Ray<Scalar, 3>;
using bvh_Node    = bvh::v2::Node<Scalar, 3>;
using Bvh     = bvh::v2::Bvh<bvh_Node>;
using PrecomputedTri = bvh::v2::PrecomputedTri<Scalar>;