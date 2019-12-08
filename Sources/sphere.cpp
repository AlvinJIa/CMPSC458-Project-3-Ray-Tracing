#include "sphere.h"

const float PI = 3.1415926;
//constructor given  center, radius, and material
sphere::sphere(glm::vec3 c, float r, int m, scene* s) : rtObject(s)
{
	center = c;
	radius = r;
	matIndex = m;
	myScene = s;
}

float sphere::testIntersection(glm::vec3 eye, glm::vec3 dir)
{
	//see the book for a description of how to use the quadratic rule to solve
	//for the intersection(s) of a line and a sphere, implement it here and
	//return the minimum positive distance or 9999999 if none

	glm::vec3 eTop = eye - center;

	float a = glm::dot(dir, dir);
	float b = 2 * glm::dot(dir, eTop);
	float c = glm::dot(eTop, eTop) - radius;
	float del = b * b - 4 * a * c;
	if (del < 0) return 9999999;
	float t0 = (-b - sqrt(del)) / (2*a);
	float t1 = (-b + sqrt(del)) / (2*a);

	float t;
	if (t0 <= 0.001f || t0 >= 9999999)
		t = t1;
	else
		t = t0;

	if (t1 <= 0.001f || t1 >= 9999999)
		return (9999999);
	return t;
}

glm::vec3 sphere::getNormal(glm::vec3 eye, glm::vec3 dir)
{
	glm::vec3 normal;
	float d = testIntersection(eye, dir);
	normal = glm::normalize(eye + d * dir - center);
	
	return normal;
}

glm::vec2 sphere::getTextureCoords(glm::vec3 eye, glm::vec3 dir)
{
	glm::vec3 x(1, 0, 0);
	glm::vec3 y(0, 1, 0);
	glm::vec3 z(0, 0, 1);
	glm::vec3 normal = glm::normalize(getNormal(eye, dir));

	float phi = acos((glm::dot(normal, z)));
	glm::vec3 ck = { normal.x, normal.y, 0 };
	float theta = acos((glm::dot(glm::normalize(ck), x)));

	if ((ck.x < 0 && ck.y < 0) || (ck.x > 0 && ck.y < 0))
		theta = (2 * float(PI)) - theta;

	glm::vec2 coords;
	coords.x = float(theta) / float(2 * float(PI));
	coords.y = float(phi) / float(PI);
	return coords;
}