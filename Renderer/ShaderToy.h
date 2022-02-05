#ifndef ShaderToy_h__
#define ShaderToy_h__

#include "Core\QVKWindow.h"

class ShaderToy :public QVKRenderer {
public:
	ShaderToy();
	void initResources() override;
	void releaseResources() override;
	void startNextFrame(FrameContext frameCtx) override;
	void setCode(QString code);
protected:
	vk::PipelineLayout piplineLayout_;
	vk::Pipeline pipline_;
	QByteArray code_;

	struct ShaderParams {
		QVector2D mouse;
		QVector2D resolution;
		float time;
	};
};

#endif // ShaderToy_h__
