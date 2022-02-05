#ifndef QVKPrimitive_h__
#define QVKPrimitive_h__

#include "QVKWindow.h"

class QVKPrimitive :public QVKRenderer {
public:
	struct AABB {
		QVector3D min = QVector3D(FLT_MAX, FLT_MAX, FLT_MAX);
		QVector3D max = QVector3D(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	};
public:
	QMatrix4x4 calculateMVP();
	QRect calculateOverlayArea();
	QMatrix4x4 calculateMatrixModel();
	QVector3D getPosition() const { return mPosition; }
	void setPosition(QVector3D val) { mPosition = val; }
	QVector3D getRotation() const { return mRotation; }
	void setRotation(QVector3D val) { mRotation = val; }
	QVector3D getScaling() const { return mScaling; }
	void setScaling(QVector3D val) { mScaling = val; }
	QVKPrimitive::AABB getAABB() const { return mAABB; }
	void setAABB(QVKPrimitive::AABB val) { mAABB = val; }
private:
	QVector3D mPosition = QVector3D(0.0f, 0.0f, 0.0f);
	QVector3D mRotation = QVector3D(0.0f, 0.0f, 0.0f);
	QVector3D mScaling = QVector3D(1.0f, 1.0f, 1.0f);
	AABB mAABB;
};

#endif // QVKPrimitive_h__
