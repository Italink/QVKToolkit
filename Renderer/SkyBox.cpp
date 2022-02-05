#include "SkyBox.h"

static float vertexData[] = { // Y up, front = CCW
		// positions
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
};

// 1011.6.0
#pragma once
const uint32_t skybox_vert[] = {
	0x07230203,0x00010000,0x0008000a,0x00000025,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0008000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000b,0x00000010,
	0x00030003,0x00000002,0x000001b8,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00050005,
	0x00000009,0x65745f76,0x6f6f6378,0x00006472,0x00050005,0x0000000b,0x69736f70,0x6e6f6974,
	0x00000000,0x00060005,0x0000000e,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,
	0x0000000e,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00030005,0x00000010,0x00000000,
	0x00060005,0x00000014,0x68737550,0x736e6f43,0x746e6174,0x00000000,0x00040006,0x00000014,
	0x00000000,0x0070766d,0x00060005,0x00000016,0x68737570,0x736e6f43,0x746e6174,0x00000000,
	0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000b,0x0000001e,0x00000000,
	0x00050048,0x0000000e,0x00000000,0x0000000b,0x00000000,0x00030047,0x0000000e,0x00000002,
	0x00040048,0x00000014,0x00000000,0x00000005,0x00050048,0x00000014,0x00000000,0x00000023,
	0x00000000,0x00050048,0x00000014,0x00000000,0x00000007,0x00000010,0x00030047,0x00000014,
	0x00000002,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,
	0x00000020,0x00040017,0x00000007,0x00000006,0x00000003,0x00040020,0x00000008,0x00000003,
	0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040020,0x0000000a,0x00000001,
	0x00000007,0x0004003b,0x0000000a,0x0000000b,0x00000001,0x00040017,0x0000000d,0x00000006,
	0x00000004,0x0003001e,0x0000000e,0x0000000d,0x00040020,0x0000000f,0x00000003,0x0000000e,
	0x0004003b,0x0000000f,0x00000010,0x00000003,0x00040015,0x00000011,0x00000020,0x00000001,
	0x0004002b,0x00000011,0x00000012,0x00000000,0x00040018,0x00000013,0x0000000d,0x00000004,
	0x0003001e,0x00000014,0x00000013,0x00040020,0x00000015,0x00000009,0x00000014,0x0004003b,
	0x00000015,0x00000016,0x00000009,0x00040020,0x00000017,0x00000009,0x00000013,0x0004002b,
	0x00000006,0x0000001b,0x447a0000,0x0004002b,0x00000006,0x0000001d,0x3f800000,0x00040020,
	0x00000023,0x00000003,0x0000000d,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,
	0x000200f8,0x00000005,0x0004003d,0x00000007,0x0000000c,0x0000000b,0x0003003e,0x00000009,
	0x0000000c,0x00050041,0x00000017,0x00000018,0x00000016,0x00000012,0x0004003d,0x00000013,
	0x00000019,0x00000018,0x0004003d,0x00000007,0x0000001a,0x0000000b,0x0005008e,0x00000007,
	0x0000001c,0x0000001a,0x0000001b,0x00050051,0x00000006,0x0000001e,0x0000001c,0x00000000,
	0x00050051,0x00000006,0x0000001f,0x0000001c,0x00000001,0x00050051,0x00000006,0x00000020,
	0x0000001c,0x00000002,0x00070050,0x0000000d,0x00000021,0x0000001e,0x0000001f,0x00000020,
	0x0000001d,0x00050091,0x0000000d,0x00000022,0x00000019,0x00000021,0x00050041,0x00000023,
	0x00000024,0x00000010,0x00000012,0x0003003e,0x00000024,0x00000022,0x000100fd,0x00010038
};

const uint32_t skybox_frag[] = {
	0x07230203,0x00010000,0x0008000a,0x00000014,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x00000011,0x00030010,
	0x00000004,0x00000007,0x00030003,0x00000002,0x000001b8,0x00040005,0x00000004,0x6e69616d,
	0x00000000,0x00050005,0x00000009,0x67617266,0x6f6c6f43,0x00000072,0x00030005,0x0000000d,
	0x00786574,0x00050005,0x00000011,0x65745f76,0x6f6f6378,0x00006472,0x00040047,0x00000009,
	0x0000001e,0x00000000,0x00040047,0x0000000d,0x00000022,0x00000000,0x00040047,0x0000000d,
	0x00000021,0x00000000,0x00040047,0x00000011,0x0000001e,0x00000000,0x00020013,0x00000002,
	0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,
	0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,
	0x00000009,0x00000003,0x00090019,0x0000000a,0x00000006,0x00000003,0x00000000,0x00000000,
	0x00000000,0x00000001,0x00000000,0x0003001b,0x0000000b,0x0000000a,0x00040020,0x0000000c,
	0x00000000,0x0000000b,0x0004003b,0x0000000c,0x0000000d,0x00000000,0x00040017,0x0000000f,
	0x00000006,0x00000003,0x00040020,0x00000010,0x00000001,0x0000000f,0x0004003b,0x00000010,
	0x00000011,0x00000001,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,
	0x00000005,0x0004003d,0x0000000b,0x0000000e,0x0000000d,0x0004003d,0x0000000f,0x00000012,
	0x00000011,0x00050057,0x00000007,0x00000013,0x0000000e,0x00000012,0x0003003e,0x00000009,
	0x00000013,0x000100fd,0x00010038
};

