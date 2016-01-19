#ifndef THINKS_FASTMARCHINGMETHOD_HPP_INCLUDED
#define THINKS_FASTMARCHINGMETHOD_HPP_INCLUDED

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <numeric>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>


namespace thinks {
namespace fmm {
namespace detail {

//! Returns the product of the elements in array @a a.
//! Note: Not checking for overflow here!
template<std::size_t N> inline
std::size_t linearSize(std::array<std::size_t, N> const& a)
{
  using namespace std;
  return accumulate(begin(a), end(a), 1, multiplies<size_t>());
}


template<typename T> inline constexpr
T squared(T const x)
{
  return x * x;
}


template<typename T> inline constexpr
T inverseSquared(T const x)
{
  static_assert(std::is_floating_point<T>::value, "value must be floating point");
  return T(1) / squared(x);
}

template<typename T, std::size_t N> inline
std::array<T, N> inverseSquared(std::array<T, N> const& a)
{
  using namespace std;
  array<T, N> r;
  transform(begin(a), end(a), begin(r),
            [](T const x) { return inverseSquared(x); });
  return r;
}


template<typename T, std::size_t N> inline
T squaredMagnitude(std::array<T, N> const& v)
{
  auto sm = T(0);
  for (auto i = size_t{0}; i < N; ++i) {
    sm += v[i] * v[i];
  }
  return sm;
}


template<std::size_t N> inline
bool inside(
  std::array<std::int32_t, N> const& index,
  std::array<std::size_t, N> const& size)
{
  using namespace std;

  for (size_t i = 0; i < N; ++i) {
    // Cast is safe since we check that index[i] is greater than or
    // equal to zero first.
    if (!(0 <= index[i] && static_cast<size_t>(index[i]) < size[i])) {
      return false;
    }
  }
  return true;
}


template<std::size_t N> inline
void throwIfInvalidSize(std::array<std::size_t, N> const& size)
{
  using namespace std;

  if (find_if(begin(size), end(size),
              [](auto const x) { return x < size_t{1}; }) != end(size)) {
    throw runtime_error("invalid size");
  }
}


template<typename T, std::size_t N> inline
void throwIfInvalidSpacing(std::array<T, N> const& dx)
{
  using namespace std;

  if (find_if(begin(dx), end(dx),
              [](auto const x) { return x <= T(0); }) != end(dx)) {
    throw runtime_error("invalid spacing");
  }
}


template<typename T> inline
void throwIfInvalidSpeed(T const speed)
{
  if (speed <= T(0)) {
    throw std::runtime_error("invalid speed");
  }
}


template<typename U, typename V> inline
void throwIfSizeNotEqual(U const& u, V const& v)
{
  if (u.size() != v.size()) {
    throw std::runtime_error("size mismatch");
  }
}


template<typename U, typename V, typename W> inline
void throwIfSizeNotEqual(U const& u, V const& v, W const& w)
{
  throwIfSizeNotEqual<U, V>(u, v);
  throwIfSizeNotEqual<U, W>(u, w);
}


template<std::size_t N> inline
void throwIfInvalidIndex(
  std::vector<std::array<std::int32_t, N>> const& indices,
  std::array<std::size_t, N> const& size)
{
  for (auto const& index : indices) {
    if (!inside(index, size)) {
      throw std::runtime_error("invalid index");
    }
  }
}


template<typename T, typename U> inline
void throwIfInvalidDistance(std::vector<T> const& distances, U const unary_pred)
{
  for (auto const distance : distances) {
    if (!unary_pred(distance)) {
      throw std::runtime_error("invalid distance");
    }
  }
}


template<typename T, std::size_t N> inline
void throwIfInvalidNormal(std::vector<std::array<T, N>> const& normals)
{
  for (auto const& normal : normals) {
    if (squaredMagnitude(normal) < T(0.25)) {
      throw std::runtime_error("invalid normal");
    }
  }
}


template<typename T, std::size_t N>
class Grid
{
public:
  typedef T cell_type;
  typedef std::array<std::size_t, N> size_type;
  typedef std::array<std::int32_t, N> index_type;

  Grid(size_type const& size, T& cells)
    : size_(size)
    , cells_(&cells)
  {
    using namespace std;

    auto stride = size_t{1};
    for (auto i = size_t{1}; i < N; ++i) {
      stride *= size_[i - 1];
      strides_[i - 1] = stride;
    }
  }

