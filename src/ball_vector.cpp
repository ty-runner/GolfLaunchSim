#include "ball_vector.h"


// Default constructor
ballVector::ballVector()
    : m_velocity(0.0), m_pair{0.0, 0.0}
{
    std::cout << "Default vector made\n";
}

// Parameterized constructor

ballVector::ballVector(double velocity, double launchAngle, double dirAngle)
    : m_velocity{velocity}, m_pair{launchAngle, dirAngle}
{
    std::cout << "Vector constructed with parameters\n";
}

