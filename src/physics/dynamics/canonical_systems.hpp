#pragma once

#include "src/physics/canonical_engine.hpp"
#include "src/physics/chart.hpp"
#include "src/physics/dynamics/hamiltonian_system.hpp"

namespace gargantua::physics {

// Typed host adapters over flows generated from the same Slang modules used by
// Vulkan. Boost.Odeint supplies stepping policy only; it never re-derives the
// Hamilton equations.
class CanonicalKerrSchildSystem final
    : public HamiltonianSystem<KerrSchildCartesianChart> {
public:
  explicit CanonicalKerrSchildSystem(double mass = 1.0, double spin = 0.0);

  scalar_type hamiltonian(const State &state,
                          scalar_type affineParameter = 0.0) const override;
  Derivative derivative(const State &state,
                        scalar_type affineParameter = 0.0) const override;

private:
  canonical::KerrSchildEngine engine_;
};

class CanonicalQuarticDispersionSystem final
    : public HamiltonianSystem<CartesianChart> {
public:
  explicit CanonicalQuarticDispersionSystem(double coupling = 0.0);

  scalar_type hamiltonian(const State &state,
                          scalar_type affineParameter = 0.0) const override;
  Derivative derivative(const State &state,
                        scalar_type affineParameter = 0.0) const override;

private:
  canonical::QuarticDispersionEngine engine_;
};

} // namespace gargantua::physics
