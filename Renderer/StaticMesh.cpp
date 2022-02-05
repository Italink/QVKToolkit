#include "StaticMesh.h"
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include "QImage"
#include "QFileInfo"
#include <QDir>

StaticMesh::StaticMesh() {
}

void StaticMesh::loadMeshFromFile(QString filePath) {
	meshPath_ = filePath;
	scene = importer_.ReadFile(filePath.toUtf8().data(), aiProcess_Triangulate | aiProcess_FlipUVs);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		printf("ERROR::ASSIMP:: %s", importer_.GetErrorString());
		return;
	}
	processMaterialTextures(scene);
	processNode(scene->mRootNode, scene, aiMatrix4x4());
	resetVkSource();
}

void StaticMesh::processNode(const aiNode* node, const aiScene* scene, aiMatrix4x4 mat)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes_.push_back(std::make_shared<StaticMeshNode>(this, mesh, scene, mat));
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene, mat * node->mChildren[i]->mTransformation);
	}
}

void StaticMesh::processMaterialTextures(const aiScene* scene)
{
	textures_.resize(scene->mNumMaterials);
	for (int i = 0; i < scene->mNumMaterials; i++) {
		aiMaterial* material = scene->mMaterials[i];
		for (auto type : textureTypes_) {
			for (int j = 0; j < material->GetTextureCount(type); j++) {
				aiString path;
				material->GetTexture(type, j, &path);
				auto item = textureSet_.find(path.C_Str());
				if (item != textureSet_.end()) {
					textures_[i].push_back(item->second);
				}
				else {
					auto texture = std::shared_ptr<StaticMeshNode::Texture>(new StaticMeshNode::Texture, [this](StaticMeshNode::Texture* texture) {
						vk::Device device = window_->device();
						if (texture->image)
							device.destroyImage(texture->image);
						if (texture->imageView)
							device.destroyImageView(texture->imageView);
						if (texture->imageMemory)
							device.freeMemory(texture->imageMemory);
					});
					texture->path = path.C_Str();
					texture->type = type;
					textures_[i].push_back(texture);
					textureSet_[path.C_Str()] = texture;
				}
			}
		}
	}
}

void StaticMesh::initResources()
{
	if (!scene)
		return;
	initVulkanTexture();
	initVulkanDescriptor();
	initVulkanPipline();
	for (auto& mesh : meshes_) {
		mesh->initVulkanResource(window_->device(), window_->hostVisibleMemoryIndex());
	}
}

void StaticMesh::releaseResources() {
	if (!scene)
		return;
	vk::Device device = window_->device();
	device.destroyPipeline(pipline_);
	device.destroyPipelineLayout(piplineLayout_);
	device.destroyDescriptorPool(descPool_);
	device.destroyDescriptorSetLayout(descSetLayout_);
	device.destroySampler(commonSampler_);
	meshes_.clear();
	textures_.clear();
	textureSet_.clear();
}

void StaticMesh::startNextFrame(FrameContext frameCtx)
{
	if (!scene)
		return;
	vk::CommandBuffer cmdBuffer = frameCtx.cmdBuffer;
	vk::RenderPassBeginInfo beginInfo;
	beginInfo.renderPass = window_->windowRenderPass();
	beginInfo.framebuffer = frameCtx.frameBuffer;
	beginInfo.renderArea.extent.width = frameCtx.viewport.width();
	beginInfo.renderArea.extent.height = frameCtx.viewport.height();
	cmdBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipline_);
	for (auto& mesh : meshes_) {
		QMatrix4x4 localMatrix;
		memcpy(localMatrix.data(), &mesh->localMatrix_, sizeof(aiMatrix4x4));
		QMatrix4x4 flipY;
		flipY.scale(1, -1, 1);
		QMatrix4x4 mvp = window_->camera_.getMatrixVP() * flipY * localMatrix.transposed();
		cmdBuffer.pushConstants(piplineLayout_, vk::ShaderStageFlagBits::eVertex, 0, sizeof(float) * 16, mvp.constData());

		if (mesh->descSet_)
			cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplineLayout_, 0, 1, &mesh->descSet_, 0, nullptr);

		cmdBuffer.bindVertexBuffers(0, mesh->vertexBufferInfo_.buffer, mesh->vertexBufferInfo_.offset);
		cmdBuffer.bindIndexBuffer(mesh->indexBufferInfo_.buffer, mesh->indexBufferInfo_.offset, vk::IndexType::eUint32);
		cmdBuffer.drawIndexed(mesh->indexBufferInfo_.range / (sizeof(unsigned int)), 1, 0, 0, 0);
	}
	cmdBuffer.endRenderPass();
}

