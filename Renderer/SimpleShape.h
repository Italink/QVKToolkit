#ifndef SimpleShape_h__
#define SimpleShape_h__

#include "Core\QVKPrimitive.h"

class SimpleShape :public QVKPrimitive {
public:
	struct Vertex {
		QVector3D position;
		QVector4D color;
	};
	void submit(const QVector<Vertex>& vertices, const QVector<unsigned int>& indices = {});
	void update();
protected:
	virtual const char* vertexShaderCode();
	virtual const char* fragmentShaderCode();
	virtual void initResources() override;
	virtual void releaseResources() override;
	virtual void startNextFrame(FrameContext ctx) override;
private:
	QVector<Vertex> mVertices;
	QVector<unsigned int> mIndices;
	bool needToUpdate = false;
	vk::Buffer singleBuffer_;
	vk::DescriptorBufferInfo vertexBufferInfo_;
	vk::DescriptorBufferInfo indexBufferInfo_;
	vk::DeviceMemory singleDevMemory_;
	vk::PipelineLayout piplineLayout_;
	vk::Pipeline pipline_;
};

#endif // SimpleShape_h__
