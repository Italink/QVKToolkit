#ifndef SkyBox_h__
#define SkyBox_h__

#include "Core\QVKPrimitive.h"

class SkyBox : public QVKRenderer {
public:
	void setupImage(QStringList pathList = {});
protected:
	void initImageResource();
	void initResources() override;
	void releaseResources() override;
	void startNextFrame(FrameContext ctx) override;
private:
	vk::Buffer vertexBuffer_;
	vk::DeviceMemory vertexDevMemory_;

	vk::DescriptorPool descPool_;
	vk::DescriptorSetLayout descSetLayout_;
	vk::DescriptorSet descSet_;

	vk::PipelineCache piplineCache_;
	vk::PipelineLayout piplineLayout_;
	vk::Pipeline pipline_;

	QImage images[6];

	vk::Sampler sampler_;
	vk::Image image_;
	vk::DeviceMemory imageDevMemory_;
	vk::ImageView imageView_;

	bool needInitImage = false;
};

#endif // SkyBox_h__
