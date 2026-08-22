import KerrSchild;
import PhysicsConfig;
import PhysicsTypes;

struct BoyerLindquistState
{
    Real r;
    Real theta;
    Real phi;
    Real pR;
    Real pTheta;
    Real pT;
    Real pPhi;
}

struct KerrCameraRay
{
    PhaseSpaceState state;
    Real angularMomentum;
    Real observerEnergy;
}

Real kerrDelta(Real radius, Real mass, Real spin)
{
    return radius * radius - Real(2.0) * mass * radius + spin * spin;
}

Real kerrOuterHorizon(Real mass, Real spin)
{
    return mass + sqrt(max(mass * mass - spin * spin, Real(0.0)));
}

Real kerrSchildAzimuthShift(Real radius, Real mass, Real spin)
{
    Real root = sqrt(max(mass * mass - spin * spin, Real(1.0e-12)));
    Real outer = mass + root;
    Real inner = mass - root;
    return spin / (outer - inner) *
        log(max((radius - outer) / max(radius - inner, Real(1.0e-12)),
                Real(1.0e-12)));
}

vector<Real, 3> kerrSchildRadiusGradient(
    vector<Real, 3> position,
    Real radius,
    Real spin)
{
    Real radius2 = radius * radius;
    Real denominator = radius2 * radius2 + spin * spin * position.z * position.z;
    return vector<Real, 3>(
        radius * radius2 * position.x / denominator,
        radius * radius2 * position.y / denominator,
        radius * position.z * (radius2 + spin * spin) / denominator);
}

PhaseSpaceState boyerLindquistToKerrSchild(
    BoyerLindquistState source,
    Real mass,
    Real spin)
{
    Real sine = sin(source.theta);
    Real cosine = cos(source.theta);
    Real azimuth = source.phi +
        kerrSchildAzimuthShift(source.r, mass, spin);
    Real cosineAzimuth = cos(azimuth);
    Real sineAzimuth = sin(azimuth);

    vector<Real, 3> position = vector<Real, 3>(
        (source.r * cosineAzimuth - spin * sineAzimuth) * sine,
        (source.r * sineAzimuth + spin * cosineAzimuth) * sine,
        source.r * cosine);
    Real delta = max(kerrDelta(source.r, mass, spin), Real(1.0e-10));
    Real transformedRadialMomentum = source.pR -
        (Real(2.0) * mass * source.r * source.pT + spin * source.pPhi) / delta;
    vector<Real, 3> gradientRadius =
        kerrSchildRadiusGradient(position, source.r, spin);
    Real safeSine = max(abs(sine), Real(1.0e-8));
    vector<Real, 3> gradientTheta =
        (cosine / (source.r * safeSine)) * gradientRadius -
        vector<Real, 3>(Real(0.0), Real(0.0), Real(1.0) / (source.r * safeSine));
    Real cylindricalRadius2 = max(
        position.x * position.x + position.y * position.y, Real(1.0e-12));
    vector<Real, 3> gradientAzimuth = vector<Real, 3>(
        -position.y / cylindricalRadius2,
         position.x / cylindricalRadius2,
         Real(0.0)) +
        spin / (source.r * source.r + spin * spin) * gradientRadius;
    vector<Real, 3> momentum =
        transformedRadialMomentum * gradientRadius +
        source.pTheta * gradientTheta + source.pPhi * gradientAzimuth;

    PhaseSpaceState result;
    result.position.components = vector<Real, 4>(Real(0.0), position);
    result.momentum.components = vector<Real, 4>(source.pT, momentum);
    return result;
}

Real kerrSchildBoyerLindquistAzimuth(
    Point4 point,
    Real mass,
    Real spin,
    Real radius)
{
    Real cartesianAngle = atan2(point.components[2], point.components[1]);
    Real oblateAngle = cartesianAngle - atan2(spin, radius);
    return oblateAngle - kerrSchildAzimuthShift(radius, mass, spin);
}

Real kerrSchildAxialMomentum(PhaseSpaceState state)
{
    return state.position.components[1] * state.momentum.components[2] -
           state.position.components[2] * state.momentum.components[1];
}

KerrCameraRay makeStationaryFidoRay(
    Real cameraRadius,
    Real cameraTheta,
    Real cameraPhi,
    Real mass,
    Real spin,
    vector<Real, 3> localDirection)
{
    Real sine = sin(cameraTheta);
    Real cosine = cos(cameraTheta);
    Real rho2 = cameraRadius * cameraRadius + spin * spin * cosine * cosine;
    Real delta = kerrDelta(cameraRadius, mass, spin);
    Real sigma2 =
        (cameraRadius * cameraRadius + spin * spin) *
        (cameraRadius * cameraRadius + spin * spin) -
        spin * spin * delta * sine * sine;
    Real rho = sqrt(max(rho2, Real(1.0e-10)));
    Real sigma = sqrt(max(sigma2, Real(1.0e-10)));
    Real alpha = rho * sqrt(max(delta, Real(1.0e-10))) / sigma;
    Real omega = Real(2.0) * mass * spin * cameraRadius /
        max(sigma2, Real(1.0e-10));
    Real varpi = sigma * max(abs(sine), Real(1.0e-8)) / rho;
    Real energyScale = Real(1.0) /
        max(alpha + omega * varpi * localDirection.z, Real(1.0e-8));

    BoyerLindquistState boyerLindquist;
    boyerLindquist.r = cameraRadius;
    boyerLindquist.theta = cameraTheta;
    boyerLindquist.phi = cameraPhi;
    boyerLindquist.pR = energyScale * rho * localDirection.x /
        sqrt(max(delta, Real(1.0e-10)));
    boyerLindquist.pTheta = energyScale * rho * localDirection.y;
    boyerLindquist.pT = Real(-1.0);
    boyerLindquist.pPhi = energyScale * varpi * localDirection.z;

    KerrCameraRay result;
    result.state = boyerLindquistToKerrSchild(
        boyerLindquist, mass, spin);
    result.angularMomentum = boyerLindquist.pPhi;
    result.observerEnergy =
        (Real(1.0) - omega * boyerLindquist.pPhi) / max(alpha, Real(1.0e-8));
    return result;
}