void StaticMesh::initVulkanTexture() {
	vk::Device device = window_->device();
	vk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = vk::Filter::eNearest;
	samplerInfo.minFilter = vk::Filter::eNearest;
	samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.maxAnisotropy = 1.0f;
	commonSampler_ = device.createSampler(samplerInfo);

	vk::CommandBufferAllocateInfo cmdBufferAllocInfo;
	cmdBufferAllocInfo.commandBufferCount = 1;
	cmdBufferAllocInfo.commandPool = window_->graphicsCommandPool();
	cmdBufferAllocInfo.level = vk::CommandBufferLevel::ePrimary;
	vk::CommandBuffer cmdBuffer = device.allocateCommandBuffers(cmdBufferAllocInfo).front();
	vk::CommandBufferBeginInfo cmdBufferBeginInfo;
	cmdBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	cmdBuffer.begin(cmdBufferBeginInfo);

	QFileInfo fileInfo(meshPath_);

	for (auto& textureIter : textureSet_) {
		std::shared_ptr<StaticMeshNode::Texture> texture = textureIter.second;
		texture->sampler = commonSampler_;

		QImage image(fileInfo.dir().filePath(QString::fromStdString(texture->path)));

		if (image.isNull())
			continue;

		image = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
		vk::ImageCreateInfo imageInfo;
		imageInfo.imageType = vk::ImageType::e2D;
		imageInfo.format = vk::Format::eR8G8B8A8Unorm;
		imageInfo.extent.width = image.width();
		imageInfo.extent.height = image.height();
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = vk::SampleCountFlagBits::e1;
		imageInfo.tiling = vk::ImageTiling::eLinear;
		imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
		imageInfo.initialLayout = vk::ImageLayout::eUndefined;
		texture->image = device.createImage(imageInfo);

		vk::MemoryRequirements texMemReq = device.getImageMemoryRequirements(texture->image);
		vk::MemoryAllocateInfo allocInfo(texMemReq.size, window_->hostVisibleMemoryIndex());
		texture->imageMemory = device.allocateMemory(allocInfo);
		device.bindImageMemory(texture->image, texture->imageMemory, 0);

		vk::ImageSubresource subres(vk::ImageAspectFlagBits::eColor, 0, 0/*imageInfo.mipLevels, imageInfo.arrayLayers*/);
		vk::SubresourceLayout subresLayout = device.getImageSubresourceLayout(texture->image, subres);
		uint8_t* texMemPtr = (uint8_t*)device.mapMemory(texture->imageMemory, subresLayout.offset, subresLayout.size);
		for (int y = 0; y < image.height(); ++y) {
			const uint8_t* imgLine = image.constScanLine(y);
			memcpy(texMemPtr + y * subresLayout.rowPitch, imgLine, image.width() * 4);
		}
		device.unmapMemory(texture->imageMemory);

		vk::ImageViewCreateInfo imageViewInfo;
		imageViewInfo.image = texture->image;
		imageViewInfo.viewType = vk::ImageViewType::e2D;
		imageViewInfo.format = vk::Format::eR8G8B8A8Unorm;
		imageViewInfo.components.r = vk::ComponentSwizzle::eR;
		imageViewInfo.components.g = vk::ComponentSwizzle::eG;
		imageViewInfo.components.b = vk::ComponentSwizzle::eB;
		imageViewInfo.components.a = vk::ComponentSwizzle::eA;
		imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		imageViewInfo.subresourceRange.levelCount = imageViewInfo.subresourceRange.layerCount = 1;
		texture->imageView = device.createImageView(imageViewInfo);

		vk::ImageMemoryBarrier barrier;
		barrier.image = texture->image;
		barrier.oldLayout = vk::ImageLayout::eUndefined;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		barrier.subresourceRange.layerCount = barrier.subresourceRange.levelCount = 1;
		cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
	}

	cmdBuffer.end();

	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	vk::Queue queue = window_->graphicsQueue();
	queue.submit(submitInfo);
	queue.waitIdle();
}