void SkyBox::initResources()
{
	vk::Device device = window_->device();
	vk::BufferCreateInfo vertexBufferInfo;
	vertexBufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
	vertexBufferInfo.size = sizeof(vertexData);
	vertexBuffer_ = device.createBuffer(vertexBufferInfo);
	vk::MemoryRequirements vertexMemReq = device.getBufferMemoryRequirements(vertexBuffer_);
	vk::MemoryAllocateInfo vertexMemAllocInfo(vertexMemReq.size, window_->hostVisibleMemoryIndex());
	vertexDevMemory_ = device.allocateMemory(vertexMemAllocInfo);
	device.bindBufferMemory(vertexBuffer_, vertexDevMemory_, 0);
	uint8_t* vertexBufferMemPtr = (uint8_t*)device.mapMemory(vertexDevMemory_, 0, vertexMemReq.size);
	memcpy(vertexBufferMemPtr, vertexData, sizeof(vertexData));
	device.unmapMemory(vertexDevMemory_);

	vk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = vk::Filter::eNearest;
	samplerInfo.minFilter = vk::Filter::eNearest;
	samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.maxAnisotropy = 1.0f;
	sampler_ = device.createSampler(samplerInfo);

	QImage images[6] = {
		QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/right.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied),
		QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/left.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied),
		QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/top.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied),
		QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/bottom.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied),
		QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/front.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied),
		QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/back.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied),
	};

	vk::BufferCreateInfo stagingBufferInfo;
	stagingBufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
	stagingBufferInfo.size = images[0].sizeInBytes() * 6;
	vk::Buffer stagingBuffer = device.createBuffer(stagingBufferInfo);
	vk::MemoryRequirements stagingMemReq = device.getBufferMemoryRequirements(stagingBuffer);
	vk::MemoryAllocateInfo stagingMemInfo(stagingMemReq.size, window_->hostVisibleMemoryIndex());
	vk::DeviceMemory stagingMemory = device.allocateMemory(stagingMemInfo);
	device.bindBufferMemory(stagingBuffer, stagingMemory, 0);

	uint8_t* stagingBufferMemPtr = (uint8_t*)device.mapMemory(stagingMemory, 0, stagingMemReq.size);
	for (int i = 0; i < 6; i++) {
		memcpy(stagingBufferMemPtr + i * images[i].sizeInBytes(), images[i].bits(), images[i].sizeInBytes());
	}
	device.unmapMemory(stagingMemory);

	vk::ImageCreateInfo imageInfo;
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.format = vk::Format::eR8G8B8A8Unorm;
	imageInfo.extent.width = images[0].width();
	imageInfo.extent.height = images[0].height();
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;

	imageInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
	imageInfo.arrayLayers = 6;

	image_ = device.createImage(imageInfo);
	vk::MemoryRequirements texMemReq = device.getImageMemoryRequirements(image_);
	vk::MemoryAllocateInfo allocInfo(texMemReq.size, window_->deviceLocalMemoryIndex());
	imageDevMemory_ = device.allocateMemory(allocInfo);
	device.bindImageMemory(image_, imageDevMemory_, 0);

	vk::ImageViewCreateInfo imageViewInfo;
	imageViewInfo.image = image_;
	imageViewInfo.viewType = vk::ImageViewType::eCube;
	imageViewInfo.format = vk::Format::eR8G8B8A8Unorm;
	imageViewInfo.components.r = vk::ComponentSwizzle::eR;
	imageViewInfo.components.g = vk::ComponentSwizzle::eG;
	imageViewInfo.components.b = vk::ComponentSwizzle::eB;
	imageViewInfo.components.a = vk::ComponentSwizzle::eA;
	imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.layerCount = 6;
	imageView_ = device.createImageView(imageViewInfo);

	vk::CommandBufferAllocateInfo cmdBufferAllocInfo;
	cmdBufferAllocInfo.commandBufferCount = 1;
	cmdBufferAllocInfo.commandPool = window_->graphicsCommandPool();
	cmdBufferAllocInfo.level = vk::CommandBufferLevel::ePrimary;
	vk::CommandBuffer cmdBuffer = device.allocateCommandBuffers(cmdBufferAllocInfo).front();
	vk::CommandBufferBeginInfo cmdBufferBeginInfo;
	cmdBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	cmdBuffer.begin(cmdBufferBeginInfo);

	vk::ImageMemoryBarrier barrier;
	barrier.image = image_;
	barrier.oldLayout = vk::ImageLayout::eUndefined;
	barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
	barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.layerCount = 6;
	barrier.subresourceRange.levelCount = 1;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barrier);

	vk::BufferImageCopy bufferCopyRegion;
	bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	bufferCopyRegion.imageSubresource.mipLevel = 0;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 6;
	bufferCopyRegion.imageExtent.width = images[0].width();
	bufferCopyRegion.imageExtent.height = images[0].height();
	bufferCopyRegion.imageExtent.depth = 1;
	bufferCopyRegion.bufferOffset = 0;

	cmdBuffer.copyBufferToImage(stagingBuffer, image_, vk::ImageLayout::eTransferDstOptimal, bufferCopyRegion);

	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barrier);

	cmdBuffer.end();

	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	vk::Queue queue = window_->graphicsQueue();
	queue.submit(submitInfo);
	queue.waitIdle();

	device.destroyBuffer(stagingBuffer);
	device.freeMemory(stagingMemory);

	vk::DescriptorPoolSize descPoolSize = {
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler,1)
	};

	vk::DescriptorPoolCreateInfo descPoolInfo;
	descPoolInfo.maxSets = 1;
	descPoolInfo.poolSizeCount = 1;
	descPoolInfo.pPoolSizes = &descPoolSize;
	descPool_ = device.createDescriptorPool(descPoolInfo);

	vk::DescriptorSetLayoutBinding layoutBinding[2] = {
		{0, vk::DescriptorType::eCombinedImageSampler,1,vk::ShaderStageFlagBits::eFragment}
	};

	vk::DescriptorSetLayoutCreateInfo descLayoutInfo;
	descLayoutInfo.pNext = nullptr;
	descLayoutInfo.flags = {};
	descLayoutInfo.bindingCount = 1;
	descLayoutInfo.pBindings = layoutBinding;

	descSetLayout_ = device.createDescriptorSetLayout(descLayoutInfo);

	vk::DescriptorSetAllocateInfo descSetAllocInfo(descPool_, 1, &descSetLayout_);
	descSet_ = device.allocateDescriptorSets(descSetAllocInfo).front();
	vk::WriteDescriptorSet descWrite;
	vk::DescriptorImageInfo descImageInfo(sampler_, imageView_, vk::ImageLayout::eShaderReadOnlyOptimal);
	descWrite.dstSet = descSet_;
	descWrite.dstBinding = 0;
	descWrite.descriptorCount = 1;
	descWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	descWrite.pImageInfo = &descImageInfo;
	device.updateDescriptorSets(1, &descWrite, 0, nullptr);

	vk::GraphicsPipelineCreateInfo piplineInfo;
	piplineInfo.stageCount = 2;

	vk::ShaderModuleCreateInfo shaderInfo;
	shaderInfo.codeSize = sizeof(skybox_vert);
	shaderInfo.pCode = skybox_vert;
	vk::ShaderModule vertShader = device.createShaderModule(shaderInfo);

	shaderInfo.codeSize = sizeof(skybox_frag);
	shaderInfo.pCode = skybox_frag;
	vk::ShaderModule fragShader = device.createShaderModule(shaderInfo);

	vk::PipelineShaderStageCreateInfo piplineShaderStage[2];
	piplineShaderStage[0].stage = vk::ShaderStageFlagBits::eVertex;
	piplineShaderStage[0].module = vertShader;
	piplineShaderStage[0].pName = "main";
	piplineShaderStage[1].stage = vk::ShaderStageFlagBits::eFragment;
	piplineShaderStage[1].module = fragShader;
	piplineShaderStage[1].pName = "main";
	piplineInfo.pStages = piplineShaderStage;

	vk::VertexInputBindingDescription vertexBindingDesc;
	vertexBindingDesc.binding = 0;
	vertexBindingDesc.stride = 3 * sizeof(float);
	vertexBindingDesc.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputAttributeDescription vertexAttrDesc;
	vertexAttrDesc.binding = 0;
	vertexAttrDesc.location = 0;
	vertexAttrDesc.format = vk::Format::eR32G32B32Sfloat;
	vertexAttrDesc.offset = 0;

	vk::PipelineVertexInputStateCreateInfo vertexInputState({}, 1, &vertexBindingDesc, 1, &vertexAttrDesc);
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
	colorBlendAttachmentState.blendEnable = false;
	colorBlendAttachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendState.pAttachments = &colorBlendAttachmentState;
	piplineInfo.pColorBlendState = &colorBlendState;
	vk::PipelineDynamicStateCreateInfo dynamicState;
	vk::DynamicState dynamicEnables[] = { vk::DynamicState::eViewport ,vk::DynamicState::eScissor };
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicEnables;
	piplineInfo.pDynamicState = &dynamicState;

	vk::PipelineLayoutCreateInfo piplineLayoutInfo;
	piplineLayoutInfo.setLayoutCount = 1;
	piplineLayoutInfo.pSetLayouts = &descSetLayout_;

	vk::PushConstantRange pcRange;
	pcRange.size = sizeof(float) * 16;
	pcRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
	piplineLayoutInfo.pushConstantRangeCount = 1;
	piplineLayoutInfo.pPushConstantRanges = &pcRange;

	piplineLayout_ = device.createPipelineLayout(piplineLayoutInfo);
	piplineInfo.layout = piplineLayout_;

	piplineInfo.renderPass = window_->windowRenderPass();

	piplineCache_ = device.createPipelineCache(vk::PipelineCacheCreateInfo());
	pipline_ = device.createGraphicsPipeline(piplineCache_, piplineInfo).value;

	device.destroyShaderModule(vertShader);
	device.destroyShaderModule(fragShader);
}

