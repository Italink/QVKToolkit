#ifndef Glow_h__
#define Glow_h__

#include "Core\QVKEffect.h"

class Glow :public QVKEffect {
public:
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;
public:
	Glow();
	void setBlurSize(int size);
protected:
	void initResources() override;
	void releaseResources() override;
	void startNextFrame(FrameContext ctx) override;
private:
	QVKWindow::SingleFrameSource frameBuffer_[2];
	vk::Sampler sampler_;
	vk::DescriptorPool descPool_;
	vk::DescriptorSetLayout blurDescSetLayout_;
	vk::DescriptorSet blurDescSet[2];
	vk::PipelineLayout blurPipelineLayout_;

	vk::Pipeline hBlurPipline_;
	vk::Pipeline vBlurPipline_;

	vk::DescriptorSetLayout hdrDescSetLayout_;
	vk::DescriptorSet hdrDescSet_;
	vk::PipelineLayout hdrPipelineLayout_;
	vk::Pipeline hdrPipeline_;

	struct EffectRect {
		QVector2D points[4];
	};

	struct BlurParams {
		int size = 5;
		float weight[40] = { 0.227027f,0.1945946f,0.1216216f,0.054054f , 0.016216f };
	}blurParams_;

	struct HDRParams {
		float exposure = 3.0f;
		float gamma = 1.0f;
	}hdrParams_;
};

#endif // Glow_h__
