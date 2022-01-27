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

#include "QVkWindowPrivate.h"
#include "qvulkanfunctions.h"
#include "ResourceLimits.h"
#include "SPIRV/GlslangToSpv.h"
#include "SPIRV/SpvTools.h"
#include <QCoreApplication>
#include <qevent.h>
#include <QFile>
#include <QLoggingCategory>
#include <QThread>
#include <QTimer>
#include <vulkan/vulkan.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

Q_LOGGING_CATEGORY(lcGuiVk, "qt.vulkan")

/*!
  \class QVKWindow
  \inmodule QtGui
  \since 5.10
  \brief The QVKWindow class is a convenience subclass of QWindow to perform Vulkan rendering.

  QVKWindow is a Vulkan-capable QWindow that manages a Vulkan device, a
  graphics queue, a command pool and buffer, a depth-stencil image and a
  double-buffered FIFO swapchain, while taking care of correct behavior when it
  comes to events like resize, special situations like not having a device
  queue supporting both graphics and presentation, device lost scenarios, and
  additional functionality like reading the rendered content back. Conceptually
  it is the counterpart of QOpenGLWindow in the Vulkan world.

  \note QVKWindow does not always eliminate the need to implement a fully
  custom QWindow subclass as it will not necessarily be sufficient in advanced
  use cases.

  QVKWindow can be embedded into QWidget-based user interfaces via
  QWidget::createWindowContainer(). This approach has a number of limitations,
  however. Make sure to study the
  \l{QWidget::createWindowContainer()}{documentation} first.

  A typical application using QVKWindow may look like the following:

  \snippet code/src_gui_vulkan_QVKWindow.cpp 0

  As it can be seen in the example, the main patterns in QVKWindow usage are:

  \list

  \li The QVulkanInstance is associated via QWindow::setVulkanInstance(). It is
  then retrievable via QWindow::vulkanInstance() from everywhere, on any
  thread.

  \li Similarly to QVulkanInstance, device extensions can be queried via
  supportedDeviceExtensions() before the actual initialization. Requesting an
  extension to be enabled is done via setDeviceExtensions(). Such calls must be
  made before the window becomes visible, that is, before calling show() or
  similar functions. Unsupported extension requests are gracefully ignored.

  \li The renderer is implemented in a QVKWindowRenderer subclass, an
  instance of which is created in the createRenderer() factory function.

  \li The core Vulkan commands are exposed via the QVulkanFunctions object,
  retrievable by calling QVulkanInstance::functions(). Device level functions
  are available after creating a VkDevice by calling
  QVulkanInstance::deviceFunctions().

  \li The building of the draw calls for the next frame happens in
  QVKWindowRenderer::startNextFrame(). The implementation is expected to
  add commands to the command buffer returned from currentCommandBuffer().
  Returning from the function does not indicate that the commands are ready for
  submission. Rather, an explicit call to frameReady() is required. This allows
  asynchronous generation of commands, possibly on multiple threads. Simple
  implementations will simply call frameReady() at the end of their
  QVKWindowRenderer::startNextFrame().

  \li The basic Vulkan resources (physical device, graphics queue, a command
  pool, the window's main command buffer, image formats, etc.) are exposed on
  the QVKWindow via lightweight getter functions. Some of these are for
  convenience only, and applications are always free to query, create and
  manage additional resources directly via the Vulkan API.

  \li The renderer lives in the gui/main thread, like the window itself. This
  thread is then throttled to the presentation rate, similarly to how OpenGL
  with a swap interval of 1 would behave. However, the renderer implementation
  is free to utilize multiple threads in any way it sees fit. The accessors
  like vulkanInstance(), currentCommandBuffer(), etc. can be called from any
  thread. The submission of the main command buffer, the queueing of present,
  and the building of the next frame do not start until frameReady() is
  invoked on the gui/main thread.

  \li When the window is made visible, the content is updated automatically.
  Further updates can be requested by calling QWindow::requestUpdate(). To
  render continuously, call requestUpdate() after frameReady().

  \endlist

  For troubleshooting, enable the logging category \c{qt.vulkan}. Critical
  errors are printed via qWarning() automatically.

  \section1 Coordinate system differences between OpenGL and Vulkan

  There are two notable differences to be aware of: First, with Vulkan Y points
  down the screen in clip space, while OpenGL uses an upwards pointing Y axis.
  Second, the standard OpenGL projection matrix assume a near and far plane
  values of -1 and 1, while Vulkan prefers 0 and 1.

  In order to help applications migrate from OpenGL-based code without having
  to flip Y coordinates in the vertex data, and to allow using QMatrix4x4
  functions like QMatrix4x4::perspective() while keeping the Vulkan viewport's
  minDepth and maxDepth set to 0 and 1, QVKWindow provides a correction
  matrix retrievable by calling clipCorrectionMatrix().

  \section1 Multisampling

  While disabled by default, multisample antialiasing is fully supported by
  QVKWindow. Additional color buffers and resolving into the swapchain's
  non-multisample buffers are all managed automatically.

  To query the supported sample counts, call supportedSampleCounts(). When the
  returned set contains 4, 8, ..., passing one of those values to setSampleCount()
  requests multisample rendering.

  \note unlike QSurfaceFormat::setSamples(), the list of supported sample
  counts are exposed to the applications in advance and there is no automatic
  falling back to lower sample counts in setSampleCount(). If the requested value
  is not supported, a warning is shown and a no multisampling will be used.

  \section1 Reading images back

  When supportsGrab() returns true, QVKWindow can perform readbacks from
  the color buffer into a QImage. grab() is a slow and inefficient operation,
  so frequent usage should be avoided. It is nonetheless valuable since it
  allows applications to take screenshots, or tools and tests to process and
  verify the output of the GPU rendering.

  \section1 sRGB support

  While many applications will be fine with the default behavior of
  QVKWindow when it comes to swapchain image formats,
  setPreferredColorFormats() allows requesting a pre-defined format. This is
  useful most notably when working in the sRGB color space. Passing a format
  like \c{VK_FORMAT_B8G8R8A8_SRGB} results in choosing an sRGB format, when
  available.

  \section1 Validation layers

  During application development it can be extremely valuable to have the
  Vulkan validation layers enabled. As shown in the example code above, calling
  QVulkanInstance::setLayers() on the QVulkanInstance before
  QVulkanInstance::create() enables validation, assuming the Vulkan driver
  stack in the system contains the necessary layers.

  \note Be aware of platform-specific differences. On desktop platforms
  installing the \l{https://www.lunarg.com/vulkan-sdk/}{Vulkan SDK} is
  typically sufficient. However, Android for example requires deploying
  additional shared libraries together with the application, and also mandates
  a different list of validation layer names. See
  \l{https://developer.android.com/ndk/guides/graphics/validation-layer.html}{the
  Android Vulkan development pages} for more information.

  \note QVKWindow does not expose device layers since this functionality
  has been deprecated since version 1.0.13 of the Vulkan API.

  \sa QVulkanInstance, QWindow
 */

 /*!
   \class QVKWindowRenderer
   \inmodule QtGui
   \since 5.10

   \brief The QVKWindowRenderer class is used to implement the
   application-specific rendering logic for a QVKWindow.

   Applications typically subclass both QVKWindow and QVKWindowRenderer.
   The former allows handling events, for example, input, while the latter allows
   implementing the Vulkan resource management and command buffer building that
   make up the application's rendering.

   In addition to event handling, the QVKWindow subclass is responsible for
   providing an implementation for QVKWindow::createRenderer() as well. This
   is where the window and renderer get connected. A typical implementation will
   simply create a new instance of a subclass of QVKWindowRenderer.
  */

  /*!
	  Constructs a new QVKWindow with the given \a parent.

	  The surface type is set to QSurface::VulkanSurface.
   */
	QVKWindow::QVKWindow(QWindow* parent) : QWindow(*(new QVKWindowPrivate), parent)
{
	setSurfaceType(QSurface::VulkanSurface);
}

/*!
	Destructor.
*/
QVKWindow::~QVKWindow() { }

QVKWindowPrivate::~QVKWindowPrivate()
{
	// graphics resource cleanup is already done at this point due to
	// QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed
}

/*!
	\enum QVKWindow::Flag

	This enum describes the flags that can be passed to setFlags().

	\value PersistentResources Ensures no graphics resources are released when
	the window becomes unexposed. The default behavior is to release
	everything, and reinitialize later when becoming visible again.
 */

 /*!
	 Configures the behavior based on the provided \a flags.

	 \note This function must be called before the window is made visible or at
	 latest in QVKWindowRenderer::preInitResources(), and has no effect if
	 called afterwards.
  */
void QVKWindow::setFlags(Flags flags)
{
	Q_D(QVKWindow);
	if (d->status != QVKWindowPrivate::StatusUninitialized) {
		qWarning("QVKWindow: Attempted to set flags when already initialized");
		return;
	}
	d->flags = flags;
}

/*!
	Return the requested flags.
 */
QVKWindow::Flags QVKWindow::flags() const
{
	Q_D(const QVKWindow);
	return d->flags;
}

/*!
   Returns the list of properties for the supported physical devices in the system.

   \note This function can be called before making the window visible.
 */
QList<VkPhysicalDeviceProperties> QVKWindow::availablePhysicalDevices()
{
	Q_D(QVKWindow);
	if (!d->physDevs.isEmpty() && !d->physDevProps.isEmpty())
		return d->physDevProps;

	QVulkanInstance* inst = vulkanInstance();
	if (!inst) {
		qWarning("QVKWindow: Attempted to call availablePhysicalDevices() without a "
				 "QVulkanInstance");
		return d->physDevProps;
	}

	QVulkanFunctions* f = inst->functions();
	uint32_t count = 1;
	VkResult err = f->vkEnumeratePhysicalDevices(inst->vkInstance(), &count, nullptr);
	if (err != VK_SUCCESS) {
		qWarning("QVKWindow: Failed to get physical device count: %d", err);
		return d->physDevProps;
	}

	qCDebug(lcGuiVk, "%d physical devices", count);
	if (!count)
		return d->physDevProps;

	QList<VkPhysicalDevice> devs(count);
	err = f->vkEnumeratePhysicalDevices(inst->vkInstance(), &count, devs.data());
	if (err != VK_SUCCESS) {
		qWarning("QVKWindow: Failed to enumerate physical devices: %d", err);
		return d->physDevProps;
	}

	d->physDevs = devs;
	d->physDevProps.resize(count);
	for (uint32_t i = 0; i < count; ++i) {
		VkPhysicalDeviceProperties* p = &d->physDevProps[i];
		f->vkGetPhysicalDeviceProperties(d->physDevs.at(i), p);
		qCDebug(lcGuiVk, "Physical device [%d]: name '%s' version %d.%d.%d", i, p->deviceName,
				VK_VERSION_MAJOR(p->driverVersion), VK_VERSION_MINOR(p->driverVersion),
				VK_VERSION_PATCH(p->driverVersion));
	}

	return d->physDevProps;
}

/*!
	Requests the usage of the physical device with index \a idx. The index
	corresponds to the list returned from availablePhysicalDevices().

	By default the first physical device is used.

	\note This function must be called before the window is made visible or at
	latest in QVKWindowRenderer::preInitResources(), and has no effect if
	called afterwards.
 */
void QVKWindow::setPhysicalDeviceIndex(int idx)
{
	Q_D(QVKWindow);
	if (d->status != QVKWindowPrivate::StatusUninitialized) {
		qWarning("QVKWindow: Attempted to set physical device when already initialized");
		return;
	}
	const int count = availablePhysicalDevices().count();
	if (idx < 0 || idx >= count) {
		qWarning("QVKWindow: Invalid physical device index %d (total physical devices: %d)",
				 idx, count);
		return;
	}
	d->physDevIndex = idx;
}

