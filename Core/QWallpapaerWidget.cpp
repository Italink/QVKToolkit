#include <QtGui/qpa/qplatformintegration.h>
#include <QtOpenGL/QOpenGLPaintDevice>
#include <QtGui/private/qguiapplication_p.h>
#include <QtOpenGL/private/qopenglpaintdevice_p.h>
#include <QtOpenGL/qpa/qplatformbackingstoreopenglsupport.h>
#include <QtWidgets/private/qwidget_p.h>
#include "private/qwindow_p.h"
#include "QOpenGLTextureBlitter"
#include "qpa/qplatformgraphicsbuffer.h"
#include "qpa/qplatformgraphicsbufferhelper.h"
#include "QOffscreenSurface"
#include "QWallpapaerWidget.h"
#include "QtGui/private/qopenglcontext_p.h"

QT_BEGIN_NAMESPACE

class QWallparperWidgetPaintDevicePrivate : public QOpenGLPaintDevicePrivate
{
public:
	explicit QWallparperWidgetPaintDevicePrivate(QWallparperWidget* widget)
		: QOpenGLPaintDevicePrivate(QSize()),
		w(widget) { }
	QWallparperWidget* w;
};

class QOpenGLWidgetPaintDevice : public QOpenGLPaintDevice
{
public:
	explicit QOpenGLWidgetPaintDevice(QWallparperWidget* widget)
		: QOpenGLPaintDevice(*new QWallparperWidgetPaintDevicePrivate(widget)) { }
};

class QPlatformBackingStoreWallpaperSupport : public QPlatformBackingStoreOpenGLSupportBase
{
private:
	QScopedPointer<QOpenGLContext> context;
	QOpenGLTextureBlitter* blitter = nullptr;
public:
	~QPlatformBackingStoreWallpaperSupport() override {}
	void composeAndFlush(QWindow* window, const QRegion& region, const QPoint& offset,
		QPlatformTextureList* textures, bool translucentBackground) override {
		if (!qt_window_private(window)->receivedExpose)
			return;
		if (!context) {
			context.reset(new QOpenGLContext);
			context->setFormat(window->requestedFormat());
			context->setScreen(window->screen());
			context->setShareContext(qt_window_private(window)->shareContext());
			if (!context->create()) {
				qCWarning(lcQpaBackingStore, "composeAndFlush: QOpenGLContext creation failed");
				return;
			}
		}
		context->makeCurrent(window);
	}
	GLuint toTexture(const QRegion& dirtyRegion, QSize* textureSize, QPlatformBackingStore::TextureFlags* flags) const override {
		return 0;
	}
};

void qt_registerDefaultPlatformBackingStoreWallpaperSupport()
{
	if (!QPlatformBackingStoreOpenGLSupportBase::factoryFunction()) {
		QPlatformBackingStoreOpenGLSupportBase::setFactoryFunction([]() -> QPlatformBackingStoreOpenGLSupportBase* {
			return new QPlatformBackingStoreWallpaperSupport;
		});
	}
}

Q_CONSTRUCTOR_FUNCTION(qt_registerDefaultPlatformBackingStoreWallpaperSupport);

class QWallparperWidgetPrivate : public QWidgetPrivate
{
	Q_DECLARE_PUBLIC(QWallparperWidget)
public:
	QWallparperWidgetPrivate() = default;

	void reset();
	void resizeDevice();
	void initialize();

	QOpenGLPaintDevice* paintDevice = nullptr;
	bool initialized = false;
};

void QWallparperWidgetPrivate::reset()
{
	delete paintDevice;
	paintDevice = nullptr;
	initialized = false;
}

void QWallparperWidgetPrivate::resizeDevice()
{
	Q_Q(QWallparperWidget);
	const QSize deviceSize = q->size() * q->devicePixelRatio();
	paintDevice->setSize(deviceSize);
	paintDevice->setDevicePixelRatio(q->devicePixelRatio());
}

void QWallparperWidgetPrivate::initialize()
{
	Q_Q(QWallparperWidget);
	if (initialized)
		return;
	QWidget* tlw = q->window();
	QOpenGLContext* shareContext = qt_gl_global_share_context();
	if (!shareContext)
		shareContext = get(tlw)->shareContext();

	paintDevice = new QOpenGLWidgetPaintDevice(q);
	paintDevice->setSize(q->size() * q->devicePixelRatio());
	paintDevice->setDevicePixelRatio(q->devicePixelRatio());
}

QWallparperWidget::QWallparperWidget(QWidget* parent, Qt::WindowFlags f)
	: QWidget(*(new QWallparperWidgetPrivate), parent, f)
{
	Q_D(QWallparperWidget);
	if (Q_UNLIKELY(!QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::RasterGLSurface)))
		qWarning("QWallparperWidget is not supported on this platform.");
	else
		d->setRenderToTexture();
}

