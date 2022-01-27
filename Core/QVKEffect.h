#ifndef QVKEffect_h__
#define QVKEffect_h__

#include "QVKWindow.h"

class QVKEffect :public QVKRenderer {
public:
	struct FrameBufferSource {
		vk::Framebuffer framebuffer;
		vk::Image image;
		vk::DeviceMemory imageMemory;
		vk::ImageView imageView;
	};
	QRect lastRect_;
protected:
	void startNextFrame(FrameContext beginInfo) override final;
	virtual void paintEffect(FrameContext beginInfo) = 0;
};

class FullScreenTextureRenderer :public QVKRenderer {
public:

	FullScreenTextureRenderer();
	void initResources() override;
	void releaseResources() override;
	void startNextFrame(FrameContext frameBeginInfo) override;
	void updateImage(vk::ImageView image);
protected:
	vk::Sampler sampler_;
	vk::DescriptorPool descPool_;
	vk::DescriptorSetLayout descSetLayout_;
	vk::DescriptorSet descSet_;
	vk::PipelineLayout piplineLayout_;
	vk::Pipeline pipline_;
};

#endif // QVKEffect_h__
