import PhysicsConfig;
import PhysicsTypes;

// A chart-specific implementation supplies both index placements. The
// inverse form drives Hamiltonian propagation; the covariant form is exposed
// for observables, connections, curvature, and scientific tests.
interface IMetric
{
    bool isDefined(Point4 point);

    [Differentiable]
    [NoDiffThis]
    CovariantTensor2 covariantMetric(Point4 point);

    [Differentiable]
    [NoDiffThis]
    ContravariantTensor2 contravariantMetric(Point4 point);

    Real radialCoordinate(Point4 point);
}
