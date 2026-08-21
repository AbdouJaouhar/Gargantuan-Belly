import CanonicalFlow;
import Hamiltonian;
import KerrCoordinates;
import KerrSchild;
import KerrSchildDynamics;
import PhysicsConfig;
import PhysicsTypes;

struct CameraParameters
{
    float radius;
    float inclinationDegrees;
    float verticalFovDegrees;
    float horizontalShift;
}

struct BlackHoleParameters
{
    float spin;
    float diskInnerRadius;
    float diskOuterRadius;
    float diskTemperatureKelvin;
}

struct RenderOptions
{
    float verticalShift;
    float frequencyShiftsEnabled;
    float cameraRollDegrees;
    float padding;
}

struct RenderParameters
{
    float2 resolution;
    float time;
    float exposure;
    CameraParameters camera;
    BlackHoleParameters blackHole;
    RenderOptions options;
}

[[vk::push_constant]] ConstantBuffer<RenderParameters> pc;

[[vk::constant_id(0)]]
const int kMaxGeodesicSteps = 360;
[[vk::constant_id(1)]]
const float kGeodesicStepScale = 1.0;

static const float kPi = 3.14159265358979323846;
static const float kTwoPi = 6.28318530717958647692;
// A permanently frozen texture winds into subpixel spirals under differential
// rotation. Renew its turbulent material before that accumulated shear becomes
// unresolved, then blend to the next deterministic material without a jump.
static const float kDiskMaterialLifetime = 8.0;
static const float kDiskMaterialFadeStart = 0.75;

float saturateValue(float value)
{
    return clamp(value, 0.0, 1.0);
}

