import PhysicsConfig;

// Cross-target differential-geometry value types. These wrappers make index
// variance explicit while keeping storage compatible with both SPIR-V and the
// C++ emitted by Slang.
struct Point4 : IDifferentiable
{
    vector<Real, 4> components;
}

struct TangentVector4 : IDifferentiable
{
    vector<Real, 4> components;
}

struct Covector4 : IDifferentiable
{
    vector<Real, 4> components;
}

struct CovariantTensor2 : IDifferentiable
{
    matrix<Real, 4, 4> components;
}

struct ContravariantTensor2 : IDifferentiable
{
    matrix<Real, 4, 4> components;
}

struct PhaseSpaceState : IDifferentiable
{
    Point4 position;
    Covector4 momentum;
}

struct PhaseSpaceDerivative : IDifferentiable
{
    TangentVector4 position;
    Covector4 momentum;
}

[Differentiable]
Real contract(Covector4 covector, TangentVector4 vectorValue)
{
    Real result = Real(0.0);
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
        result += covector.components[index] * vectorValue.components[index];
    return result;
}

[Differentiable]
TangentVector4 raise(
    ContravariantTensor2 inverseMetric,
    Covector4 covector)
{
    TangentVector4 result;
    result.components = vector<Real, 4>(Real(0.0));
    [ForceUnroll]
    for (int mu = 0; mu < 4; ++mu)
    {
        [ForceUnroll]
        for (int nu = 0; nu < 4; ++nu)
            result.components[mu] +=
                inverseMetric.components[mu][nu] * covector.components[nu];
    }
    return result;
}

[Differentiable]
Covector4 lower(CovariantTensor2 metric, TangentVector4 vectorValue)
{
    Covector4 result;
    result.components = vector<Real, 4>(Real(0.0));
    [ForceUnroll]
    for (int mu = 0; mu < 4; ++mu)
    {
        [ForceUnroll]
        for (int nu = 0; nu < 4; ++nu)
            result.components[mu] +=
                metric.components[mu][nu] * vectorValue.components[nu];
    }
    return result;
}