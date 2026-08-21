#ifndef GARGANTUA_RAY_STATE_GLSL
#define GARGANTUA_RAY_STATE_GLSL

struct RayState {
    float r;
    float theta;
    float phi;
    float p_r;
    float p_theta;
};

struct RayDerivative {
    float r;
    float theta;
    float phi;
    float p_r;
    float p_theta;
};

struct CameraRay {
    RayState state;
    float b;
    float observer_energy;
};

#endif