  size_type size() const
  {
    return size_;
  }

  //! Returns a reference to the cell at @a index. No range checking!
  cell_type& cell(index_type const& index)
  {
    return cells_[linearIndex_(index)];
  }

  //! Returns a const reference to the cell at @a index. No range checking!
  cell_type const& cell(index_type const& index) const
  {
    return cells_[linearIndex_(index)];
  }

private:
  //! Returns a linear (scalar) index into an array representing an
  //! N-dimensional grid for integer coordinate @a index.
  //! Note that this function does not check for integer overflow!
  std::size_t linearIndex_(index_type const& index) const
  {
    using namespace std;

    // Cast is safe since we check that index[i] is greater than or
    // equal to zero first.
    assert(0 <= index[0] && static_cast<size_t>(index[0]) < size_[0]);
    size_t k = index[0];
    for (size_t i = 1; i < N; ++i) {
      assert(0 <= index[i] && static_cast<size_t>(index[i]) < size_[i]);
      k += index[i] * strides_[i - 1];
    }
    return k;
  }

  std::array<std::size_t, N> const size_;
  std::array<std::size_t, N - 1> strides_;
  cell_type* const cells_;
};


template<std::size_t N>
struct Neighborhood
{
  static std::array<std::array<std::int32_t, N>, 2 * N> offsets()
  {
    using namespace std;

    auto n = array<array<int32_t, N>, 2 * N>{};
    for (auto i = size_t{0}; i < N; ++i) {
      for (auto j = size_t{0}; j < N; ++j) {
        if (j == i) {
          n[2 * i + 0][j] = +1;
          n[2 * i + 1][j] = -1;
        }
        else {
          n[2 * i + 0][j] = 0;
          n[2 * i + 1][j] = 0;
        }
      }
    }
    return n;
  }
};


template<typename T, std::size_t N>
class NarrowBandStore
{
public:
  typedef T distance_type;
  typedef std::array<std::int32_t, N> index_type;
  typedef std::pair<distance_type, index_type> value_type;

  NarrowBandStore()
  {}

  bool empty() const
  {
    assert(values_.empty() == index_to_pos_.empty());
    return values_.empty();
  }

  // O(log N)
  value_type pop()
  {
    if (empty()) {
      throw std::runtime_error("cannot pop empty narrow band store");
    }

    // Grab the top of the heap and use as return value below.
    auto const value = *values_.begin();

    // Place value from leaf level on top.
    swap_(0, values_.size() - 1);
    index_to_pos_.erase(value.second); // ~O(1), depends on hashing.
    values_.pop_back(); // O(1)
    assert(values_.size() == index_to_pos_.size());

    // Sift the new top value downwards to restore heap constraints.
    if (!empty()) {
      sift_down_(0);
    }

    return value;
  }

  void insert(value_type const& value)
  {
    if (index_to_pos_.find(value.second) != index_to_pos_.end()) {
      throw std::runtime_error("index must be unique");
    }

    // Insert value at leaf level and sift it upwards.
    auto const pos = values_.size();
    values_.push_back(value);
    index_to_pos_.insert({value.second, pos});
    sift_up_(pos);
  }

  void increase_distance(index_type const& index,
                         distance_type const new_distance)
  {
    auto const pos_iter = index_to_pos_.find(index);
    if (pos_iter == index_to_pos_.end()) {
      throw std::runtime_error("index not found");
    }

    auto& value = values_[pos_iter->second];
    if (new_distance <= value.first) {
      throw std::runtime_error("new distance must be greater than existing distance");
    }

    value.first = new_distance;
    sift_down_(pos_iter->second);
  }

  void decrease_distance(index_type const& index,
                         distance_type const new_distance)
  {
    auto const pos_iter = index_to_pos_.find(index);
    if (pos_iter == index_to_pos_.end()) {
      throw std::runtime_error("index not found");
    }

    auto& value = values_[pos_iter->second];
    if (new_distance >= value.first) {
      throw std::runtime_error("new distance must be less than existing distance");
    }

    value.first = new_distance;
    sift_up_(pos_iter->second);
  }

private:
  typedef typename std::vector<value_type>::size_type size_type_;

