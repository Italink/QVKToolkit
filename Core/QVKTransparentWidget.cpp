#include "QVKTransparentWidget.h"
#include <QHBoxLayout>
#include "qpa\qplatformbackingstore.h"
#include "QOpenGLTextureBlitter"
#include <QtGui/qpa/qplatformintegration.h>
#include <QtOpenGL/QOpenGLPaintDevice>
#include <QtGui/private/qguiapplication_p.h>
#include <QtOpenGL/private/qopenglpaintdevice_p.h>
#include <QtOpenGL/qpa/qplatformbackingstoreopenglsupport.h>
#include <QtWidgets/private/qwidget_p.h>

QVKTransparentWidget::QVKTransparentWidget(QVulkanInstance* instance)
{
	window.setVulkanInstance(instance);
	window.setSampleCount(8);
	QHBoxLayout* h = new QHBoxLayout(this);
	h->setContentsMargins(0, 0, 0, 0);
	h->addWidget(QWidget::createWindowContainer(&window));
	//setWindowLevel(QWallparperWidget::Wallpaper);
	//setWindowFlag(Qt::FramelessWindowHint);
	//setAttribute(Qt::WA_TranslucentBackground);
}

void QVKTransparentWidget::addRenderer(QSharedPointer<QVKRenderer> renderer)
{
	window.addRenderer(renderer);
}

void QVKTransparentWidget::removeRenderer(QSharedPointer<QVKRenderer> renderer)
{
	window.removeRenderer(renderer);
}