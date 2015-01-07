#include "lbl/parsed_factored_weights.h"

#include <iomanip>

#include <boost/make_shared.hpp>

#include "lbl/operators.h"

namespace oxlm {

ParsedFactoredWeights::ParsedFactoredWeights()
    : data(NULL), K(0, 0, 0), L(0, 0), PW(0, 0) {}

ParsedFactoredWeights::ParsedFactoredWeights(
    const boost::shared_ptr<ModelConfig>& config,
    const boost::shared_ptr<ParsedFactoredMetadata>& metadata,
    bool init)
    : FactoredWeights(config, metadata, init), metadata(metadata),
      data(NULL), K(0, 0, 0), L(0, 0), PW(0, 0) { 
  allocate();
  //std::cout << config->num_actions << " actions" << std::endl;

  if (init) {
   // Initialize model weights randomly.
    mt19937 gen(1);
    normal_distribution<Real> gaussian(0, 0.1);
    for (int i = 0; i < size; ++i) {
      PW(i) = gaussian(gen);
    }

    L = metadata->getActionBias();
  } else {
    PW.setZero();
  }
}

ParsedFactoredWeights::ParsedFactoredWeights(const ParsedFactoredWeights& other)
    : FactoredWeights(other), metadata(other.metadata),
    data(NULL), K(0, 0, 0), L(0, 0), PW(0, 0) { 
  allocate();
  memcpy(data, other.data, size * sizeof(Real));  
}

size_t ParsedFactoredWeights::numParameters() const {
  return FactoredWeights::numParameters() + size;
}

void ParsedFactoredWeights::allocate() {
  //set vector sizes
  int num_actions = config->num_actions;
  int word_width = config->word_representation_size;
  int K_size = num_actions * word_width;
  int L_size = num_actions;

  size = K_size + L_size;
  std::cout << "parse factored weights allocating size " << size << std::endl;
  data = new Real[size]; 

  for (int i = 0; i < config->threads; ++i) {
    mutexes.push_back(boost::make_shared<mutex>());
  }

  setModelParameters();
}

void ParsedFactoredWeights::setModelParameters() {
  //new the model parameters
  
  int num_actions = config->num_actions;
  int word_width = config->word_representation_size;
  int K_size = num_actions * word_width;
  int L_size = num_actions;

  new (&PW) WeightsType(data, size);

  new (&K) WordVectorsType(data, word_width, num_actions);
  new (&L) WeightsType(data + K_size, L_size);
}

Real ParsedFactoredWeights::predictWord(int word, Words context) const {
  //std::cout << "context: " << std::endl;
  //std::cout << context.size() << std::endl;
  return FactoredWeights::predict(word, context);
}

Reals ParsedFactoredWeights::predictWord(Words context) const {
  return FactoredWeights::predict(context);
}

Real ParsedFactoredWeights::predictTag(int tag, Words context) const {
  return 0.0;
}
 
Reals ParsedFactoredWeights::predictTag(Words context) const {
  return Reals(numTags(), 0.0);
}
  
Real ParsedFactoredWeights::predictAction(WordId action, Words context) const {
  VectorReal prediction_vector = getPredictionVector(context);
  Real prob = 0;

  //TODO bug in cache
  //auto ret = actionNormalizerCache.get(context);
  //if (ret.second) {
  //  prob = (K.col(action).dot(prediction_vector) + L(action) - ret.first);
  //} else {  
    Real normalizer = 0;
    VectorReal action_probs = logSoftMax(
        K.transpose() * prediction_vector + L, normalizer);
    //actionNormalizerCache.set(context, normalizer);
    prob = action_probs(action);
  //}

  return -prob;
  //return -std::log(1.0/numActions());
}

Reals ParsedFactoredWeights::predictAction(Words context) const {
  VectorReal prediction_vector = getPredictionVector(context);
  Reals probs(numActions(), 0);

  Real normalizer = 0;
  VectorReal action_probs = logSoftMax(
      K.transpose() * prediction_vector + L, normalizer);
  //actionNormalizerCache.set(context, normalizer);
  for (int i = 0; i < numActions(); ++i) //this might slow things down
    probs[i] = -action_probs(i);  //check the sign
  
  return probs;
  //return Reals(numActions(), -std::log(1.0/numActions()));
}

int ParsedFactoredWeights::numWords() const {
  return FactoredWeights::vocabSize();
}

int ParsedFactoredWeights::numTags() const {
  return 1;
}

int ParsedFactoredWeights::numActions() const {
  return config->num_actions;
}
 
void ParsedFactoredWeights::getGradient(
    const boost::shared_ptr<ParseDataSet>& examples,
    const boost::shared_ptr<ParsedFactoredWeights>& gradient,
    Real& objective,
    MinibatchWords& words) const {
  vector<vector<int>> word_contexts;
  vector<vector<int>> action_contexts;
  vector<MatrixReal> word_context_vectors;
  vector<MatrixReal> action_context_vectors;
  MatrixReal word_prediction_vectors;
  MatrixReal action_prediction_vectors;
  MatrixReal class_probs;
  vector<VectorReal> word_probs;
  MatrixReal action_probs;
  objective += getObjective(examples, word_contexts, action_contexts, word_context_vectors, 
          action_context_vectors, word_prediction_vectors, action_prediction_vectors,
          class_probs, word_probs, action_probs); 

  setContextWords(word_contexts, words); 
  setContextWords(action_contexts, words); 

  MatrixReal word_weighted_representations = FactoredWeights::getWeightedRepresentations(
      examples->word_examples(), word_prediction_vectors, class_probs, word_probs);
  
  MatrixReal action_weighted_representations = getActionWeightedRepresentations(
      examples, action_prediction_vectors, action_probs);

  getFullGradient(
      examples, word_contexts, action_contexts, word_context_vectors, action_context_vectors,
      word_prediction_vectors, action_prediction_vectors, word_weighted_representations,
      action_weighted_representations, class_probs, word_probs, action_probs, gradient, words);
}

bool ParsedFactoredWeights::checkGradient(
    const boost::shared_ptr<ParseDataSet>& examples,
    const boost::shared_ptr<ParsedFactoredWeights>& gradient,
    double eps) {
if (!FactoredWeights::checkGradient(examples->word_examples(), gradient, eps)) {
    return false;
  }

  std::cout << size << std::endl;
  for (int i = 0; i < size; ++i) {
    PW(i) += eps;
    Real objective_plus = getObjective(examples);
    PW(i) -= eps;

    PW(i) -= eps;
    Real objective_minus = getObjective(examples);
    PW(i) += eps;

    double est_gradient = (objective_plus - objective_minus) / (2 * eps);
    if (fabs(gradient->PW(i) - est_gradient) > eps) {
      return false;
    }
  }

  return true;
}

void ParsedFactoredWeights::estimateGradient(
    const boost::shared_ptr<ParseDataSet>& examples,
    const boost::shared_ptr<ParsedFactoredWeights>& gradient,
    Real& objective,
    MinibatchWords& words) const {
  FactoredWeights::estimateGradient(examples->word_examples(), gradient, objective, words);
} 

Real ParsedFactoredWeights::getObjective(
    const boost::shared_ptr<ParseDataSet>& examples) const {
  vector<vector<int>> word_contexts;
  vector<vector<int>> action_contexts;
  vector<MatrixReal> word_context_vectors;
  vector<MatrixReal> action_context_vectors;
  MatrixReal word_prediction_vectors;
  MatrixReal action_prediction_vectors;
  MatrixReal class_probs;
  vector<VectorReal> word_probs;
  MatrixReal action_probs;
  return getObjective(
      examples, word_contexts, action_contexts, word_context_vectors, action_context_vectors, 
      word_prediction_vectors, action_prediction_vectors, class_probs, word_probs, action_probs);
}

Real ParsedFactoredWeights::getObjective(
    const boost::shared_ptr<ParseDataSet>& examples,
    vector<vector<int>>& word_contexts,
    vector<vector<int>>& action_contexts,
    vector<MatrixReal>& word_context_vectors,
    vector<MatrixReal>& action_context_vectors,
    MatrixReal& word_prediction_vectors,
    MatrixReal& action_prediction_vectors,
    MatrixReal& class_probs,
    vector<VectorReal>& word_probs,
    MatrixReal& action_probs) const {
  //it is somewhat double work to compute these twice, but else need a more complicated example 
  //representation
    //std::cout << "preparing to get compute objective" << std::endl;
  getContextVectors(examples->word_examples(), word_contexts, word_context_vectors);
  getContextVectors(examples->action_examples(), action_contexts, action_context_vectors);
  word_prediction_vectors = getPredictionVectors(examples->word_example_size(), word_context_vectors); 
  action_prediction_vectors = getPredictionVectors(examples->action_example_size(), 
                                                   action_context_vectors); 
  getProbabilities(
      examples, word_contexts, action_contexts, word_prediction_vectors, action_prediction_vectors,
      class_probs, word_probs, action_probs);

  //std::cout << "computing objective" << std::endl;
  Real objective = 0;
  for (size_t i = 0; i < examples->word_example_size(); ++i) {
    int word_id = examples->word_at(i);
    int class_id = index->getClass(word_id);
    int word_class_id = index->getWordIndexInClass(word_id);

    objective -= class_probs(class_id, i);
    objective -= word_probs[i](word_class_id);
  }

  for (size_t i = 0; i < examples->action_example_size(); ++i) {
    int action_id = examples->action_at(i);
    objective -= action_probs(action_id, i);
  } 

  //convert out of log-space to probabilities
  for (size_t i = 0; i < examples->word_example_size(); ++i) {
    class_probs.col(i).array() = class_probs.col(i).array().exp();      
    word_probs[i].array() = word_probs[i].array().exp();
  }

  for (size_t i = 0; i < examples->action_example_size(); ++i) {
    action_probs.col(i).array() = action_probs.col(i).array().exp();      
  }
      
  return objective;
}

void ParsedFactoredWeights::getProbabilities(
    const boost::shared_ptr<ParseDataSet>& examples,
    const vector<vector<int>>& word_contexts,
    const vector<vector<int>>& action_contexts,
    const MatrixReal& word_prediction_vectors,
    const MatrixReal& action_prediction_vectors,
    MatrixReal& class_probs,
    vector<VectorReal>& word_probs,
    MatrixReal& action_probs) const {
  FactoredWeights::getProbabilities(examples->word_examples(), word_contexts, word_prediction_vectors, class_probs, word_probs);
  
  action_probs = K.transpose() * action_prediction_vectors 
                + L * MatrixReal::Ones(1, examples->action_example_size());
  for (size_t i = 0; i < examples->action_example_size(); ++i) {
    action_probs.col(i) = logSoftMax(action_probs.col(i));
  }
}

MatrixReal ParsedFactoredWeights::getActionWeightedRepresentations(
    const boost::shared_ptr<ParseDataSet>& examples,
    const MatrixReal& action_prediction_vectors,
    const MatrixReal& action_probs) const {
  MatrixReal weighted_representations = K * action_probs;

  for (size_t i = 0; i < examples->action_example_size(); ++i) {
    int action_id = examples->action_at(i);
    weighted_representations.col(i) -= K.col(action_id);
  }

  if (config->sigmoid) {
    weighted_representations.array() *= sigmoidDerivative(action_prediction_vectors);
  }

  return weighted_representations;
}

//alternatively call base method first
void ParsedFactoredWeights::getFullGradient(
    const boost::shared_ptr<ParseDataSet>& examples,
    const vector<vector<int>>& word_contexts,
    const vector<vector<int>>& action_contexts,
    const vector<MatrixReal>& word_context_vectors,
    const vector<MatrixReal>& action_context_vectors,
    const MatrixReal& word_prediction_vectors,
    const MatrixReal& action_prediction_vectors,
    const MatrixReal& word_weighted_representations,
    const MatrixReal& action_weighted_representations,
    MatrixReal& class_probs,
    vector<VectorReal>& word_probs,
    MatrixReal& action_probs,
    const boost::shared_ptr<ParsedFactoredWeights>& gradient,
    MinibatchWords& words) const {
  for (size_t i = 0; i < examples->word_example_size(); ++i) {
    int word_id = examples->word_at(i); 
    int class_id = index->getClass(word_id);
    int word_class_id = index->getWordIndexInClass(word_id);
    class_probs(class_id, i) -= 1;
    word_probs[i](word_class_id) -= 1;
  }

  for (size_t i = 0; i < examples->action_example_size(); ++i) {
    int action_id = examples->action_at(i); 
    action_probs(action_id, i) -= 1;
  }

  gradient->S += word_prediction_vectors * class_probs.transpose();
  gradient->T += class_probs.rowwise().sum();
  for (size_t i = 0; i < examples->word_example_size(); ++i) {
    int word_id = examples->word_at(i); 
    int class_id = index->getClass(word_id);
    int class_start = index->getClassMarker(class_id);
    int class_size = index->getClassSize(class_id);

    for (int j = 0; j < class_size; ++j) {
      words.addOutputWord(class_start + j);
    }

    gradient->B.segment(class_start, class_size) += word_probs[i];
    gradient->R.block(0, class_start, gradient->R.rows(), class_size) +=
        word_prediction_vectors.col(i) * word_probs[i].transpose();
  }

  gradient->K += action_prediction_vectors * action_probs.transpose();
  gradient->L += action_probs.rowwise().sum();

  getContextGradient(
      examples->word_example_size(), word_contexts, word_context_vectors, word_weighted_representations, gradient);
  getContextGradient(
      examples->action_example_size(), action_contexts, action_context_vectors, action_weighted_representations, gradient);
}

std::vector<Words> ParsedFactoredWeights::getNoiseWords(
    const boost::shared_ptr<ParseDataSet>& examples) const {
  FactoredWeights::getNoiseWords(examples->word_examples());
} 

void ParsedFactoredWeights::estimateProjectionGradient(
    const boost::shared_ptr<ParseDataSet>& examples,
    const MatrixReal& prediction_vectors,
    const boost::shared_ptr<ParsedFactoredWeights>& gradient,
    MatrixReal& weighted_representations,
    Real& objective,
    MinibatchWords& words) const {
  FactoredWeights::estimateProjectionGradient(examples->word_examples(), prediction_vectors, gradient, weighted_representations, objective, words);
} 

void ParsedFactoredWeights::syncUpdate(
    const MinibatchWords& words,
    const boost::shared_ptr<ParsedFactoredWeights>& gradient) {
  FactoredWeights::syncUpdate(words, gradient);

  size_t block_size = PW.size() / mutexes.size() + 1;
  size_t block_start = 0;
  for (size_t i = 0; i < mutexes.size(); ++i) {
    block_size = min(block_size, PW.size() - block_start);
    lock_guard<mutex> lock(*mutexes[i]);
    FW.segment(block_start, block_size) +=
        gradient->PW.segment(block_start, block_size);
    block_start += block_size;
  }
}

Block ParsedFactoredWeights::getBlock() const {
  int thread_id = omp_get_thread_num();
  size_t block_size = PW.size() / config->threads + 1;
  size_t block_start = thread_id * block_size;
  block_size = min(block_size, PW.size() - block_start);
  return make_pair(block_start, block_size);
}

void ParsedFactoredWeights::updateSquared(
    const MinibatchWords& global_words,
    const boost::shared_ptr<ParsedFactoredWeights>& global_gradient) {
  FactoredWeights::updateSquared(global_words, global_gradient);

  Block block = getBlock();
  PW.segment(block.first, block.second).array() +=
      global_gradient->PW.segment(block.first, block.second).array().square();
}

void ParsedFactoredWeights::updateAdaGrad(
    const MinibatchWords& global_words,
    const boost::shared_ptr<ParsedFactoredWeights>& global_gradient,
    const boost::shared_ptr<ParsedFactoredWeights>& adagrad) {
  FactoredWeights::updateAdaGrad(global_words, global_gradient, adagrad);

  Block block = getBlock();
  PW.segment(block.first, block.second) -=
      global_gradient->PW.segment(block.first, block.second).binaryExpr(
          adagrad->PW.segment(block.first, block.second),
          CwiseAdagradUpdateOp<Real>(config->step_size));
}

Real ParsedFactoredWeights::regularizerUpdate(
    const boost::shared_ptr<ParsedFactoredWeights>& global_gradient,
    Real minibatch_factor) {
  Real ret = FactoredWeights::regularizerUpdate(global_gradient, minibatch_factor);

  Block block = getBlock();
  Real sigma = minibatch_factor * config->step_size * config->l2_lbl;
  PW.segment(block.first, block.second) -=
      PW.segment(block.first, block.second) * sigma;

  Real squares = PW.segment(block.first, block.second).array().square().sum();
  ret += 0.5 * minibatch_factor * config->l2_lbl * squares;

  return ret;
}

void ParsedFactoredWeights::clear(const MinibatchWords& words, bool parallel_update) {
  FactoredWeights::clear(words, parallel_update);

  if (parallel_update) {
    Block block = getBlock();
    PW.segment(block.first, block.second).setZero();
  } else {
    PW.setZero();
  }
}

void ParsedFactoredWeights::clearCache() {
  FactoredWeights::clearCache();
  actionNormalizerCache.clear();
}

bool ParsedFactoredWeights::operator==(const ParsedFactoredWeights& other) const {
  return Weights::operator==(other)
      && *metadata == *other.metadata
      && size == other.size
      && PW == other.PW;
}

ParsedFactoredWeights::~ParsedFactoredWeights() {
  delete data;
}

} //namespace oxlm
