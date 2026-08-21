import CanonicalFlow;
import Hamiltonian;
import KerrSchild;
import KerrSchildDynamics;
import Metric;
import ModifiedDispersion;
import PhysicsConfig;
import PhysicsTypes;
import ReissnerNordstrom;

struct HostMetricInput
{
    Real mass;
    Real spin;
    Real position[4];
}

struct HostMetricOutput
{
    int status;
    Real covariant[16];
    Real contravariant[16];
    // derivative[lambda * 16 + mu * 4 + nu] = partial_lambda g_mu_nu.
    Real derivative[64];
    // secondDerivative[(lambda * 4 + kappa) * 16 + mu * 4 + nu].
    Real secondDerivative[256];
    Real radius;
}

struct HostPhaseInput
{
    Real mass;
    Real spin;
    Real position[4];
    Real momentum[4];
}

struct HostPhaseOutput
{
    Real hamiltonian;
    Real positionRate[4];
    Real momentumRate[4];
}

struct HostStepInput
{
    Real mass;
    Real spin;
    Real stepSize;
    Real position[4];
    Real momentum[4];
}

struct HostStepOutput
{
    Real position[4];
    Real momentum[4];
}

struct HostQuarticPhaseInput
{
    Real coupling;
    Real position[4];
    Real momentum[4];
}

struct HostChargedMetricInput
{
    Real mass;
    Real charge;
    Real position[4];
}

struct HostChargedPhaseInput
{
    Real mass;
    Real charge;
    Real position[4];
    Real momentum[4];
}

struct HostChargedStepInput
{
    Real mass;
    Real charge;
    Real stepSize;
    Real position[4];
    Real momentum[4];
}

[Differentiable]
CovariantTensor2 evaluateCovariantMetric<M : IMetric>(
    no_diff M metric,
    vector<Real, 4> position)
{
    Point4 point;
    point.components = position;
    return metric.covariantMetric(point);
}

CovariantTensor2 evaluateCovariantMetricDirectional<M : IMetric>(
    no_diff M metric,
    vector<Real, 4> position,
    no_diff vector<Real, 4> direction)
{
    return fwd_diff(evaluateCovariantMetric<M>)(
        metric, diffPair(position, direction)).d;
}

KerrSchildMetric makeHostMetric(Real mass, Real spin)
{
    KerrSchildMetric metric;
    metric.mass = mass;
    metric.spin = spin;
    return metric;
}

ReissnerNordstromMetric makeHostChargedMetric(Real mass, Real charge)
{
    ReissnerNordstromMetric metric;
    metric.mass = mass;
    metric.charge = charge;
    return metric;
}

PhaseSpaceState makeHostState(HostPhaseInput input)
{
    PhaseSpaceState state;
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
    {
        state.position.components[index] = input.position[index];
        state.momentum.components[index] = input.momentum[index];
    }
    return state;
}

PhaseSpaceState makeHostChargedState(HostChargedPhaseInput input)
{
    PhaseSpaceState state;
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
    {
        state.position.components[index] = input.position[index];
        state.momentum.components[index] = input.momentum[index];
    }
    return state;
}

void writeHostMetric<M : IMetric>(
    no_diff M metric,
    vector<Real, 4> position,
    out HostMetricOutput output)
{
    Point4 point;
    point.components = position;
    output.status = metric.isDefined(point) ? 0 : 1;
    if (output.status != 0)
        return;

    CovariantTensor2 covariant = metric.covariantMetric(point);
    ContravariantTensor2 contravariant = metric.contravariantMetric(point);
    output.radius = metric.radialCoordinate(point);
    [ForceUnroll]
    for (int mu = 0; mu < 4; ++mu)
    {
        [ForceUnroll]
        for (int nu = 0; nu < 4; ++nu)
        {
            output.covariant[mu * 4 + nu] = covariant.components[mu][nu];
            output.contravariant[mu * 4 + nu] =
                contravariant.components[mu][nu];
        }
    }

    [ForceUnroll]
    for (int lambda = 0; lambda < 4; ++lambda)
    {
        vector<Real, 4> seed = vector<Real, 4>(Real(0.0));
        seed[lambda] = Real(1.0);
        DifferentialPair<CovariantTensor2> componentAndDerivative = fwd_diff(
            evaluateCovariantMetric<M>)(
                metric, diffPair(point.components, seed));
        [ForceUnroll]
        for (int mu = 0; mu < 4; ++mu)
        {
            [ForceUnroll]
            for (int nu = 0; nu < 4; ++nu)
                output.derivative[lambda * 16 + mu * 4 + nu] =
                    componentAndDerivative.d.components[mu][nu];
        }


        [ForceUnroll]
        for (int kappa = 0; kappa < 4; ++kappa)
        {
            Real finiteDifferenceStep = Real(1.0e-5) *
                max(abs(point.components[kappa]), Real(1.0));
            vector<Real, 4> forwardPosition = point.components;
            vector<Real, 4> backwardPosition = point.components;
            forwardPosition[kappa] += finiteDifferenceStep;
            backwardPosition[kappa] -= finiteDifferenceStep;
            CovariantTensor2 forwardDerivative =
                evaluateCovariantMetricDirectional<M>(
                    metric, forwardPosition, seed);
            CovariantTensor2 backwardDerivative =
                evaluateCovariantMetricDirectional<M>(
                    metric, backwardPosition, seed);
            [ForceUnroll]
            for (int mu = 0; mu < 4; ++mu)
            {
                [ForceUnroll]
                for (int nu = 0; nu < 4; ++nu)
                    output.secondDerivative[
                        (lambda * 4 + kappa) * 16 + mu * 4 + nu] =
                            (forwardDerivative.components[mu][nu] -
                             backwardDerivative.components[mu][nu]) /
                            (Real(2.0) * finiteDifferenceStep);
            }
        }
    }
}

