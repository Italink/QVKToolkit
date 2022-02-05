#include "Glow.h"

Glow::Glow()
{
	setBlurSize(20);
}

void Glow::setBlurSize(int size)
{
	if (size <= 0 || size == blurParams_.size || size >= std::size(blurParams_.weight))
		return;
	blurParams_.size = size;
	float sum = 1, s = 1;
	blurParams_.weight[size - 1] = 1;
	for (int i = size - 2; i >= 0; i--) {
		blurParams_.weight[i] = (blurParams_.weight[i + 1] + s);
		++s;
		sum += blurParams_.weight[i] * 2;
	}
	blurParams_.weight[0] /= sum / 2;
	for (int i = 1; i < size; i++) {
		blurParams_.weight[i] /= sum;
	}

	sum = blurParams_.weight[0];
	for (int i = 1; i < blurParams_.size; i++) {
		sum += blurParams_.weight[i] * 2;
	}
	int k = sum;
}

void Glow::initResources()
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

	vk::DescriptorPoolSize descPoolSize = {
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 4)
	};

	vk::DescriptorPoolCreateInfo descPoolInfo;
	descPoolInfo.maxSets = 3;
	descPoolInfo.poolSizeCount = 1;
	descPoolInfo.pPoolSizes = &descPoolSize;
	descPool_ = device.createDescriptorPool(descPoolInfo);

	vk::DescriptorSetLayoutBinding layoutBinding{
		0, vk::DescriptorType::eCombinedImageSampler,1,vk::ShaderStageFlagBits::eFragment
	};

	vk::DescriptorSetLayoutCreateInfo descLayoutInfo;
	descLayoutInfo.pNext = nullptr;
	descLayoutInfo.flags = {};
	descLayoutInfo.bindingCount = 1;
	descLayoutInfo.pBindings = &layoutBinding;

	blurDescSetLayout_ = device.createDescriptorSetLayout(descLayoutInfo);

	for (int i = 0; i < 2; ++i) {
		vk::DescriptorSetAllocateInfo descSetAllocInfo(descPool_, 1, &blurDescSetLayout_);
		blurDescSet[i] = device.allocateDescriptorSets(descSetAllocInfo).front();
	}

	vk::GraphicsPipelineCreateInfo piplineInfo;
	vk::ShaderModule vertShader = window_->createShaderFromCode(EShLangVertex, R"(#version 450
layout (location = 0) out vec2 outUV;
layout(push_constant) uniform EffectRect{
	vec2 points[4];
}effectRect;

out gl_PerVertex{
	vec4 gl_Position;
};
void main() {
	outUV = effectRect.points[gl_VertexIndex];
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
}
)");

	vk::ShaderModule fragShader = window_->createShaderFromCode(EShLangFragment, R"(#version 450

layout (binding = 0) uniform sampler2D samplerColor;
layout(push_constant) uniform BlurParams{
	layout(offset = 32) int size;
	float weight[10];
}blurParams;
layout (constant_id = 0) const int blurdirection = 0;
layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outFragColor;
void main(){
	vec2 tex_offset = 1.0 / textureSize(samplerColor, 0); // gets size of single texel
	vec4 raw = texture(samplerColor, inUV);
	vec4 result = raw * blurParams.weight[0]; // current fragment's contribution
	for(int i = 1; i < blurParams.size; ++i){
		if (blurdirection == 1){
			result += texture(samplerColor, inUV + vec2(tex_offset.x * i, 0.0)) * blurParams.weight[i];
			result += texture(samplerColor, inUV - vec2(tex_offset.x * i, 0.0)) * blurParams.weight[i];
		}
		else{
			result += texture(samplerColor, inUV + vec2(0.0, tex_offset.y * i)) * blurParams.weight[i];
			result += texture(samplerColor, inUV - vec2(0.0, tex_offset.y * i)) * blurParams.weight[i];
		}
	}
    outFragColor = result;
})");

	vk::SpecializationMapEntry specMapEntry;
	specMapEntry.constantID = 0;
	specMapEntry.offset = 0;
	specMapEntry.size = sizeof(int);
	vk::SpecializationInfo specInfo;
	int specData = 0;
	specInfo.dataSize = sizeof(int);
	specInfo.mapEntryCount = 1;
	specInfo.pMapEntries = &specMapEntry;
	specInfo.pData = &specData;

	vk::PipelineShaderStageCreateInfo piplineShaderStage[2];
	piplineShaderStage[0].stage = vk::ShaderStageFlagBits::eVertex;
	piplineShaderStage[0].module = vertShader;
	piplineShaderStage[0].pName = "main";
	piplineShaderStage[1].stage = vk::ShaderStageFlagBits::eFragment;
	piplineShaderStage[1].module = fragShader;
	piplineShaderStage[1].pName = "main";
	piplineShaderStage[1].pSpecializationInfo = &specInfo;

	piplineInfo.stageCount = 2;
	piplineInfo.pStages = piplineShaderStage;

	vk::PipelineVertexInputStateCreateInfo vertexInputState({}, 0, nullptr, 0, nullptr);
	piplineInfo.pVertexInputState = &vertexInputState;

	vk::PipelineInputAssemblyStateCreateInfo vertexAssemblyState({}, vk::PrimitiveTopology::eTriangleStrip);
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

	MSState.rasterizationSamples = vk::SampleCountFlagBits::e1;
	piplineInfo.pMultisampleState = &MSState;

	vk::PipelineDepthStencilStateCreateInfo DSState;
	DSState.depthTestEnable = false;
	DSState.depthWriteEnable = false;
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

	vk::PushConstantRange pcRange[2];
	pcRange[0].offset = 0;
	pcRange[0].stageFlags = vk::ShaderStageFlagBits::eVertex;
	pcRange[0].size = sizeof(EffectRect);
	pcRange[1].offset = pcRange[0].size;
	pcRange[1].stageFlags = vk::ShaderStageFlagBits::eFragment;
	pcRange[1].size = sizeof(BlurParams);

	vk::PipelineLayoutCreateInfo piplineLayoutInfo;
	piplineLayoutInfo.setLayoutCount = 1;
	piplineLayoutInfo.pSetLayouts = &blurDescSetLayout_;
	piplineLayoutInfo.pushConstantRangeCount = 2;
	piplineLayoutInfo.pPushConstantRanges = pcRange;

	blurPipelineLayout_ = device.createPipelineLayout(piplineLayoutInfo);

	piplineInfo.layout = blurPipelineLayout_;

	piplineInfo.renderPass = window_->singleRenderPass();

	hBlurPipline_ = device.createGraphicsPipeline(window_->pipelineCache(), piplineInfo).value;

	specData = 1;
	vBlurPipline_ = device.createGraphicsPipeline(window_->pipelineCache(), piplineInfo).value;

	device.destroyShaderModule(fragShader);

	vk::DescriptorSetLayoutBinding hdrLayoutBinding[2] = {
		{0, vk::DescriptorType::eCombinedImageSampler,1,vk::ShaderStageFlagBits::eFragment},
		{1, vk::DescriptorType::eCombinedImageSampler,1,vk::ShaderStageFlagBits::eFragment},
	};
	vk::DescriptorSetLayoutCreateInfo hdrDescLayoutInfo;
	hdrDescLayoutInfo.pNext = nullptr;
	hdrDescLayoutInfo.flags = {};
	hdrDescLayoutInfo.bindingCount = 2;
	hdrDescLayoutInfo.pBindings = hdrLayoutBinding;
	hdrDescSetLayout_ = device.createDescriptorSetLayout(hdrDescLayoutInfo);

	vk::DescriptorSetAllocateInfo hdrDescSetAllocInfo(descPool_, 1, &hdrDescSetLayout_);
	hdrDescSet_ = device.allocateDescriptorSets(hdrDescSetAllocInfo).front();

	vk::ShaderModule hdrFragShader = window_->createShaderFromCode(EShLangFragment, R"(#version 450
layout (binding = 0) uniform sampler2D blurImage;
layout (binding = 1) uniform sampler2D rawImage;
layout(push_constant) uniform HDRParams{
	layout(offset = 32) float exposure;
	float gamma;
}params;
layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outFragColor;
void main(){
	vec4 rawColor = texture(rawImage,inUV);
	vec4 blurColor = texture(blurImage,inUV);
	vec3 hdrRGB = blurColor.rgb;
	vec3 mapped = vec3(1.0)-exp(-(hdrRGB*params.exposure));
	mapped = pow(mapped,vec3(1.0f/params.gamma));
    outFragColor =  vec4(mix(mapped,rawColor.rgb*params.exposure,rawColor.a), blurColor.a);
    //outFragColor =  vec4(mapped.rgb,1.0);
})");
	piplineShaderStage[1].module = hdrFragShader;
	piplineShaderStage[1].pSpecializationInfo = nullptr;
	vk::PushConstantRange hdrPCRange[2];

	hdrPCRange[0].offset = 0;
	hdrPCRange[0].stageFlags = vk::ShaderStageFlagBits::eVertex;
	hdrPCRange[0].size = sizeof(EffectRect);
	hdrPCRange[1].offset = pcRange[0].size;
	hdrPCRange[1].stageFlags = vk::ShaderStageFlagBits::eFragment;
	hdrPCRange[1].size = sizeof(HDRParams);

	vk::PipelineLayoutCreateInfo hdrPiplineLayoutInfo;
	hdrPiplineLayoutInfo.setLayoutCount = 1;
	hdrPiplineLayoutInfo.pSetLayouts = &hdrDescSetLayout_;
	hdrPiplineLayoutInfo.pushConstantRangeCount = 2;
	hdrPiplineLayoutInfo.pPushConstantRanges = hdrPCRange;
	hdrPipelineLayout_ = device.createPipelineLayout(hdrPiplineLayoutInfo);

	MSState.rasterizationSamples = window_->sampleCountFlagBits();
	piplineInfo.layout = hdrPipelineLayout_;
	piplineInfo.renderPass = window_->windowRenderPass();
	hdrPipeline_ = device.createGraphicsPipeline(window_->pipelineCache(), piplineInfo).value;
	device.destroyShaderModule(hdrFragShader);
	device.destroyShaderModule(vertShader);
}

