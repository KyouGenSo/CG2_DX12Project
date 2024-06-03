#define _USE_MATH_DEFINES
#include <math.h>


#include"Easing.h"

float Lerp(float EasingTime, float start, float end) {

	return (1 - EasingTime) * start + EasingTime * end;
}

float EaseInSine(float EasingTime, float start, float end) {
	float result;

	result = 1 - cosf((EasingTime * float(M_PI)) / 2);

	return (1 - result) * start + result * end;
}

float easeInQuart(float EasingTime, float start, float end) {
	float result;

	result = EasingTime * EasingTime * EasingTime * EasingTime;

	return (1 - result) * start + result * end;

}

float easeInOutBack(float EasingTime, float start, float end) {
	float result;

	const float c1 = 1.70158f;
	const float c2 = c1 * 1.525f;

	if (EasingTime < 0.5f) {

		result = (powf(2 * EasingTime, 2) * ((c2 + 1) * 2 * EasingTime - c2)) / 2;

	} else {

		result = (powf(2 * EasingTime - 2, 2) * ((c2 + 1) * (EasingTime * 2 - 2) + c2) + 2) / 2;
	}

	return (1 - result) * start + result * end;

}

float easeInBack(float EasingTime, float start, float end) {
	float result;

	float c1 = 1.70158f;
	float c3 = c1 + 1;

	result = c3 * EasingTime * EasingTime * EasingTime - c1 * EasingTime * EasingTime;

	return (1 - result) * start + result * end;
}

float easeOutBack(float EasingTime, float start, float end) {
	float result;

	float c1 = 1.70158f;
	float c3 = c1 + 1;

	result = 1 + c3 * powf(EasingTime - 1, 3) + c1 * powf(EasingTime - 1, 2);

	return (1 - result) * start + result * end;

}

float easeOutCirc(float EasingTime, float start, float end) {
	float result;

	result = sqrtf(1 - powf(EasingTime - 1, 2));

	return (1 - result) * start + result * end;
}

float easeOutSine(float EasingTime, float start, float end) {
	float result;

	result = sinf((EasingTime * float(M_PI)) / 2);

	return (1 - result) * start + result * end;
}

float easeOutElastic(float EasingTime, float start, float end) {
	float result;

	const float c4 = (2 * float(M_PI)) / 3;

	if (EasingTime == 0) {
		result = 0;
	}
	else if (EasingTime == 1) {
		result = 1;
	}
	else {
		result = powf(2, -10 * EasingTime) * sinf((EasingTime * 10 - 0.75f) * c4) + 1;
	}

	return (1 - result) * start + result * end;
}

float easeOutCubic(float EasingTime, float start, float end) {
	float result;
	result = 1 - powf(1 - EasingTime, 3);
	return (1 - result) * start + result * end;
}

float easeOutExpo(float EasingTime, float start, float end) {
	float result;
	if (EasingTime == 1.0f) {
		result = 1.0f;
	}
	else {
		result = 1.0f - powf(2.0f, -10.0f * EasingTime);
	}
	return (1 - result) * start + result * end;
}