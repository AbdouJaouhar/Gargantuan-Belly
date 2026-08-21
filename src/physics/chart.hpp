#pragma once

#include <array>
#include <string_view>

namespace gargantua::physics {

// Coordinate-chart tags are deliberately empty.  They make coordinates from
// different charts distinct C++ types without adding storage or run-time cost.
struct CartesianChart {
  static constexpr int dimension = 4;
};

struct SphericalChart {
  static constexpr int dimension = 4;
};

struct BoyerLindquistChart {
  static constexpr int dimension = 4;
};

// Horizon-penetrating Cartesian Kerr-Schild coordinates. Unlike spherical
// and Boyer-Lindquist charts, this chart has no rotation-axis singularity.
struct KerrSchildCartesianChart {
  static constexpr int dimension = 4;
};

template <typename Chart> struct ChartTraits;

template <> struct ChartTraits<CartesianChart> {
  static constexpr std::string_view name = "Cartesian";
  static constexpr std::array<std::string_view, 4> coordinates = {"t", "x", "y",
                                                                  "z"};
};

template <> struct ChartTraits<SphericalChart> {
  static constexpr std::string_view name = "Spherical";
  static constexpr std::array<std::string_view, 4> coordinates = {
      "t", "r", "theta", "phi"};
};

template <> struct ChartTraits<BoyerLindquistChart> {
  static constexpr std::string_view name = "Boyer-Lindquist";
  static constexpr std::array<std::string_view, 4> coordinates = {
      "t", "r", "theta", "phi"};
};

template <> struct ChartTraits<KerrSchildCartesianChart> {
  static constexpr std::string_view name = "Cartesian Kerr-Schild";
  static constexpr std::array<std::string_view, 4> coordinates = {"t", "x", "y",
                                                                  "z"};
};

} // namespace gargantua::physics
