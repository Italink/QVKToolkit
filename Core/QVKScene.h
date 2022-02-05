#ifndef QVKScene_h__
#define QVKScene_h__

#include "QVKWindow.h"

class FullScreenTextureRenderer :public QVKRenderer {
public:
	FullScreenTextureRenderer();
	void initResources() override;
	void releaseResources() override;
	void startNextFrame(FrameContext frameCtx) override;
	void updateImage(vk::ImageView image);
protected:
	vk::Sampler sampler_;
	vk::DescriptorPool descPool_;
	vk::DescriptorSetLayout descSetLayout_;
	vk::DescriptorSet descSet_;
	vk::PipelineLayout piplineLayout_;
	vk::Pipeline pipline_;
};

class QVKScene :public QVKRenderer {
public:
	QVKScene();
	void addRenderer(QSharedPointer<QVKRenderer> renderer);
	void removeRenderer(QSharedPointer<QVKRenderer> renderer);
	void setWindow(QVKWindow* window) override;

protected:
	void initResources() override;
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;
	void releaseResources() override;
	void startNextFrame(FrameContext frameCtx) override;
	void physicalDeviceLost() override;
	void logicalDeviceLost() override;

private:
	QVector<QSharedPointer<QVKRenderer>> rendererList;
	QVKWindow::WindowFrameSource wfs_;
	FullScreenTextureRenderer texRenderer_;
};

#endif // QVKScene_h__