  void sift_up_(size_type_ const pos)
  {
    assert(pos < values_.size());
    if (pos == 0) {
      return; // Reached the top of the heap.
    }

    // Swap "upwards" (i.e. with parent) while parent value is larger.
    auto const parent_pos = parent_pos_(pos);
    assert(parent_pos < values_.size());
    auto& pos_value = values_[pos].first;
    auto& parent_value = values_[parent_pos].first;
    if (pos_value < parent_value) {
      swap_(pos, parent_pos);
      sift_up_(parent_pos); // Recursive!
    }
  }

  void sift_down_(size_type_ const pos)
  {
    assert(pos < values_.size());
    auto const left_pos = left_child_pos_(pos);
    auto const right_pos = right_child_pos_(pos);
    auto const max_pos = values_.size() - 1;

    assert(left_pos < right_pos);
    if (left_pos > max_pos) {
      // Pos is a leaf since left child is outside array,
      // and right child is even further outside.
      return;
    }

    // Check distance values of left and right children.
    auto min_pos = pos;
    auto min_value = values_[min_pos].first;

    auto const left_value = values_[left_pos].first;
    if (left_value < min_value) {
      min_pos = left_pos;
      min_value = values_[min_pos].first;
    }

    if (right_pos <= max_pos) {
      auto const right_value = values_[right_pos].first;
      if (right_value < min_value) {
        min_pos = right_pos;
      }
    }

    // Swap with the child that has the smaller distance value,
    // if any of the child distance values is smaller than the current distance.
    if (min_pos != pos) {
      swap_(min_pos, pos);
      sift_down_(min_pos); // Recursive!
    }
  }

  static size_type_ parent_pos_(size_type_ const child_pos)
  {
    assert(child_pos > 0);
    return (child_pos - 1) / 2;
  }

  static size_type_ left_child_pos_(size_type_ const parent_pos)
  {
    return 2 * parent_pos + 1;
  }

  static size_type_ right_child_pos_(size_type_ const parent_pos)
  {
    return 2 * parent_pos + 2;
  }

  void swap_(size_type_ const pos0, size_type_ const pos1)
  {
    assert(pos0 < values_.size());
    assert(pos1 < values_.size());
    auto const iter0 = index_to_pos_.find(values_[pos0].second);
    auto const iter1 = index_to_pos_.find(values_[pos1].second);
    assert(iter0 != index_to_pos_.end());
    assert(iter1 != index_to_pos_.end());
    iter0->second = pos1;
    iter1->second = pos0;
    std::swap(values_[pos0], values_[pos1]);
  }

  template<typename V, typename H> static inline
  void hashCombine_(V const& v, H const& hasher, std::size_t& seed)
  {
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  struct hash_type_
  {
    typedef index_type argument_type;
    typedef size_t result_type;

    result_type operator()(argument_type const& a) const
    {
      using namespace std;

      typedef typename argument_type::value_type value_type;
      hash<value_type> hasher;
      size_t seed = 0;
      for (auto i = size_t{0}; i < N; ++i) {
        hashCombine_(a[i], hasher, seed);
      }
      return seed;
    }
  };

  struct equal_type_
  {
    typedef bool result_type;
    typedef index_type first_argument_type;
    typedef index_type second_argument_type;

    result_type operator()(first_argument_type const& lhs,
                           second_argument_type const& rhs) const
    {
      for (auto i = size_t{0}; i < N; ++i) {
        if (lhs[i] != rhs[i]) {
          return false;
        }
      }
      return true;
    }
  };

  std::vector<value_type> values_;
  std::unordered_map<index_type, size_type_,
                     hash_type_, equal_type_> index_to_pos_;
};


enum class CellState
{
  Far = 0,
  NarrowBand,
  Frozen
};


template <typename T, std::size_t N>
class EikonalSolver
{
public:
  EikonalSolver(
    std::array<T, N> const& dx,
    T const speed)
    : inv_dx_squared_(inverseSquared(dx))
    , inv_speed_squared_(inverseSquared(speed))
  {}