void SkyBox::setupImage(QStringList pathList)
{
	vk::Device device = window_->device();
	images[0] = QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/right.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied);
	images[1] = QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/left.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied);
	images[2] = QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/top.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied);
	images[3] = QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/bottom.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied);
	images[4] = QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/front.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied);
	images[5] = QImage("F:/HelloVulkan/02-Advance/Ex03-SkyBox_CubeMaps/skybox/back.jpg").convertedTo(QImage::Format_RGBA8888_Premultiplied);
	if (isVkTime())
		initImageResource();
	else
		needInitImage = true;
}

void SkyBox::initImageResource()
{
	vk::Device device = window_->device();
	vk::BufferCreateInfo stagingBufferInfo;
	stagingBufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
	stagingBufferInfo.size = images[0].sizeInBytes() * 6;
	vk::Buffer stagingBuffer = device.createBuffer(stagingBufferInfo);
	vk::MemoryRequirements stagingMemReq = device.getBufferMemoryRequirements(stagingBuffer);
	vk::MemoryAllocateInfo stagingMemInfo(stagingMemReq.size, window_->hostVisibleMemoryIndex());
	vk::DeviceMemory stagingMemory = device.allocateMemory(stagingMemInfo);
	device.bindBufferMemory(stagingBuffer, stagingMemory, 0);

	uint8_t* stagingBufferMemPtr = (uint8_t*)device.mapMemory(stagingMemory, 0, stagingMemReq.size);
	for (int i = 0; i < 6; i++) {
		memcpy(stagingBufferMemPtr + i * images[i].sizeInBytes(), images[i].bits(), images[i].sizeInBytes());
	}
	device.unmapMemory(stagingMemory);

	vk::ImageCreateInfo imageInfo;
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.format = vk::Format::eR8G8B8A8Unorm;
	imageInfo.extent.width = images[0].width();
	imageInfo.extent.height = images[0].height();
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;

	imageInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;
	imageInfo.arrayLayers = 6;

	image_ = device.createImage(imageInfo);
	vk::MemoryRequirements texMemReq = device.getImageMemoryRequirements(image_);
	vk::MemoryAllocateInfo allocInfo(texMemReq.size, window_->deviceLocalMemoryIndex());
	imageDevMemory_ = device.allocateMemory(allocInfo);
	device.bindImageMemory(image_, imageDevMemory_, 0);

	vk::ImageViewCreateInfo imageViewInfo;
	imageViewInfo.image = image_;
	imageViewInfo.viewType = vk::ImageViewType::eCube;
	imageViewInfo.format = vk::Format::eR8G8B8A8Unorm;
	imageViewInfo.components.r = vk::ComponentSwizzle::eR;
	imageViewInfo.components.g = vk::ComponentSwizzle::eG;
	imageViewInfo.components.b = vk::ComponentSwizzle::eB;
	imageViewInfo.components.a = vk::ComponentSwizzle::eA;
	imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.layerCount = 6;
	imageView_ = device.createImageView(imageViewInfo);

	vk::CommandBufferAllocateInfo cmdBufferAllocInfo;
	cmdBufferAllocInfo.commandBufferCount = 1;
	cmdBufferAllocInfo.commandPool = window_->graphicsCommandPool();
	cmdBufferAllocInfo.level = vk::CommandBufferLevel::ePrimary;
	vk::CommandBuffer cmdBuffer = device.allocateCommandBuffers(cmdBufferAllocInfo).front();
	vk::CommandBufferBeginInfo cmdBufferBeginInfo;
	cmdBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	cmdBuffer.begin(cmdBufferBeginInfo);

	vk::ImageMemoryBarrier barrier;
	barrier.image = image_;
	barrier.oldLayout = vk::ImageLayout::eUndefined;
	barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
	barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.layerCount = 6;
	barrier.subresourceRange.levelCount = 1;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barrier);

	vk::BufferImageCopy bufferCopyRegion;
	bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	bufferCopyRegion.imageSubresource.mipLevel = 0;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 6;
	bufferCopyRegion.imageExtent.width = images[0].width();
	bufferCopyRegion.imageExtent.height = images[0].height();
	bufferCopyRegion.imageExtent.depth = 1;
	bufferCopyRegion.bufferOffset = 0;

	cmdBuffer.copyBufferToImage(stagingBuffer, image_, vk::ImageLayout::eTransferDstOptimal, bufferCopyRegion);

	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barrier);

	cmdBuffer.end();

	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	vk::Queue queue = window_->graphicsQueue();
	queue.submit(submitInfo);
	queue.waitIdle();

	device.destroyBuffer(stagingBuffer);
	device.freeMemory(stagingMemory);

	vk::WriteDescriptorSet descWrite;
	vk::DescriptorImageInfo descImageInfo(sampler_, imageView_, vk::ImageLayout::eShaderReadOnlyOptimal);
	descWrite.dstSet = descSet_;
	descWrite.dstBinding = 0;
	descWrite.descriptorCount = 1;
	descWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	descWrite.pImageInfo = &descImageInfo;
	device.updateDescriptorSets(1, &descWrite, 0, nullptr);
}

