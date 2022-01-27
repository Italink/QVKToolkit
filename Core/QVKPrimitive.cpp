#include "QVKPrimitive.h"

QMatrix4x4 QVKPrimitive::calculateMVP()
{
	QMatrix4x4 matrix;
	matrix.translate(mPosition);
	matrix.rotate(mRotation.x(), 1, 0, 0);
	matrix.rotate(mRotation.y(), 0, 1, 0);
	matrix.rotate(mRotation.z(), 0, 0, 1);
	matrix.scale(mScaling);
	return matrix;
}