import PhysicsConfig;
import PhysicsTypes;
import ReissnerNordstrom;

struct ReissnerNordstromCameraRay
{
    PhaseSpaceState state;
    Real angularMomentum;
    Real observerEnergy;
}

ReissnerNordstromCameraRay makeReissnerNordstromStationaryRay(
    Real cameraRadius,
    Real cameraTheta,
    Real mass,
    Real charge,
    vector<Real, 3> localDirection)
{
    Real sine = sin(cameraTheta);
    Real cosine = cos(cameraTheta);
    Real charge2 = charge * charge;
    Real lapse2 = Real(1.0) - Real(2.0) * mass / cameraRadius +
        charge2 / (cameraRadius * cameraRadius);
    Real safeLapse2 = max(lapse2, Real(1.0e-10));
    Real lapse = sqrt(safeLapse2);
    Real energyScale = Real(1.0) / lapse;

    // Covector in the usual static spherical chart, normalized to E=-p_t=1.
    Real pT = Real(-1.0);
    Real pR = energyScale * localDirection.x / lapse;
    Real pTheta = energyScale * cameraRadius * localDirection.y;
    Real pPhi = energyScale * cameraRadius *
        max(abs(sine), Real(1.0e-8)) * localDirection.z;

    // T = t + integral((1/F)-1)dr converts the static chart to Cartesian
    // Kerr-Schild time. Covectors transform with p'_r = p_r-(1/F-1)p_t.
    Real kerrSchildFactor = (Real(1.0) - safeLapse2) / safeLapse2;
    Real transformedRadialMomentum = pR - kerrSchildFactor * pT;
    vector<Real, 3> radialDirection = vector<Real, 3>(
        sine, Real(0.0), cosine);
    vector<Real, 3> thetaGradient = vector<Real, 3>(
        cosine / cameraRadius,
        Real(0.0),
        -sine / cameraRadius);
    vector<Real, 3> phiGradient = vector<Real, 3>(
        Real(0.0),
        Real(1.0) /
            (cameraRadius * max(abs(sine), Real(1.0e-8))),
        Real(0.0));

    ReissnerNordstromCameraRay result;
    result.state.position.components = vector<Real, 4>(
        Real(0.0), cameraRadius * radialDirection);
    result.state.momentum.components = vector<Real, 4>(
        pT,
        transformedRadialMomentum * radialDirection +
            pTheta * thetaGradient + pPhi * phiGradient);
    result.angularMomentum = pPhi;
    result.observerEnergy = energyScale;
    return result;
}
