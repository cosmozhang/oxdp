#pragma once

#include <boost/shared_ptr.hpp>

#include "lbl/metadata.h"
#include "lbl/utils.h"
#include "lbl/word_to_class_index.h"
#include "utils/serialization_helpers.h"

namespace oxlm {

// Metadata for FactoredWeights.  
class FactoredMetadata : public Metadata {
 public:
  FactoredMetadata();

  FactoredMetadata(const boost::shared_ptr<ModelConfig>& config,
                   boost::shared_ptr<Dict>& dict);

  FactoredMetadata(const boost::shared_ptr<ModelConfig>& config,
                   boost::shared_ptr<Dict>& dict,
                   const boost::shared_ptr<WordToClassIndex>& index);

  void initialize(const boost::shared_ptr<CorpusInterface>& corpus);

  boost::shared_ptr<WordToClassIndex> getIndex() const;

  VectorReal getClassBias() const;

  bool operator==(const FactoredMetadata& other) const;

 private:
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& boost::serialization::base_object<Metadata>(*this);

    ar& classBias;
    ar& index;
  }

 protected:
  VectorReal classBias;
  boost::shared_ptr<WordToClassIndex> index;
};

}  // namespace oxlm