void StaticMesh::initVulkanDescriptor()
{
	vk::Device device = window_->device();
	vk::DescriptorPoolSize descPoolSize(vk::DescriptorType::eCombinedImageSampler, (uint32_t)textureTypes_.size() * scene->mNumMeshes);
	vk::DescriptorPoolCreateInfo descPoolInfo;
	descPoolInfo.maxSets = scene->mNumMeshes;
	descPoolInfo.poolSizeCount = 1;
	descPoolInfo.pPoolSizes = &descPoolSize;
	descPool_ = device.createDescriptorPool(descPoolInfo);

	std::vector<vk::DescriptorSetLayoutBinding>  layoutBinding(textureTypes_.size());
	for (int i = 0; i < textureTypes_.size(); i++) {
		layoutBinding[i] = { (uint32_t)textureTypes_[i], vk::DescriptorType::eCombinedImageSampler,1,vk::ShaderStageFlagBits::eFragment };
	}
	vk::DescriptorSetLayoutCreateInfo descLayoutInfo;
	descLayoutInfo.pNext = nullptr;
	descLayoutInfo.flags = {};
	descLayoutInfo.bindingCount = layoutBinding.size();
	descLayoutInfo.pBindings = layoutBinding.data();
	descSetLayout_ = device.createDescriptorSetLayout(descLayoutInfo);
}

