#include "QVKScene.h"
#include "QVKWindowPrivate.h"
#include "QVKPrimitive.h"

QVKScene::QVKScene()
{
}

void QVKScene::addRenderer(QSharedPointer<QVKRenderer> renderer)
{
	rendererList << renderer;
	renderer->setWindow(window_);
	if (isVkTime())
		renderer->initResources();
}

void QVKScene::removeRenderer(QSharedPointer<QVKRenderer> renderer)
{
	rendererList.removeOne(renderer);
}

void QVKScene::setWindow(QVKWindow* window)
{
	QVKRenderer::setWindow(window);
	for (auto& renderer : rendererList) {
		renderer->setWindow(window);
	}
	texRenderer_.setWindow(window);
}

void QVKScene::initResources()
{
	texRenderer_.initResources();
	for (auto& renderer : rendererList) {
		renderer->initResources();
	}
}

void QVKScene::initSwapChainResources() {
	wfs_ = window_->createWindowFrameSource();
	for (auto& renderer : rendererList) {
		renderer->initSwapChainResources();
	}
	texRenderer_.updateImage(wfs_.imageView);
}

void QVKScene::releaseSwapChainResources()
{
	for (auto& renderer : rendererList) {
		renderer->releaseSwapChainResources();
	}
	window_->destoryWindowFrameSource(wfs_);
}

void QVKScene::releaseResources()
{
	texRenderer_.releaseResources();
	for (auto& renderer : rendererList) {
		renderer->releaseResources();
	}
}

void QVKScene::startNextFrame(FrameContext frameCtx)
{
	FrameContext newCtx = frameCtx;
	newCtx.frameImage = wfs_.image;
	newCtx.frameImageView = wfs_.imageView;
	newCtx.overlayRect = QRect();
	newCtx.frameBuffer = wfs_.framebuffer;

	vk::ClearColorValue clearColor(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f });
	vk::ClearDepthStencilValue clearDS(1.0f, 0);
	vk::RenderPassBeginInfo beginInfo;
	beginInfo.renderPass = window_->windowRenderPass();
	beginInfo.framebuffer = newCtx.frameBuffer;
	beginInfo.renderArea.extent.width = newCtx.viewport.width();
	beginInfo.renderArea.extent.height = newCtx.viewport.height();
	frameCtx.cmdBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
	vk::ClearAttachment cleatAtt[2];
	cleatAtt[0].aspectMask = vk::ImageAspectFlagBits::eColor;
	cleatAtt[0].clearValue = clearColor;
	cleatAtt[1].aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
	cleatAtt[1].clearValue = clearDS;
	vk::ClearRect clearRect[2];
	clearRect[0].layerCount = 1;
	clearRect[0].rect.extent.width = window_->swapChainImageSize().width();
	clearRect[0].rect.extent.height = window_->swapChainImageSize().height();
	clearRect[1] = clearRect[0];
	frameCtx.cmdBuffer.clearAttachments(2, cleatAtt, 2, clearRect);
	frameCtx.cmdBuffer.endRenderPass();

	for (auto& renderer : rendererList) {
		renderer->startNextFrame(newCtx);
		QVKPrimitive* primitive = dynamic_cast<QVKPrimitive*>(renderer.data());
		if (primitive) {
			newCtx.overlayRect |= primitive->calculateOverlayArea();
		}
	}

	vk::ImageMemoryBarrier barrier;
	barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
	barrier.oldLayout = vk::ImageLayout::ePresentSrcKHR;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.image = newCtx.frameImage;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;
	frameCtx.cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barrier);

	texRenderer_.startNextFrame(frameCtx);

	barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
	barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
	frameCtx.cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barrier);
}

void QVKScene::physicalDeviceLost()
{
	for (auto& renderer : rendererList) {
		renderer->physicalDeviceLost();
	}
}

void QVKScene::logicalDeviceLost()
{
	for (auto& renderer : rendererList) {
		renderer->logicalDeviceLost();
	}
}

FullScreenTextureRenderer::FullScreenTextureRenderer()
{}