/*!
	Returns the list of the extensions that are supported by logical devices
	created from the physical device selected by setPhysicalDeviceIndex().

	\note This function can be called before making the window visible.
  */
QVulkanInfoVector<QVulkanExtension> QVKWindow::supportedDeviceExtensions()
{
	Q_D(QVKWindow);

	availablePhysicalDevices();

	if (d->physDevs.isEmpty()) {
		qWarning("QVKWindow: No physical devices found");
		return QVulkanInfoVector<QVulkanExtension>();
	}

	VkPhysicalDevice physDev = d->physDevs.at(d->physDevIndex);
	if (d->supportedDevExtensions.contains(physDev))
		return d->supportedDevExtensions.value(physDev);

	QVulkanFunctions* f = vulkanInstance()->functions();
	uint32_t count = 0;
	VkResult err = f->vkEnumerateDeviceExtensionProperties(physDev, nullptr, &count, nullptr);
	if (err == VK_SUCCESS) {
		QList<VkExtensionProperties> extProps(count);
		err = f->vkEnumerateDeviceExtensionProperties(physDev, nullptr, &count, extProps.data());
		if (err == VK_SUCCESS) {
			QVulkanInfoVector<QVulkanExtension> exts;
			for (const VkExtensionProperties& prop : extProps) {
				QVulkanExtension ext;
				ext.name = prop.extensionName;
				ext.version = prop.specVersion;
				exts.append(ext);
			}
			d->supportedDevExtensions.insert(physDev, exts);
			qDebug(lcGuiVk) << "Supported device extensions:" << exts;
			return exts;
		}
	}

	qWarning("QVKWindow: Failed to query device extension count: %d", err);
	return QVulkanInfoVector<QVulkanExtension>();
}

/*!
	Sets the list of device \a extensions to be enabled.

	Unsupported extensions are ignored.

	The swapchain extension will always be added automatically, no need to
	include it in this list.

	\note This function must be called before the window is made visible or at
	latest in QVKWindowRenderer::preInitResources(), and has no effect if
	called afterwards.
 */
void QVKWindow::setDeviceExtensions(const QByteArrayList& extensions)
{
	Q_D(QVKWindow);
	if (d->status != QVKWindowPrivate::StatusUninitialized) {
		qWarning("QVKWindow: Attempted to set device extensions when already initialized");
		return;
	}
	d->requestedDevExtensions = extensions;
}

/*!
	Sets the preferred \a formats of the swapchain.

	By default no application-preferred format is set. In this case the
	surface's preferred format will be used or, in absence of that,
	\c{VK_FORMAT_B8G8R8A8_UNORM}.

	The list in \a formats is ordered. If the first format is not supported,
	the second will be considered, and so on. When no formats in the list are
	supported, the behavior is the same as in the default case.

	To query the actual format after initialization, call colorFormat().

	\note This function must be called before the window is made visible or at
	latest in QVKWindowRenderer::preInitResources(), and has no effect if
	called afterwards.

	\note Reimplementing QVKWindowRenderer::preInitResources() allows
	dynamically examining the list of supported formats, should that be
	desired. There the surface is retrievable via
	QVulkanInstace::surfaceForWindow(), while this function can still safely be
	called to affect the later stages of initialization.

	\sa colorFormat()
 */
void QVKWindow::setPreferredColorFormats(const QList<VkFormat>& formats)
{
	Q_D(QVKWindow);
	if (d->status != QVKWindowPrivate::StatusUninitialized) {
		qWarning("QVKWindow: Attempted to set preferred color format when already initialized");
		return;
	}
	d->requestedColorFormats = formats;
}

static struct
{
	VkSampleCountFlagBits mask;
	int count;
} qvk_sampleCounts[] = {
	// keep this sorted by 'count'
	{ VK_SAMPLE_COUNT_1_BIT, 1 },  { VK_SAMPLE_COUNT_2_BIT, 2 },   { VK_SAMPLE_COUNT_4_BIT, 4 },
	{ VK_SAMPLE_COUNT_8_BIT, 8 },  { VK_SAMPLE_COUNT_16_BIT, 16 }, { VK_SAMPLE_COUNT_32_BIT, 32 },
	{ VK_SAMPLE_COUNT_64_BIT, 64 }
};

/*!
	Returns the set of supported sample counts when using the physical device
	selected by setPhysicalDeviceIndex(), as a sorted list.

	By default QVKWindow uses a sample count of 1. By calling setSampleCount()
	with a different value (2, 4, 8, ...) from the set returned by this
	function, multisample anti-aliasing can be requested.

	\note This function can be called before making the window visible.

	\sa setSampleCount()
 */
QList<int> QVKWindow::supportedSampleCounts()
{
	Q_D(const QVKWindow);
	QList<int> result;

	availablePhysicalDevices();

	if (d->physDevs.isEmpty()) {
		qWarning("QVKWindow: No physical devices found");
		return result;
	}

	const VkPhysicalDeviceLimits* limits = &d->physDevProps[d->physDevIndex].limits;
	VkSampleCountFlags color = limits->framebufferColorSampleCounts;
	VkSampleCountFlags depth = limits->framebufferDepthSampleCounts;
	VkSampleCountFlags stencil = limits->framebufferStencilSampleCounts;

	for (const auto& qvk_sampleCount : qvk_sampleCounts) {
		if ((color & qvk_sampleCount.mask) && (depth & qvk_sampleCount.mask)
			&& (stencil & qvk_sampleCount.mask)) {
			result.append(qvk_sampleCount.count);
		}
	}

	return result;
}

/*!
	Requests multisample antialiasing with the given \a sampleCount. The valid
	values are 1, 2, 4, 8, ... up until the maximum value supported by the
	physical device.

	When the sample count is greater than 1, QVKWindow will create a
	multisample color buffer instead of simply targeting the swapchain's
	images. The rendering in the multisample buffer will get resolved into the
	non-multisample buffers at the end of each frame.

	To examine the list of supported sample counts, call supportedSampleCounts().

	When setting up the rendering pipeline, call sampleCountFlagBits() to query the
	active sample count as a \c VkSampleCountFlagBits value.

	\note This function must be called before the window is made visible or at
	latest in QVKWindowRenderer::preInitResources(), and has no effect if
	called afterwards.

	\sa supportedSampleCounts(), sampleCountFlagBits()
 */
void QVKWindow::setSampleCount(int sampleCount)
{
	Q_D(QVKWindow);
	if (d->status != QVKWindowPrivate::StatusUninitialized) {
		qWarning("QVKWindow: Attempted to set sample count when already initialized");
		return;
	}

	// Stay compatible with QSurfaceFormat and friends where samples == 0 means the same as 1.
	sampleCount = qBound(1, sampleCount, 64);

	if (!supportedSampleCounts().contains(sampleCount)) {
		qWarning("QVKWindow: Attempted to set unsupported sample count %d", sampleCount);
		return;
	}

	for (const auto& qvk_sampleCount : qvk_sampleCounts) {
		if (qvk_sampleCount.count == sampleCount) {
			d->sampleCount = qvk_sampleCount.mask;
			return;
		}
	}

	Q_UNREACHABLE();
}

