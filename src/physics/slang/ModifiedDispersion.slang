import Hamiltonian;
import PhysicsConfig;

// A small non-metric theory used as an executable extension example. New
// dispersion relations implement IHamiltonianSystem once; Slang's canonical
// derivative then feeds both host integrators and GPU pipeline specializations.
struct QuarticDispersionSystem : IHamiltonianSystem
{
    no_diff Real coupling;

    [Differentiable]
    [NoDiffThis]
    Real hamiltonian(vector<Real, 4> position, vector<Real, 4> momentum)
    {
        Real spatialNormSquared =
            momentum[1] * momentum[1] +
            momentum[2] * momentum[2] +
            momentum[3] * momentum[3];
        return Real(0.5) *
                (-momentum[0] * momentum[0] + spatialNormSquared) +
            coupling * spatialNormSquared * spatialNormSquared;
    }
}
