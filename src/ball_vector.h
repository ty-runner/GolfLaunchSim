//ball_vector.h
#ifndef BALL_VECTOR_H
#define BALL_VECTOR_H
#define PI 3.14159265358979323846
#include <iostream>
#include <cmath>

struct anglePair
{
    double m_launchAngle {};
    double m_directionAngle {};
};

struct spinPair
{
    double m_spinSpeed {};
    double m_spinAxis {};
};
class ballVector
{
private:
    double m_velocity {}; //assume velocity is in feet per second
    anglePair m_pair {};
    spinPair m_spinPair {};

public:
    ballVector();
    ballVector(double velocity, double launchAngle, double dirAngle);
    double getProjection();
};
#endif
