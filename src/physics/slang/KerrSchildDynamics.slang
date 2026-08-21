import CanonicalFlow;
import Hamiltonian;
import KerrSchild;
import PhysicsConfig;
import PhysicsTypes;

// Exact factorization of Hamilton's equations for the stationary Cartesian
// Kerr-Schild inverse metric g^uv = eta^uv - 2 h l^u l^v. It is algebraically
// identical to differentiating MetricHamiltonianSystem, but shares the radius,
// h, and null-vector work instead of running AD inside every RK stage.
PhaseSpaceDerivative kerrSchildPhaseDerivative(
    KerrSchildMetric metric,
    PhaseSpaceState state)
{
    vector<Real, 4> position = state.position.components;
    vector<Real, 4> momentum = state.momentum.components;
    Real x = position[1];
    Real y = position[2];
    Real z = position[3];
    Real spin = metric.spin;
    Real spin2 = spin * spin;

    Real rawRadius2 = metric.oblateRadiusSquared(state.position);
    Real radius2 = max(rawRadius2, Real(1.0e-20));
    Real radius = sqrt(radius2);
    Real radialDenominator = radius2 + spin2;

    Real discriminant = x * x + y * y + z * z - spin2;
    Real implicitDenominator = Real(2.0) * rawRadius2 - discriminant;
    vector<Real, 3> radius2Gradient = vector<Real, 3>(Real(0.0));
    // This is also the derivative of the clamp used by KerrSchildMetric.
    if (rawRadius2 > Real(1.0e-20))
    {
        radius2Gradient = vector<Real, 3>(
            Real(2.0) * x * rawRadius2,
            Real(2.0) * y * rawRadius2,
            Real(2.0) * z * (rawRadius2 + spin2)) /
            implicitDenominator;
    }
    vector<Real, 3> radiusGradient =
        radius2Gradient / (Real(2.0) * radius);

    Real spinHeightRatio = spin * z / radius2;
    Real h = (metric.mass / radius) /
        (Real(1.0) + spinHeightRatio * spinHeightRatio);
    vector<Real, 4> nullVector = vector<Real, 4>(
        Real(-1.0),
        (radius * x + spin * y) / radialDenominator,
        (radius * y - spin * x) / radialDenominator,
        z / radius);
    Real nullMomentum = dot(nullVector, momentum);

    PhaseSpaceDerivative result;
    result.position.components = vector<Real, 4>(
        -momentum[0], momentum[1], momentum[2], momentum[3]) -
        Real(2.0) * h * nullMomentum * nullVector;

    Real hDenominator = radius2 * radius2 + spin2 * z * z;
    vector<Real, 3> zAxis =
        vector<Real, 3>(Real(0.0), Real(0.0), Real(1.0));
    vector<Real, 3> hGradient = h * (
        Real(1.5) * radius2Gradient / radius2 -
        (Real(2.0) * radius2 * radius2Gradient +
         Real(2.0) * spin2 * z * zAxis) / hDenominator);

    Real xyMomentumPosition = momentum[1] * x + momentum[2] * y;
    Real xyMomentumNull =
        momentum[1] * nullVector[1] + momentum[2] * nullVector[2];
    vector<Real, 3> nullMomentumGradient =
        radiusGradient *
            (xyMomentumPosition / radialDenominator -
             momentum[3] * z / radius2) -
        radius2Gradient * (xyMomentumNull / radialDenominator) +
        vector<Real, 3>(
            (radius * momentum[1] - spin * momentum[2]) / radialDenominator,
            (spin * momentum[1] + radius * momentum[2]) / radialDenominator,
            momentum[3] / radius);

    result.momentum.components = vector<Real, 4>(
        Real(0.0),
        hGradient * (nullMomentum * nullMomentum) +
            Real(2.0) * h * nullMomentum * nullMomentumGradient);
    return result;
}

struct KerrSchildHamiltonianSystem : ICanonicalFlowSystem
{
    no_diff KerrSchildMetric metric;

    [Differentiable]
    [NoDiffThis]
    Real hamiltonian(vector<Real, 4> position, vector<Real, 4> momentum)
    {
        MetricHamiltonianSystem<KerrSchildMetric> metricSystem;
        metricSystem.metric = metric;
        return metricSystem.hamiltonian(position, momentum);
    }

    [NoDiffThis]
    PhaseSpaceDerivative phaseDerivative(PhaseSpaceState state)
    {
        return kerrSchildPhaseDerivative(metric, state);
    }
}
