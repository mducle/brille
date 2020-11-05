/* Copyright 2019 Greg Tucker
//
// This file is part of brille.
//
// brille is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// brille is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with brille. If not, see <https://www.gnu.org/licenses/>.            */
#include <vector>
#include <array>
#include <utility>
#include <mutex>
#include <cassert>
#include <functional>
#include <omp.h>
#include "array.hpp"
#include "array_latvec.hpp" // defines bArray
#include "phonon.hpp"
#include "permutation.hpp"
#include "permutation_table.hpp"
#include "approx.hpp"
#include "utilities.hpp"

#ifndef _INTERPOLATOR_HPP_
#define _INTERPOLATOR_HPP_

//template<class T> using CostFunction = std::function<typename CostTraits<T>::type(ind_t, T*, T*)>;
template<class T>
using CostFunction = std::function<double(brille::ind_t, const T*, const T*)>;

template<class T> struct is_complex {enum{value = false};};
template<class T> struct is_complex<std::complex<T>> {enum {value=true};};
// template<bool C, typename T> using enable_if_t = typename std::enable_if<C,T>::type;

enum class RotatesLike {
  Real, Reciprocal, Axial, Gamma
};

template<class T>
class Interpolator{
public:
  using ind_t = brille::ind_t;
  template<class Z> using element_t =std::array<Z,3>;
  using costfun_t = CostFunction<T>;
  //
  template<class R> using data_t = brille::Array<R>;
  using shape_t = typename data_t<T>::shape_t;
private:
  data_t<T> data_;      //!< The stored Array of points indexed like the holding-Object's vertices
  element_t<ind_t> _elements; //!< The number of each element type per point and per mode
  RotatesLike rotlike_;   //!< How the elements of `data_` rotate
  element_t<double> _costmult; //!< The relative (multiplicative) cost for differences in each element type
  costfun_t _scalarfun; //!< A function to calculate differences between the scalars at two stored points
  costfun_t _vectorfun; //!< A function to calculate differences between the vectors at two stored points
  //costfun_t _matrixfun; //!< A function to calculate the differences between matrices at two stored points
public:
  explicit Interpolator(size_t scf_type=0, size_t vcf_type=0)
  : data_(0,0), _elements({{0,0,0}}), rotlike_{RotatesLike::Real}, _costmult({{1,1,1}})
  {
    this->set_cost_info(scf_type, vcf_type);
  }
  Interpolator(costfun_t scf, costfun_t vcf)
  : data_(0,0), _elements({{0,0,0}}), rotlike_{RotatesLike::Real},
    _costmult({{1,1,1}}), _scalarfun(scf), _vectorfun(vcf)
  {}
  Interpolator(data_t<T>& d, element_t<ind_t> el, RotatesLike rl)
  : data_(d), _elements(el), rotlike_{rl}, _costmult({{1,1,1,}})
  {
    this->set_cost_info(0,0);
    this->check_elements();
  }
  Interpolator(data_t<T>& d, element_t<ind_t> el, RotatesLike rl, size_t csf, size_t cvf, element_t<double> wg)
  : data_(d), _elements(el), rotlike_{rl}, _costmult(wg)
  {
    this->set_cost_info(csf, cvf);
    this->check_elements();
  }
  //
  void setup_fake(const ind_t sz, const ind_t br){
    data_ = data_t<T>(sz, br);
    _elements = {1u,0u,0u};
  }
  //
  void set_cost_info(const int scf, const int vcf){
    switch (scf){
      default:
      this->_scalarfun = [](ind_t n, const T* i, const T* j){
        double s{0};
        for (ind_t z=0; z<n; ++z) s += brille::utils::magnitude(i[z]-j[z]);
        return s;
      };
    }
    switch (vcf){
      case 1:
      debug_update("selecting brille::utils::vector_distance");
      this->_vectorfun = [](ind_t n, const T* i, const T* j){
        return brille::utils::vector_distance(n, i, j);
      };
      break;
      case 2:
      debug_update("selecting 1-brille::utils::vector_product");
      this->_vectorfun = [](ind_t n, const T* i, const T* j){
        return 1-brille::utils::vector_product(n, i, j);
      };
      break;
      case 3:
      debug_update("selecting brille::utils::vector_angle");
      this->_vectorfun = [](ind_t n, const T* i, const T* j){
        return brille::utils::vector_angle(n, i, j);
      };
      break;
      case 4:
      debug_update("selecting brille::utils::hermitian_angle");
      this->_vectorfun = [](ind_t n, const T* i, const T* j){
         return brille::utils::hermitian_angle(n,i,j);
      };
      break;
      default:
      debug_update("selecting sin**2(brille::utils::hermitian_angle)");
      // this->_vectorfun = [](ind_t n, T* i, T* j){return std::abs(std::sin(brille::utils::hermitian_angle(n, i, j)));};
      this->_vectorfun = [](ind_t n, const T* i, const T* j){
        auto sin_theta_H = std::sin(brille::utils::hermitian_angle(n, i, j));
        return sin_theta_H*sin_theta_H;
      };
    }
  }
  void set_cost_info(const int scf, const int vcf, const element_t<double>& elcost){
    _costmult = elcost;
    this->set_cost_info(scf, vcf);
  }
  //
  ind_t size(void) const {return data_.size(0);}
  ind_t branches(void) const {
    /*
    The data Array is allowed to be anywhere from 1 to 5 dimensional.
    Possible shapes are:
      (P,)            - one branch with one scalar per point
      (P, X)          - one branch with X elements per point
      (P, B, Y)       - B branches with Y elements per branch per point
      (P, B, V, 3)    - B branches with V 3-vectors per branch per point
      (P, B, M, 3, 3) - B branches with M (3,3) matrices per branch per point
    In the two-dimensional case there is a potential ambiguity in that X counts
    both the number of branches and the number of elements.
      X = B*Y
    where Y is the sum of scalar, vector, and matrix elements per branch
      Y = S + 3*V + 9*M
    */
    ind_t nd = data_.ndim();
    ind_t b = nd>1 ? data_.size(1) : 1u;
    if (2 == nd){
      ind_t y = std::accumulate(_elements.begin(), _elements.end(), ind_t(0));
      // zero-elements is the special (initial) case → 1 scalar per branch
      if (y > 0) b /= y;
    }
    return b;
  }
  bool only_vector_or_matrix(void) const {
    // if V or M can not be deduced directly from the shape of data_
    // then this Interpolator represents mixed data
    // purely-scalar data is also classed as 'mixed' for our purposes
    ind_t nd = data_.ndim();
    if (5u == nd && 3u == data_.size(4) && 3u == data_.size(3)) return true;
    if (4u == nd && 3u == data_.size(3)) return true;
    if (nd < 4u) return false; // (P,), (P,X), (P,B,Y)
    std::string msg = "Interpolator can not handle a {";
    for (auto x: data_.shape()) msg += " " + std::to_string(x);
    msg += " } data array";
    throw std::runtime_error(msg);
  }
  const data_t<T>& data(void) const {return data_;}
  const element_t<ind_t>& elements(void) const {return _elements;}
  //
  template<class... Args>
  void interpolate_at(Args... args) const {
    if (this->only_vector_or_matrix()){
      if (data_.ndim() == 4u){
        this->interpolate_at_vec(args...);
      }
      else{
        this->interpolate_at_mat(args...);
      }
    } else {
      this->interpolate_at_mix(args...);
    }
  }
  //
  template<class R, class RotT>
  bool rotate_in_place(data_t<T>& x,
                       const LQVec<R>& q,
                       const RotT& rt,
                       const PointSymmetry& ps,
                       const std::vector<size_t>& r,
                       const std::vector<size_t>& invr,
                       const int nth) const {
    if (this->only_vector_or_matrix()){
      if (data_.ndim() == 4u) switch (rotlike_) {
        case RotatesLike::Real:       return this->rip_real_vec(x,ps,r,invr,nth);
        case RotatesLike::Axial:      return this->rip_axial_vec(x,ps,r,invr,nth);
        case RotatesLike::Reciprocal: return this->rip_recip_vec(x,ps,r,invr,nth);
        case RotatesLike::Gamma:      return this->rip_gamma_vec(x,q,rt,ps,r,invr,nth);
        default: throw std::runtime_error("Impossible RotatesLike value!");
      }
      else switch (rotlike_) {
        case RotatesLike::Real:       return this->rip_real_mat(x,ps,r,invr,nth);
        case RotatesLike::Axial:      return this->rip_axial_mat(x,ps,r,invr,nth);
        case RotatesLike::Reciprocal: return this->rip_recip_mat(x,ps,r,invr,nth);
        case RotatesLike::Gamma:      return this->rip_gamma_mat(x,q,rt,ps,r,invr,nth);
        default: throw std::runtime_error("Impossible RotatesLike value!");
      }
    } else {
      switch (rotlike_){
        case RotatesLike::Real:       return this->rip_real(x,ps,r,invr,nth);
        case RotatesLike::Axial:      return this->rip_axial(x,ps,r,invr,nth);
        case RotatesLike::Reciprocal: return this->rip_recip(x,ps,r,invr,nth);
        case RotatesLike::Gamma:      return this->rip_gamma(x,q,rt,ps,r,invr,nth);
        default: throw std::runtime_error("Impossible RotatesLike value!");
      }
    }
  }
  //
  RotatesLike rotateslike() const { return rotlike_; }
  RotatesLike rotateslike(const RotatesLike a) {
    rotlike_ = a;
    return rotlike_;
  }
  // Replace the data within this object.
  template<class I>
  void replace_data(
      const data_t<T>& nd,
      const std::array<I,3>& ne,
      const RotatesLike rl = RotatesLike::Real)
  {
    data_ = nd;
    rotlike_ = rl;
    // convert the elements datatype as necessary
    if (ne[1]%3)
      throw std::logic_error("Vectors must have 3N elements per branch");
    if (ne[2]%9)
      throw std::logic_error("Matrices must have 9N elements per branch");
    for (size_t i=0; i<3u; ++i) _elements[i] = static_cast<ind_t>(ne[i]);
    this->check_elements();
  }
  // Replace the data in this object without specifying the data shape or its elements
  // this variant is necessary since the template specialization above can not have a default value for the elements
  void replace_data(const data_t<T>& nd){
    return this->replace_data(nd, element_t<ind_t>({{0,0,0}}));
  }
  ind_t branch_span() const { return this->branch_span(_elements);}
  //
  std::string to_string() const {
    std::string str= "{ ";
    for (auto s: data_.shape()) str += std::to_string(s) + " ";
    str += "} data";
    auto b = this->branches();
    if (b){
      str += " with " + std::to_string(b) + " mode";
      if (b>1) str += "s";
    }
    auto n = std::count_if(_elements.begin(), _elements.end(), [](ind_t a){return a>0;});
    if (n){
      str += " of ";
      std::array<std::string,3> types{"scalar", "vector", "matrix"};
      for (size_t i=0; i<3u; ++i) if (_elements[i]) {
        str += std::to_string(_elements[i]) + " " + types[i];
        if (--n>1) str += ", ";
        if (1==n) str += " and ";
      }
      str += " element";
      if (this->branch_span()>1) str += "s";
    }
    return str;
  }

