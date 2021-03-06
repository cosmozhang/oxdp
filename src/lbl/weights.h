#pragma once

#include <mutex>
#include <vector>

#include <boost/make_shared.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/thread/tss.hpp>

#include "corpus/data_set.h"
#include "utils/random.h"

#include "lbl/context_cache.h"
#include "lbl/metadata.h"
#include "lbl/minibatch_words.h"
#include "lbl/utils.h"
#include "lbl/word_distributions.h"

namespace oxlm {

typedef Eigen::Map<MatrixReal> ContextTransformType;
typedef vector<ContextTransformType> ContextTransformsType;
typedef Eigen::Map<MatrixReal> WordVectorsType;
typedef Eigen::Map<VectorReal> WeightsType;

typedef boost::shared_ptr<mutex> Mutex;
typedef pair<size_t, size_t> Block;

// Implements a neural language model.
class Weights {
 public:
  Weights();

  Weights(const boost::shared_ptr<ModelConfig>& config,
          const boost::shared_ptr<Metadata>& metadata, bool init);

  Weights(const Weights& other);

  virtual size_t numParameters() const;

  void getGradient(const boost::shared_ptr<DataSet>& examples,
                   const boost::shared_ptr<Weights>& gradient, Real& objective,
                   MinibatchWords& words) const;

  virtual Real getObjective(const boost::shared_ptr<DataSet>& examples) const;

  bool checkGradient(const boost::shared_ptr<DataSet>& examples,
                     const boost::shared_ptr<Weights>& gradient, double eps);

  void estimateGradient(const boost::shared_ptr<DataSet>& examples,
                        const boost::shared_ptr<Weights>& gradient,
                        Real& objective, MinibatchWords& words) const;

  void syncUpdate(const MinibatchWords& words,
                  const boost::shared_ptr<Weights>& gradient);

  void updateSquared(const MinibatchWords& global_words,
                     const boost::shared_ptr<Weights>& global_gradient);

  void updateAdaGrad(const MinibatchWords& global_words,
                     const boost::shared_ptr<Weights>& global_gradient,
                     const boost::shared_ptr<Weights>& adagrad);

  Real regularizerUpdate(const MinibatchWords& global_words,
                         const boost::shared_ptr<Weights>& global_gradient,
                         Real minibatch_factor);

  void clear(const MinibatchWords& words, bool parallel_update);

  Real predict(int word, Context context) const;

  Reals predict(Context context) const;

  // Computes the unnormalised weights of only the most likely words.
  Reals predictViterbi(Context context) const;

  int vocabSize() const;

  void clearCache();

  MatrixReal getWordVectors() const;

  MatrixReal getFeatureVectors() const;

  bool operator==(const Weights& other) const;

  virtual ~Weights();

 protected:
  Real getObjective(const boost::shared_ptr<DataSet>& examples,
                    vector<WordsList>& contexts,
                    vector<MatrixReal>& context_vectors,
                    MatrixReal& prediction_vectors,
                    MatrixReal& word_probs) const;

  void getContextVectors(const boost::shared_ptr<DataSet>& examples,
                         vector<WordsList>& contexts,
                         vector<MatrixReal>& context_vectors) const;

  void setContextWords(const vector<WordsList>& contexts,
                       MinibatchWords& words) const;

  MatrixReal getPredictionVectors(
      size_t prediction_size, const vector<MatrixReal>& context_vectors) const;

  MatrixReal getContextProduct(int index, const MatrixReal& representations,
                               bool transpose = false) const;

  MatrixReal getProbabilities(const boost::shared_ptr<DataSet>& examples,
                              const MatrixReal& prediction_vectors) const;

  MatrixReal getWeightedRepresentations(
      const boost::shared_ptr<DataSet>& examples,
      const MatrixReal& prediction_vectors, const MatrixReal& word_probs) const;

  void getFullGradient(const boost::shared_ptr<DataSet>& examples,
                       const vector<WordsList>& contexts,
                       const vector<MatrixReal>& context_vectors,
                       const MatrixReal& prediction_vectors,
                       const MatrixReal& weighted_representations,
                       MatrixReal& word_probs,
                       const boost::shared_ptr<Weights>& gradient,
                       MinibatchWords& words) const;

  void getContextGradient(size_t prediction_size,
                          const vector<WordsList>& contexts,
                          const vector<MatrixReal>& context_vectors,
                          const MatrixReal& weighted_representations,
                          const boost::shared_ptr<Weights>& gradient) const;

  virtual vector<vector<int>> getNoiseWords(
      const boost::shared_ptr<DataSet>& examples) const;

  void estimateProjectionGradient(const boost::shared_ptr<DataSet>& examples,
                                  const MatrixReal& prediction_vectors,
                                  const boost::shared_ptr<Weights>& gradient,
                                  MatrixReal& weighted_representations,
                                  Real& objective, MinibatchWords& words) const;

  VectorReal getPredictionVector(const Context& context) const;

 private:
  void allocate();

  void setModelParameters();

  Block getBlock(int start, int size) const;

  friend class boost::serialization::access;

  template <class Archive>
  void save(Archive& ar, const unsigned int version) const {
    ar << config;
    ar << metadata;

    ar << size;
    ar << boost::serialization::make_array(data, size);
  }

  template <class Archive>
  void load(Archive& ar, const unsigned int version) {
    ar >> config;

    ar >> metadata;

    ar >> size;
    data = new Real[size];
    ar >> boost::serialization::make_array(data, size);

    setModelParameters();
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER();

 protected:
  boost::shared_ptr<ModelConfig> config;
  boost::shared_ptr<Metadata> metadata;

  ContextTransformsType C;
  WordVectorsType Q;
  WordVectorsType R;
  WeightsType B;
  WeightsType W;

  mutable ContextCache normalizerCache;

 private:
  int size;
  Real* data;
  vector<Mutex> mutexesC;
  vector<Mutex> mutexesQ;
  vector<Mutex> mutexesR;
  Mutex mutexB;

  mutable boost::thread_specific_ptr<WordDistributions> wordDists;
};

}  // namespace oxlm
