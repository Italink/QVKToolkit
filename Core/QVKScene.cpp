#include "QVKScene.h"

void QVKScene::addRenderer(QSharedPointer<QVKRenderer> renderer)
{
	rendererList << renderer;
	renderer->window_ = window_;
	if (isVkTime())
		renderer->initResources();
}

void QVKScene::removeRenderer(QSharedPointer<QVKRenderer> renderer)
{
	rendererList.removeOne(renderer);
}

void QVKScene::initResources()
{
	for (auto& renderer : rendererList) {
		renderer->initResources();
	}
}

void QVKScene::initSwapChainResources()
{
	for (auto& renderer : rendererList) {
		renderer->initSwapChainResources();
	}
}

void QVKScene::releaseSwapChainResources()
{
	for (auto& renderer : rendererList) {
		renderer->releaseSwapChainResources();
	}
}

void QVKScene::releaseResources()
{
	for (auto& renderer : rendererList) {
		renderer->releaseResources();
	}
}

void QVKScene::startNextFrame(FrameContext beginInfo)
{
	for (auto& renderer : rendererList) {
		renderer->startNextFrame(beginInfo);
	}
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

void QVKScene::recreateFrameBuffer() {
	vk::Device device = window_->device();
	vk::ImageCreateInfo imageInfo;
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.extent.width = window_->swapChainImageSize().width();
	imageInfo.extent.height = window_->swapChainImageSize().height();
	imageInfo.extent.depth = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.mipLevels = 1;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.format = vk::Format::eB8G8R8A8Unorm;
	imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.initialLayout = vk::ImageLayout::ePreinitialized;
	frameBuffer_.image = device.createImage(imageInfo);
	vk::MemoryRequirements memReq = device.getImageMemoryRequirements(frameBuffer_.image);
	vk::MemoryAllocateInfo memAllocInfo(memReq.size, window_->deviceLocalMemoryIndex());
	frameBuffer_.imageMemory = device.allocateMemory(memAllocInfo);
	device.bindImageMemory(frameBuffer_.image, frameBuffer_.imageMemory, 0);
	vk::ImageViewCreateInfo imageViewInfo;
	imageViewInfo.viewType = vk::ImageViewType::e2D;
	imageViewInfo.components = { vk::ComponentSwizzle::eR,vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA };
	imageViewInfo.format = imageInfo.format;
	imageViewInfo.image = frameBuffer_.image;
	imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	imageViewInfo.subresourceRange.layerCount = imageViewInfo.subresourceRange.levelCount = 1;
	frameBuffer_.imageView = device.createImageView(imageViewInfo);

	vk::FramebufferCreateInfo framebufferInfo;
	framebufferInfo.renderPass = window_->defaultRenderPass();
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.pAttachments = &frameBuffer_.imageView;
	framebufferInfo.width = window_->swapChainImageSize().width();
	framebufferInfo.height = window_->swapChainImageSize().height();
	framebufferInfo.layers = 1;
	frameBuffer_.framebuffer = device.createFramebuffer(framebufferInfo);

	vk::ImageMemoryBarrier barrier;
	barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.oldLayout = vk::ImageLayout::ePreinitialized;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.image = frameBuffer_.image;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	vk::CommandBufferAllocateInfo cmdBufferAlllocInfo;
	cmdBufferAlllocInfo.level = vk::CommandBufferLevel::ePrimary;
	cmdBufferAlllocInfo.commandPool = window_->graphicsCommandPool();
	cmdBufferAlllocInfo.commandBufferCount = 1;
	vk::CommandBuffer cmdBuffer = device.allocateCommandBuffers(cmdBufferAlllocInfo).front();
	vk::CommandBufferBeginInfo cmdBufferBeginInfo;
	cmdBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue;
	cmdBuffer.begin(cmdBufferBeginInfo);
	cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);
	cmdBuffer.end();
}