void StaticMesh::initVulkanPipline()
{
	vk::Device device = window_->device();
	vk::ShaderModule vertShader = window_->createShaderFromCode(EShLangVertex, R"(#version 440

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aTangent;
layout (location = 3) in vec3 aBitangent;
layout (location = 4) in vec2 aTexCoords;

layout(push_constant) uniform PushConstant{
    mat4 mvp;
}pushConstant;

layout(location = 0) out vec2 vTexCoords;
layout(location = 1) out vec3 vColor;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    vTexCoords = aTexCoords;

    vColor = dot(aNormal,vec3(1,0,0)) * vec3(1) + vec3(0.5);

    gl_Position = pushConstant.mvp * vec4(aPos, 1.0);
}
)");

	vk::ShaderModule fragShader = window_->createShaderFromCode(EShLangFragment, R"(#version 440

layout(location = 0) in vec2 TexCoords;
layout(location = 1) in vec3 vColor;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D diffuse;
//layout(binding = 2) uniform sampler2D specular;
//layout(binding = 3) uniform sampler2D ambient;
//layout(binding = 4) uniform sampler2D emissive;
//layout(binding = 5) uniform sampler2D height;
//layout(binding = 6) uniform sampler2D normals;
//layout(binding = 7) uniform sampler2D shininess;
//layout(binding = 8) uniform sampler2D opacity;
//layout(binding = 9) uniform sampler2D displacement;
//layout(binding = 10) uniform sampler2D lightmap;
//layout(binding = 11) uniform sampler2D reflection;
//layout(binding = 12) uniform sampler2D base_color;
//layout(binding = 13) uniform sampler2D normal_camera;
//layout(binding = 14) uniform sampler2D emission_color;
//layout(binding = 15) uniform sampler2D metalness;
//layout(binding = 16) uniform sampler2D diffuse_roughness;
//layout(binding = 17) uniform sampler2D ambient_occlusion;
//layout(binding = 19) uniform sampler2D sheen;
//layout(binding = 20) uniform sampler2D clearcoat;
//layout(binding = 21) uniform sampler2D transmission;

void main()
{
    fragColor = texture(diffuse,TexCoords);
})");

	vk::GraphicsPipelineCreateInfo piplineInfo;
	piplineInfo.stageCount = 2;

	vk::PipelineShaderStageCreateInfo piplineShaderStage[2];
	piplineShaderStage[0].stage = vk::ShaderStageFlagBits::eVertex;
	piplineShaderStage[0].module = vertShader;
	piplineShaderStage[0].pName = "main";
	piplineShaderStage[1].stage = vk::ShaderStageFlagBits::eFragment;
	piplineShaderStage[1].module = fragShader;
	piplineShaderStage[1].pName = "main";
	piplineInfo.pStages = piplineShaderStage;
	piplineInfo.pStages = piplineShaderStage;

	vk::VertexInputBindingDescription vertexBindingDesc;
	vertexBindingDesc.binding = 0;
	vertexBindingDesc.stride = sizeof(StaticMeshNode::Vertex);
	vertexBindingDesc.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputAttributeDescription vertexAttrDesc[5] = {
		{0,0,vk::Format::eR32G32B32Sfloat,offsetof(StaticMeshNode::Vertex,position)},
		{1,0,vk::Format::eR32G32B32Sfloat,offsetof(StaticMeshNode::Vertex,normal)},
		{2,0,vk::Format::eR32G32B32Sfloat,offsetof(StaticMeshNode::Vertex,tangent)},
		{3,0,vk::Format::eR32G32B32Sfloat,offsetof(StaticMeshNode::Vertex,bitangent)},
		{4,0,vk::Format::eR32G32Sfloat,offsetof(StaticMeshNode::Vertex,texCoords)},
	};

	vk::PipelineVertexInputStateCreateInfo vertexInputState({}, 1, &vertexBindingDesc, 5, vertexAttrDesc);
	piplineInfo.pVertexInputState = &vertexInputState;

	vk::PipelineInputAssemblyStateCreateInfo vertexAssemblyState({}, vk::PrimitiveTopology::eTriangleList);
	piplineInfo.pInputAssemblyState = &vertexAssemblyState;

	vk::PipelineViewportStateCreateInfo viewportState;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;
	piplineInfo.pViewportState = &viewportState;

	vk::PipelineRasterizationStateCreateInfo rasterizationState;
	rasterizationState.polygonMode = vk::PolygonMode::eFill;
	rasterizationState.cullMode = vk::CullModeFlagBits::eNone;
	rasterizationState.frontFace = vk::FrontFace::eCounterClockwise;
	rasterizationState.lineWidth = 1.0f;
	piplineInfo.pRasterizationState = &rasterizationState;

	vk::PipelineMultisampleStateCreateInfo MSState;
	MSState.rasterizationSamples = (vk::SampleCountFlagBits)window_->sampleCountFlagBits();
	piplineInfo.pMultisampleState = &MSState;

	vk::PipelineDepthStencilStateCreateInfo DSState;
	DSState.depthTestEnable = true;
	DSState.depthWriteEnable = true;
	DSState.depthCompareOp = vk::CompareOp::eLessOrEqual;
	piplineInfo.pDepthStencilState = &DSState;

	vk::PipelineColorBlendStateCreateInfo colorBlendState;
	colorBlendState.attachmentCount = 1;
	vk::PipelineColorBlendAttachmentState colorBlendAttachmentState;
	colorBlendAttachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendState.pAttachments = &colorBlendAttachmentState;
	piplineInfo.pColorBlendState = &colorBlendState;

	vk::PipelineDynamicStateCreateInfo dynamicState;
	vk::DynamicState dynamicEnables[] = { vk::DynamicState::eViewport ,vk::DynamicState::eScissor };
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicEnables;
	piplineInfo.pDynamicState = &dynamicState;

	vk::PipelineLayoutCreateInfo piplineLayoutInfo;

	vk::PushConstantRange pushConstantRange;
	pushConstantRange.size = sizeof(float) * 16;
	pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
	piplineLayoutInfo.pushConstantRangeCount = 1;
	piplineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	piplineLayoutInfo.setLayoutCount = 1;
	piplineLayoutInfo.pSetLayouts = &descSetLayout_;
	piplineLayout_ = device.createPipelineLayout(piplineLayoutInfo);

	piplineInfo.layout = piplineLayout_;

	piplineInfo.renderPass = window_->windowRenderPass();

	pipline_ = device.createGraphicsPipeline(window_->pipelineCache(), piplineInfo).value;

	device.destroyShaderModule(vertShader);
	device.destroyShaderModule(fragShader);
}