void QVKWindowPrivate::init()
{
	Q_Q(QVKWindow);
	Q_ASSERT(status == StatusUninitialized);

	qCDebug(lcGuiVk, "QVKWindow init");

	inst = q->vulkanInstance();
	if (!inst) {
		qWarning("QVKWindow: Attempted to initialize without a QVulkanInstance");
		// This is a simple user error, recheck on the next expose instead of
		// going into the permanent failure state.
		status = StatusFailRetry;
		return;
	}

	surface = QVulkanInstance::surfaceForWindow(q);
	if (surface == VK_NULL_HANDLE) {
		qWarning("QVKWindow: Failed to retrieve Vulkan surface for window");
		status = StatusFailRetry;
		return;
	}

	q->availablePhysicalDevices();

	if (physDevs.isEmpty()) {
		qWarning("QVKWindow: No physical devices found");
		status = StatusFail;
		return;
	}

	if (physDevIndex < 0 || physDevIndex >= physDevs.count()) {
		qWarning("QVKWindow: Invalid physical device index; defaulting to 0");
		physDevIndex = 0;
	}
	qCDebug(lcGuiVk, "Using physical device [%d]", physDevIndex);

	// Give a last chance to do decisions based on the physical device and the surface.
	for (auto& renderer : rendererList) {
		renderer->preInitResources();
	}

	VkPhysicalDevice physDev = physDevs.at(physDevIndex);
	QVulkanFunctions* f = inst->functions();

	uint32_t queueCount = 0;
	f->vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueCount, nullptr);
	QList<VkQueueFamilyProperties> queueFamilyProps(queueCount);
	f->vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueCount, queueFamilyProps.data());
	gfxQueueFamilyIdx = uint32_t(-1);
	presQueueFamilyIdx = uint32_t(-1);
	for (int i = 0; i < queueFamilyProps.count(); ++i) {
		const bool supportsPresent = inst->supportsPresent(physDev, i, q);
		qCDebug(lcGuiVk, "queue family %d: flags=0x%x count=%d supportsPresent=%d", i,
					  queueFamilyProps[i].queueFlags, queueFamilyProps[i].queueCount,
					  supportsPresent);
		if (gfxQueueFamilyIdx == uint32_t(-1)
			&& (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && supportsPresent)
			gfxQueueFamilyIdx = i;
	}
	if (gfxQueueFamilyIdx != uint32_t(-1)) {
		presQueueFamilyIdx = gfxQueueFamilyIdx;
	}
	else {
		qCDebug(lcGuiVk, "No queue with graphics+present; trying separate queues");
		for (int i = 0; i < queueFamilyProps.count(); ++i) {
			if (gfxQueueFamilyIdx == uint32_t(-1)
				&& (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
				gfxQueueFamilyIdx = i;
			if (presQueueFamilyIdx == uint32_t(-1) && inst->supportsPresent(physDev, i, q))
				presQueueFamilyIdx = i;
		}
	}
	if (gfxQueueFamilyIdx == uint32_t(-1)) {
		qWarning("QVKWindow: No graphics queue family found");
		status = StatusFail;
		return;
	}
	if (presQueueFamilyIdx == uint32_t(-1)) {
		qWarning("QVKWindow: No present queue family found");
		status = StatusFail;
		return;
	}
#ifdef QT_DEBUG
	// allow testing the separate present queue case in debug builds on AMD cards
	if (qEnvironmentVariableIsSet("QT_VK_PRESENT_QUEUE_INDEX"))
		presQueueFamilyIdx = qEnvironmentVariableIntValue("QT_VK_PRESENT_QUEUE_INDEX");
#endif
	qCDebug(lcGuiVk, "Using queue families: graphics = %u present = %u", gfxQueueFamilyIdx,
			presQueueFamilyIdx);

	QList<VkDeviceQueueCreateInfo> queueInfo;
	queueInfo.reserve(2);
	const float prio[] = { 0 };
	VkDeviceQueueCreateInfo addQueueInfo;
	memset(&addQueueInfo, 0, sizeof(addQueueInfo));
	addQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	addQueueInfo.queueFamilyIndex = gfxQueueFamilyIdx;
	addQueueInfo.queueCount = 1;
	addQueueInfo.pQueuePriorities = prio;
	queueInfo.append(addQueueInfo);
	if (gfxQueueFamilyIdx != presQueueFamilyIdx) {
		addQueueInfo.queueFamilyIndex = presQueueFamilyIdx;
		addQueueInfo.queueCount = 1;
		addQueueInfo.pQueuePriorities = prio;
		queueInfo.append(addQueueInfo);
	}
	if (queueCreateInfoModifier) {
		queueCreateInfoModifier(queueFamilyProps.constData(), queueCount, queueInfo);
		bool foundGfxQueue = false;
		bool foundPresQueue = false;
		for (const VkDeviceQueueCreateInfo& createInfo : qAsConst(queueInfo)) {
			foundGfxQueue |= createInfo.queueFamilyIndex == gfxQueueFamilyIdx;
			foundPresQueue |= createInfo.queueFamilyIndex == presQueueFamilyIdx;
		}
		if (!foundGfxQueue) {
			qWarning("QVKWindow: Graphics queue missing after call to queueCreateInfoModifier");
			status = StatusFail;
			return;
		}
		if (!foundPresQueue) {
			qWarning("QVKWindow: Present queue missing after call to queueCreateInfoModifier");
			status = StatusFail;
			return;
		}
	}

	// Filter out unsupported extensions in order to keep symmetry
	// with how QVulkanInstance behaves. Add the swapchain extension.
	QList<const char*> devExts;
	QVulkanInfoVector<QVulkanExtension> supportedExtensions = q->supportedDeviceExtensions();
	QByteArrayList reqExts = requestedDevExtensions;
	reqExts.append("VK_KHR_swapchain");

	QByteArray envExts = qgetenv("QT_VULKAN_DEVICE_EXTENSIONS");
	if (!envExts.isEmpty()) {
		QByteArrayList envExtList = envExts.split(';');
		for (auto ext : reqExts)
			envExtList.removeAll(ext);
		reqExts.append(envExtList);
	}

	for (const QByteArray& ext : reqExts) {
		if (supportedExtensions.contains(ext))
			devExts.append(ext.constData());
	}
	qCDebug(lcGuiVk) << "Enabling device extensions:" << devExts;

	VkDeviceCreateInfo devInfo;
	memset(&devInfo, 0, sizeof(devInfo));
	devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devInfo.queueCreateInfoCount = queueInfo.size();
	devInfo.pQueueCreateInfos = queueInfo.constData();
	devInfo.enabledExtensionCount = devExts.count();
	devInfo.ppEnabledExtensionNames = devExts.constData();

	// Device layers are not supported by QVKWindow since that's an already deprecated
	// API. However, have a workaround for systems with older API and layers (f.ex. L4T
	// 24.2 for the Jetson TX1 provides API 1.0.13 and crashes when the validation layer
	// is enabled for the instance but not the device).
	uint32_t apiVersion = physDevProps[physDevIndex].apiVersion;
	if (VK_VERSION_MAJOR(apiVersion) == 1 && VK_VERSION_MINOR(apiVersion) == 0
		&& VK_VERSION_PATCH(apiVersion) <= 13) {
		// Make standard validation work at least.
		const QByteArray stdValName = QByteArrayLiteral("VK_LAYER_KHRONOS_validation");
		const char* stdValNamePtr = stdValName.constData();
		if (inst->layers().contains(stdValName)) {
			uint32_t count = 0;
			VkResult err = f->vkEnumerateDeviceLayerProperties(physDev, &count, nullptr);
			if (err == VK_SUCCESS) {
				QList<VkLayerProperties> layerProps(count);
				err = f->vkEnumerateDeviceLayerProperties(physDev, &count, layerProps.data());
				if (err == VK_SUCCESS) {
					for (const VkLayerProperties& prop : layerProps) {
						if (!strncmp(prop.layerName, stdValNamePtr, stdValName.count())) {
							devInfo.enabledLayerCount = 1;
							devInfo.ppEnabledLayerNames = &stdValNamePtr;
							break;
						}
					}
				}
			}
		}
	}

	VkResult err = f->vkCreateDevice(physDev, &devInfo, nullptr, &dev);
	if (err == VK_ERROR_DEVICE_LOST) {
		qWarning("QVKWindow: Physical device lost");
		for (auto& renderer : rendererList) {
			renderer->physicalDeviceLost();
		}
		// clear the caches so the list of physical devices is re-queried
		physDevs.clear();
		physDevProps.clear();
		status = StatusUninitialized;
		qCDebug(lcGuiVk, "Attempting to restart in 2 seconds");
		QTimer::singleShot(2000, q, [this]() { ensureStarted(); });
		return;
	}
	if (err != VK_SUCCESS) {
		qWarning("QVKWindow: Failed to create device: %d", err);
		status = StatusFail;
		return;
	}

	devFuncs = inst->deviceFunctions(dev);
	Q_ASSERT(devFuncs);

	VkPipelineCacheCreateInfo cacheInfo;
	memset(&cacheInfo, 0, sizeof(cacheInfo));
	cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	devFuncs->vkCreatePipelineCache(dev, &cacheInfo, nullptr, &pipelineCache);

	devFuncs->vkGetDeviceQueue(dev, gfxQueueFamilyIdx, 0, &gfxQueue);
	if (gfxQueueFamilyIdx == presQueueFamilyIdx)
		presQueue = gfxQueue;
	else
		devFuncs->vkGetDeviceQueue(dev, presQueueFamilyIdx, 0, &presQueue);

	VkCommandPoolCreateInfo poolInfo;
	memset(&poolInfo, 0, sizeof(poolInfo));
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = gfxQueueFamilyIdx;
	err = devFuncs->vkCreateCommandPool(dev, &poolInfo, nullptr, &cmdPool);
	if (err != VK_SUCCESS) {
		qWarning("QVKWindow: Failed to create command pool: %d", err);
		status = StatusFail;
		return;
	}
	if (gfxQueueFamilyIdx != presQueueFamilyIdx) {
		poolInfo.queueFamilyIndex = presQueueFamilyIdx;
		err = devFuncs->vkCreateCommandPool(dev, &poolInfo, nullptr, &presCmdPool);
		if (err != VK_SUCCESS) {
			qWarning("QVKWindow: Failed to create command pool for present queue: %d", err);
			status = StatusFail;
			return;
		}
	}

	hostVisibleMemIndex = 0;
	VkPhysicalDeviceMemoryProperties physDevMemProps;
	bool hostVisibleMemIndexSet = false;
	f->vkGetPhysicalDeviceMemoryProperties(physDev, &physDevMemProps);
	for (uint32_t i = 0; i < physDevMemProps.memoryTypeCount; ++i) {
		const VkMemoryType* memType = physDevMemProps.memoryTypes;
		qCDebug(lcGuiVk, "memtype %d: flags=0x%x", i, memType[i].propertyFlags);
		// Find a host visible, host coherent memtype. If there is one that is
		// cached as well (in addition to being coherent), prefer that.
		const int hostVisibleAndCoherent =
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		if ((memType[i].propertyFlags & hostVisibleAndCoherent) == hostVisibleAndCoherent) {
			if (!hostVisibleMemIndexSet
				|| (memType[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) {
				hostVisibleMemIndexSet = true;
				hostVisibleMemIndex = i;
			}
		}
	}
	qCDebug(lcGuiVk, "Picked memtype %d for host visible memory", hostVisibleMemIndex);
	deviceLocalMemIndex = 0;
	for (uint32_t i = 0; i < physDevMemProps.memoryTypeCount; ++i) {
		const VkMemoryType* memType = physDevMemProps.memoryTypes;
		// Just pick the first device local memtype.
		if (memType[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
			deviceLocalMemIndex = i;
			break;
		}
	}
	qCDebug(lcGuiVk, "Picked memtype %d for device local memory", deviceLocalMemIndex);

	if (!vkGetPhysicalDeviceSurfaceCapabilitiesKHR || !vkGetPhysicalDeviceSurfaceFormatsKHR) {
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR =
			reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
					inst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
		vkGetPhysicalDeviceSurfaceFormatsKHR =
			reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
					inst->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceFormatsKHR"));
		if (!vkGetPhysicalDeviceSurfaceCapabilitiesKHR || !vkGetPhysicalDeviceSurfaceFormatsKHR) {
			qWarning("QVKWindow: Physical device surface queries not available");
			status = StatusFail;
			return;
		}
	}

	// Figure out the color format here. Must not wait until recreateSwapChain()
	// because the renderpass should be available already from initResources (so
	// that apps do not have to defer pipeline creation to
	// initSwapChainResources), but the renderpass needs the final color format.

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &formatCount, nullptr);
	QList<VkSurfaceFormatKHR> formats(formatCount);
	if (formatCount)
		vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &formatCount, formats.data());

	colorFormat = VK_FORMAT_B8G8R8A8_UNORM; // our documented default if all else fails
	colorSpace = VkColorSpaceKHR(0); // this is in fact VK_COLOR_SPACE_SRGB_NONLINEAR_KHR

	// Pick the preferred format, if there is one.
	if (!formats.isEmpty() && formats[0].format != VK_FORMAT_UNDEFINED) {
		colorFormat = formats[0].format;
		colorSpace = formats[0].colorSpace;
	}

	// Try to honor the user request.
	if (!formats.isEmpty() && !requestedColorFormats.isEmpty()) {
		for (VkFormat reqFmt : qAsConst(requestedColorFormats)) {
			auto r = std::find_if(
					formats.cbegin(), formats.cend(),
					[reqFmt](const VkSurfaceFormatKHR& sfmt) { return sfmt.format == reqFmt; });
			if (r != formats.cend()) {
				colorFormat = r->format;
				colorSpace = r->colorSpace;
				break;
			}
		}
	}

	const VkFormat dsFormatCandidates[] = { VK_FORMAT_D24_UNORM_S8_UINT,
											VK_FORMAT_D32_SFLOAT_S8_UINT,
											VK_FORMAT_D16_UNORM_S8_UINT };
	const int dsFormatCandidateCount = sizeof(dsFormatCandidates) / sizeof(VkFormat);
	int dsFormatIdx = 0;
	while (dsFormatIdx < dsFormatCandidateCount) {
		dsFormat = dsFormatCandidates[dsFormatIdx];
		VkFormatProperties fmtProp;
		f->vkGetPhysicalDeviceFormatProperties(physDev, dsFormat, &fmtProp);
		if (fmtProp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			break;
		++dsFormatIdx;
	}
	if (dsFormatIdx == dsFormatCandidateCount)
		qWarning("QVKWindow: Failed to find an optimal depth-stencil format");

	qCDebug(lcGuiVk, "Color format: %d Depth-stencil format: %d", colorFormat, dsFormat);

	if (!createDefaultRenderPass())
		return;

	for (auto& renderer : rendererList) {
		renderer->initResources();
	}
	status = StatusDeviceReady;
}

void QVKWindowPrivate::reset()
{
	if (!dev) // do not rely on 'status', a half done init must be cleaned properly too
		return;

	qCDebug(lcGuiVk, "QVKWindow reset");

	devFuncs->vkDeviceWaitIdle(dev);

	for (auto& renderer : rendererList) {
		renderer->releaseResources();
	}
	devFuncs->vkDeviceWaitIdle(dev);

	if (defaultRenderPass) {
		devFuncs->vkDestroyRenderPass(dev, defaultRenderPass, nullptr);
		defaultRenderPass = VK_NULL_HANDLE;
	}

	if (pipelineCache) {
		devFuncs->vkDestroyPipelineCache(dev, pipelineCache, nullptr);
		pipelineCache = VK_NULL_HANDLE;
	}

	if (cmdPool) {
		devFuncs->vkDestroyCommandPool(dev, cmdPool, nullptr);
		cmdPool = VK_NULL_HANDLE;
	}

	if (presCmdPool) {
		devFuncs->vkDestroyCommandPool(dev, presCmdPool, nullptr);
		presCmdPool = VK_NULL_HANDLE;
	}

	if (frameGrabImage) {
		devFuncs->vkDestroyImage(dev, frameGrabImage, nullptr);
		frameGrabImage = VK_NULL_HANDLE;
	}

	if (frameGrabImageMem) {
		devFuncs->vkFreeMemory(dev, frameGrabImageMem, nullptr);
		frameGrabImageMem = VK_NULL_HANDLE;
	}

	if (dev) {
		devFuncs->vkDestroyDevice(dev, nullptr);
		inst->resetDeviceFunctions(dev);
		dev = VK_NULL_HANDLE;
		vkCreateSwapchainKHR =
			nullptr; // re-resolve swapchain funcs later on since some come via the device
	}

	surface = VK_NULL_HANDLE;

	status = StatusUninitialized;
}

bool QVKWindowPrivate::createDefaultRenderPass()
{
	VkAttachmentDescription attDesc[3];
	memset(attDesc, 0, sizeof(attDesc));

	const bool msaa = sampleCount > VK_SAMPLE_COUNT_1_BIT;

	// This is either the non-msaa render target or the resolve target.
	attDesc[0].format = colorFormat;
	attDesc[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attDesc[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // ignored when msaa
	attDesc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attDesc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attDesc[0].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attDesc[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	attDesc[1].format = dsFormat;
	attDesc[1].samples = sampleCount;
	attDesc[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attDesc[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attDesc[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attDesc[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attDesc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attDesc[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	if (msaa) {
		// msaa render target
		attDesc[2].format = colorFormat;
		attDesc[2].samples = sampleCount;
		attDesc[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference resolveRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference dsRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subPassDesc;
	memset(&subPassDesc, 0, sizeof(subPassDesc));
	subPassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPassDesc.colorAttachmentCount = 1;
	subPassDesc.pColorAttachments = &colorRef;
	subPassDesc.pDepthStencilAttachment = &dsRef;

	VkRenderPassCreateInfo rpInfo;
	memset(&rpInfo, 0, sizeof(rpInfo));
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpInfo.attachmentCount = 2;
	rpInfo.pAttachments = attDesc;
	rpInfo.subpassCount = 1;
	rpInfo.pSubpasses = &subPassDesc;

	if (msaa) {
		colorRef.attachment = 2;
		subPassDesc.pResolveAttachments = &resolveRef;
		rpInfo.attachmentCount = 3;
	}

	VkResult err = devFuncs->vkCreateRenderPass(dev, &rpInfo, nullptr, &defaultRenderPass);
	if (err != VK_SUCCESS) {
		qWarning("QVKWindow: Failed to create renderpass: %d", err);
		return false;
	}

	return true;
}

void QVKWindowPrivate::recreateSwapChain()
{
	Q_Q(QVKWindow);
	Q_ASSERT(status >= StatusDeviceReady);

	swapChainImageSize =
		q->size() * q->devicePixelRatio(); // note: may change below due to surfaceCaps

	if (swapChainImageSize.isEmpty()) // handle null window size gracefully
		return;

	QVulkanInstance* inst = q->vulkanInstance();
	QVulkanFunctions* f = inst->functions();
	devFuncs->vkDeviceWaitIdle(dev);

	if (!vkCreateSwapchainKHR) {
		vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
				f->vkGetDeviceProcAddr(dev, "vkCreateSwapchainKHR"));
		vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
				f->vkGetDeviceProcAddr(dev, "vkDestroySwapchainKHR"));
		vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
				f->vkGetDeviceProcAddr(dev, "vkGetSwapchainImagesKHR"));
		vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
				f->vkGetDeviceProcAddr(dev, "vkAcquireNextImageKHR"));
		vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
				f->vkGetDeviceProcAddr(dev, "vkQueuePresentKHR"));
	}

	VkPhysicalDevice physDev = physDevs.at(physDevIndex);
	VkSurfaceCapabilitiesKHR surfaceCaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev, surface, &surfaceCaps);
	uint32_t reqBufferCount;
	if (surfaceCaps.maxImageCount == 0)
		reqBufferCount = qMax<uint32_t>(2, surfaceCaps.minImageCount);
	else
		reqBufferCount =
		qMax(qMin<uint32_t>(surfaceCaps.maxImageCount, 3), surfaceCaps.minImageCount);

	VkExtent2D bufferSize = surfaceCaps.currentExtent;
	if (bufferSize.width == uint32_t(-1)) {
		Q_ASSERT(bufferSize.height == uint32_t(-1));
		bufferSize.width = swapChainImageSize.width();
		bufferSize.height = swapChainImageSize.height();
	}
	else {
		swapChainImageSize = QSize(bufferSize.width, bufferSize.height);
	}

	VkSurfaceTransformFlagBitsKHR preTransform =
		(surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
		: surfaceCaps.currentTransform;

	VkCompositeAlphaFlagBitsKHR compositeAlpha =
		(surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
		? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
		: VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	if (q->requestedFormat().hasAlpha()) {
		if (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
			compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
		else if (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
			compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
	}

	VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainSupportsReadBack = (surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	if (swapChainSupportsReadBack)
		usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	swapChainSupportsWriteBack = surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (swapChainSupportsWriteBack)
		usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VkSwapchainKHR oldSwapChain = swapChain;
	VkSwapchainCreateInfoKHR swapChainInfo;
	memset(&swapChainInfo, 0, sizeof(swapChainInfo));
	swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainInfo.surface = surface;
	swapChainInfo.minImageCount = reqBufferCount;
	swapChainInfo.imageFormat = colorFormat;
	swapChainInfo.imageColorSpace = colorSpace;
	swapChainInfo.imageExtent = bufferSize;
	swapChainInfo.imageArrayLayers = 1;
	swapChainInfo.imageUsage = usage;
	swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChainInfo.preTransform = preTransform;
	swapChainInfo.compositeAlpha = compositeAlpha;
	swapChainInfo.presentMode = presentMode;
	swapChainInfo.clipped = true;
	swapChainInfo.oldSwapchain = oldSwapChain;

	qCDebug(lcGuiVk, "Creating new swap chain of %d buffers, size %dx%d", reqBufferCount,
			bufferSize.width, bufferSize.height);

	VkSwapchainKHR newSwapChain;
	VkResult err = vkCreateSwapchainKHR(dev, &swapChainInfo, nullptr, &newSwapChain);
	if (err != VK_SUCCESS) {
		qWarning("QVKWindow: Failed to create swap chain: %d", err);
		return;
	}

	if (oldSwapChain)
		releaseSwapChain();

	swapChain = newSwapChain;

	uint32_t actualSwapChainBufferCount = 0;
	err = vkGetSwapchainImagesKHR(dev, swapChain, &actualSwapChainBufferCount, nullptr);
	if (err != VK_SUCCESS || actualSwapChainBufferCount < 2) {
		qWarning("QVKWindow: Failed to get swapchain images: %d (count=%d)", err,
				 actualSwapChainBufferCount);
		return;
	}

	qCDebug(lcGuiVk, "Actual swap chain buffer count: %d (supportsReadback=%d)",
			actualSwapChainBufferCount, swapChainSupportsReadBack);
	if (actualSwapChainBufferCount > MAX_SWAPCHAIN_BUFFER_COUNT) {
		qWarning("QVKWindow: Too many swapchain buffers (%d)", actualSwapChainBufferCount);
		return;
	}
	swapChainBufferCount = actualSwapChainBufferCount;

	VkImage swapChainImages[MAX_SWAPCHAIN_BUFFER_COUNT];
	err = vkGetSwapchainImagesKHR(dev, swapChain, &actualSwapChainBufferCount, swapChainImages);
	if (err != VK_SUCCESS) {
		qWarning("QVKWindow: Failed to get swapchain images: %d", err);
		return;
	}

	if (!createTransientImage(dsFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, &dsImage,
		&dsMem, &dsView, 1)) {
		return;
	}

	const bool msaa = sampleCount > VK_SAMPLE_COUNT_1_BIT;
	VkImage msaaImages[MAX_SWAPCHAIN_BUFFER_COUNT];
	VkImageView msaaViews[MAX_SWAPCHAIN_BUFFER_COUNT];

	if (msaa) {
		if (!createTransientImage(colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT, msaaImages, &msaaImageMem, msaaViews,
			swapChainBufferCount)) {
			return;
		}
	}

	VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
									VK_FENCE_CREATE_SIGNALED_BIT };

	for (int i = 0; i < swapChainBufferCount; ++i) {
		ImageResources& image(imageRes[i]);
		image.image = swapChainImages[i];

		if (msaa) {
			image.msaaImage = msaaImages[i];
			image.msaaImageView = msaaViews[i];
		}

		VkImageViewCreateInfo imgViewInfo;
		memset(&imgViewInfo, 0, sizeof(imgViewInfo));
		imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imgViewInfo.image = swapChainImages[i];
		imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imgViewInfo.format = colorFormat;
		imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgViewInfo.subresourceRange.levelCount = imgViewInfo.subresourceRange.layerCount = 1;
		err = devFuncs->vkCreateImageView(dev, &imgViewInfo, nullptr, &image.imageView);
		if (err != VK_SUCCESS) {
			qWarning("QVKWindow: Failed to create swapchain image view %d: %d", i, err);
			return;
		}

		err = devFuncs->vkCreateFence(dev, &fenceInfo, nullptr, &image.cmdFence);
		if (err != VK_SUCCESS) {
			qWarning("QVKWindow: Failed to create command buffer fence: %d", err);
			return;
		}
		image.cmdFenceWaitable = true; // fence was created in signaled state

		VkImageView views[3] = { image.imageView, dsView,
								 msaa ? image.msaaImageView : VK_NULL_HANDLE };
		VkFramebufferCreateInfo fbInfo;
		memset(&fbInfo, 0, sizeof(fbInfo));
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.renderPass = defaultRenderPass;
		fbInfo.attachmentCount = msaa ? 3 : 2;
		fbInfo.pAttachments = views;
		fbInfo.width = swapChainImageSize.width();
		fbInfo.height = swapChainImageSize.height();
		fbInfo.layers = 1;
		VkResult err = devFuncs->vkCreateFramebuffer(dev, &fbInfo, nullptr, &image.fb);
		if (err != VK_SUCCESS) {
			qWarning("QVKWindow: Failed to create framebuffer: %d", err);
			return;
		}

		VkCommandBufferAllocateInfo cmdBufInfo = {
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, cmdPool,
				VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1
		};
		VkCommandBuffer cmdBuffer;
		err = devFuncs->vkAllocateCommandBuffers(dev, &cmdBufInfo, &cmdBuffer);
		if (err != VK_SUCCESS) {
			qWarning("QVKWindow: Failed to allocate acquire-on-present-queue command "
					 "buffer: %d",
					 err);
			return;
		}
		VkCommandBufferBeginInfo cmdBufBeginInfo = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
			VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, nullptr
		};
		err = devFuncs->vkBeginCommandBuffer(cmdBuffer, &cmdBufBeginInfo);
		if (err != VK_SUCCESS) {
			qWarning("QVKWindow: Failed to begin acquire-on-present-queue command buffer: "
					 "%d",
					 err);
			return;
		}
		VkImageMemoryBarrier presTrans;
		memset(&presTrans, 0, sizeof(presTrans));
		presTrans.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		presTrans.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		presTrans.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		presTrans.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		presTrans.srcQueueFamilyIndex = gfxQueueFamilyIdx;
		presTrans.dstQueueFamilyIndex = presQueueFamilyIdx;
		presTrans.image = image.image;
		presTrans.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		presTrans.subresourceRange.levelCount = presTrans.subresourceRange.layerCount = 1;
		devFuncs->vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
									   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
									   nullptr, 1, &presTrans);
		err = devFuncs->vkEndCommandBuffer(cmdBuffer);
		if (err != VK_SUCCESS) {
			qWarning("QVKWindow: Failed to end acquire-on-present-queue command buffer: %d",
					 err);
			return;
		}

		vk::Queue graphicsQueue = gfxQueue;
		vk::SubmitInfo submitInfo;
		submitInfo.commandBufferCount = 1;
		vk::CommandBuffer cmd = cmdBuffer;
		submitInfo.pCommandBuffers = &cmd;
		graphicsQueue.submit(submitInfo);
		graphicsQueue.waitIdle();
		devFuncs->vkFreeCommandBuffers(dev, cmdPool, 1, &cmdBuffer);

		if (gfxQueueFamilyIdx != presQueueFamilyIdx) {
			// pre-build the static image-acquire-on-present-queue command buffer
			VkCommandBufferAllocateInfo cmdBufInfo = {
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, presCmdPool,
				VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1
			};
			err = devFuncs->vkAllocateCommandBuffers(dev, &cmdBufInfo, &image.presTransCmdBuf);
			if (err != VK_SUCCESS) {
				qWarning("QVKWindow: Failed to allocate acquire-on-present-queue command "
						 "buffer: %d",
						 err);
				return;
			}
			VkCommandBufferBeginInfo cmdBufBeginInfo = {
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
				VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, nullptr
			};
			err = devFuncs->vkBeginCommandBuffer(image.presTransCmdBuf, &cmdBufBeginInfo);
			if (err != VK_SUCCESS) {
				qWarning("QVKWindow: Failed to begin acquire-on-present-queue command buffer: "
						 "%d",
						 err);
				return;
			}
			VkImageMemoryBarrier presTrans;
			memset(&presTrans, 0, sizeof(presTrans));
			presTrans.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			presTrans.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			presTrans.oldLayout = presTrans.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			presTrans.srcQueueFamilyIndex = gfxQueueFamilyIdx;
			presTrans.dstQueueFamilyIndex = presQueueFamilyIdx;
			presTrans.image = image.image;
			presTrans.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			presTrans.subresourceRange.levelCount = presTrans.subresourceRange.layerCount = 1;
			devFuncs->vkCmdPipelineBarrier(image.presTransCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
										   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0,
										   nullptr, 1, &presTrans);
			err = devFuncs->vkEndCommandBuffer(image.presTransCmdBuf);
			if (err != VK_SUCCESS) {
				qWarning("QVKWindow: Failed to end acquire-on-present-queue command buffer: %d",
						 err);
				return;
			}
		}
	}

	currentImage = 0;

	VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };
	for (int i = 0; i < frameLag; ++i) {
		FrameResources& frame(frameRes[i]);

		frame.imageAcquired = false;
		frame.imageSemWaitable = false;

		devFuncs->vkCreateFence(dev, &fenceInfo, nullptr, &frame.fence);
		frame.fenceWaitable = true; // fence was created in signaled state

		devFuncs->vkCreateSemaphore(dev, &semInfo, nullptr, &frame.imageSem);
		devFuncs->vkCreateSemaphore(dev, &semInfo, nullptr, &frame.drawSem);
		if (gfxQueueFamilyIdx != presQueueFamilyIdx)
			devFuncs->vkCreateSemaphore(dev, &semInfo, nullptr, &frame.presTransSem);
	}

	currentFrame = 0;

	for (auto& renderer : rendererList) {
		renderer->initSwapChainResources();
	}

	status = StatusReady;
}

uint32_t QVKWindowPrivate::chooseTransientImageMemType(VkImage img, uint32_t startIndex)
{
	VkPhysicalDeviceMemoryProperties physDevMemProps;
	inst->functions()->vkGetPhysicalDeviceMemoryProperties(physDevs[physDevIndex],
														   &physDevMemProps);

	VkMemoryRequirements memReq;
	devFuncs->vkGetImageMemoryRequirements(dev, img, &memReq);
	uint32_t memTypeIndex = uint32_t(-1);

	if (memReq.memoryTypeBits) {
		// Find a device local + lazily allocated, or at least device local memtype.
		const VkMemoryType* memType = physDevMemProps.memoryTypes;
		bool foundDevLocal = false;
		for (uint32_t i = startIndex; i < physDevMemProps.memoryTypeCount; ++i) {
			if (memReq.memoryTypeBits & (1 << i)) {
				if (memType[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
					if (!foundDevLocal) {
						foundDevLocal = true;
						memTypeIndex = i;
					}
					if (memType[i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
						memTypeIndex = i;
						break;
					}
				}
			}
		}
	}

	return memTypeIndex;
}

static inline VkDeviceSize aligned(VkDeviceSize v, VkDeviceSize byteAlign)
{
	return (v + byteAlign - 1) & ~(byteAlign - 1);
}

bool QVKWindowPrivate::createTransientImage(VkFormat format, VkImageUsageFlags usage,
												VkImageAspectFlags aspectMask, VkImage* images,
												VkDeviceMemory* mem, VkImageView* views, int count)
{
	VkMemoryRequirements memReq;
	VkResult err;

	for (int i = 0; i < count; ++i) {
		VkImageCreateInfo imgInfo;
		memset(&imgInfo, 0, sizeof(imgInfo));
		imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imgInfo.imageType = VK_IMAGE_TYPE_2D;
		imgInfo.format = format;
		imgInfo.extent.width = swapChainImageSize.width();
		imgInfo.extent.height = swapChainImageSize.height();
		imgInfo.extent.depth = 1;
		imgInfo.mipLevels = imgInfo.arrayLayers = 1;
		imgInfo.samples = sampleCount;
		imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imgInfo.usage = usage;

		err = devFuncs->vkCreateImage(dev, &imgInfo, nullptr, images + i);
		if (err != VK_SUCCESS) {
			qWarning("QVKWindow: Failed to create image: %d", err);
			return false;
		}

		// Assume the reqs are the same since the images are same in every way.
		// Still, call GetImageMemReq for every image, in order to prevent the
		// validation layer from complaining.
		devFuncs->vkGetImageMemoryRequirements(dev, images[i], &memReq);
	}

	VkMemoryAllocateInfo memInfo;
	memset(&memInfo, 0, sizeof(memInfo));
	memInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memInfo.allocationSize = aligned(memReq.size, memReq.alignment) * count;

	uint32_t startIndex = 0;
	do {
		memInfo.memoryTypeIndex = chooseTransientImageMemType(images[0], startIndex);
		if (memInfo.memoryTypeIndex == uint32_t(-1)) {
			qWarning("QVKWindow: No suitable memory type found");
			return false;
		}
		startIndex = memInfo.memoryTypeIndex + 1;
		qCDebug(lcGuiVk, "Allocating %u bytes for transient image (memtype %u)",
				uint32_t(memInfo.allocationSize), memInfo.memoryTypeIndex);
		err = devFuncs->vkAllocateMemory(dev, &memInfo, nullptr, mem);
		if (err != VK_SUCCESS && err != VK_ERROR_OUT_OF_DEVICE_MEMORY) {
			qWarning("QVKWindow: Failed to allocate image memory: %d", err);
			return false;
		}
	} while (err != VK_SUCCESS);

	VkDeviceSize ofs = 0;
	for (int i = 0; i < count; ++i) {
		err = devFuncs->vkBindImageMemory(dev, images[i], *mem, ofs);
		if (err != VK_SUCCESS) {
			qWarning("QVKWindow: Failed to bind image memory: %d", err);
			return false;
		}
		ofs += aligned(memReq.size, memReq.alignment);

		VkImageViewCreateInfo imgViewInfo;
		memset(&imgViewInfo, 0, sizeof(imgViewInfo));
		imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imgViewInfo.image = images[i];
		imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imgViewInfo.format = format;
		imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		imgViewInfo.subresourceRange.aspectMask = aspectMask;
		imgViewInfo.subresourceRange.levelCount = imgViewInfo.subresourceRange.layerCount = 1;

		err = devFuncs->vkCreateImageView(dev, &imgViewInfo, nullptr, views + i);
		if (err != VK_SUCCESS) {
			qWarning("QVKWindow: Failed to create image view: %d", err);
			return false;
		}
	}

	return true;
}

void QVKWindowPrivate::releaseSwapChain()
{
	if (!dev
		|| !swapChain) // do not rely on 'status', a half done init must be cleaned properly too
		return;

	qCDebug(lcGuiVk, "Releasing swapchain");

	devFuncs->vkDeviceWaitIdle(dev);

	for (auto& renderer : rendererList) {
		renderer->releaseSwapChainResources();
	}
	devFuncs->vkDeviceWaitIdle(dev);

	for (int i = 0; i < frameLag; ++i) {
		FrameResources& frame(frameRes[i]);
		if (frame.fence) {
			if (frame.fenceWaitable)
				devFuncs->vkWaitForFences(dev, 1, &frame.fence, VK_TRUE, UINT64_MAX);
			devFuncs->vkDestroyFence(dev, frame.fence, nullptr);
			frame.fence = VK_NULL_HANDLE;
			frame.fenceWaitable = false;
		}
		if (frame.imageSem) {
			devFuncs->vkDestroySemaphore(dev, frame.imageSem, nullptr);
			frame.imageSem = VK_NULL_HANDLE;
		}
		if (frame.drawSem) {
			devFuncs->vkDestroySemaphore(dev, frame.drawSem, nullptr);
			frame.drawSem = VK_NULL_HANDLE;
		}
		if (frame.presTransSem) {
			devFuncs->vkDestroySemaphore(dev, frame.presTransSem, nullptr);
			frame.presTransSem = VK_NULL_HANDLE;
		}
	}

	for (int i = 0; i < swapChainBufferCount; ++i) {
		ImageResources& image(imageRes[i]);
		if (image.cmdFence) {
			if (image.cmdFenceWaitable)
				devFuncs->vkWaitForFences(dev, 1, &image.cmdFence, VK_TRUE, UINT64_MAX);
			devFuncs->vkDestroyFence(dev, image.cmdFence, nullptr);
			image.cmdFence = VK_NULL_HANDLE;
			image.cmdFenceWaitable = false;
		}
		if (image.fb) {
			devFuncs->vkDestroyFramebuffer(dev, image.fb, nullptr);
			image.fb = VK_NULL_HANDLE;
		}
		if (image.imageView) {
			devFuncs->vkDestroyImageView(dev, image.imageView, nullptr);
			image.imageView = VK_NULL_HANDLE;
		}
		if (image.cmdBuf) {
			devFuncs->vkFreeCommandBuffers(dev, cmdPool, 1, &image.cmdBuf);
			image.cmdBuf = VK_NULL_HANDLE;
		}
		if (image.presTransCmdBuf) {
			devFuncs->vkFreeCommandBuffers(dev, presCmdPool, 1, &image.presTransCmdBuf);
			image.presTransCmdBuf = VK_NULL_HANDLE;
		}
		if (image.msaaImageView) {
			devFuncs->vkDestroyImageView(dev, image.msaaImageView, nullptr);
			image.msaaImageView = VK_NULL_HANDLE;
		}
		if (image.msaaImage) {
			devFuncs->vkDestroyImage(dev, image.msaaImage, nullptr);
			image.msaaImage = VK_NULL_HANDLE;
		}
	}

	if (msaaImageMem) {
		devFuncs->vkFreeMemory(dev, msaaImageMem, nullptr);
		msaaImageMem = VK_NULL_HANDLE;
	}

	if (dsView) {
		devFuncs->vkDestroyImageView(dev, dsView, nullptr);
		dsView = VK_NULL_HANDLE;
	}
	if (dsImage) {
		devFuncs->vkDestroyImage(dev, dsImage, nullptr);
		dsImage = VK_NULL_HANDLE;
	}
	if (dsMem) {
		devFuncs->vkFreeMemory(dev, dsMem, nullptr);
		dsMem = VK_NULL_HANDLE;
	}

	if (swapChain) {
		vkDestroySwapchainKHR(dev, swapChain, nullptr);
		swapChain = VK_NULL_HANDLE;
	}

	if (status == StatusReady)
		status = StatusDeviceReady;
}

/*!
   \internal
 */
void QVKWindow::exposeEvent(QExposeEvent*)
{
	Q_D(QVKWindow);

	if (isExposed()) {
		d->ensureStarted();
	}
	else {
		if (!d->flags.testFlag(PersistentResources)) {
			d->releaseSwapChain();
			d->reset();
		}
	}
}

void QVKWindowPrivate::ensureStarted()
{
	Q_Q(QVKWindow);
	if (status == QVKWindowPrivate::StatusFailRetry)
		status = QVKWindowPrivate::StatusUninitialized;
	if (status == QVKWindowPrivate::StatusUninitialized) {
		init();
		if (status == QVKWindowPrivate::StatusDeviceReady)
			recreateSwapChain();
	}
	if (status == QVKWindowPrivate::StatusReady)
		q->requestUpdate();
}

/*!
   \internal
 */
void QVKWindow::resizeEvent(QResizeEvent*)
{
	// Nothing to do here - recreating the swapchain is handled when building the next frame.
}

/*!
   \internal
 */
bool QVKWindow::event(QEvent* e)
{
	Q_D(QVKWindow);

	switch (e->type()) {
	case QEvent::UpdateRequest:
		d->beginFrame();
		break;

		// The swapchain must be destroyed before the surface as per spec. This is
		// not ideal for us because the surface is managed by the QPlatformWindow
		// which may be gone already when the unexpose comes, making the validation
		// layer scream. The solution is to listen to the PlatformSurface events.
	case QEvent::PlatformSurface:
		if (static_cast<QPlatformSurfaceEvent*>(e)->surfaceEventType()
			== QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
			d->releaseSwapChain();
			d->reset();
		}
		break;

	default:
		break;
	}

	return QWindow::event(e);
}

/*!
	\typedef QVKWindow::QueueCreateInfoModifier

	A function that is called during graphics initialization to add
	additional queues that should be created.

	Set if the renderer needs additional queues besides the default graphics
	queue (e.g. a transfer queue).
	The provided queue family properties can be used to select the indices for
	the additional queues.
	The renderer can subsequently request the actual queue in initResources().

	\note When requesting additional graphics queues, Qt itself always requests
	a graphics queue. You'll need to search queueCreateInfo for the appropriate
	entry and manipulate it to obtain the additional queue.

	\sa setQueueCreateInfoModifier()
 */

 /*!
	 Sets the queue create info modification function \a modifier.

	 \sa QueueCreateInfoModifier

	 \since 5.15
  */
void QVKWindow::setQueueCreateInfoModifier(const QueueCreateInfoModifier& modifier)
{
	Q_D(QVKWindow);
	d->queueCreateInfoModifier = modifier;
}

/*!
	Returns true if this window has successfully initialized all Vulkan
	resources, including the swapchain.

	\note Initialization happens on the first expose event after the window is
	made visible.
 */
bool QVKWindow::isValid() const
{
	Q_D(const QVKWindow);
	return d->status == QVKWindowPrivate::StatusReady;
}

void QVKWindow::addRenderer(QSharedPointer<QVKRenderer> renderer)
{
	Q_D(QVKWindow);
	d->rendererList.push_back(renderer);
	renderer->window_ = this;
	if (device()) {
		renderer->initResources();
		renderer->initSwapChainResources();
	}
}

void QVKWindow::removeRenderer(QSharedPointer<QVKRenderer> renderer)
{
	Q_D(QVKWindow);
	d->rendererList.removeOne(renderer);
}

void QVKWindowPrivate::beginFrame()
{
	if (!swapChain || framePending)
		return;

	Q_Q(QVKWindow);
	if (q->size() * q->devicePixelRatio() != swapChainImageSize) {
		recreateSwapChain();
		if (!swapChain)
			return;
	}

	FrameResources& frame(frameRes[currentFrame]);

	if (!frame.imageAcquired) {
		// Wait if we are too far ahead, i.e. the thread gets throttled based on the presentation
		// rate (note that we are using FIFO mode -> vsync)
		if (frame.fenceWaitable) {
			devFuncs->vkWaitForFences(dev, 1, &frame.fence, VK_TRUE, UINT64_MAX);
			devFuncs->vkResetFences(dev, 1, &frame.fence);
			frame.fenceWaitable = false;
		}

		// move on to next swapchain image
		VkResult err = vkAcquireNextImageKHR(dev, swapChain, UINT64_MAX, frame.imageSem,
											 frame.fence, &currentImage);
		if (err == VK_SUCCESS || err == VK_SUBOPTIMAL_KHR) {
			frame.imageSemWaitable = true;
			frame.imageAcquired = true;
			frame.fenceWaitable = true;
		}
		else if (err == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			q->requestUpdate();
			return;
		}
		else {
			if (!checkDeviceLost(err))
				qWarning("QVKWindow: Failed to acquire next swapchain image: %d", err);
			q->requestUpdate();
			return;
		}
	}

	// make sure the previous draw for the same image has finished
	ImageResources& image(imageRes[currentImage]);
	if (image.cmdFenceWaitable) {
		devFuncs->vkWaitForFences(dev, 1, &image.cmdFence, VK_TRUE, UINT64_MAX);
		devFuncs->vkResetFences(dev, 1, &image.cmdFence);
		image.cmdFenceWaitable = false;
	}

	// build new draw command buffer
	if (image.cmdBuf) {
		devFuncs->vkFreeCommandBuffers(dev, cmdPool, 1, &image.cmdBuf);
		image.cmdBuf = nullptr;
	}

	VkCommandBufferAllocateInfo cmdBufInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
											   nullptr, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
											   1 };
	VkResult err = devFuncs->vkAllocateCommandBuffers(dev, &cmdBufInfo, &image.cmdBuf);
	if (err != VK_SUCCESS) {
		if (!checkDeviceLost(err))
			qWarning("QVKWindow: Failed to allocate frame command buffer: %d", err);
		return;
	}

	VkCommandBufferBeginInfo cmdBufBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
												 nullptr, 0, nullptr };
	err = devFuncs->vkBeginCommandBuffer(image.cmdBuf, &cmdBufBeginInfo);
	if (err != VK_SUCCESS) {
		if (!checkDeviceLost(err))
			qWarning("QVKWindow: Failed to begin frame command buffer: %d", err);
		return;
	}

	if (frameGrabbing)
		frameGrabTargetImage = QImage(swapChainImageSize, QImage::Format_RGBA8888);

	QVKRenderer::FrameContext beginfo;
	beginfo.frameBuffer = imageRes[currentImage].fb;
	beginfo.frameImage = imageRes[currentImage].image;
	beginfo.frameImageView = imageRes[currentImage].imageView;
	beginfo.cmdBuffer = imageRes[currentImage].cmdBuf;
	beginfo.viewport = swapChainImageSize;
	beginfo.overlayRect = QRect(QPoint(0, 0), swapChainImageSize);

	if (!rendererList.isEmpty()) {
		vk::Viewport viewport;
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = beginfo.viewport.width();
		viewport.height = beginfo.viewport.height();
		viewport.minDepth = 0;
		viewport.maxDepth = 1;
		beginfo.cmdBuffer.setViewport(0, viewport);
		vk::Rect2D scissor;
		scissor.offset.x = scissor.offset.y = 0;
		scissor.extent.width = beginfo.viewport.width();
		scissor.extent.height = beginfo.viewport.height();
		beginfo.cmdBuffer.setScissor(0, scissor);
	}

	for (auto& renderer : rendererList) {
		framePending = true;
		renderer->startNextFrame(beginfo);
	}
	if (!rendererList.isEmpty()) {
		q->frameReady();
		q->requestUpdate();
	}

	else {
		VkRenderPassBeginInfo rpBeginInfo;
		memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
		rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpBeginInfo.renderPass = defaultRenderPass;
		rpBeginInfo.framebuffer = image.fb;
		rpBeginInfo.renderArea.extent.width = swapChainImageSize.width();
		rpBeginInfo.renderArea.extent.height = swapChainImageSize.height();
		devFuncs->vkCmdBeginRenderPass(image.cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		devFuncs->vkCmdEndRenderPass(image.cmdBuf);
		endFrame();
	}
}

void QVKWindowPrivate::endFrame()
{
	Q_Q(QVKWindow);

	FrameResources& frame(frameRes[currentFrame]);
	ImageResources& image(imageRes[currentImage]);

	if (gfxQueueFamilyIdx != presQueueFamilyIdx && !frameGrabbing) {
		// Add the swapchain image release to the command buffer that will be
		// submitted to the graphics queue.
		VkImageMemoryBarrier presTrans;
		memset(&presTrans, 0, sizeof(presTrans));
		presTrans.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		presTrans.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		presTrans.oldLayout = presTrans.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		presTrans.srcQueueFamilyIndex = gfxQueueFamilyIdx;
		presTrans.dstQueueFamilyIndex = presQueueFamilyIdx;
		presTrans.image = image.image;
		presTrans.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		presTrans.subresourceRange.levelCount = presTrans.subresourceRange.layerCount = 1;
		devFuncs->vkCmdPipelineBarrier(image.cmdBuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
									   VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
									   nullptr, 1, &presTrans);
	}

	// When grabbing a frame, add a readback at the end and skip presenting.
	if (frameGrabbing)
		addReadback();

	VkResult err = devFuncs->vkEndCommandBuffer(image.cmdBuf);
	if (err != VK_SUCCESS) {
		if (!checkDeviceLost(err))
			qWarning("QVKWindow: Failed to end frame command buffer: %d", err);
		return;
	}

	// submit draw calls
	VkSubmitInfo submitInfo;
	memset(&submitInfo, 0, sizeof(submitInfo));
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &image.cmdBuf;
	if (frame.imageSemWaitable) {
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &frame.imageSem;
	}
	if (!frameGrabbing) {
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &frame.drawSem;
	}
	VkPipelineStageFlags psf = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitInfo.pWaitDstStageMask = &psf;

	Q_ASSERT(!image.cmdFenceWaitable);

	err = devFuncs->vkQueueSubmit(gfxQueue, 1, &submitInfo, image.cmdFence);
	if (err == VK_SUCCESS) {
		frame.imageSemWaitable = false;
		image.cmdFenceWaitable = true;
	}
	else {
		if (!checkDeviceLost(err))
			qWarning("QVKWindow: Failed to submit to graphics queue: %d", err);
		return;
	}

	// block and then bail out when grabbing
	if (frameGrabbing) {
		finishBlockingReadback();
		frameGrabbing = false;
		// Leave frame.imageAcquired set to true.
		// Do not change currentFrame.
		emit q->frameGrabbed(frameGrabTargetImage);
		return;
	}

	if (gfxQueueFamilyIdx != presQueueFamilyIdx) {
		// Submit the swapchain image acquire to the present queue.
		submitInfo.pWaitSemaphores = &frame.drawSem;
		submitInfo.pSignalSemaphores = &frame.presTransSem;
		submitInfo.pCommandBuffers = &image.presTransCmdBuf; // must be USAGE_SIMULTANEOUS
		err = devFuncs->vkQueueSubmit(presQueue, 1, &submitInfo, VK_NULL_HANDLE);
		if (err != VK_SUCCESS) {
			if (!checkDeviceLost(err))
				qWarning("QVKWindow: Failed to submit to present queue: %d", err);
			return;
		}
	}

	// queue present
	VkPresentInfoKHR presInfo;
	memset(&presInfo, 0, sizeof(presInfo));
	presInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presInfo.swapchainCount = 1;
	presInfo.pSwapchains = &swapChain;
	presInfo.pImageIndices = &currentImage;
	presInfo.waitSemaphoreCount = 1;
	presInfo.pWaitSemaphores =
		gfxQueueFamilyIdx == presQueueFamilyIdx ? &frame.drawSem : &frame.presTransSem;

	// Do platform-specific WM notification. F.ex. essential on Wayland in
	// order to circumvent driver frame callbacks
	inst->presentAboutToBeQueued(q);

	err = vkQueuePresentKHR(gfxQueue, &presInfo);
	if (err != VK_SUCCESS) {
		if (err == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			q->requestUpdate();
			return;
		}
		else if (err != VK_SUBOPTIMAL_KHR) {
			if (!checkDeviceLost(err))
				qWarning("QVKWindow: Failed to present: %d", err);
			return;
		}
	}

	frame.imageAcquired = false;

	inst->presentQueued(q);

	currentFrame = (currentFrame + 1) % frameLag;
}

/*!
	This function must be called exactly once in response to each invocation of
	the QVKWindowRenderer::startNextFrame() implementation. At the time of
	this call, the main command buffer, exposed via currentCommandBuffer(),
	must have all necessary rendering commands added to it since this function
	will trigger submitting the commands and queuing the present command.

	\note This function must only be called from the gui/main thread, which is
	where QVKWindowRenderer's functions are invoked and where the
	QVKWindow instance lives.

	\sa QVKWindowRenderer::startNextFrame()
 */
void QVKWindow::frameReady()
{
	Q_ASSERT_X(QThread::currentThread() == QCoreApplication::instance()->thread(), "QVKWindow",
			   "frameReady() can only be called from the GUI (main) thread");

	Q_D(QVKWindow);

	if (!d->framePending) {
		qWarning("QVKWindow: frameReady() called without a corresponding startNextFrame()");
		return;
	}

	d->framePending = false;

	d->endFrame();
}

bool QVKWindowPrivate::checkDeviceLost(VkResult err)
{
	if (err == VK_ERROR_DEVICE_LOST) {
		qWarning("QVKWindow: Device lost");
		for (auto& renderer : rendererList) {
			renderer->logicalDeviceLost();
		}
		qCDebug(lcGuiVk, "Releasing all resources due to device lost");
		releaseSwapChain();
		reset();
		qCDebug(lcGuiVk, "Restarting");
		ensureStarted();
		return true;
	}
	return false;
}

void QVKWindowPrivate::addReadback()
{
	VkImageCreateInfo imageInfo;
	memset(&imageInfo, 0, sizeof(imageInfo));
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent.width = frameGrabTargetImage.width();
	imageInfo.extent.height = frameGrabTargetImage.height();
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

	VkResult err = devFuncs->vkCreateImage(dev, &imageInfo, nullptr, &frameGrabImage);
	if (err != VK_SUCCESS) {
		qWarning("QVKWindow: Failed to create image for readback: %d", err);
		return;
	}

	VkMemoryRequirements memReq;
	devFuncs->vkGetImageMemoryRequirements(dev, frameGrabImage, &memReq);

	VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReq.size,
									   hostVisibleMemIndex };

	err = devFuncs->vkAllocateMemory(dev, &allocInfo, nullptr, &frameGrabImageMem);
	if (err != VK_SUCCESS) {
		qWarning("QVKWindow: Failed to allocate memory for readback image: %d", err);
		return;
	}

	err = devFuncs->vkBindImageMemory(dev, frameGrabImage, frameGrabImageMem, 0);
	if (err != VK_SUCCESS) {
		qWarning("QVKWindow: Failed to bind readback image memory: %d", err);
		return;
	}

	ImageResources& image(imageRes[currentImage]);

	VkImageMemoryBarrier barrier;
	memset(&barrier, 0, sizeof(barrier));
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;

	barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.image = image.image;

	devFuncs->vkCmdPipelineBarrier(image.cmdBuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
								   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
								   &barrier);

	barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.image = frameGrabImage;

	devFuncs->vkCmdPipelineBarrier(image.cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
								   VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
								   &barrier);

	VkImageCopy copyInfo;
	memset(&copyInfo, 0, sizeof(copyInfo));
	copyInfo.srcSubresource.aspectMask = copyInfo.dstSubresource.aspectMask =
		VK_IMAGE_ASPECT_COLOR_BIT;
	copyInfo.srcSubresource.layerCount = copyInfo.dstSubresource.layerCount = 1;
	copyInfo.extent.width = frameGrabTargetImage.width();
	copyInfo.extent.height = frameGrabTargetImage.height();
	copyInfo.extent.depth = 1;

	devFuncs->vkCmdCopyImage(image.cmdBuf, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							 frameGrabImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyInfo);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	barrier.image = frameGrabImage;

	devFuncs->vkCmdPipelineBarrier(image.cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
								   VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 0, nullptr, 1,
								   &barrier);
}

void QVKWindowPrivate::finishBlockingReadback()
{
	ImageResources& image(imageRes[currentImage]);

	// Block until the current frame is done. Normally this wait would only be
	// done in current + concurrentFrameCount().
	devFuncs->vkWaitForFences(dev, 1, &image.cmdFence, VK_TRUE, UINT64_MAX);
	devFuncs->vkResetFences(dev, 1, &image.cmdFence);
	// will reuse the same image for the next "real" frame, do not wait then
	image.cmdFenceWaitable = false;

	VkImageSubresource subres = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
	VkSubresourceLayout layout;
	devFuncs->vkGetImageSubresourceLayout(dev, frameGrabImage, &subres, &layout);

	uchar* p;
	VkResult err = devFuncs->vkMapMemory(dev, frameGrabImageMem, layout.offset, layout.size, 0,
										 reinterpret_cast<void**>(&p));
	if (err != VK_SUCCESS) {
		qWarning("QVKWindow: Failed to map readback image memory after transfer: %d", err);
		return;
	}

	for (int y = 0; y < frameGrabTargetImage.height(); ++y) {
		memcpy(frameGrabTargetImage.scanLine(y), p, frameGrabTargetImage.width() * 4);
		p += layout.rowPitch;
	}

	devFuncs->vkUnmapMemory(dev, frameGrabImageMem);

	devFuncs->vkDestroyImage(dev, frameGrabImage, nullptr);
	frameGrabImage = VK_NULL_HANDLE;
	devFuncs->vkFreeMemory(dev, frameGrabImageMem, nullptr);
	frameGrabImageMem = VK_NULL_HANDLE;
}

/*!
	Returns the active physical device.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::preInitResources() up until
	QVKWindowRenderer::releaseResources().
 */
vk::PhysicalDevice QVKWindow::physicalDevice() const
{
	Q_D(const QVKWindow);
	if (d->physDevIndex < d->physDevs.count())
		return d->physDevs[d->physDevIndex];
	qWarning("QVKWindow: Physical device not available");
	return VK_NULL_HANDLE;
}

/*!
	Returns a pointer to the properties for the active physical device.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::preInitResources() up until
	QVKWindowRenderer::releaseResources().
 */
const VkPhysicalDeviceProperties* QVKWindow::physicalDeviceProperties() const
{
	Q_D(const QVKWindow);
	if (d->physDevIndex < d->physDevProps.count())
		return &d->physDevProps[d->physDevIndex];
	qWarning("QVKWindow: Physical device properties not available");
	return nullptr;
}

/*!
	Returns the active logical device.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initResources() up until
	QVKWindowRenderer::releaseResources().
 */
vk::Device QVKWindow::device() const
{
	Q_D(const QVKWindow);
	return d->dev;
}

/*!
	Returns the active graphics queue.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initResources() up until
	QVKWindowRenderer::releaseResources().
 */
vk::Queue QVKWindow::graphicsQueue() const
{
	Q_D(const QVKWindow);
	return d->gfxQueue;
}

/*!
	Returns the family index of the active graphics queue.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initResources() up until
	QVKWindowRenderer::releaseResources(). Implementations of
	QVKWindowRenderer::updateQueueCreateInfo() can also call this
	function.

	\since 5.15
 */
uint32_t QVKWindow::graphicsQueueFamilyIndex() const
{
	Q_D(const QVKWindow);
	return d->gfxQueueFamilyIdx;
}

/*!
	Returns the active graphics command pool.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initResources() up until
	QVKWindowRenderer::releaseResources().
 */
vk::CommandPool QVKWindow::graphicsCommandPool() const
{
	Q_D(const QVKWindow);
	return d->cmdPool;
}

/*!
	Returns a host visible memory type index suitable for general use.

	The returned memory type will be both host visible and coherent. In
	addition, it will also be cached, if possible.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initResources() up until
	QVKWindowRenderer::releaseResources().
 */
uint32_t QVKWindow::hostVisibleMemoryIndex() const
{
	Q_D(const QVKWindow);
	return d->hostVisibleMemIndex;
}

/*!
	Returns a device local memory type index suitable for general use.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initResources() up until
	QVKWindowRenderer::releaseResources().

	\note It is not guaranteed that this memory type is always suitable. The
	correct, cross-implementation solution - especially for device local images
	- is to manually pick a memory type after checking the mask returned from
	\c{vkGetImageMemoryRequirements}.
 */
uint32_t QVKWindow::deviceLocalMemoryIndex() const
{
	Q_D(const QVKWindow);
	return d->deviceLocalMemIndex;
}

/*!
	Returns a typical render pass with one sub-pass.

	\note Applications are not required to use this render pass. However, they
	are then responsible for ensuring the current swap chain and depth-stencil
	images get transitioned from \c{VK_IMAGE_LAYOUT_UNDEFINED} to
	\c{VK_IMAGE_LAYOUT_PRESENT_SRC_KHR} and
	\c{VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL} either via the
	application's custom render pass or by other means.

	\note Stencil read/write is not enabled in this render pass.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initResources() up until
	QVKWindowRenderer::releaseResources().

	\sa currentFramebuffer()
 */
vk::RenderPass QVKWindow::defaultRenderPass() const
{
	Q_D(const QVKWindow);
	return d->defaultRenderPass;
}

/*!
	Returns the color buffer format used by the swapchain.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initResources() up until
	QVKWindowRenderer::releaseResources().

	\sa setPreferredColorFormats()
 */
vk::Format QVKWindow::colorFormat()
{
	Q_D(const QVKWindow);
	return 	vk::Format(d->colorFormat);
}

/*!
	Returns the format used by the depth-stencil buffer(s).

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initResources() up until
	QVKWindowRenderer::releaseResources().
 */
vk::Format QVKWindow::depthStencilFormat() const
{
	Q_D(const QVKWindow);
	return (vk::Format)d->dsFormat;
}

/*!
	Returns the image size of the swapchain.

	This usually matches the size of the window, but may also differ in case
	\c vkGetPhysicalDeviceSurfaceCapabilitiesKHR reports a fixed size.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initSwapChainResources() up until
	QVKWindowRenderer::releaseSwapChainResources().
 */
QSize QVKWindow::swapChainImageSize() const
{
	Q_D(const QVKWindow);
	return d->swapChainImageSize;
}

/*!
	Returns The active command buffer for the current swap chain image.
	Implementations of QVKWindowRenderer::startNextFrame() are expected to
	add commands to this command buffer.

	\note This function must only be called from within startNextFrame() and, in
	case of asynchronous command generation, up until the call to frameReady().
 */
vk::CommandBuffer QVKWindow::currentCommandBuffer() const
{
	Q_D(const QVKWindow);
	if (!d->framePending) {
		qWarning("QVKWindow: Attempted to call currentCommandBuffer() without an active frame");
		return VK_NULL_HANDLE;
	}
	return d->imageRes[d->currentImage].cmdBuf;
}

/*!
	Returns a VkFramebuffer for the current swapchain image using the default
	render pass.

	The framebuffer has two attachments (color, depth-stencil) when
	multisampling is not in use, and three (color resolve, depth-stencil,
	multisample color) when sampleCountFlagBits() is greater than
	\c{VK_SAMPLE_COUNT_1_BIT}. Renderers must take this into account, for
	example when providing clear values.

	\note Applications are not required to use this framebuffer in case they
	provide their own render pass instead of using the one returned from
	defaultRenderPass().

	\note This function must only be called from within startNextFrame() and, in
	case of asynchronous command generation, up until the call to frameReady().

	\sa defaultRenderPass()
 */
vk::Framebuffer QVKWindow::currentFramebuffer() const
{
	Q_D(const QVKWindow);
	if (!d->framePending) {
		qWarning("QVKWindow: Attempted to call currentFramebuffer() without an active frame");
		return VK_NULL_HANDLE;
	}
	return d->imageRes[d->currentImage].fb;
}

/*!
	Returns the current frame index in the range [0, concurrentFrameCount() - 1].

	Renderer implementations will have to ensure that uniform data and other
	dynamic resources exist in multiple copies, in order to prevent frame N
	altering the data used by the still-active frames N - 1, N - 2, ... N -
	concurrentFrameCount() + 1.

	To avoid relying on dynamic array sizes, applications can use
	MAX_CONCURRENT_FRAME_COUNT when declaring arrays. This is guaranteed to be
	always equal to or greater than the value returned from
	concurrentFrameCount(). Such arrays can then be indexed by the value
	returned from this function.

	\snippet code/src_gui_vulkan_QVKWindow.cpp 1

	\note This function must only be called from within startNextFrame() and, in
	case of asynchronous command generation, up until the call to frameReady().

	\sa concurrentFrameCount()
 */
int QVKWindow::currentFrame() const
{
	Q_D(const QVKWindow);
	if (!d->framePending)
		qWarning("QVKWindow: Attempted to call currentFrame() without an active frame");
	return d->currentFrame;
}

/*!
	\variable QVKWindow::MAX_CONCURRENT_FRAME_COUNT

	\brief A constant value that is always equal to or greater than the maximum value
	of concurrentFrameCount().
 */

 /*!
	 Returns the number of frames that can be potentially active at the same time.

	 \note The value is constant for the entire lifetime of the QVKWindow.

	 \snippet code/src_gui_vulkan_QVKWindow.cpp 2

	 \sa currentFrame()
  */
int QVKWindow::concurrentFrameCount() const
{
	Q_D(const QVKWindow);
	return d->frameLag;
}

int QVKWindow::clearValueCount() const
{
	return sampleCountFlagBits() == vk::SampleCountFlagBits::e1 ? 2 : 3;
}

/*!
	Returns the number of images in the swap chain.

	\note Accessing this is necessary when providing a custom render pass and
	framebuffer. The framebuffer is specific to the current swapchain image and
	hence the application must provide multiple framebuffers.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initSwapChainResources() up until
	QVKWindowRenderer::releaseSwapChainResources().
 */
int QVKWindow::swapChainImageCount() const
{
	Q_D(const QVKWindow);
	return d->swapChainBufferCount;
}

/*!
	Returns the current swap chain image index in the range [0, swapChainImageCount() - 1].

	\note This function must only be called from within startNextFrame() and, in
	case of asynchronous command generation, up until the call to frameReady().
 */
int QVKWindow::currentSwapChainImageIndex() const
{
	Q_D(const QVKWindow);
	if (!d->framePending)
		qWarning("QVKWindow: Attempted to call currentSwapChainImageIndex() without an active "
				 "frame");
	return d->currentImage;
}

/*!
	Returns the specified swap chain image.

	\a idx must be in the range [0, swapChainImageCount() - 1].

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initSwapChainResources() up until
	QVKWindowRenderer::releaseSwapChainResources().
 */
vk::Image QVKWindow::swapChainImage(int idx) const
{
	Q_D(const QVKWindow);
	return idx >= 0 && idx < d->swapChainBufferCount ? d->imageRes[idx].image : VK_NULL_HANDLE;
}

/*!
	Returns the specified swap chain image view.

	\a idx must be in the range [0, swapChainImageCount() - 1].

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initSwapChainResources() up until
	QVKWindowRenderer::releaseSwapChainResources().
 */
vk::ImageView QVKWindow::swapChainImageView(int idx) const
{
	Q_D(const QVKWindow);
	return idx >= 0 && idx < d->swapChainBufferCount ? d->imageRes[idx].imageView : VK_NULL_HANDLE;
}

/*!
	Returns the depth-stencil image.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initSwapChainResources() up until
	QVKWindowRenderer::releaseSwapChainResources().
 */
vk::Image QVKWindow::depthStencilImage() const
{
	Q_D(const QVKWindow);
	return d->dsImage;
}

/*!
	Returns the depth-stencil image view.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initSwapChainResources() up until
	QVKWindowRenderer::releaseSwapChainResources().
 */
vk::ImageView QVKWindow::depthStencilImageView() const
{
	Q_D(const QVKWindow);
	return d->dsView;
}

/*!
	Returns the current sample count as a \c VkSampleCountFlagBits value.

	When targeting the default render target, the \c rasterizationSamples field
	of \c VkPipelineMultisampleStateCreateInfo must be set to this value.

	\sa setSampleCount(), supportedSampleCounts()
 */
vk::SampleCountFlagBits QVKWindow::sampleCountFlagBits() const
{
	Q_D(const QVKWindow);
	return (vk::SampleCountFlagBits)d->sampleCount;
}

/*!
	Returns the specified multisample color image, or \c{VK_NULL_HANDLE} if
	multisampling is not in use.

	\a idx must be in the range [0, swapChainImageCount() - 1].

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initSwapChainResources() up until
	QVKWindowRenderer::releaseSwapChainResources().
 */
vk::Image QVKWindow::msaaColorImage(int idx) const
{
	Q_D(const QVKWindow);
	return idx >= 0 && idx < d->swapChainBufferCount ? d->imageRes[idx].msaaImage : VK_NULL_HANDLE;
}

/*!
	Returns the specified multisample color image view, or \c{VK_NULL_HANDLE} if
	multisampling is not in use.

	\a idx must be in the range [0, swapChainImageCount() - 1].

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initSwapChainResources() up until
	QVKWindowRenderer::releaseSwapChainResources().
 */
vk::ImageView QVKWindow::msaaColorImageView(int idx) const
{
	Q_D(const QVKWindow);
	return idx >= 0 && idx < d->swapChainBufferCount ? d->imageRes[idx].msaaImageView
		: VK_NULL_HANDLE;
}

vk::PipelineCache QVKWindow::pipelineCache()
{
	Q_D(const QVKWindow);
	return d->pipelineCache;
}

/*!
	Returns true if the swapchain supports usage as transfer source, meaning
	grab() is functional.

	\note Calling this function is only valid from the invocation of
	QVKWindowRenderer::initSwapChainResources() up until
	QVKWindowRenderer::releaseSwapChainResources().
 */
bool QVKWindow::supportsGrab() const
{
	Q_D(const QVKWindow);
	return d->swapChainSupportsReadBack;
}

/*!
  \fn void QVKWindow::frameGrabbed(const QImage &image)

  This signal is emitted when the \a image is ready.
*/

/*!
	Builds and renders the next frame without presenting it, then performs a
	blocking readback of the image content.

	Returns the image if the renderer's
	\l{QVKWindowRenderer::startNextFrame()}{startNextFrame()}
	implementation calls back frameReady() directly. Otherwise, returns an
	incomplete image, that has the correct size but not the content yet. The
	content will be delivered via the frameGrabbed() signal in the latter case.

	\note This function should not be called when a frame is in progress
	(that is, frameReady() has not yet been called back by the application).

	\note This function is potentially expensive due to the additional,
	blocking readback.

	\note This function currently requires that the swapchain supports usage as
	a transfer source (\c{VK_IMAGE_USAGE_TRANSFER_SRC_BIT}), and will fail otherwise.
 */
QImage QVKWindow::grab()
{
	Q_D(QVKWindow);
	if (!d->swapChain) {
		qWarning("QVKWindow: Attempted to call grab() without a swapchain");
		return QImage();
	}
	if (d->framePending) {
		qWarning("QVKWindow: Attempted to call grab() while a frame is still pending");
		return QImage();
	}
	if (!d->swapChainSupportsReadBack) {
		qWarning("QVKWindow: Attempted to call grab() with a swapchain that does not support "
				 "usage as transfer source");
		return QImage();
	}

	d->frameGrabbing = true;
	d->beginFrame();

	return d->frameGrabTargetImage;
}

/*!
   Returns a QMatrix4x4 that can be used to correct for coordinate
   system differences between OpenGL and Vulkan.

   By pre-multiplying the projection matrix with this matrix, applications can
   continue to assume that Y is pointing upwards, and can set minDepth and
   maxDepth in the viewport to 0 and 1, respectively, without having to do any
   further corrections to the vertex Z positions. Geometry from OpenGL
   applications can then be used as-is, assuming a rasterization state matching
   the OpenGL culling and front face settings.
 */
QMatrix4x4 QVKWindow::clipCorrectionMatrix()
{
	Q_D(QVKWindow);
	if (d->m_clipCorrect.isIdentity()) {
		// NB the ctor takes row-major
		d->m_clipCorrect = QMatrix4x4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
									  0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f);
	}
	return d->m_clipCorrect;
}

vk::ShaderModule QVKWindow::createShaderFromCode(EShLanguage shaderType, const char* code)
{
	glslang::InitializeProcess();
	std::vector<uint32_t> spvShader;
	glslang::TShader shader(shaderType);
	shader.setStrings(&code, 1);
	EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
	shader.parse(&glslang::DefaultTBuiltInResource, 100, false, messages);
	qDebug() << shader.getInfoLog();
	glslang::SpvOptions option;
	spv::SpvBuildLogger logger;
	glslang::GlslangToSpv(*shader.getIntermediate(), spvShader, &logger, &option);
	glslang::FinalizeProcess();

	vk::ShaderModuleCreateInfo shaderInfo;
	shaderInfo.codeSize = sizeof(uint32_t) * spvShader.size();
	shaderInfo.pCode = spvShader.data();
	return device().createShaderModule(shaderInfo);
}

vk::ShaderModule QVKWindow::createShaderFromSpirv(QString path)
{
	QFile file(path);
	if (!file.open(QFile::ReadOnly)) {
		return VK_NULL_HANDLE;
	}
	QByteArray data = file.readAll();
	vk::ShaderModuleCreateInfo shaderInfo;
	shaderInfo.codeSize = data.size();
	shaderInfo.pCode = (uint32_t*)data.data();
	return device().createShaderModule(shaderInfo);
}

bool QVKRenderer::isVkTime() const
{
	return window_ != nullptr && window_->device();
}