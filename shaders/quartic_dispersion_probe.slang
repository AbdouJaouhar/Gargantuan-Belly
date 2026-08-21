import CanonicalFlow;
import Hamiltonian;
import ModifiedDispersion;
import PhysicsConfig;
import PhysicsTypes;

// Compile-time GPU conformance fixture for the public theory interface. The
// scientific test exercises the same QuarticDispersionSystem through generated
// host code; this entry point proves that no host-only construct slipped into
// the theory or automatic canonical derivative.
RWStructuredBuffer<float> probeOutput;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    QuarticDispersionSystem system;
    system.coupling = 0.1;
    AutomaticCanonicalFlowSystem<QuarticDispersionSystem> flow;
    flow.system = system;
    PhaseSpaceState state;
    state.position.components = vector<Real, 4>(0.0, 3.0, -2.0, 1.0);
    state.momentum.components = vector<Real, 4>(-2.0, 1.0, 0.0, 0.0);
    PhaseSpaceDerivative derivative = flow.phaseDerivative(state);
    uint offset = dispatchThreadId.x * 9;
    probeOutput[offset] = flow.hamiltonian(
        state.position.components, state.momentum.components);
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
    {
        probeOutput[offset + 1 + index] =
            float(derivative.position.components[index]);
        probeOutput[offset + 5 + index] =
            float(derivative.momentum.components[index]);
    }
}