float hash12(float2 p)
{
    float3 p3 = frac(float3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float valueNoise(float2 p)
{
    float2 integerPart = floor(p);
    float2 fractionalPart = frac(p);
    float2 curve = fractionalPart * fractionalPart *
        (3.0 - 2.0 * fractionalPart);
    float a = hash12(integerPart);
    float b = hash12(integerPart + float2(1.0, 0.0));
    float c = hash12(integerPart + float2(0.0, 1.0));
    float d = hash12(integerPart + float2(1.0, 1.0));
    return lerp(lerp(a, b, curve.x), lerp(c, d, curve.x), curve.y);
}

float fbm(float2 p)
{
    float sum = 0.0;
    float weight = 0.55;
    for (int octave = 0; octave < 4; ++octave)
    {
        sum += weight * valueNoise(p);
        p = float2(0.80 * p.x - 0.60 * p.y,
                   0.60 * p.x + 0.80 * p.y) * 2.03 + float2(7.1, 3.7);
        weight *= 0.48;
    }
    return sum;
}

float wrapDiskAngle(float angle)
{
    return angle - kTwoPi * floor(angle / kTwoPi + 0.5);
}

float3 diskMaterialSeed(float materialIndex)
{
    // Material zero intentionally has no offset, preserving the initial image.
    if (materialIndex == 0.0)
        return float3(0.0);

    float seed = hash12(float2(
        materialIndex,
        materialIndex * 0.754877666 + 19.19));
    return frac(seed * float3(1.0, 17.17, 43.43) +
                float3(0.0, 0.381966011, 0.754877666));
}

float sampleDiskTexture(
    float radius,
    float logRadius,
    float phi,
    float orbitalRate,
    float materialIndex,
    float localMaterialTime)
{
    float3 seed = diskMaterialSeed(materialIndex);
    float phaseAdvance =
        localMaterialTime * orbitalRate * 7.0 - kTwoPi * seed.x;
    float advectedPhi = phi - wrapDiskAngle(phaseAdvance);

    float2 flow = float2(
        logRadius * 17.0 + 1.15 * cos(3.0 * advectedPhi),
        1.15 * sin(3.0 * advectedPhi));
    flow += float2(31.0 * seed.y, 43.0 * seed.z);
    float broadStructure = fbm(flow);

    float2 fineFlow = float2(
        logRadius * 76.0 + 1.80 * cos(7.0 * advectedPhi),
        1.80 * sin(7.0 * advectedPhi));
    fineFlow += float2(71.0 * seed.z, 59.0 * seed.x);
    float fineStructure = fbm(fineFlow + float2(4.0, -2.0));

    float rings = 0.5 + 0.5 * sin(
        radius * 15.0 + broadStructure * 3.2 +
        0.55 * sin(3.0 * advectedPhi));
    float fineRings = 0.5 + 0.5 * sin(
        radius * 31.0 + fineStructure * 2.0);

    float2 gapFlow = float2(
        logRadius * 11.0 + 0.85 * cos(advectedPhi),
        0.85 * sin(advectedPhi));
    gapFlow += float2(41.0 * seed.x, 37.0 * seed.y);
    float gaps = smoothstep(0.28, 0.76, fbm(gapFlow + 9.0));

    float texture = 0.10 + 0.38 * broadStructure +
                    0.45 * fineStructure + 0.05 * rings +
                    0.02 * fineRings;
    return pow(saturateValue(texture), 1.20) * lerp(0.30, 1.0, gaps);
}

float3 blackbodyRgb(float temperatureKelvin)
{
    float temperature = clamp(temperatureKelvin, 1000.0, 40000.0) / 100.0;
    float3 srgb;
    if (temperature <= 66.0)
    {
        srgb.r = 1.0;
        srgb.g = saturateValue(
            (99.4708026 * log(temperature) - 161.119568) / 255.0);
    }
    else
    {
        srgb.r = saturateValue(
            329.698727 * pow(temperature - 60.0, -0.13320476) / 255.0);
        srgb.g = saturateValue(
            288.122170 * pow(temperature - 60.0, -0.07551485) / 255.0);
    }
    if (temperature >= 66.0)
        srgb.b = 1.0;
    else if (temperature <= 19.0)
        srgb.b = 0.0;
    else
        srgb.b = saturateValue(
            (138.517731 * log(temperature - 10.0) - 305.044793) / 255.0);
    return pow(max(srgb, float3(0.0)), float3(2.2));
}

float3 paperDiskPalette(float level)
{
    float x = saturateValue(level);
    float3 srgb;
    if (x < 0.12)
        srgb = lerp(float3(0.030, 0.020, 0.016),
                    float3(0.165, 0.094, 0.067), x / 0.12);
    else if (x < 0.28)
        srgb = lerp(float3(0.165, 0.094, 0.067),
                    float3(0.310, 0.176, 0.141), (x - 0.12) / 0.16);
    else if (x < 0.48)
        srgb = lerp(float3(0.310, 0.176, 0.141),
                    float3(0.584, 0.345, 0.259), (x - 0.28) / 0.20);
    else if (x < 0.67)
        srgb = lerp(float3(0.584, 0.345, 0.259),
                    float3(0.792, 0.616, 0.580), (x - 0.48) / 0.19);
    else if (x < 0.82)
        srgb = lerp(float3(0.792, 0.616, 0.580),
                    float3(0.850, 0.740, 0.740), (x - 0.67) / 0.15);
    else if (x < 0.94)
        srgb = lerp(float3(0.850, 0.740, 0.740),
                    float3(0.925, 0.865, 0.890), (x - 0.82) / 0.12);
    else
        srgb = lerp(float3(0.925, 0.865, 0.890),
                    float3(0.975, 0.920, 0.890), (x - 0.94) / 0.06);
    return pow(max(srgb, float3(0.0)), float3(2.2));
}

float diskFrequencyShift(
    float radius,
    float spin,
    float angularMomentum,
    float observerEnergy)
{
    float omegaOrbit = 1.0 / (pow(max(radius, 1.01), 1.5) + spin);
    float gTT = -(1.0 - 2.0 / radius);
    float gTPhi = -2.0 * spin / radius;
    float gPhiPhi = radius * radius + spin * spin +
        2.0 * spin * spin / radius;
    float normalization = -(gTT + 2.0 * gTPhi * omegaOrbit +
                            gPhiPhi * omegaOrbit * omegaOrbit);
    float emitterEnergy = rsqrt(max(normalization, 1.0e-5)) *
        (1.0 - omegaOrbit * angularMomentum);
    return clamp(observerEnergy / max(emitterEnergy, 1.0e-4), 0.18, 3.5);
}

float3 diskEmission(
    float radius,
    float phi,
    float spin,
    float angularMomentum,
    float observerEnergy,
    float edgeMask,
    out float materialDensity)
{
    float horizon = 1.0 + sqrt(max(1.0 - spin * spin, 0.0));
    float inner = max(pc.blackHole.diskInnerRadius, horizon * 1.05);
    float outer = max(pc.blackHole.diskOuterRadius, inner + 0.1);
    float radialCoordinate = (radius - inner) / max(outer - inner, 1.0e-4);
    float orbitalRate = 1.0 / (pow(max(radius, 1.0), 1.5) + spin);
    float logRadius = log(max(radius / inner, 1.001));

    float diskTime = max(pc.time, 0.0);
    float materialIndex = floor(diskTime / kDiskMaterialLifetime);
    float materialAge = clamp(
        diskTime - materialIndex * kDiskMaterialLifetime,
        0.0,
        kDiskMaterialLifetime);
    float materialFraction = materialAge / kDiskMaterialLifetime;
    float materialBlend = smoothstep(
        kDiskMaterialFadeStart, 1.0, materialFraction);
    float texture = sampleDiskTexture(
        radius,
        logRadius,
        phi,
        orbitalRate,
        materialIndex,
        materialAge);
    // This is uniform for the draw. During the first 75% of an epoch only one
    // material is evaluated. At the boundary the next sample has age zero and
    // becomes the following epoch's current sample, making the handoff exact.
    if (materialBlend > 0.0)
    {
        float nextTexture = sampleDiskTexture(
            radius,
            logRadius,
            phi,
            orbitalRate,
            materialIndex + 1.0,
            materialAge - kDiskMaterialLifetime);
        texture = lerp(texture, nextTexture, materialBlend);
    }

    float radialEnvelope = pow(saturateValue(1.0 - radialCoordinate), 0.58);
    float emissionLevel = smoothstep(
        0.02, 0.98, radialEnvelope * (0.34 + 1.35 * texture));
    materialDensity = (0.14 + 1.20 * pow(texture, 1.10)) *
        lerp(0.40, 1.0, radialEnvelope);

    float shift = 1.0;
    float intensityShift = 1.0;
    if (pc.options.frequencyShiftsEnabled > 0.5)
    {
        shift = diskFrequencyShift(
            radius, spin, angularMomentum, observerEnergy);
        intensityShift = pow(shift, 3.0);
    }

    float3 colour = paperDiskPalette(emissionLevel);
    if (pc.options.frequencyShiftsEnabled > 0.5)
    {
        float3 base = max(
            blackbodyRgb(pc.blackHole.diskTemperatureKelvin), float3(1.0e-4));
        float3 shifted = blackbodyRgb(
            pc.blackHole.diskTemperatureKelvin * shift);
        colour *= clamp(shifted / base, float3(0.20), float3(3.0));
    }
    return colour * edgeMask * intensityShift;
}

struct TraceResult
{
    float3 radiance;
    float transmittance;
    float escaped;
}

KerrCameraRay makeCameraRay(float2 pixel, float spin)
{
    float2 screen = pixel * 2.0 - 1.0;
    float aspect = pc.resolution.x / max(pc.resolution.y, 1.0);
    screen = float2(
        (screen.x - pc.camera.horizontalShift) * aspect,
        screen.y - pc.options.verticalShift);
    float roll = radians(pc.options.cameraRollDegrees);
    float sineRoll = sin(roll);
    float cosineRoll = cos(roll);
    screen = float2(
        cosineRoll * screen.x - sineRoll * screen.y,
        sineRoll * screen.x + cosineRoll * screen.y);
    float tangentHalfFov = tan(0.5 * radians(
        clamp(pc.camera.verticalFovDegrees, 5.0, 100.0)));
    float3 cameraDirection = normalize(float3(
        -1.0,
        screen.y * tangentHalfFov,
        screen.x * tangentHalfFov));
    float3 fidoDirection = -cameraDirection;
    float horizon = kerrOuterHorizon(1.0, spin);
    return makeStationaryFidoRay(
        max(pc.camera.radius, horizon * 1.025),
        radians(clamp(pc.camera.inclinationDegrees, 2.0, 178.0)),
        1.0,
        spin,
        fidoDirection);
}

TraceResult traceKerrRay(float2 pixel)
{
    float spin = clamp(pc.blackHole.spin, -0.998, 0.998);
    KerrCameraRay cameraRay = makeCameraRay(pixel, spin);
    PhaseSpaceState state = cameraRay.state;
    KerrSchildMetric metric;
    metric.mass = 1.0;
    metric.spin = spin;
    KerrSchildHamiltonianSystem system;
    system.metric = metric;

    float horizon = kerrOuterHorizon(1.0, spin);
    float inner = max(pc.blackHole.diskInnerRadius, horizon * 1.05);
    float outer = max(pc.blackHole.diskOuterRadius, inner + 0.1);
    float initialRadius = metric.radialCoordinate(state.position);
    float escapeRadius = max(initialRadius * 1.45, outer * 2.5);
    float previousRadius = initialRadius;

    TraceResult result;
    result.radiance = float3(0.0);
    result.transmittance = 1.0;
    result.escaped = 0.0;

    for (int stepIndex = 0; stepIndex < kMaxGeodesicSteps; ++stepIndex)
    {
        float radius = metric.radialCoordinate(state.position);
        if (radius <= horizon * 1.008 || isnan(radius) || isinf(radius))
            break;
        if (stepIndex > 8 && radius >= escapeRadius && radius > previousRadius)
        {
            result.escaped = 1.0;
            break;
        }

        float distanceToHorizon = radius - horizon;
        float stepMagnitude = clamp(
            0.038 * kGeodesicStepScale * radius,
            0.018 * kGeodesicStepScale,
            0.78 * kGeodesicStepScale);
        stepMagnitude = min(
            stepMagnitude, max(0.004, distanceToHorizon * 0.20));
        float affineStep = -stepMagnitude;
        PhaseSpaceState nextState =
            integrateCanonicalRk4(system, state, affineStep);

        float startHeight = state.position.components[3];
        float endHeight = nextState.position.components[3];
        float heightDelta = startHeight - endHeight;
        float segmentFraction = 0.5;
        if (abs(heightDelta) > 1.0e-6)
            segmentFraction = clamp(startHeight / heightDelta, 0.0, 1.0);

        PhaseSpaceState sampleState;
        sampleState.position.components = lerp(
            state.position.components,
            nextState.position.components,
            segmentFraction);
        sampleState.momentum.components = lerp(
            state.momentum.components,
            nextState.momentum.components,
            segmentFraction);
        float sampleRadius = metric.radialCoordinate(sampleState.position);
        if (sampleRadius > inner - 0.6 && sampleRadius < outer + 0.8)
        {
            float height = lerp(startHeight, endHeight, segmentFraction);
            float scaleHeight = 0.035 + 0.009 * sampleRadius;
            if (abs(height) < 4.5 * scaleHeight)
            {
                float verticalDensity = exp(
                    -pow(abs(height) / scaleHeight, 1.60));
                float innerEdge = smoothstep(
                    inner - 0.10, inner + 0.42, sampleRadius);
                float outerEdge = 1.0 - smoothstep(
                    outer - 1.0, outer + 0.18, sampleRadius);
                float edgeMask = innerEdge * outerEdge;
                float samplePhi = kerrSchildBoyerLindquistAzimuth(
                    sampleState.position, 1.0, spin, sampleRadius);
                float materialDensity;
                float3 emission = diskEmission(
                    sampleRadius,
                    samplePhi,
                    spin,
                    cameraRay.angularMomentum,
                    cameraRay.observerEnergy,
                    edgeMask,
                    materialDensity);
                float density = verticalDensity * edgeMask * materialDensity;
                float opacity = 1.0 - exp(-density * abs(affineStep) * 9.0);
                result.radiance += result.transmittance * opacity * emission;
                result.transmittance *= 1.0 - opacity;
                if (result.transmittance < 0.008)
                    break;
            }
        }

        previousRadius = radius;
        state = nextState;
    }

    float finalRadius = metric.radialCoordinate(state.position);
    if (result.escaped < 0.5 && finalRadius > escapeRadius * 0.92)
        result.escaped = 1.0;
    return result;
}

float3 acesFitted(float3 colour)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp(
        colour * (a * colour + b) /
        (colour * (c * colour + d) + e),
        0.0,
        1.0);
}

float3 filmicLuminance(float3 colour)
{
    float luminance = dot(colour, float3(0.2126, 0.7152, 0.0722));
    float mappedLuminance = acesFitted(float3(luminance)).x;
    return colour * (mappedLuminance / max(luminance, 1.0e-6));
}

struct FragmentInput
{
    [[vk::location(0)]] float2 uv : TEXCOORD0;
    float4 position : SV_Position;
}

[shader("fragment")]
float4 fragmentMain(FragmentInput input) : SV_Target0
{
    TraceResult trace = traceKerrRay(input.uv);
    float3 hdr = trace.radiance;
    if (trace.escaped > 0.5 && trace.transmittance > 0.0)
        hdr += trace.transmittance * float3(0.00018, 0.00028, 0.00034);
    float dither = hash12(input.position.xy + 0.01 * pc.time) - 0.5;
    float3 mapped = filmicLuminance(hdr * max(pc.exposure, 0.01));
    mapped += dither / 3294.0;
    return float4(clamp(mapped, 0.0, 1.0), 1.0);
}
