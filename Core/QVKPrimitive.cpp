#include "QVKPrimitive.h"
#include "assimp\matrix4x4.h"
#include "assimp\vector3.h"

QMatrix4x4 QVKPrimitive::calculateMVP()
{
	QMatrix4x4 MVP = window_->camera_.getMatrixVP() * calculateMatrixModel();

	//QVector<QVector3D> vertices(3);
	//vertices[0] = QVector3D(-0.5, 0, 0);
	//vertices[1] = QVector3D(0.5, 0, 0);
	//vertices[2] = QVector3D(0, 0.5, 0);

	//QSize swapChainImgaeSize = window_->swapChainImageSize();
	//vertices[0] = vertices[0].project(modelMatrix, window_->camera_.getMatrix(), QRect(0, 0, window_->width(), window_->height()));
	//vertices[1] = vertices[1].project(modelMatrix, window_->camera_.getMatrix(), QRect(0, 0, window_->width(), window_->height()));
	//vertices[2] = vertices[2].project(modelMatrix, window_->camera_.getMatrix(), QRect(0, 0, window_->width(), window_->height()));

	return MVP;
}

QRect QVKPrimitive::calculateOverlayArea()
{
	QSize swapChainImgaeSize = window_->swapChainImageSize();

	QMatrix4x4 mvp = calculateMVP();

	QVector3D points[8] = {
		QVector3D(mAABB.min.x(),mAABB.min.y(),mAABB.min.z()),
		QVector3D(mAABB.min.x(),mAABB.min.y(),mAABB.max.z()),
		QVector3D(mAABB.min.x(),mAABB.max.y(),mAABB.min.z()),
		QVector3D(mAABB.min.x(),mAABB.max.y(),mAABB.max.z()),
		QVector3D(mAABB.max.x(),mAABB.min.y(),mAABB.min.z()),
		QVector3D(mAABB.max.x(),mAABB.min.y(),mAABB.max.z()),
		QVector3D(mAABB.max.x(),mAABB.max.y(),mAABB.min.z()),
		QVector3D(mAABB.max.x(),mAABB.max.y(),mAABB.max.z()),
	};

	for (int i = 0; i < 8; i++) {
		QVector4D tmp(points[i], 1.0f);
		tmp = mvp * tmp;
		if (qFuzzyIsNull(tmp.w()))
			tmp.setW(1.0f);
		if (tmp.w() < 0) {
			tmp.setW(qAbs(tmp.w()));
		}
		tmp /= tmp.w();
		tmp = tmp * 0.5f + QVector4D(0.5f, 0.5f, 0.5f, 0.5f);
		tmp.setX(tmp.x() * swapChainImgaeSize.width());
		tmp.setY(tmp.y() * swapChainImgaeSize.height());
		points[i] = tmp.toVector3D();
	}

	QPointF minCoord = points[0].toPointF();
	QPointF maxCoord = points[0].toPointF();

	for (int i = 1; i < 8; i++) {
		minCoord.setX(qMin(points[i].x(), minCoord.x()));
		minCoord.setY(qMin(points[i].y(), minCoord.y()));
		maxCoord.setX(qMax(points[i].x(), maxCoord.x()));
		maxCoord.setY(qMax(points[i].y(), maxCoord.y()));
	}

	QRect rect;
	rect.setBottomLeft({ (int)minCoord.x(),(int)maxCoord.y() });
	rect.setTopRight({ (int)maxCoord.x(),(int)minCoord.y() });
	return rect & QRect(0, 0, swapChainImgaeSize.width(), swapChainImgaeSize.height());
}

QMatrix4x4 QVKPrimitive::calculateMatrixModel()
{
	QMatrix4x4 modelMatrix;
	modelMatrix.translate(mPosition);
	modelMatrix.rotate(mRotation.x(), 1, 0, 0);
	modelMatrix.rotate(mRotation.y(), 0, 1, 0);
	modelMatrix.rotate(mRotation.z(), 0, 0, 1);
	modelMatrix.scale(mScaling);
	return modelMatrix;
}