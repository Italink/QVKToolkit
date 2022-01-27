#include <QGuiApplication>
#include <QLoggingCategory>
#include <QVulkanInstance>
#include "Core/QVkWindow.h"
#include "Core/QVKPrimitive.h"
#include "Core/QVKSimpleShape.h"
#include "Effect/Glow.h"

Q_LOGGING_CATEGORY(lcVk, "qt.vulkan")

int main(int argc, char* argv[]) {
	QGuiApplication app(argc, argv);

	static vk::DynamicLoader  dynamicLoader;
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	QVulkanInstance instance;
	instance.setLayers({ "VK_LAYER_KHRONOS_validation" });
	if (!instance.create())
		qFatal("Failed to create Vulkan instance: %d", instance.errorCode());
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.vkInstance());
	QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

	QVKWindow vkWindow;
	vkWindow.setVulkanInstance(&instance);
	vkWindow.resize(1024, 768);
	vkWindow.show();

	auto shape = QSharedPointer<QVKSimpleShape>::create();
	QVector<QVKSimpleShape::Vertex> vertices(3);
	vertices[0].position = QVector3D(-0.5, 0, 0);
	vertices[1].position = QVector3D(0.5, 0, 0);
	vertices[2].position = QVector3D(0, 0.5, 0);
	vertices[0].color = vertices[1].color = vertices[2].color = QVector4D(0.3, 0.5, 0.9, 1.0);
	shape->submit(vertices);
	vkWindow.addRenderer(shape);

	shape = QSharedPointer<QVKSimpleShape>::create();
	vertices[0].position = QVector3D(-0.5, 0, 0);
	vertices[1].position = QVector3D(0.5, 0, 0);
	vertices[2].position = QVector3D(0, -0.5, 0);
	vertices[0].color = vertices[1].color = vertices[2].color = QVector4D(0.3, 0.5, 0.9, 1.0);
	shape->submit(vertices);
	vkWindow.addRenderer(shape);

	auto glowEffect = QSharedPointer<Glow>::create();
	vkWindow.addRenderer(glowEffect);

	return app.exec();
}