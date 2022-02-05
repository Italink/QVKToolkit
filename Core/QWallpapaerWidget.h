#ifndef QWallpapaerWidget_h__
#define QWallpapaerWidget_h__

#include <QWidget>

class QWallparperWidgetPrivate;

class QWallparperWidget : public QWidget
{
	Q_OBJECT;
	Q_DECLARE_PRIVATE(QWallparperWidget);
public:
	explicit QWallparperWidget(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~QWallparperWidget();

	enum WindowLevel {
		Top,
		Normal,
		Bottom,
		Wallpaper
	}windowLevel_;
	Q_ENUM(WindowLevel);
	void setWindowLevel(WindowLevel level, bool toolWindow = false);
protected:
protected:
	HWND getDesktopHWND();
protected:
	void resizeEvent(QResizeEvent* e) override;
	void closeEvent(QCloseEvent* event) override;
	bool event(QEvent* e) override;
	QPaintEngine* paintEngine() const override;
private:
	Q_DISABLE_COPY(QWallparperWidget)
};

#endif // QWallpapaerWidget_h__
