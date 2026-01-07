//ball_vector.h
#ifndef BALL_VECTOR_H
#define BALL_VECTOR_H
#include <iostream>
#include <cmath>

struct anglePair
{
    double m_launchAngle {};
    double m_directionAngle {};
};
class ballVector
{
private:
    double m_velocity {};
    anglePair m_pair {};

public:
    ballVector();
    ballVector(double velocity, double launchAngle, double dirAngle);
};
#endif