void Glow::initSwapChainResources()
{
	vk::Device device = window_->device();
	frameBuffer_[0] = window_->createSingleFrameSource();
	frameBuffer_[1] = window_->createSingleFrameSource();
	for (int i = 0; i < 2; ++i) {
		vk::WriteDescriptorSet descWrite;
		vk::DescriptorImageInfo descImageInfo(sampler_, frameBuffer_[i].imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
		descWrite.dstSet = blurDescSet[i];
		descWrite.dstBinding = 0;
		descWrite.descriptorCount = 1;
		descWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descWrite.pImageInfo = &descImageInfo;
		device.updateDescriptorSets(1, &descWrite, 0, nullptr);
	}

	vk::WriteDescriptorSet descWrite[2];
	vk::DescriptorImageInfo descImageInfo[2];
	for (int i = 0; i < 2; i++) {
		descImageInfo[i].sampler = sampler_;
		descImageInfo[i].imageView = frameBuffer_[i].imageView;
		descImageInfo[i].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		descWrite[i].dstSet = hdrDescSet_;;
		descWrite[i].dstBinding = i;
		descWrite[i].descriptorCount = 1;
		descWrite[i].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descWrite[i].pImageInfo = &descImageInfo[i];
	}
	device.updateDescriptorSets(2, descWrite, 0, nullptr);
}

void Glow::releaseSwapChainResources()
{
	vk::Device device = window_->device();
	window_->destorySingleFrameSource(frameBuffer_[0]);
	window_->destorySingleFrameSource(frameBuffer_[1]);
}

void Glow::releaseResources() {
	vk::Device device = window_->device();
	device.destroySampler(sampler_);
	device.destroyDescriptorPool(descPool_);
	device.destroyDescriptorSetLayout(blurDescSetLayout_);
	device.destroyPipeline(hBlurPipline_);
	device.destroyPipeline(vBlurPipline_);
	device.destroyPipelineLayout(blurPipelineLayout_);

	device.destroyDescriptorSetLayout(hdrDescSetLayout_);
	device.destroyPipeline(hdrPipeline_);
	device.destroyPipelineLayout(hdrPipelineLayout_);
}

void Glow::startNextFrame(FrameContext ctx) {
	vk::Device device = window_->device();

	if (!ctx.overlayRect.size().isValid() || ctx.overlayRect.width() < 2 || ctx.overlayRect.height() < 2)
		return;

	ctx.overlayRect.adjust(-blurParams_.size, -blurParams_.size, blurParams_.size, blurParams_.size);
	ctx.overlayRect &= QRect(0, 0, window_->swapChainImageSize().width(), window_->swapChainImageSize().height());
	vk::CommandBuffer cmdBuffer = ctx.cmdBuffer;

	vk::Image currentImage = ctx.frameImage;
	vk::ImageMemoryBarrier barrier;
	barrier.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
	barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
	barrier.oldLayout = vk::ImageLayout::ePresentSrcKHR;
	barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
	barrier.image = currentImage;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

	barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.image = frameBuffer_[0].image;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

	vk::ImageBlit imageCopy;
	imageCopy.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	imageCopy.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	imageCopy.srcSubresource.layerCount = 1;
	imageCopy.dstSubresource.layerCount = 1;

	imageCopy.srcOffsets[0] = vk::Offset3D(ctx.overlayRect.left(), ctx.overlayRect.top(), 0);
	imageCopy.srcOffsets[1] = vk::Offset3D(ctx.overlayRect.right(), ctx.overlayRect.bottom(), 1);

	imageCopy.dstOffsets[0] = vk::Offset3D(ctx.overlayRect.left(), ctx.overlayRect.top(), 0);
	imageCopy.dstOffsets[1] = vk::Offset3D(ctx.overlayRect.right(), ctx.overlayRect.bottom(), 1);

	cmdBuffer.blitImage(currentImage, vk::ImageLayout::eTransferSrcOptimal, frameBuffer_[0].image, vk::ImageLayout::eTransferDstOptimal, imageCopy, vk::Filter::eNearest);

	vk::ClearValue clearValues[2] = {
		vk::ClearColorValue(std::array<float,4>{0.0f,0.0f,0.0f,0.0f }),
		vk::ClearDepthStencilValue(1.0f,0),
	};

	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.image = frameBuffer_[0].image;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

	barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
	barrier.image = frameBuffer_[1].image;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);
	QRectF localRect;
	localRect.setX(ctx.overlayRect.x() / (float)window_->swapChainImageSize().width());
	localRect.setY(ctx.overlayRect.y() / (float)window_->swapChainImageSize().height());
	localRect.setWidth(ctx.overlayRect.width() / (float)window_->swapChainImageSize().width());
	localRect.setHeight(ctx.overlayRect.height() / (float)window_->swapChainImageSize().height());

	EffectRect effectRect;
	effectRect.points[0] = QVector2D(localRect.bottomLeft());
	effectRect.points[1] = QVector2D(localRect.bottomRight());
	effectRect.points[2] = QVector2D(localRect.topLeft());
	effectRect.points[3] = QVector2D(localRect.topRight());

	vk::RenderPassBeginInfo beginInfo;
	beginInfo.renderPass = window_->singleRenderPass();
	beginInfo.framebuffer = frameBuffer_[1].framebuffer;
	beginInfo.renderArea.extent.width = window_->swapChainImageSize().width();
	beginInfo.renderArea.extent.height = window_->swapChainImageSize().height();
	beginInfo.clearValueCount = 2;
	beginInfo.pClearValues = clearValues;
	cmdBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, hBlurPipline_);
	cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blurPipelineLayout_, 0, 1, &blurDescSet[0], 0, nullptr);
	cmdBuffer.pushConstants<EffectRect>(blurPipelineLayout_, vk::ShaderStageFlagBits::eVertex, 0, effectRect);
	cmdBuffer.pushConstants<BlurParams>(blurPipelineLayout_, vk::ShaderStageFlagBits::eFragment, sizeof(EffectRect), blurParams_);
	cmdBuffer.draw(4, 1, 0, 0);
	cmdBuffer.endRenderPass();

	barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
	barrier.image = frameBuffer_[0].image;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

	barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.image = frameBuffer_[1].image;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

	beginInfo.framebuffer = frameBuffer_[0].framebuffer;
	cmdBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vBlurPipline_);
	cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, blurPipelineLayout_, 0, 1, &blurDescSet[1], 0, nullptr);
	cmdBuffer.pushConstants<EffectRect>(blurPipelineLayout_, vk::ShaderStageFlagBits::eVertex, 0, effectRect);
	cmdBuffer.pushConstants<BlurParams>(blurPipelineLayout_, vk::ShaderStageFlagBits::eFragment, sizeof(EffectRect), blurParams_);
	cmdBuffer.draw(4, 1, 0, 0);
	cmdBuffer.endRenderPass();

	barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.image = frameBuffer_[0].image;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

	barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.image = frameBuffer_[1].image;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);
	cmdBuffer.blitImage(currentImage, vk::ImageLayout::eTransferSrcOptimal, frameBuffer_[1].image, vk::ImageLayout::eTransferDstOptimal, imageCopy, vk::Filter::eNearest);

	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.image = frameBuffer_[1].image;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

	barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
	barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
	barrier.image = currentImage;
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

	vk::RenderPassBeginInfo sbeginInfo;
	sbeginInfo.renderPass = window_->windowRenderPass();
	sbeginInfo.framebuffer = ctx.frameBuffer;
	sbeginInfo.renderArea.extent.width = ctx.viewport.width();
	sbeginInfo.renderArea.extent.height = ctx.viewport.height();

	cmdBuffer.beginRenderPass(sbeginInfo, vk::SubpassContents::eInline);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, hdrPipeline_);
	cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, hdrPipelineLayout_, 0, 1, &hdrDescSet_, 0, nullptr);
	cmdBuffer.pushConstants<EffectRect>(hdrPipelineLayout_, vk::ShaderStageFlagBits::eVertex, 0, effectRect);
	cmdBuffer.pushConstants<HDRParams>(hdrPipelineLayout_, vk::ShaderStageFlagBits::eFragment, sizeof(EffectRect), hdrParams_);
	cmdBuffer.draw(4, 1, 0, 0);
	cmdBuffer.endRenderPass();
}