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

double ballVector::getTimeUntilImpact(){
    double launchRad {(PI/180) * m_pair.m_launchAngle};

    double vertical {std::sin(launchRad)};

    double v_velo {vertical * m_velocity};

    double timeUntilImpact {v_velo / (0.5 * GRAVITY_CONSTANT)};
    return timeUntilImpact;
}
double ballVector::getProjection(){
    //1. convert launch angle to radians
    double launchRad {(PI/180) * m_pair.m_launchAngle};
    //2. convert dir angle to radians
    double dirRad {(PI/180) * m_pair.m_directionAngle};

    //3. get the cos and sine components of the launch angle
    double vertical {std::sin(launchRad)};
    double horizontal {std::cos(launchRad)};

    //4. translate to velocity components
    double v_velo {vertical * m_velocity};
    double h_velo {horizontal * m_velocity};

    std::cout << "horizontal speed: " << h_velo << std::endl;
    std::cout << "vertical speed: " << v_velo << std::endl;

    //5. determine time until ball hits the ground
    double timeUntilImpact {getTimeUntilImpact()};
    std::cout << "time until impact: " << timeUntilImpact << std::endl;
    std::cout << "horizontal distance(feet): " << timeUntilImpact*h_velo << std::endl;
    return 0.0;
}
