#ifndef StaticMesh_h__
#define StaticMesh_h__

#include "assimp\Importer.hpp"
#include "StaticMeshNode.h"
#include "Core\QVKPrimitive.h"
#include "QString"

class StaticMesh : public QVKPrimitive {
	friend class StaticMeshNode;
public:
	StaticMesh();
	void loadMeshFromFile(QString filePath);
	void initResources() override;
	void releaseResources() override;
	void startNextFrame(FrameContext beginInfo) override;
protected:
	void initVulkanTexture();
	void initVulkanDescriptor();
	void initVulkanPipline();
private:
	void processNode(const aiNode* node, const aiScene* scene, aiMatrix4x4 mat);
	void processMaterialTextures(const aiScene* scene);

private:
	Assimp::Importer importer_;
	QString meshPath_;
	const aiScene* scene = nullptr;
	std::vector<std::shared_ptr<StaticMeshNode>> meshes_;
	inline static std::vector<aiTextureType> textureTypes_ = { aiTextureType_DIFFUSE };
	std::map<std::string, std::shared_ptr<StaticMeshNode::Texture>> textureSet_;
	std::vector<std::vector<std::shared_ptr<StaticMeshNode::Texture>>> textures_;
	vk::Sampler commonSampler_;
	vk::DescriptorPool descPool_;
	vk::DescriptorSetLayout descSetLayout_;
	vk::PipelineLayout piplineLayout_;
	vk::Pipeline pipline_;
};

#endif // StaticMesh_h__