  T solve(
    std::array<std::int32_t, N> const& index,
    std::array<std::array<std::int32_t, N>, 2* N> const& neighbor_offsets,
    Grid<T, N> const& distance_grid,
    Grid<CellState, N> const& state_grid) const
  {
    using namespace std;

    assert(inside(index, distance_grid.size()));

    auto q = array<T, 3>{{-inv_speed_squared_, T(0), T(0)}};

    for (auto i = size_t{0}; i < N; ++i) {
      auto min_frozen_neighbor_distance = numeric_limits<T>::max();
      for (auto j = size_t{0}; j < 2; ++j) {
        auto neighbor_index = index;
        for (auto k = size_t{0}; k < N; ++k) {
          neighbor_index[k] += neighbor_offsets[2 * i + j][k];
        }

        if (inside(neighbor_index, distance_grid.size()) &&
            state_grid.cell(neighbor_index) == CellState::Frozen) {
          //assert(distance_grid.cell(neighbor_index) <= distance_grid.cell(index));
          min_frozen_neighbor_distance = min(
            min_frozen_neighbor_distance,
            distance_grid.cell(neighbor_index));
        }
      }

      if (min_frozen_neighbor_distance < numeric_limits<T>::max()) {
        q[0] += squared(min_frozen_neighbor_distance) * inv_dx_squared_[i];
        q[1] += T(-2) * min_frozen_neighbor_distance * inv_dx_squared_[i];
        q[2] += inv_dx_squared_[i];
      }
    }

    auto const r = solveQuadratic_(q);
    assert(!isnan(r.first));
    assert(r.first >= T(0));
    return r.first;
  }

private:
  //! Polynomial coefficients are equivalent to array index,
  //! i.e. Sum(coefficients[i] * x^i) = 0, for i in [0, 2]
  //!
  //! Returns the real roots of the quadratic if any exist, otherwise NaN.
  //! If there are two roots the larger one is the first of the pair.
  template<typename T> static inline
  std::pair<T, T> solveQuadratic_(std::array<T, 3> const& coefficients)
  {
    using namespace std;

    static_assert(is_floating_point<T>::value,
                  "quadratic coefficients must be floating point");

    T const eps = T(1e-9);

    T const c = coefficients[0];
    T const b = coefficients[1];
    T const a = coefficients[2];

    if (fabs(a) < eps) {
      if (fabs(b) < eps) {
        // c = 0, no solutions (or infinite solutions if c happens to be zero).
        return make_pair(numeric_limits<T>::quiet_NaN(),
                         numeric_limits<T>::quiet_NaN());
      }
      // bx + c = 0, one solution.
      return make_pair(-c / b, numeric_limits<T>::quiet_NaN());
    }

    if (fabs(b) < eps) {
      // ax^2 + c = 0
      T const r = std::sqrt(-c / a);
      return make_pair(r, -r);
    }

    T const discriminant_squared = b * b - 4 * a * c;
    if (discriminant_squared <= eps) {
      // Complex solution.
      return make_pair(numeric_limits<T>::quiet_NaN(),
                       numeric_limits<T>::quiet_NaN());
    }
    T const discriminant = std::sqrt(discriminant_squared);

    T const r0 = (b < T(0)) ?
      (-b + discriminant) / (2 * a) : // b < 0
      (-b - discriminant) / (2 * a);  // b > 0
    T const r1 = c / (a * r0);
    return make_pair(max(r0, r1), min(r0, r1));
  }

