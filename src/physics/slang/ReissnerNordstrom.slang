import CanonicalFlow;
import Hamiltonian;
import Metric;
import PhysicsConfig;
import PhysicsTypes;

// Reissner-Nordstrom in horizon-penetrating Cartesian Kerr-Schild coordinates
// (T,x,y,z), signature (-,+,+,+). Geometrized units set G = c = 1.
struct ReissnerNordstromMetric : IMetric
{
    no_diff Real mass;
    no_diff Real charge;

    bool isDefined(Point4 point)
    {
        bool finite = mass > Real(0.0) && !isnan(mass) && !isinf(mass) &&
            !isnan(charge) && !isinf(charge);
        [ForceUnroll]
        for (int index = 0; index < 4; ++index)
            finite = finite && !isnan(point.components[index]) &&
                !isinf(point.components[index]);
        Real spatialRadius2 =
            point.components[1] * point.components[1] +
            point.components[2] * point.components[2] +
            point.components[3] * point.components[3];
        return finite && spatialRadius2 > Real(0.0);
    }

    [Differentiable]
    [NoDiffThis]
    Real radialCoordinate(Point4 point)
    {
        Real spatialRadius2 =
            point.components[1] * point.components[1] +
            point.components[2] * point.components[2] +
            point.components[3] * point.components[3];
        return sqrt(max(spatialRadius2, Real(1.0e-20)));
    }

    [Differentiable]
    [NoDiffThis]
    void geometry(
        Point4 point,
        out Real h,
        out vector<Real, 4> nullCovector,
        out vector<Real, 4> nullVector)
    {
        Real radius = radialCoordinate(point);
        Real inverseRadius = Real(1.0) / radius;
        Real chargeOverRadius = charge * inverseRadius;
        h = mass * inverseRadius -
            Real(0.5) * chargeOverRadius * chargeOverRadius;
        vector<Real, 3> radialDirection = vector<Real, 3>(
            point.components[1],
            point.components[2],
            point.components[3]) * inverseRadius;
        nullCovector = vector<Real, 4>(Real(1.0), radialDirection);
        nullVector = vector<Real, 4>(Real(-1.0), radialDirection);
    }

    [Differentiable]
    [NoDiffThis]
    CovariantTensor2 covariantMetric(Point4 point)
    {
        Real h;
        vector<Real, 4> nullCovector;
        vector<Real, 4> nullVector;
        geometry(point, h, nullCovector, nullVector);

        CovariantTensor2 result;
        [ForceUnroll]
        for (int mu = 0; mu < 4; ++mu)
        {
            [ForceUnroll]
            for (int nu = 0; nu < 4; ++nu)
            {
                Real flat = mu == nu
                    ? (mu == 0 ? Real(-1.0) : Real(1.0))
                    : Real(0.0);
                result.components[mu][nu] =
                    flat + Real(2.0) * h * nullCovector[mu] * nullCovector[nu];
            }
        }
        return result;
    }

    [Differentiable]
    [NoDiffThis]
    ContravariantTensor2 contravariantMetric(Point4 point)
    {
        Real h;
        vector<Real, 4> nullCovector;
        vector<Real, 4> nullVector;
        geometry(point, h, nullCovector, nullVector);

        ContravariantTensor2 result;
        [ForceUnroll]
        for (int mu = 0; mu < 4; ++mu)
        {
            [ForceUnroll]
            for (int nu = 0; nu < 4; ++nu)
            {
                Real flat = mu == nu
                    ? (mu == 0 ? Real(-1.0) : Real(1.0))
                    : Real(0.0);
                result.components[mu][nu] =
                    flat - Real(2.0) * h * nullVector[mu] * nullVector[nu];
            }
        }
        return result;
    }
}

Real reissnerNordstromOuterHorizon(Real mass, Real charge)
{
    return mass + sqrt(max(
        mass * mass - charge * charge, Real(0.0)));
}

// Exact factorization of H = 1/2 (eta^munu - f l^mu l^nu) p_mu p_nu,
// where f = 2M/r - Q^2/r^2. This avoids automatic differentiation in the
// production fragment while retaining the generic metric Hamiltonian contract.
struct ReissnerNordstromHamiltonianSystem : ICanonicalFlowSystem
{
    no_diff ReissnerNordstromMetric metric;

    [Differentiable]
    [NoDiffThis]
    Real hamiltonian(vector<Real, 4> position, vector<Real, 4> momentum)
    {
        MetricHamiltonianSystem<ReissnerNordstromMetric> system;
        system.metric = metric;
        return system.hamiltonian(position, momentum);
    }

    [NoDiffThis]
    PhaseSpaceDerivative phaseDerivative(PhaseSpaceState state)
    {
        vector<Real, 3> position = vector<Real, 3>(
            state.position.components[1],
            state.position.components[2],
            state.position.components[3]);
        vector<Real, 3> spatialMomentum = vector<Real, 3>(
            state.momentum.components[1],
            state.momentum.components[2],
            state.momentum.components[3]);
        Real radius = sqrt(max(dot(position, position), Real(1.0e-20)));
        Real inverseRadius = Real(1.0) / radius;
        vector<Real, 3> radialDirection = position * inverseRadius;
        Real radialMomentum = dot(radialDirection, spatialMomentum);
        Real nullMomentum = -state.momentum.components[0] + radialMomentum;
        Real charge2 = metric.charge * metric.charge;
        Real f = Real(2.0) * metric.mass * inverseRadius -
            charge2 * inverseRadius * inverseRadius;
        Real fDerivative = -Real(2.0) * metric.mass * inverseRadius *
                inverseRadius +
            Real(2.0) * charge2 * inverseRadius * inverseRadius *
                inverseRadius;

        PhaseSpaceDerivative result;
        result.position.components[0] = -state.momentum.components[0] +
            f * nullMomentum;
        vector<Real, 3> spatialPositionRate = spatialMomentum -
            f * nullMomentum * radialDirection;
        result.position.components[1] = spatialPositionRate.x;
        result.position.components[2] = spatialPositionRate.y;
        result.position.components[3] = spatialPositionRate.z;
        result.momentum.components[0] = Real(0.0);
        vector<Real, 3> spatialMomentumRate =
            Real(0.5) * fDerivative * nullMomentum * nullMomentum *
                radialDirection +
            f * nullMomentum * inverseRadius *
                (spatialMomentum - radialMomentum * radialDirection);
        result.momentum.components[1] = spatialMomentumRate.x;
        result.momentum.components[2] = spatialMomentumRate.y;
        result.momentum.components[3] = spatialMomentumRate.z;
        return result;
    }
}
