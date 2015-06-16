#pragma once

#include <boost/shared_ptr.hpp>

#include "corpus/dict.h"
#include "corpus/parallel_sentence_corpus.h"
#include "corpus/data_set.h"
#include "corpus/model_config.h"

#include "lbl/factored_metadata.h"
#include "lbl/factored_weights.h"
#include "lbl/metadata.h"
#include "lbl/minibatch_words.h"
#include "lbl/model_utils.h"
#include "lbl/utils.h"
#include "lbl/weights.h"

#include "gdp/aligned_ngram_model.h"

namespace oxlm {

template<class GlobalWeights, class MinibatchWeights, class Metadata>
class LblGecModel {
 public:
  LblGecModel();

  LblGecModel(const boost::shared_ptr<ModelConfig>& config);

  boost::shared_ptr<Dict> getDict() const;

  boost::shared_ptr<ModelConfig> getConfig() const;

  void learn();

  void update(
      const MinibatchWords& global_words,
      const boost::shared_ptr<MinibatchWeights>& global_gradient,
      const boost::shared_ptr<GlobalWeights>& adagrad);

  Real regularize(
      const MinibatchWords& global_words,
      const boost::shared_ptr<MinibatchWeights>& global_gradient,
      Real minibatch_factor);

  void evaluate(
      const boost::shared_ptr<ParallelSentenceCorpus>& corpus, Real& accumulator) const;

  Real predict(int word_id, const vector<int>& context) const;

  MatrixReal getWordVectors() const;

  void save() const;

  void load(const string& filename);

  void clearCache();

  bool operator==(
      const LblGecModel<GlobalWeights, MinibatchWeights, Metadata>& other) const;

 private:
  void evaluate(
      const boost::shared_ptr<ParallelSentenceCorpus>& corpus, const Time& iteration_start,
      int minibatch_counter, Real& objective, Real& best_perplexity) const;

  boost::shared_ptr<ModelConfig> config;
  boost::shared_ptr<Dict> dict;
  boost::shared_ptr<Metadata> metadata;
  boost::shared_ptr<GlobalWeights> weights;
  boost::shared_ptr<AlignedNGramModel<GlobalWeights>> ngram_model;
};

} // namespace oxlm