  std::array<T, N> const inv_dx_squared_;
  T const inv_speed_squared_;
};


//! Set frozen cell state and distance.
template <typename T, std::size_t N> inline
void initializeFrozenCells(
  std::vector<std::array<std::int32_t, N>> const& frozen_indices,
  std::vector<T> const& frozen_distances,
  T const multiplier,
  Grid<T, N>* distance_grid,
  Grid<CellState, N>* state_grid)
{
  using namespace std;

  assert(frozen_indices.size() == frozen_distances.size());
  assert(distance_grid != nullptr);
  assert(state_grid != nullptr);

  for (auto i = size_t{0}; i < frozen_indices.size(); ++i) {
    assert(inside(frozen_indices[i], distance_grid->size()));
    assert(inside(frozen_indices[i], state_grid->size()));

    distance_grid->cell(frozen_indices[i]) = multiplier * frozen_distances[i];
    state_grid->cell(frozen_indices[i]) = CellState::Frozen;
  }
}


template <typename T, typename P, std::size_t N> inline
void updateNeighbors(
  EikonalSolver<T, N> const& eikonal_solver,
  std::array<std::int32_t, N> const& index,
  std::array<std::array<std::int32_t, N>, 2* N> const& neighbor_offsets,
  std::array<T, N> const& normal,
  P const neighbor_pred,
  Grid<T, N>* distance_grid,
  Grid<CellState, N>* state_grid,
  NarrowBandStore<T, N>* narrow_band)
{
  using namespace std;

  assert(distance_grid != nullptr);
  assert(state_grid != nullptr);
  //assert same size!!
  assert(narrow_band != nullptr);
  assert(inside(index, distance_grid->size()));
  assert(inside(index, state_grid->size()));
  assert(state_grid->cell(index) == CellState::Frozen);

  for (auto const& neighbor_offset : neighbor_offsets) {
    // Check if the neighbor predicate allows this offset direction.
    if (neighbor_pred(normal, neighbor_offset)) {
      auto neighbor_index = index;
      for (auto i = size_t{0}; i < N; ++i) {
        neighbor_index[i] += neighbor_offset[i];
      }

      if (inside(neighbor_index, distance_grid->size())) {
        // Update the narrow band.
        auto& neighbor_state = state_grid->cell(neighbor_index);
        switch (neighbor_state) {
        case CellState::Far:
          {
            auto const distance = eikonal_solver.solve(
              neighbor_index,
              neighbor_offsets,
              *distance_grid,
              *state_grid);
            distance_grid->cell(neighbor_index) = distance;
            neighbor_state = CellState::NarrowBand;
            narrow_band->insert(
              {
                distance,
                neighbor_index
              });
          }
          break;
        case CellState::NarrowBand:
          {
            auto& neighbor_distance = distance_grid->cell(neighbor_index);
            auto const new_neighbor_distance = eikonal_solver.solve(
              neighbor_index,
              neighbor_offsets,
              *distance_grid,
              *state_grid);
            if (new_neighbor_distance < neighbor_distance) {
              narrow_band->decrease_distance(neighbor_index, new_neighbor_distance);
              neighbor_distance = new_neighbor_distance;
            }
          }
          break;
        // If neighbor state is frozen do nothing!
        }
      }
    }
  }
}


template <typename T, std::size_t N, typename P> inline
std::unique_ptr<NarrowBandStore<T, N>> initializeNarrowBand(
  EikonalSolver<T, N> const& eikonal_solver,
  std::vector<std::array<std::int32_t, N>> const& frozen_indices,
  std::array<std::array<std::int32_t, N>, 2* N> const& neighbor_offsets,
  std::vector<std::array<T, N>> const& normals,
  P const& pred,
  Grid<T, N>* distance_grid,
  Grid<CellState, N>* state_grid)
{
  using namespace std;

  auto narrow_band = make_unique<NarrowBandStore<T, N>>();

  // Initialize the narrow band cells.
  for (auto i = size_t{0}; i < frozen_indices.size(); ++i) {
    updateNeighbors(
      eikonal_solver,
      frozen_indices[i],
      neighbor_offsets,
      normals[i],
      pred,
      distance_grid,
      state_grid,
      narrow_band.get());
  }

  if (narrow_band->empty()) {
    throw runtime_error("narrow band cannot be empty after initialization");
  }

  return move(narrow_band);
}


template <typename T, std::size_t N> inline
void marchNarrowBand(
  EikonalSolver<T, N> const& eikonal_solver,
  std::array<std::array<std::int32_t, N>, 2* N> const& neighbor_offsets,
  Grid<T, N>* distance_grid,
  Grid<CellState, N>* state_grid,
  NarrowBandStore<T, N>* narrow_band)
{
  using namespace std;

  assert(distance_grid != nullptr);
  assert(state_grid != nullptr);
  assert(narrow_band != nullptr);

  array<T, N> dummy_normal;
  fill(begin(dummy_normal), end(dummy_normal), numeric_limits<T>::quiet_NaN());

  while (!narrow_band->empty()) {
    // Take smallest distance from narrow band and freeze it.
    auto const value = narrow_band->pop();
    auto const distance = value.first;
    auto const index = value.second;

    assert(state_grid->cell(index) == CellState::NarrowBand);

    distance_grid->cell(index) = distance;
    state_grid->cell(index) = CellState::Frozen;

    updateNeighbors(
      eikonal_solver,
      index,
      neighbor_offsets,
      dummy_normal,
      [](auto const&, auto const&) {
        return true; // Always update all non-frozen neighbors while marching.
      },
      distance_grid,
      state_grid,
      narrow_band);
  }
}

} // namespace detail


template<typename T, std::size_t N> inline
std::vector<T> unsignedDistance(
  std::array<std::size_t, N> const& size,
  std::array<T, N> const& dx,
  T const speed,
  std::vector<std::array<std::int32_t, N>> const& frozen_indices,
  std::vector<T> const& frozen_distances,
  std::vector<std::array<T, N>> const& normals)
{
  using namespace std;
  using namespace detail;

  typedef T DistanceType;
  typedef EikonalSolver<DistanceType, N> EikonalSolverType;

  static_assert(is_floating_point<DistanceType>::value,
                "distance type must be floating point");
  static_assert(N > 0, "number of dimensions must be > 0");

  throwIfInvalidSize(size);
  throwIfInvalidSpacing(dx);
  throwIfInvalidSpeed(speed);
  throwIfSizeNotEqual(frozen_indices, frozen_distances, normals);
  throwIfInvalidIndex(frozen_indices, size);
  throwIfInvalidDistance(
    frozen_distances,
    [](auto const d) { return !isnan(d); });

  auto state_buffer = vector<CellState>(linearSize(size), CellState::Far);
  auto state_grid = Grid<CellState, N>(size, state_buffer.front());

  auto const neighbor_offsets = Neighborhood<N>::offsets();

  auto const eikonal_solver = EikonalSolverType(dx, speed);

  auto distance_buffer = vector<DistanceType>(
    linearSize(size), numeric_limits<DistanceType>::max());
  auto distance_grid = Grid<DistanceType, N>(size, distance_buffer.front());

  // Solve inside.
  initializeFrozenCells(
    frozen_indices,
    frozen_distances,
    DistanceType(-1),
    &distance_grid,
    &state_grid);

  auto inside_narrow_band = initializeNarrowBand<DistanceType, N>(
    eikonal_solver,
    frozen_indices,
    neighbor_offsets,
    normals,
    [](auto const& normal, auto const& neighbor_offset) {
      auto sum = DistanceType(0);
      for (auto i = size_t{0}; i < N; ++i) {
        // Flip normal.
        sum += (DistanceType(-1) * normal[i]) * neighbor_offset[i];
      }
      return sum >= DistanceType(0);
    },
    &distance_grid,
    &state_grid);
  marchNarrowBand(
    eikonal_solver,
    neighbor_offsets,
    &distance_grid,
    &state_grid,
    inside_narrow_band.get());

  // Solve outside.
  initializeFrozenCells(
    frozen_indices,
    frozen_distances,
    DistanceType(1),
    &distance_grid,
    &state_grid);
  auto outside_narrow_band = initializeNarrowBand<DistanceType, N>(
    eikonal_solver,
    frozen_indices,
    neighbor_offsets,
    normals,
    [](auto const& normal, auto const& neighbor_offset) {
      auto sum = DistanceType(0);
      for (auto i = size_t{0}; i < N; ++i) {
        sum += normal[i] * neighbor_offset[i];
      }
      return sum >= DistanceType(0);
    },
    &distance_grid,
    &state_grid);
  marchNarrowBand(
    eikonal_solver,
    neighbor_offsets,
    &distance_grid,
    &state_grid,
    outside_narrow_band.get());

  // Set unsigned frozen cell values.
  for (auto i = size_t{0}; i < frozen_indices.size(); ++i) {
    distance_grid.cell(frozen_indices[i]) = fabs(frozen_distances[i]);
  }

  return distance_buffer;
}


template<typename T, std::size_t N>
std::vector<T> signedDistance(
  std::array<std::size_t, N> const& size,
  std::array<T, N> const& dx,
  T const speed,
  std::vector<std::array<std::int32_t, N>> const& frozen_indices,
  std::vector<T> const& frozen_distances,
  std::vector<std::array<T, N>> const& normals)
{
  using namespace std;
  using namespace detail;

  typedef T DistanceType;
  typedef EikonalSolver<DistanceType, N> EikonalSolverType;

  static_assert(is_floating_point<DistanceType>::value,
                "distance type must be floating point");
  static_assert(N > 0, "number of dimensions must be > 0");

  throwIfInvalidSize(size);
  throwIfInvalidSpacing(dx);
  throwIfInvalidSpeed(speed);
  throwIfSizeNotEqual(frozen_indices, frozen_distances, normals);
  throwIfInvalidIndex(frozen_indices, size);
  throwIfInvalidDistance(
    frozen_distances,
    [](auto const d) { return !isnan(d); });
  throwIfInvalidNormal(normals);

  auto state_buffer = vector<CellState>(linearSize(size), CellState::Far);
  auto state_grid = Grid<CellState, N>(size, state_buffer.front());

  auto const neighbor_offsets = Neighborhood<N>::offsets();

  auto eikonal_solver = EikonalSolverType(dx, speed);

  // Solve inside.
  auto inside_distance_buffer = vector<DistanceType>(
    linearSize(size), numeric_limits<DistanceType>::max());
  auto inside_distance_grid = Grid<DistanceType, N>(
    size, inside_distance_buffer.front());

  initializeFrozenCells(
    frozen_indices,
    frozen_distances,
    DistanceType(-1), // multiplier
    &inside_distance_grid,
    &state_grid);

  auto inside_narrow_band = initializeNarrowBand<DistanceType, N>(
    eikonal_solver,
    frozen_indices,
    neighbor_offsets,
    normals,
    [](auto const& normal, auto const& neighbor_offset) {
      auto sum = DistanceType(0);
      for (auto i = size_t{0}; i < N; ++i) {
        // Flip normal.
        sum += (DistanceType(-1) * normal[i]) * neighbor_offset[i];
      }
      return sum >= DistanceType(0);
    },
    &inside_distance_grid,
    &state_grid);
  marchNarrowBand(
    eikonal_solver,
    neighbor_offsets,
    &inside_distance_grid,
    &state_grid,
    inside_narrow_band.get());

  // Solve outside.
  auto outside_distance_buffer = vector<DistanceType>(
    linearSize(size), numeric_limits<DistanceType>::max());
  auto outside_distance_grid = Grid<DistanceType, N>(
    size, outside_distance_buffer.front());

  initializeFrozenCells(
    frozen_indices,
    frozen_distances,
    DistanceType(1), // multiplier
    &outside_distance_grid,
    &state_grid);

  auto outside_narrow_band = initializeNarrowBand<DistanceType, N>(
    eikonal_solver,
    frozen_indices,
    neighbor_offsets,
    normals,
    [](auto const& normal, auto const& neighbor_offset) {
      auto sum = DistanceType(0);
      for (auto i = size_t{0}; i < N; ++i) {
        sum += normal[i] * neighbor_offset[i];
      }
      return sum >= DistanceType(0);
    },
    &outside_distance_grid,
    &state_grid);
  marchNarrowBand(
    eikonal_solver,
    neighbor_offsets,
    &outside_distance_grid,
    &state_grid,
    outside_narrow_band.get());

  auto distance_buffer = vector<DistanceType>(
    linearSize(size), numeric_limits<DistanceType>::max());

  for (auto i = size_t{0}; i < inside_distance_buffer.size(); ++i) {
    if (inside_distance_buffer[i] < numeric_limits<DistanceType>::max()) {
      // Negative inside.
      distance_buffer[i] = DistanceType(-1) * inside_distance_buffer[i];
    }
  }
  for (auto i = size_t{0}; i < outside_distance_buffer.size(); ++i) {
    if (outside_distance_buffer[i] < numeric_limits<DistanceType>::max()) {
      // Positive outside.
      distance_buffer[i] = outside_distance_buffer[i];
    }
  }

  auto distance_grid = Grid<DistanceType, N>(size, distance_buffer.front());
  for (auto i = size_t{0}; i < frozen_indices.size(); ++i) {
    distance_grid.cell(frozen_indices[i]) = frozen_distances[i];
  }

  return distance_buffer;
}

} // namespace fmm
} // namespace thinks

#endif // THINKS_FASTMARCHINGMETHOD_HPP_INCLUDED