void QWallparperWidget::setWindowLevel(WindowLevel level, bool toolWindow /*= false*/)
{
	if (windowLevel_ == level)
		return;
	close();

	Qt::WindowType toolWindowType = toolWindow ? Qt::Tool : Qt::Widget;

	switch (level) {
	case WindowLevel::Top: {
		setAttribute(Qt::WA_TransparentForMouseEvents, true);
		LONG l = GetWindowLongA((HWND)winId(), GWL_EXSTYLE) | WS_EX_LAYERED;
		SetWindowLongA((HWND)winId(), GWL_EXSTYLE, l);
		SetParent((HWND)winId(), NULL);
		SetWindowLongA((HWND)winId(), GWL_STYLE, (WS_POPUP | WS_VISIBLE));
		SetWindowLongA((HWND)winId(), GWL_EXSTYLE, 0);
		setWindowFlags(Qt::Window | toolWindowType | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
		break;
	}
	case WindowLevel::Normal: {
		setAttribute(Qt::WA_TransparentForMouseEvents, true);
		LONG l = GetWindowLongA((HWND)winId(), GWL_EXSTYLE) | WS_EX_LAYERED;
		SetWindowLongA((HWND)winId(), GWL_EXSTYLE, l);
		SetParent((HWND)winId(), NULL);
		SetWindowLongA((HWND)winId(), GWL_STYLE, (WS_POPUP | WS_VISIBLE));
		SetWindowLongA((HWND)winId(), GWL_EXSTYLE, 0);
		setWindowFlags(Qt::Window | toolWindowType | Qt::FramelessWindowHint);
		break;
	}
	case WindowLevel::Bottom: {
		setAttribute(Qt::WA_TransparentForMouseEvents, true);
		LONG l = GetWindowLongA((HWND)winId(), GWL_EXSTYLE) | WS_EX_LAYERED;
		SetWindowLongA((HWND)winId(), GWL_EXSTYLE, l);
		SetParent((HWND)winId(), NULL);
		SetWindowLongA((HWND)winId(), GWL_STYLE, (WS_POPUP | WS_VISIBLE));
		SetWindowLongA((HWND)winId(), GWL_EXSTYLE, 0);
		setWindowFlags(Qt::Window | toolWindowType | Qt::FramelessWindowHint | Qt::WindowStaysOnBottomHint);
		break;
	}
	case WindowLevel::Wallpaper: {
		setAttribute(Qt::WA_TransparentForMouseEvents, false);
		HWND Parent = getDesktopHWND();
		SetParent((HWND)winId(), Parent);
		SetWindowLongA((HWND)winId(), GWL_STYLE, (WS_POPUP | WS_VISIBLE));
		SetWindowLongA((HWND)winId(), GWL_EXSTYLE, (WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOPMOST));
		setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
		break;
	}
	default:
		break;
	}
	show();
	if (windowLevel_ == Wallpaper) {
		HWND desktop = getDesktopHWND();
		ShowWindow(desktop, SW_HIDE);
		RedrawWindow(desktop, nullptr, nullptr, RDW_ERASE | RDW_INTERNALPAINT | RDW_ERASENOW | RDW_UPDATENOW | RDW_ALLCHILDREN);
		ShowWindow(desktop, SW_SHOW);
	}
	windowLevel_ = level;
}

QWallparperWidget::~QWallparperWidget()
{
	Q_D(QWallparperWidget);
	d->reset();
}

HWND QWallparperWidget::getDesktopHWND()
{
	HWND hWnd = ::FindWindow(L"Progman", L"Program Manager");
	SendMessage(hWnd, 0x052c, 0, 0);;
	HWND hwndWorkW = NULL;
	while (true) {
		hwndWorkW = ::FindWindowEx(NULL, hwndWorkW, L"WorkerW", NULL);
		if (NULL == hwndWorkW)
			continue;
		HWND hView = ::FindWindowEx(hwndWorkW, NULL, L"SHELLDLL_DefView", NULL);
		if (NULL == hView)
			continue;
		hwndWorkW = ::FindWindowEx(NULL, hwndWorkW, L"WorkerW", NULL);
		break;
	};
	return  hwndWorkW;
}

void QWallparperWidget::resizeEvent(QResizeEvent* e)
{
	Q_D(QWallparperWidget);

	if (e->size().isEmpty()) {
		return;
	}
	d->initialize();
	if (!d->initialized)
		return;
	d->resizeDevice();
	d->sendPaintEvent(QRect(QPoint(0, 0), size()));
}

void QWallparperWidget::closeEvent(QCloseEvent* event)
{
	if (windowLevel_ == Wallpaper) {
		HWND desktop = getDesktopHWND();
		ShowWindow(desktop, SW_HIDE);
		RedrawWindow(desktop, nullptr, nullptr, RDW_ERASE | RDW_INTERNALPAINT | RDW_ERASENOW | RDW_UPDATENOW | RDW_ALLCHILDREN);
		ShowWindow(desktop, SW_SHOW);
	}
}

QPaintEngine* QWallparperWidget::paintEngine() const
{
	Q_D(const QWallparperWidget);
	if (!d->initialized)
		return nullptr;
	return d->paintDevice->paintEngine();
}

bool QWallparperWidget::event(QEvent* e)
{
	Q_D(QWallparperWidget);
	switch (e->type()) {
	case QEvent::WindowChangeInternal:
		if (d->initialized)
			d->reset();
		if (isHidden())
			break;
		Q_FALLTHROUGH();
	case QEvent::Show:
		if (!d->initialized && !size().isEmpty() && window()->windowHandle()) {
			d->initialize();
			if (d->initialized) {
				d->resizeDevice();
				d->sendPaintEvent(QRect(QPoint(0, 0), size()));
			}
		}
		break;
	case QEvent::ScreenChangeInternal:
		if (d->initialized && d->paintDevice->devicePixelRatio() != devicePixelRatio())
			d->resizeDevice();
		break;
	case  QEvent::Close:
		break;
	default:
		break;
	}
	return QWidget::event(e);
}

QT_END_NAMESPACE