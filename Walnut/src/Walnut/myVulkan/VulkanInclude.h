#pragma once
#include <iostream>
#include <vector>
#include <cstring>
#include <format>
#include <sstream>
#include <optional>
#include <set>
#include <algorithm>
#include <fstream>
#include <span>
#include <deque>
#include <map>
#include <filesystem>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR    
#define NOMINMAX
#endif 


#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define IDENTITY_TRANSFORM { {1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1} }


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vk_mem_alloc.h"
