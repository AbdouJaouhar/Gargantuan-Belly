import Metric;
import PhysicsConfig;
import PhysicsTypes;

interface IHamiltonianSystem
{
    [Differentiable]
    [NoDiffThis]
    Real hamiltonian(vector<Real, 4> position, vector<Real, 4> momentum);
}

// Any metric is automatically a canonical Hamiltonian theory. More general
// dispersion relations can implement IHamiltonianSystem directly.
struct MetricHamiltonianSystem<M : IMetric> : IHamiltonianSystem
{
    no_diff M metric;

    [Differentiable]
    [NoDiffThis]
    Real hamiltonian(vector<Real, 4> position, vector<Real, 4> momentum)
    {
        Point4 point;
        point.components = position;
        Covector4 covector;
        covector.components = momentum;
        ContravariantTensor2 inverseMetric = metric.contravariantMetric(point);
        TangentVector4 velocity = raise(inverseMetric, covector);
        return Real(0.5) * contract(covector, velocity);
    }
}

[Differentiable]
Real evaluateHamiltonian<System : IHamiltonianSystem>(
    no_diff System system,
    vector<Real, 4> position,
    vector<Real, 4> momentum)
{
    return system.hamiltonian(position, momentum);
}

// Hamilton's equations are generated from the scalar H itself. Reverse mode
// obtains the full gradient of the scalar Hamiltonian in one differentiated
// evaluation instead of one forward-mode evaluation per phase-space axis.
// This remains the default extension seam for metrics and modified theories
// that do not provide an algebraically factorized flow.
PhaseSpaceDerivative canonicalDerivative<System : IHamiltonianSystem>(
    no_diff System system,
    PhaseSpaceState state)
{
    DifferentialPair<vector<Real, 4>> position =
        diffPair(state.position.components);
    DifferentialPair<vector<Real, 4>> momentum =
        diffPair(state.momentum.components);
    // Reverse-mode differentiable arguments are inout parameters in generated
    // code, so these pairs must remain named mutable lvalues.
    bwd_diff(evaluateHamiltonian<System>)(
        system, position, momentum, Real(1.0));

    PhaseSpaceDerivative result;
    result.position.components = momentum.d;
    result.momentum.components = -position.d;
    return result;
}

PhaseSpaceState offsetState(
    PhaseSpaceState state,
    PhaseSpaceDerivative derivative,
    Real amount)
{
    state.position.components += derivative.position.components * amount;
    state.momentum.components += derivative.momentum.components * amount;
    return state;
}
