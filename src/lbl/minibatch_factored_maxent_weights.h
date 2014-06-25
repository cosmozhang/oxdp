#pragma once

#include "lbl/factored_maxent_metadata.h"
#include "lbl/factored_weights.h"
#include "lbl/minibatch_feature_store.h"

namespace oxlm {

class MinibatchFactoredMaxentWeights : public FactoredWeights {
 public:
  MinibatchFactoredMaxentWeights(
      const ModelData& config,
      const boost::shared_ptr<FactoredMaxentMetadata>& metadata,
      const vector<int>& minibatch_indices);

  MinibatchFactoredMaxentWeights(
      const ModelData& config,
      const boost::shared_ptr<FactoredMaxentMetadata>& metadata,
      const vector<int>& minibatch_indices,
      const boost::shared_ptr<FactoredWeights>& base_gradient);

  MinibatchFactoredMaxentWeights(
      int num_classes, const boost::shared_ptr<FactoredWeights>& base_gradient);

  void update(
      const boost::shared_ptr<MinibatchFactoredMaxentWeights>& gradient);

 private:
  friend class GlobalFactoredMaxentWeights;

  void initialize(const vector<int>& minibatch_indices);

 protected:
  boost::shared_ptr<FactoredMaxentMetadata> metadata;

  boost::shared_ptr<MinibatchFeatureStore> U;
  vector<boost::shared_ptr<MinibatchFeatureStore>> V;
};

} // namespace oxlm
