#pragma once
#include <cmath>
#include "Vector3.h"

Vector3 operator+(const Vector3& a, const Vector3& b) {
	return Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vector3 operator-(const Vector3& a, const Vector3& b) {
	return Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vector3 operator*(const Vector3& a, float s) {
	return Vector3(a.x * s, a.y * s, a.z * s);
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
	return a + ( b - a ) * t;
}