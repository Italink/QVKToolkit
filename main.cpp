#include <QApplication>
#include <QLoggingCategory>
#include <QVulkanInstance>
#include "Core/QVkWindow.h"
#include "Effect/Glow.h"
#include "Core/QVKTransparentWidget.h"
#include "Renderer/SkyBox.h"
#include "Renderer/SimpleShape.h"
#include "Core/QVKScene.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/ShaderToy.h"

Q_LOGGING_CATEGORY(lcVk, "qt.vulkan")

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);

	static vk::DynamicLoader  dynamicLoader;
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	QVulkanInstance instance;
	instance.setLayers({ "VK_LAYER_KHRONOS_validation" });
	if (!instance.create())
		qFatal("Failed to create Vulkan instance: %d", instance.errorCode());
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.vkInstance());
	QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

	QVKTransparentWidget vkWindow(&instance);
	vkWindow.resize(1024, 768);
	vkWindow.show();

	//auto shape = QSharedPointer<SimpleShape>::create();

	//QVector<SimpleShape::Vertex> vertices(3);
	//vertices[0].position = QVector3D(-0.5, 0, 0);
	//vertices[1].position = QVector3D(0.5, 0, 0);
	//vertices[2].position = QVector3D(0, 0.5, 0);
	//vertices[0].color = vertices[1].color = vertices[2].color = QVector4D(0.3, 0.5, 0.9, 1.0);
	//shape->submit(vertices);

	////auto staticMesh = QSharedPointer<StaticMesh>::create();
	////staticMesh->loadMeshFromFile("F:/HelloVulkan/02-Advance/Ex07-AssimpLoadStaticMesh/Genji/Genji.FBX");
	////vkWindow.addRenderer(staticMesh);

	//auto scene = QSharedPointer<QVKScene>::create();
	////vkWindow.addRenderer(QSharedPointer<SkyBox>::create());
	//vkWindow.addRenderer(scene);

	//vkWindow.addRenderer(shape);
	vkWindow.addRenderer(QSharedPointer<ShaderToy>::create());

	//auto glowEffect = QSharedPointer<Glow>::create();
	//vkWindow.addRenderer(glowEffect);

	return app.exec();
}