#pragma once
#include "Ray_Hittable.h"
#include "Walnut/Application.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace vulkan
{
    class VulkanLocalBuffer;
    class VulkanAllocator;
    class CommandPool;
}

class TexturePool;
extern TexturePool* g_texturePool;


class PreTriangle : public Hittable, public RenderedNode
{
public:
    PreTriangle() = default;
    PreTriangle(const Triangle& tri);

    PreTriangle& operator=(const Triangle& tri)
    {
        verteies = tri.verteies;
        this->material = tri.material;
        edge1_ = verteies[1].position - verteies[0].position;
	    edge2_ = verteies[2].position - verteies[0].position;

        return *this;
    }

    virtual bool hit(const Ray& ray, double t_min, double t_max, HitResult& result) override;

	// bvh
    virtual bvh_Vec3 GetBvhCenter() override { return bvh_Vec3(0, 0, 0); };
	virtual bvh_BBox GetBvhBBox() override { return bvh_BBox(bvh_Vec3(0,0,0), bvh_Vec3(0,0,0)); }; ;
private:
    std::array<Vertex, 3> verteies;
    glm::vec3 edge1_;
    glm::vec3 edge2_;
};

class Mesh : public MovableNode
{
public:
    friend class Model;

    Mesh() = default;

    Mesh(const Mesh&) = default;
    Mesh& operator=(const Mesh&) = default;

    inline std::vector<Triangle>* GetTriangles() { return &triangles; }
    
private:
    std::vector<Triangle> triangles;
    uint32_t trianglesNums_{0};
};

class Model : public MovableNode, public RenderedNode
{
public:
    Model(std::shared_ptr<Material> material)
    {
        this->material = material;
    }

    Model(std::shared_ptr<Material> material, std::shared_ptr<Mesh> mesh)
    {
        this->material = material;
        meshes_.push_back(mesh);
    }

    void LoadModel(const std::string& path);

    inline void AddMesh(std::shared_ptr<Mesh> mesh) { meshes_.push_back(mesh); }
    inline uint32_t GetTrianglesNum() const { return trianglesNums_; }
    inline std::vector<std::shared_ptr<Mesh>>* GetMeshes() { return &meshes_; }

private:
    std::vector<std::shared_ptr<Mesh>> meshes_;
    uint32_t trianglesNums_{0};
    std::string loadDirectory_;

    void processNode(aiNode* node, const aiScene* scene);
	std::shared_ptr<Mesh> processMesh(aiMesh* mesh, const aiScene* scene);

};

// ============ Vulkan==============

class RTMesh;
class RTModel;
using vulkanSampleImage = std::pair<vulkan::VulkanLoadedTexture*, vulkan::VulkanSampler*>;

struct GeometryNode {
	uint64_t VertexBufferDeviceAddress;
	uint64_t IndexBufferDeviceAddress;
    alignas(16) glm::vec3 BaseColor;
    alignas(16) glm::vec3 EmissiveColor;
    alignas(16) glm::vec3 SpecularTint;
    float Roughness;
    float Metallic;
    float Specular;
    float Subsurface;
    float Anisotropic;
    // Texture
    int BaseColorTextureID;
    int IBLTextureID; // r:roughness, g:metallic, b:specular 
};

class RTNode
{
public:
    std::vector<std::shared_ptr<RTMesh>> children;
    std::weak_ptr<Node> parent;

};

class RTMesh : public RTNode
{
public:
    struct Geometry {
	    uint32_t firstVertex;
	    uint32_t vertexCount;
	    uint32_t firstIndex;
	    uint32_t indexCount;
    } geometry;

    struct Material 
    {
        glm::vec3 BaseColor{glm::vec3(1)};
        glm::vec3 EmissiveColor{glm::vec3(0)};
        glm::vec3 SpecularTint{glm::vec3(0.2f)};
        float Roughness{0.5f};
        float Metallic{0.0f};
        float Specular{0.5f};
        float Subsurface{0.0f};
        float Anisotropic{0.0f};
            // Texture
        int BaseColorTextureID{-1};
        int IBLTextureID{-1}; // r:roughness, g:metallic, b:specular 
    } material;
};

class RTModel : public RTNode
{
public:
    RTModel(vulkan::VulkanAllocator* allocator, vulkan::CommandPool* commandPool, VkQueue queue)
        :allocator_(allocator), commandPool_(commandPool), queue_(queue) {}

    std::vector<std::shared_ptr<RTMesh>> linerMeshes;

	struct Vertices {
		uint32_t count{0};
        std::shared_ptr<vulkan::VulkanLocalBuffer> buffer{nullptr};
        std::vector<Vertex> scratchArray;
	} vertices;

	struct Indices {
        uint32_t count{0};
        std::shared_ptr<vulkan::VulkanLocalBuffer> buffer{nullptr};
        std::vector<uint32_t> scratchArray;
	} indices;

    void LoadModel(const std::string& path);

private:
    std::string loadDirectory_;
    vulkan::VulkanAllocator* allocator_;
    vulkan::CommandPool* commandPool_;
    VkQueue queue_;

    void processNode(aiNode* node, const aiScene* scene);
	std::shared_ptr<RTMesh> processMesh(aiMesh* mesh, const aiScene* scene);
    RTMesh::Material loadMaterial(aiMaterial* material);
    int loadMaterialTextures(aiMaterial* material, aiTextureType type);
};

class TexturePool
{
private:
    std::vector<vulkanSampleImage> sampleImage{};
    std::unordered_map<std::string, uint32_t> path_to_imageID_map;
    std::vector<VkDescriptorImageInfo> imageInfo_{};

public:
    TexturePool() = default;
    ~TexturePool();

    int Add(const std::string&, vulkanSampleImage);
    bool isExisted(const std::string&);
    int FindID(const std::string&);

    inline std::vector<VkDescriptorImageInfo>* GetImageInfo() { return &imageInfo_; }
    inline int GetImageCount() { return imageInfo_.size(); }
};