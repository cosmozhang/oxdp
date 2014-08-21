#pragma once

#include <vector>

#include <boost/serialization/serialization.hpp>

#include "lbl/feature_context.h"

using namespace std;

namespace oxlm {

class FeatureContextExtractor {
 public:
  virtual vector<int> getFeatureContextIds(
      const vector<int>& context) const = 0;

  virtual int getFeatureContextId(
      const FeatureContext& feature_context) const = 0;

  virtual ~FeatureContextExtractor();

 private:
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive& ar, const unsigned int version) {}
};

} // namespace oxlm
