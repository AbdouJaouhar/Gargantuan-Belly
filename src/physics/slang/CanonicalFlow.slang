import Hamiltonian;
import PhysicsConfig;
import PhysicsTypes;

// A canonical flow may use the automatic Hamiltonian gradient below or an
// exact factorization that is cheaper for a particular metric. Both paths are
// defined in canonical Slang and therefore compile for host science and Vulkan.
interface ICanonicalFlowSystem : IHamiltonianSystem
{
    [NoDiffThis]
    PhaseSpaceDerivative phaseDerivative(PhaseSpaceState state);
}

// Default extension path: implementing one scalar Hamiltonian is sufficient.
// Reverse-mode AD supplies a consistent canonical flow automatically.
struct AutomaticCanonicalFlowSystem<System : IHamiltonianSystem>
    : ICanonicalFlowSystem
{
    no_diff System system;

    [Differentiable]
    [NoDiffThis]
    Real hamiltonian(vector<Real, 4> position, vector<Real, 4> momentum)
    {
        return system.hamiltonian(position, momentum);
    }

    [NoDiffThis]
    PhaseSpaceDerivative phaseDerivative(PhaseSpaceState state)
    {
        return canonicalDerivative(system, state);
    }
}

PhaseSpaceState integrateCanonicalRk4<System : ICanonicalFlowSystem>(
    no_diff System system,
    PhaseSpaceState state,
    Real stepSize)
{
    PhaseSpaceDerivative k1 = system.phaseDerivative(state);
    PhaseSpaceDerivative k2 = system.phaseDerivative(
        offsetState(state, k1, Real(0.5) * stepSize));
    PhaseSpaceDerivative k3 = system.phaseDerivative(
        offsetState(state, k2, Real(0.5) * stepSize));
    PhaseSpaceDerivative k4 = system.phaseDerivative(
        offsetState(state, k3, stepSize));

    state.position.components += stepSize *
        (k1.position.components + Real(2.0) * k2.position.components +
         Real(2.0) * k3.position.components + k4.position.components) / Real(6.0);
    state.momentum.components += stepSize *
        (k1.momentum.components + Real(2.0) * k2.momentum.components +
         Real(2.0) * k3.momentum.components + k4.momentum.components) / Real(6.0);
    return state;
}