void FullScreenTextureRenderer::initResources()
{
	vk::Device device = window_->device();
	vk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = vk::Filter::eNearest;
	samplerInfo.minFilter = vk::Filter::eNearest;
	samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.maxAnisotropy = 1.0f;
	sampler_ = device.createSampler(samplerInfo);

	vk::DescriptorPoolSize descPoolSize(vk::DescriptorType::eCombinedImageSampler, 1);

	vk::DescriptorPoolCreateInfo descPoolInfo;
	descPoolInfo.maxSets = 1;
	descPoolInfo.poolSizeCount = 1;
	descPoolInfo.pPoolSizes = &descPoolSize;
	descPool_ = device.createDescriptorPool(descPoolInfo);

	vk::DescriptorSetLayoutBinding layoutBinding = { 0, vk::DescriptorType::eCombinedImageSampler,1,vk::ShaderStageFlagBits::eFragment };

	vk::DescriptorSetLayoutCreateInfo descLayoutInfo;
	descLayoutInfo.pNext = nullptr;
	descLayoutInfo.flags = {};
	descLayoutInfo.bindingCount = 1;
	descLayoutInfo.pBindings = &layoutBinding;

	descSetLayout_ = device.createDescriptorSetLayout(descLayoutInfo);

	vk::DescriptorSetAllocateInfo descSetAllocInfo(descPool_, 1, &descSetLayout_);
	descSet_ = device.allocateDescriptorSets(descSetAllocInfo).front();

	vk::GraphicsPipelineCreateInfo piplineInfo;
	piplineInfo.stageCount = 2;

	vk::ShaderModule vertShader = window_->createShaderFromCode(EShLangVertex, R"(#version 450
layout (location = 0) out vec2 outUV;
out gl_PerVertex{
	vec4 gl_Position;
};
void main() {
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
}
)");

	vk::ShaderModule fragShader = window_->createShaderFromCode(EShLangFragment, R"(#version 450
layout (binding = 0) uniform sampler2D samplerColor;
layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outFragColor;
void main() {
	outFragColor = texture(samplerColor, inUV);
})");

	vk::PipelineShaderStageCreateInfo piplineShaderStage[2];
	piplineShaderStage[0].stage = vk::ShaderStageFlagBits::eVertex;
	piplineShaderStage[0].module = vertShader;
	piplineShaderStage[0].pName = "main";
	piplineShaderStage[1].stage = vk::ShaderStageFlagBits::eFragment;
	piplineShaderStage[1].module = fragShader;
	piplineShaderStage[1].pName = "main";
	piplineInfo.pStages = piplineShaderStage;

	vk::PipelineVertexInputStateCreateInfo vertexInputState({}, 0, nullptr, 0, nullptr);
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
	MSState.rasterizationSamples = window_->sampleCountFlagBits();
	piplineInfo.pMultisampleState = &MSState;

	vk::PipelineDepthStencilStateCreateInfo DSState;
	DSState.depthTestEnable = false;
	DSState.depthWriteEnable = false;
	DSState.depthCompareOp = vk::CompareOp::eLessOrEqual;
	piplineInfo.pDepthStencilState = &DSState;

	vk::PipelineColorBlendStateCreateInfo colorBlendState;
	colorBlendState.attachmentCount = 1;
	vk::PipelineColorBlendAttachmentState colorBlendAttachmentState;
	colorBlendAttachmentState.blendEnable = true;
	colorBlendAttachmentState.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	colorBlendAttachmentState.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	colorBlendAttachmentState.colorBlendOp = vk::BlendOp::eAdd;
	colorBlendAttachmentState.srcAlphaBlendFactor = vk::BlendFactor::eOne;
	colorBlendAttachmentState.dstAlphaBlendFactor = vk::BlendFactor::eOne;
	colorBlendAttachmentState.alphaBlendOp = vk::BlendOp::eAdd;
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
	piplineLayout_ = device.createPipelineLayout(piplineLayoutInfo);
	piplineInfo.layout = piplineLayout_;

	piplineInfo.renderPass = window_->windowRenderPass();

	pipline_ = device.createGraphicsPipeline(window_->pipelineCache(), piplineInfo).value;

	device.destroyShaderModule(vertShader);
	device.destroyShaderModule(fragShader);
}

void FullScreenTextureRenderer::releaseResources()
{
	vk::Device device = window_->device();
	device.destroySampler(sampler_);
	device.destroyDescriptorPool(descPool_);
	device.destroyDescriptorSetLayout(descSetLayout_);
	device.destroyPipeline(pipline_);
	device.destroyPipelineLayout(piplineLayout_);
}

void FullScreenTextureRenderer::startNextFrame(FrameContext frameBeginInfo)
{
	vk::CommandBuffer cmdBuffer = window_->currentCommandBuffer();

	vk::RenderPassBeginInfo beginInfo;
	beginInfo.renderPass = window_->windowRenderPass();
	beginInfo.framebuffer = frameBeginInfo.frameBuffer;
	beginInfo.renderArea.extent.width = frameBeginInfo.viewport.width();
	beginInfo.renderArea.extent.height = frameBeginInfo.viewport.height();

	cmdBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipline_);
	cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplineLayout_, 0, 1, &descSet_, 0, nullptr);
	cmdBuffer.draw(4, 1, 0, 0);
	cmdBuffer.endRenderPass();
}

void FullScreenTextureRenderer::updateImage(vk::ImageView image)
{
	vk::Device device = window_->device();
	vk::DescriptorImageInfo descImageInfo(sampler_, image, vk::ImageLayout::eShaderReadOnlyOptimal);
	vk::WriteDescriptorSet descWrite;
	descWrite.dstSet = descSet_;
	descWrite.dstBinding = 0;
	descWrite.descriptorCount = 1;
	descWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	descWrite.pImageInfo = &descImageInfo;
	device.updateDescriptorSets(1, &descWrite, 0, nullptr);
}