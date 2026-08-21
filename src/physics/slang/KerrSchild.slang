import Metric;
import PhysicsConfig;
import PhysicsTypes;

// Kerr in horizon-penetrating Cartesian Kerr-Schild coordinates (T,x,y,z),
// signature (-,+,+,+). This chart is regular at the axis and horizons.
struct KerrSchildMetric : IMetric
{
    no_diff Real mass;
    no_diff Real spin;

    [Differentiable]
    [NoDiffThis]
    Real oblateRadiusSquared(Point4 point)
    {
        Real x = point.components[1];
        Real y = point.components[2];
        Real z = point.components[3];
        Real a2 = spin * spin;
        Real cartesianRadius2 = x * x + y * y + z * z;
        Real discriminant = cartesianRadius2 - a2;
        Real crossTerm = Real(2.0) * spin * z;
        Real scale = max(abs(discriminant), abs(crossTerm));
        Real radical = scale > Real(0.0)
            ? scale * sqrt(
                (discriminant / scale) * (discriminant / scale) +
                (crossTerm / scale) * (crossTerm / scale))
            : Real(0.0);
        // The conjugate form avoids cancellation inside the r=0 branch disk.
        return discriminant < Real(0.0)
            ? (Real(0.5) * crossTerm) *
                (crossTerm / (radical - discriminant))
            : Real(0.5) * (discriminant + radical);
    }

    bool isDefined(Point4 point)
    {
        bool finite = mass > Real(0.0) && !isnan(mass) && !isinf(mass) &&
            !isnan(spin) && !isinf(spin);
        [ForceUnroll]
        for (int index = 0; index < 4; ++index)
            finite = finite && !isnan(point.components[index]) &&
                !isinf(point.components[index]);
        return finite && oblateRadiusSquared(point) > Real(0.0);
    }

    Real radialCoordinate(Point4 point)
    {
        return sqrt(max(oblateRadiusSquared(point), Real(1.0e-20)));
    }

    [Differentiable]
    [NoDiffThis]
    void geometry(
        Point4 point,
        out Real h,
        out vector<Real, 4> nullCovector,
        out vector<Real, 4> nullVector)
    {
        Real x = point.components[1];
        Real y = point.components[2];
        Real z = point.components[3];
        Real a2 = spin * spin;
        Real radius2 = oblateRadiusSquared(point);
        radius2 = max(radius2, Real(1.0e-20));
        Real radius = sqrt(radius2);
        Real denominator = radius2 + a2;
        h = (mass / radius) /
            (Real(1.0) + (spin * z / radius2) * (spin * z / radius2));

        vector<Real, 3> spatial = vector<Real, 3>(
            (radius * x + spin * y) / denominator,
            (radius * y - spin * x) / denominator,
            z / radius);
        nullCovector = vector<Real, 4>(Real(1.0), spatial);
        nullVector = vector<Real, 4>(Real(-1.0), spatial);
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