void writeHostPhase<System : ICanonicalFlowSystem>(
    no_diff System system,
    PhaseSpaceState state,
    out HostPhaseOutput output)
{
    PhaseSpaceDerivative derivative = system.phaseDerivative(state);
    output.hamiltonian = system.hamiltonian(
        state.position.components, state.momentum.components);
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
    {
        output.positionRate[index] = derivative.position.components[index];
        output.momentumRate[index] = derivative.momentum.components[index];
    }
}

export __extern_cpp void gargantua_kerr_schild_metric(
    HostMetricInput input,
    out HostMetricOutput output)
{
    vector<Real, 4> position;
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
        position[index] = input.position[index];
    writeHostMetric(makeHostMetric(input.mass, input.spin), position, output);
}

export __extern_cpp void gargantua_kerr_schild_phase(
    HostPhaseInput input,
    out HostPhaseOutput output)
{
    KerrSchildHamiltonianSystem system;
    system.metric = makeHostMetric(input.mass, input.spin);
    writeHostPhase(system, makeHostState(input), output);
}

// Scientific oracle for the optimized flow above. This deliberately retains
// the automatic path so tests can detect algebraic drift without putting AD in
// the production renderer.
export __extern_cpp void gargantua_kerr_schild_phase_automatic_reference(
    HostPhaseInput input,
    out HostPhaseOutput output)
{
    MetricHamiltonianSystem<KerrSchildMetric> metricSystem;
    metricSystem.metric = makeHostMetric(input.mass, input.spin);
    AutomaticCanonicalFlowSystem<MetricHamiltonianSystem<KerrSchildMetric>>
        system;
    system.system = metricSystem;
    writeHostPhase(system, makeHostState(input), output);
}

export __extern_cpp void gargantua_kerr_schild_rk4(
    HostStepInput input,
    out HostStepOutput output)
{
    HostPhaseInput phaseInput;
    phaseInput.mass = input.mass;
    phaseInput.spin = input.spin;
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
    {
        phaseInput.position[index] = input.position[index];
        phaseInput.momentum[index] = input.momentum[index];
    }

    KerrSchildHamiltonianSystem system;
    system.metric = makeHostMetric(input.mass, input.spin);
    PhaseSpaceState next = integrateCanonicalRk4(
        system, makeHostState(phaseInput), input.stepSize);
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
    {
        output.position[index] = next.position.components[index];
        output.momentum[index] = next.momentum.components[index];
    }
}

export __extern_cpp void gargantua_reissner_nordstrom_metric(
    HostChargedMetricInput input,
    out HostMetricOutput output)
{
    vector<Real, 4> position;
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
        position[index] = input.position[index];
    writeHostMetric(
        makeHostChargedMetric(input.mass, input.charge), position, output);
}

export __extern_cpp void gargantua_reissner_nordstrom_phase(
    HostChargedPhaseInput input,
    out HostPhaseOutput output)
{
    ReissnerNordstromHamiltonianSystem system;
    system.metric = makeHostChargedMetric(input.mass, input.charge);
    writeHostPhase(system, makeHostChargedState(input), output);
}

export __extern_cpp void
gargantua_reissner_nordstrom_phase_automatic_reference(
    HostChargedPhaseInput input,
    out HostPhaseOutput output)
{
    MetricHamiltonianSystem<ReissnerNordstromMetric> metricSystem;
    metricSystem.metric = makeHostChargedMetric(input.mass, input.charge);
    AutomaticCanonicalFlowSystem<
        MetricHamiltonianSystem<ReissnerNordstromMetric>> system;
    system.system = metricSystem;
    writeHostPhase(system, makeHostChargedState(input), output);
}

export __extern_cpp void gargantua_reissner_nordstrom_rk4(
    HostChargedStepInput input,
    out HostStepOutput output)
{
    HostChargedPhaseInput phaseInput;
    phaseInput.mass = input.mass;
    phaseInput.charge = input.charge;
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
    {
        phaseInput.position[index] = input.position[index];
        phaseInput.momentum[index] = input.momentum[index];
    }

    ReissnerNordstromHamiltonianSystem system;
    system.metric = makeHostChargedMetric(input.mass, input.charge);
    PhaseSpaceState next = integrateCanonicalRk4(
        system, makeHostChargedState(phaseInput), input.stepSize);
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
    {
        output.position[index] = next.position.components[index];
        output.momentum[index] = next.momentum.components[index];
    }
}

export __extern_cpp void gargantua_quartic_dispersion_phase(
    HostQuarticPhaseInput input,
    out HostPhaseOutput output)
{
    QuarticDispersionSystem hamiltonianSystem;
    hamiltonianSystem.coupling = input.coupling;
    AutomaticCanonicalFlowSystem<QuarticDispersionSystem> system;
    system.system = hamiltonianSystem;
    PhaseSpaceState state;
    [ForceUnroll]
    for (int index = 0; index < 4; ++index)
    {
        state.position.components[index] = input.position[index];
        state.momentum.components[index] = input.momentum[index];
    }
    writeHostPhase(system, state, output);
}
