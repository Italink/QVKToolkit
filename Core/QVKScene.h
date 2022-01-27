#ifndef QVKScene_h__
#define QVKScene_h__

#include "QVKWindow.h"

class QVKScene :public QVKRenderer {
public:
	void addRenderer(QSharedPointer<QVKRenderer> renderer);
	void removeRenderer(QSharedPointer<QVKRenderer> renderer);
protected:
	void initResources() override;
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;
	void releaseResources() override;
	void startNextFrame(FrameContext beginInfo) override;
	void physicalDeviceLost() override;
	void logicalDeviceLost() override;

private:
	void recreateFrameBuffer();

private:
	QVector<QSharedPointer<QVKRenderer>> rendererList;

	struct FrameBuffer {
		vk::Framebuffer framebuffer;
		vk::Image image;
		vk::DeviceMemory imageMemory;
		vk::ImageView imageView;
	}frameBuffer_;
};

#endif // QVKScene_h__
