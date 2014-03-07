#pragma once

#include <Eigen/Dense>

namespace oxlm {

typedef float Real;
typedef Eigen::Matrix<Real, Eigen::Dynamic, Eigen::Dynamic> MatrixReal;
typedef Eigen::Matrix<Real, Eigen::Dynamic, 1>              VectorReal;
typedef Eigen::Array<Real, Eigen::Dynamic, 1>               ArrayReal;

inline VectorReal softMax(const VectorReal& v) {
  Real max = v.maxCoeff();
  return (v.array() - (log((v.array() - max).exp().sum()) + max)).exp();
}

inline VectorReal logSoftMax(const VectorReal& v, Real* lz = NULL) {
  Real max = v.maxCoeff();
  Real log_z = log((v.array() - max).exp().sum()) + max;
  if (lz != 0) *lz = log_z;
  return v.array() - log_z;
}

inline VectorReal sigmoid(const VectorReal& v) {
  return (1.0 + (-v).array().exp()).inverse();
}

} // namespace oxlm
