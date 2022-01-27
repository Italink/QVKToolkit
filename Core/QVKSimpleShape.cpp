#include "QVKSimpleShape.h"

void QVKSimpleShape::submit(const QVector<Vertex>& vertices, const QVector<unsigned int>& indices /*= {}*/)
{
	mVertices = vertices;
	if (indices.isEmpty()) {
		mIndices.resize(vertices.size());
		for (unsigned int i = 0; i < vertices.size(); i++) {
			mIndices[i] = i;
		}
	}
	else {
		mIndices = indices;
	}
	needToUpdate = true;
	AABB aabb;
	for (auto& vertex : vertices) {
		aabb.max.setX(qMax(aabb.max.x(), vertex.position.x()));
		aabb.max.setY(qMax(aabb.max.y(), vertex.position.y()));
		aabb.max.setZ(qMax(aabb.max.z(), vertex.position.z()));
		aabb.min.setX(qMin(aabb.min.x(), vertex.position.x()));
		aabb.min.setY(qMin(aabb.min.y(), vertex.position.y()));
		aabb.min.setZ(qMin(aabb.min.z(), vertex.position.z()));
	}
	setAABB(aabb);
	if (isVkTime()) {
		update();
	}
}

void QVKSimpleShape::update()
{
	needToUpdate = false;
	vk::Device device = window_->device();

	vk::DeviceSize newVertextBufferSize = sizeof(Vertex) * mVertices.size();
	vk::DeviceSize newIndexBufferSize = sizeof(unsigned int) * mIndices.size();

	if (singleBuffer_) {		//如果原先的buffer存在，但尺寸不同，則銷毀
		if (!(vertexBufferInfo_.range == newIndexBufferSize && newIndexBufferSize == indexBufferInfo_.range)) {
			device.destroyBuffer(singleBuffer_);
			device.freeMemory(singleDevMemory_);
			singleBuffer_ = VK_NULL_HANDLE;
			singleDevMemory_ = VK_NULL_HANDLE;
		}
	}
	vertexBufferInfo_.range = newVertextBufferSize;
	indexBufferInfo_.offset = vertexBufferInfo_.range;
	indexBufferInfo_.range = newIndexBufferSize;

	if (!singleBuffer_) {
		vk::BufferCreateInfo singleBufferInfo;
		singleBufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer;
		singleBufferInfo.size = vertexBufferInfo_.range + indexBufferInfo_.range;
		singleBuffer_ = device.createBuffer(singleBufferInfo);

		vk::MemoryRequirements singleMemReq = device.getBufferMemoryRequirements(singleBuffer_);
		vk::MemoryAllocateInfo singleMemAllocInfo(singleMemReq.size, window_->hostVisibleMemoryIndex());

		singleDevMemory_ = device.allocateMemory(singleMemAllocInfo);
		device.bindBufferMemory(singleBuffer_, singleDevMemory_, 0);
	}

	uint8_t* singleBufferMemPtr = (uint8_t*)device.mapMemory(singleDevMemory_, 0, newVertextBufferSize + newIndexBufferSize);
	memcpy(singleBufferMemPtr, mVertices.data(), vertexBufferInfo_.range);
	memcpy(singleBufferMemPtr + indexBufferInfo_.offset, mIndices.data(), indexBufferInfo_.range);
	device.unmapMemory(singleDevMemory_);
}

void QVKSimpleShape::initResources()
{
	if (needToUpdate) {
		update();
	}
	vk::Device device = window_->device();

	vk::GraphicsPipelineCreateInfo piplineInfo;
	piplineInfo.stageCount = 2;

	vk::ShaderModule vertShader = window_->createShaderFromCode(EShLanguage::EShLangVertex, vertexShaderCode());

	vk::ShaderModule fragShader = window_->createShaderFromCode(EShLanguage::EShLangFragment, fragmentShaderCode());

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
	vertexBindingDesc.stride = sizeof(Vertex);
	vertexBindingDesc.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputAttributeDescription vertexAttrDesc[2];
	vertexAttrDesc[0].binding = 0;
	vertexAttrDesc[0].location = 0;
	vertexAttrDesc[0].format = vk::Format::eR32G32B32Sfloat;
	vertexAttrDesc[0].offset = 0;

	vertexAttrDesc[1].binding = 0;
	vertexAttrDesc[1].location = 1;
	vertexAttrDesc[1].format = vk::Format::eR32G32B32A32Sfloat;
	vertexAttrDesc[1].offset = offsetof(Vertex, color);

	vk::PipelineVertexInputStateCreateInfo vertexInputState({}, 1, &vertexBindingDesc, 2, vertexAttrDesc);
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
	piplineLayoutInfo.pushConstantRangeCount = 1;
	vk::PushConstantRange pcRange;
	pcRange.size = sizeof(float) * 16;
	pcRange.stageFlags = vk::ShaderStageFlagBits::eVertex;
	piplineLayoutInfo.pPushConstantRanges = &pcRange;

	piplineLayout_ = device.createPipelineLayout(piplineLayoutInfo);

	piplineInfo.layout = piplineLayout_;
	piplineInfo.renderPass = window_->defaultRenderPass();

	pipline_ = device.createGraphicsPipeline(window_->pipelineCache(), piplineInfo).value;

	device.destroyShaderModule(vertShader);
	device.destroyShaderModule(fragShader);
}

void QVKSimpleShape::releaseResources()
{
	vk::Device device = window_->device();
	device.destroyPipeline(pipline_);
	device.destroyPipelineLayout(piplineLayout_);
	device.destroyBuffer(singleBuffer_);
	device.freeMemory(singleDevMemory_);
	pipline_ = VK_NULL_HANDLE;
	piplineLayout_ = VK_NULL_HANDLE;
	singleBuffer_ = VK_NULL_HANDLE;
	singleDevMemory_ = VK_NULL_HANDLE;
	needToUpdate = true;
}

void QVKSimpleShape::startNextFrame(FrameContext ctx)
{
	vk::Device device = window_->device();

	vk::RenderPassBeginInfo beginInfo;
	beginInfo.renderPass = window_->defaultRenderPass();
	beginInfo.framebuffer = ctx.frameBuffer;
	beginInfo.renderArea.extent.width = ctx.viewport.width();
	beginInfo.renderArea.extent.height = ctx.viewport.height();
	ctx.cmdBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
	ctx.cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipline_);
	ctx.cmdBuffer.pushConstants(piplineLayout_, vk::ShaderStageFlagBits::eVertex, 0, 16 * sizeof(float), calculateMVP().constData());
	ctx.cmdBuffer.bindVertexBuffers(0, singleBuffer_, { 0 });
	ctx.cmdBuffer.bindIndexBuffer(singleBuffer_, indexBufferInfo_.offset, vk::IndexType::eUint32);
	ctx.cmdBuffer.drawIndexed(mIndices.size(), 1, 0, 0, 0);
	ctx.cmdBuffer.endRenderPass();
}

const char* QVKSimpleShape::vertexShaderCode()
{
	return R"(#version 440
layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 v_color;

layout(push_constant) uniform PushConstant{
    mat4 MVP;
}pushConstant;

out gl_PerVertex { vec4 gl_Position; };
void main()
{
    v_color = color;
    gl_Position = pushConstant.MVP * vec4(position,1);
})";
}

const char* QVKSimpleShape::fragmentShaderCode()
{
	return R"(#version 440
layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 fragColor;
void main()
{
    fragColor = v_color;
})";
}