void SkyBox::releaseResources()
{
	vk::Device device = window_->device();
	device.destroyPipeline(pipline_);
	device.destroyPipelineCache(piplineCache_);
	device.destroyPipelineLayout(piplineLayout_);
	device.destroyDescriptorSetLayout(descSetLayout_);
	device.destroyDescriptorPool(descPool_);
	device.destroyBuffer(vertexBuffer_);
	device.freeMemory(vertexDevMemory_);
	device.destroySampler(sampler_);
	device.destroyImage(image_);
	device.freeMemory(imageDevMemory_);
	device.destroyImageView(imageView_);
}

void SkyBox::startNextFrame(FrameContext ctx)
{
	if (needInitImage) {
		initImageResource();
		needInitImage = false;
	}

	vk::Device device = window_->device();
	vk::CommandBuffer cmdBuffer = window_->currentCommandBuffer();
	const QSize size = window_->swapChainImageSize();

	vk::RenderPassBeginInfo beginInfo;
	beginInfo.renderPass = window_->windowRenderPass();
	beginInfo.framebuffer = window_->currentFramebuffer();
	beginInfo.renderArea.extent.width = size.width();
	beginInfo.renderArea.extent.height = size.height();

	cmdBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

	QMatrix4x4 matrix;
	matrix *= window_->camera_.getMatrixVP();
	matrix *= window_->clipCorrectionMatrix();
	cmdBuffer.pushConstants(piplineLayout_, vk::ShaderStageFlagBits::eVertex, 0, sizeof(float) * 16, matrix.constData());

	vk::Viewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = size.width();
	viewport.height = size.height();

	viewport.minDepth = 0;
	viewport.maxDepth = 1;
	cmdBuffer.setViewport(0, viewport);

	vk::Rect2D scissor;
	scissor.offset.x = scissor.offset.y = 0;
	scissor.extent.width = size.width();
	scissor.extent.height = size.height();
	cmdBuffer.setScissor(0, scissor);

	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipline_);

	cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplineLayout_, 0, 1, &descSet_, 0, nullptr);

	cmdBuffer.bindVertexBuffers(0, vertexBuffer_, { 0 });

	cmdBuffer.draw(36, 1, 0, 0);

	cmdBuffer.endRenderPass();
}