  template<class S>
  void add_cost(const ind_t i0, const ind_t i1, std::vector<S>& cost, const bool apa) const {
    if (this->only_vector_or_matrix()){
      if (data_.ndim() == 4u){
        this->add_cost_vec(i0,i1,cost,apa);
      }
      else{
        this->add_cost_mat(i0,i1,cost,apa);
      }
    } else {
      this->add_cost_mix(i0,i1,cost,apa);
    }
  }

  template<typename I>
  bool any_equal_modes(const I idx) const {
    ind_t i = static_cast<ind_t>(idx);
    if (this->only_vector_or_matrix())
      return this->any_equal_modes_(i, data_.size(2), data_.size(3)*(data_.ndim()>4u?9u:3u));
    return this->any_equal_modes_(i, this->branches(), this->branch_span());
  }
  size_t bytes_per_point() const {
    size_t n_elements = data_.numel()/data_.size(0);
    return n_elements * sizeof(T);
  }
private:
  void check_elements(void){
    // check the input for correctness
    ind_t x = this->branch_span(_elements);
    switch (data_.ndim()) {
      case 1u: // 1 scalar per branch per point
        if (0u == x) x = _elements[0] = 1u;
        if (x > 1u) throw std::runtime_error("1-D data must represent one scalar per point!") ;
        break;
      case 2u: // (P, B*Y)
        if (0u == x) x = _elements[0] = data_.size(1); // one branch with y scalars per point
        if (data_.size(1) % x)
          throw std::runtime_error("2-D data requires an integer number of branches!");
        break;
      case 3u: // (P, B, Y)
        if (0u == x) x = _elements[0] = data_.size(2);
        if (data_.size(2) != x)
          throw std::runtime_error("3-D data requires that the last dimension matches the specified number of elements!");
        break;
      case 4u: // (P, B, V, 3)
        if (3u != data_.size(3))
          throw std::runtime_error("4-D data can only be 3-vectors");
        if (0u == x) x = _elements[1] = data_.size(2)*3u;
        if (data_.size(2)*3u != x)
          throw std::runtime_error("4-D data requires that the last two dimensions match the specified number of vector elements!");
        break;
      case 5u: // (P, B, M, 3, 3)
        if (3u != data_.size(3) || 3u != data_.size(4))
          throw std::runtime_error("5-D data can only be matrices");
        if (0u == x) x = _elements[2] = data_.size(2)*9u;
        if (data_.size(2)*9u != x)
          throw std::runtime_error("5-D data requires the last three dimensions match the specified number of matrix elements!");
        break;
      default: // higher dimensions not (yet) supported
        throw std::runtime_error("Interpolator data is expected to be 1- to 5-D");
    }
  }
  bool any_equal_modes_(const ind_t idx, const ind_t b_, const ind_t s_) {
    auto xidx = data_.slice(idx); // (1,), (B,), (Y,), (B,Y), (B,V,3), or (B,M,3,3)
    // since we're probably only using this when the data is provided and
    // most eigenproblem solvers sort their output by eigenvalue magnitude it is
    // most-likely for mode i and mode i+1 to be equal.
    // ∴ search (i,j), (i+1,j+1), (i+2,j+2), ..., i ∈ (0,N], j ∈ (1,N]
    // for each j = i+1, i+2, i+3, ..., i+N-1
    if (b_ < 2) return false;
    for (ind_t offset=1; offset < b_; ++offset)
    for (ind_t i=0, j=offset; j < b_; ++i, ++j){
      auto xi = xidx.slice(i).contiguous_copy();
      auto xj = xidx.slice(j).contiguous_copy();
      if (brille::approx::vector(s_, xi.ptr(), xj.ptr())) return true;
    }
    // no matches
    return false;
  }
  template<typename I> ind_t branch_span(const std::array<I,3>& e) const {
    return static_cast<ind_t>(e[0])+static_cast<ind_t>(e[1])+static_cast<ind_t>(e[2]);
  }
  element_t<ind_t> count_scalars_vectors_matrices(void) const {
    element_t<ind_t> no{_elements[0], _elements[1]/3u, _elements[2]/9u};
    return no;
  }
  // the 'mixed' variants of the rotate_in_place implementations
  bool rip_real(data_t<T>&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  bool rip_recip(data_t<T>&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  bool rip_axial(data_t<T>&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  template<class R>
  bool rip_gamma_complex(data_t<T>&, const LQVec<R>&, const GammaTable&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  template<class R, class S=T>
  enable_if_t<is_complex<S>::value, bool>
  rip_gamma(data_t<T>& x, const LQVec<R>& q, const GammaTable& gt, const PointSymmetry& ps, const std::vector<size_t>& r, const std::vector<size_t>& ir, const int nth) const{
    return rip_gamma_complex(x, q, gt, ps, r, ir, nth);
  }
  template<class R, class S=T>
  enable_if_t<!is_complex<S>::value, bool>
  rip_gamma(data_t<T>&, const LQVec<R>&, const GammaTable&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const{
    throw std::runtime_error("RotatesLike == Gamma requires complex valued data!");
  }
  // the vector-only variants
  bool rip_real_vec(data_t<T>&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  bool rip_recip_vec(data_t<T>&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  bool rip_axial_vec(data_t<T>&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  template<class R>
  bool rip_gamma_vec_complex(data_t<T>&, const LQVec<R>&, const GammaTable&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  template<class R, class S=T>
  enable_if_t<is_complex<S>::value, bool>
  rip_gamma_vec(data_t<T>& x, const LQVec<R>& q, const GammaTable& gt, const PointSymmetry& ps, const std::vector<size_t>& r, const std::vector<size_t>& ir, const int nth) const{
    return rip_gamma_vec_complex(x, q, gt, ps, r, ir, nth);
  }
  template<class R, class S=T>
  enable_if_t<!is_complex<S>::value, bool>
  rip_gamma_vec(data_t<T>&, const LQVec<R>&, const GammaTable&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const{
    throw std::runtime_error("RotatesLike == Gamma requires complex valued data!");
  }
  // the matrix-only variants
  bool rip_real_mat(data_t<T>&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  bool rip_recip_mat(data_t<T>&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  bool rip_axial_mat(data_t<T>&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  template<class R>
  bool rip_gamma_mat_complex(data_t<T>&, const LQVec<R>&, const GammaTable&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const;
  template<class R, class S=T>
  enable_if_t<is_complex<S>::value, bool>
  rip_gamma_mat(data_t<T>& x, const LQVec<R>& q, const GammaTable& gt, const PointSymmetry& ps, const std::vector<size_t>& r, const std::vector<size_t>& ir, const int nth) const{
    return rip_gamma_mat_complex(x, q, gt, ps, r, ir, nth);
  }
  template<class R, class S=T>
  enable_if_t<!is_complex<S>::value, bool>
  rip_gamma_mat(data_t<T>&, const LQVec<R>&, const GammaTable&, const PointSymmetry&, const std::vector<size_t>&, const std::vector<size_t>&, const int) const{
    throw std::runtime_error("RotatesLike == Gamma requires complex valued data!");
  }
  // add_cost_*
  template<class S>
  void add_cost_mix(const ind_t, const ind_t, std::vector<S>&, bool) const;
  template<class S>
  void add_cost_vec(const ind_t, const ind_t, std::vector<S>&, bool) const;
  template<class S>
  void add_cost_mat(const ind_t, const ind_t, std::vector<S>&, bool) const;
  // interpolate_at_*
  void interpolate_at_mix(const std::vector<std::vector<ind_t>>&, const std::vector<ind_t>&, const std::vector<double>&, data_t<T>&, const ind_t, const bool) const;
  void interpolate_at_vec(const std::vector<std::vector<ind_t>>&, const std::vector<ind_t>&, const std::vector<double>&, data_t<T>&, const ind_t, const bool) const;
  void interpolate_at_mat(const std::vector<std::vector<ind_t>>&, const std::vector<ind_t>&, const std::vector<double>&, data_t<T>&, const ind_t, const bool) const;
  void interpolate_at_mix(const std::vector<std::vector<ind_t>>&, const std::vector<std::pair<ind_t,double>>&, data_t<T>&, const ind_t, const bool) const;
  void interpolate_at_vec(const std::vector<std::vector<ind_t>>&, const std::vector<std::pair<ind_t,double>>&, data_t<T>&, const ind_t, const bool) const;
  void interpolate_at_mat(const std::vector<std::vector<ind_t>>&, const std::vector<std::pair<ind_t,double>>&, data_t<T>&, const ind_t, const bool) const;
};

#include "interpolator_at.tpp"
#include "interpolator_axial.tpp"
#include "interpolator_cost.tpp"
#include "interpolator_gamma.tpp"
#include "interpolator_real.tpp"
#include "interpolator_recip.tpp"
#endif
