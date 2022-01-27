/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QVKWindow_h__
#define QVKWindow_h__

#include <QtGui/qvulkaninstance.h>
#include <QtGui/qwindow.h>
#include <QtGui/qimage.h>
#include <QtGui/qmatrix4x4.h>
#include <QtCore/qset.h>
#include <vulkan/vulkan.hpp>
#include "glslang/Public/ShaderLang.h"

class QVKWindowPrivate;
class QVKWindow;

class QVKRenderer : public QObject
{
public:
	friend class QVKWindow;
	friend class QVKScene;
	struct FrameContext {
		vk::Framebuffer frameBuffer;
		vk::Image frameImage;
		vk::ImageView frameImageView;
		QSize viewport;
		QRect overlayRect;
		vk::CommandBuffer cmdBuffer;
	};
public:
	virtual ~QVKRenderer() {}

	virtual void preInitResources() {}
	virtual void initResources() {}
	virtual void initSwapChainResources() {}
	virtual void releaseSwapChainResources() {}
	virtual void releaseResources() {}

	virtual void startNextFrame(FrameContext beginInfo) = 0;

	virtual void physicalDeviceLost() {}
	virtual void logicalDeviceLost() {}

	bool isVkTime() const;

protected:
	QVKWindow* window_ = nullptr;
};

class QVKWindow : public QWindow
{
	Q_OBJECT;
	Q_DECLARE_PRIVATE(QVKWindow)
public:
	enum Flag {
		PersistentResources = 0x01
	};
	Q_DECLARE_FLAGS(Flags, Flag)

		explicit QVKWindow(QWindow* parent = nullptr);
	~QVKWindow();

	void setFlags(Flags flags);
	Flags flags() const;

	QList<VkPhysicalDeviceProperties> availablePhysicalDevices();
	void setPhysicalDeviceIndex(int idx);

	QVulkanInfoVector<QVulkanExtension> supportedDeviceExtensions();
	void setDeviceExtensions(const QByteArrayList& extensions);

	void setPreferredColorFormats(const QList<VkFormat>& formats);

	QList<int> supportedSampleCounts();
	void setSampleCount(int sampleCount);

	typedef std::function<void(const VkQueueFamilyProperties*, uint32_t, QList<VkDeviceQueueCreateInfo>&)> QueueCreateInfoModifier;
	void setQueueCreateInfoModifier(const QueueCreateInfoModifier& modifier);

	bool isValid() const;

	void addRenderer(QSharedPointer<QVKRenderer> renderer);
	void removeRenderer(QSharedPointer<QVKRenderer> renderer);

	void frameReady();

	vk::PhysicalDevice physicalDevice() const;
	const VkPhysicalDeviceProperties* physicalDeviceProperties() const;
	vk::Device device() const;
	vk::Queue graphicsQueue() const;
	uint32_t graphicsQueueFamilyIndex() const;
	vk::CommandPool graphicsCommandPool() const;
	uint32_t hostVisibleMemoryIndex() const;
	uint32_t deviceLocalMemoryIndex() const;
	vk::RenderPass defaultRenderPass() const;

	vk::Format colorFormat();
	vk::Format depthStencilFormat() const;
	QSize swapChainImageSize() const;

	vk::CommandBuffer currentCommandBuffer() const;
	vk::Framebuffer currentFramebuffer() const;
	int currentFrame() const;

	static const int MAX_CONCURRENT_FRAME_COUNT = 3;
	int concurrentFrameCount() const;

	int clearValueCount() const;

	int swapChainImageCount() const;
	int currentSwapChainImageIndex() const;
	vk::Image swapChainImage(int idx) const;
	vk::ImageView swapChainImageView(int idx) const;
	vk::Image depthStencilImage() const;
	vk::ImageView depthStencilImageView() const;

	vk::SampleCountFlagBits sampleCountFlagBits() const;
	vk::Image msaaColorImage(int idx) const;
	vk::ImageView msaaColorImageView(int idx) const;

	vk::PipelineCache pipelineCache();

	bool supportsGrab() const;
	QImage grab();

	QMatrix4x4 clipCorrectionMatrix();

public:
	vk::ShaderModule createShaderFromCode(EShLanguage shaderType, const char* code);
	vk::ShaderModule createShaderFromSpirv(QString path);
Q_SIGNALS:
	void frameGrabbed(const QImage& image);

protected:
	void exposeEvent(QExposeEvent*) override;
	void resizeEvent(QResizeEvent*) override;
	bool event(QEvent*) override;

private:
	Q_DISABLE_COPY(QVKWindow)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVKWindow::Flags)

#endif // QVKWindow_h__