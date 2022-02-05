#ifndef QVKTransparentWidget_h__
#define QVKTransparentWidget_h__

#include "QWidget"
#include "QVKWindow.h"
#include "QWallpapaerWidget.h"

class QVKTransparentWidget :public QWallparperWidget {
public:
	QVKTransparentWidget(QVulkanInstance* instance);
	void addRenderer(QSharedPointer<QVKRenderer> renderer);
	void removeRenderer(QSharedPointer<QVKRenderer> renderer);
private:
	QVKWindow window;
};

#endif // QVKTransparentWidget_h__
