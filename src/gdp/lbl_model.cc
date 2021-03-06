#include "gdp/lbl_model.h"

#include <iomanip>

#include <boost/make_shared.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/shared_ptr.hpp>

#include "lbl/factored_metadata.h"
#include "lbl/factored_weights.h"
#include "lbl/metadata.h"
#include "lbl/model_utils.h"
#include "lbl/operators.h"
#include "lbl/weights.h"
#include "utils/conditional_omp.h"

namespace oxlm {

template <class GlobalWeights, class MinibatchWeights, class Metadata>
LblModel<GlobalWeights, MinibatchWeights, Metadata>::LblModel() {
  dict = boost::make_shared<Dict>();
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
LblModel<GlobalWeights, MinibatchWeights, Metadata>::LblModel(
    const boost::shared_ptr<ModelConfig>& config)
    : config(config) {
  dict = boost::make_shared<Dict>();
  metadata = boost::make_shared<Metadata>(config, dict);
  ngram_model = boost::make_shared<NGramModel<GlobalWeights>>(
      config->ngram_order, dict->sos(), dict->eos());
  srand(1);
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
boost::shared_ptr<Dict>
LblModel<GlobalWeights, MinibatchWeights, Metadata>::getDict() const {
  return dict;
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
boost::shared_ptr<ModelConfig>
LblModel<GlobalWeights, MinibatchWeights, Metadata>::getConfig() const {
  return config;
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
MatrixReal LblModel<GlobalWeights, MinibatchWeights, Metadata>::getWordVectors()
    const {
  return weights->getWordVectors();
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
void LblModel<GlobalWeights, MinibatchWeights, Metadata>::learn() {
  MT19937 eng;
  // Initialize the dictionary now, if it hasn't been initialized when the
  // vocabulary was partitioned in classes.
  bool immutable_dict = config->classes > 0 || config->class_file.size();
  boost::shared_ptr<SentenceCorpus> training_corpus =
      boost::make_shared<SentenceCorpus>();
  training_corpus->readFile(config->training_file, dict, immutable_dict);
  config->vocab_size = dict->size();
  config->num_features = dict->size();
  std::cout << "Done reading training corpus..." << endl;

  boost::shared_ptr<SentenceCorpus> test_corpus;
  if (config->test_file.size()) {
    test_corpus = boost::make_shared<SentenceCorpus>();
    test_corpus->readFile(config->test_file, dict, true);
    std::cout << "Done reading test corpus..." << endl;
  }

  if (config->model_input_file.size() == 0) {
    metadata->initialize(training_corpus);
    weights = boost::make_shared<GlobalWeights>(config, metadata, true);
  } else {
    Real log_likelihood = 0;
    evaluate(test_corpus, log_likelihood);
    std::cerr << "Initial perplexity: "
              << perplexity(log_likelihood, test_corpus->numTokens()) << endl;
  }

  vector<int> indices(training_corpus->size());
  iota(indices.begin(), indices.end(), 0);

  Real best_perplexity = numeric_limits<Real>::infinity();
  Real global_objective = 0, test_objective = 0;
  boost::shared_ptr<MinibatchWeights> global_gradient =
      boost::make_shared<MinibatchWeights>(config, metadata, false);
  boost::shared_ptr<GlobalWeights> adagrad =
      boost::make_shared<GlobalWeights>(config, metadata, false);
  MinibatchWords global_words;

  int shared_index = 0;
  // For no particular reason. It just looks like this works best.
  int task_size = sqrt(config->minibatch_size) / 4;

  omp_set_num_threads(config->threads);
  #pragma omp parallel
  {
    int minibatch_counter = 1;
    int minibatch_size = config->minibatch_size;
    boost::shared_ptr<MinibatchWeights> gradient =
        boost::make_shared<MinibatchWeights>(config, metadata, false);

    for (int iter = 0; iter < config->iterations; ++iter) {
      auto iteration_start = get_time();

      #pragma omp master
      {
        if (config->randomise) {
          random_shuffle(indices.begin(), indices.end());
        }
        global_objective = 0;
      }

      #pragma omp barrier
      size_t start = 0;
      while (start < training_corpus->size()) {
        size_t end = min(training_corpus->size(), start + minibatch_size);

        vector<int> minibatch(indices.begin() + start,
                              min(indices.begin() + end, indices.end()));

        // Reset the set of minibatch words shared across all threads.
        #pragma omp master
        {
          global_words = MinibatchWords();
          shared_index = 0;
        }

        // Wait until the global gradient is initialized. Otherwise, some
        // gradient updates may be ignored.
        #pragma omp barrier

        Real objective = 0;
        int num_examples = 0;
        MinibatchWords words;
        size_t task_start;
        while (true) {

          #pragma omp critical
          {
            task_start = shared_index;
            shared_index += task_size;
          }

          if (task_start < minibatch.size()) {
            size_t task_end = min(task_start + task_size, minibatch.size());
            vector<int> task(minibatch.begin() + task_start,
                             minibatch.begin() + task_end);
            boost::shared_ptr<DataSet> task_examples =
                boost::make_shared<DataSet>();

            for (int j : task) {
              ngram_model->extractSentence(training_corpus->sentence_at(j),
                                           task_examples);
            }
            num_examples += task_examples->size();

            if (config->noise_samples > 0) {
              weights->estimateGradient(task_examples, gradient, objective,
                                        words);
            } else {
              weights->getGradient(task_examples, gradient, objective, words);
            }
          } else {
            break;
          }
        }

        global_gradient->syncUpdate(words, gradient);
        #pragma omp critical
        {
          global_objective += objective;
          global_words.merge(words);
        }

        // Wait until the global gradient is fully updated by all threads and
        // the global words are fully merged.
        #pragma omp barrier

        // Prepare minibatch words for parallel processing.
        #pragma omp master
        global_words.transform();

        // Wait until the minibatch words are fully prepared for parallel
        // processing.
        #pragma omp barrier
        update(global_words, global_gradient, adagrad);

        // Wait for all threads to finish making the model gradient update.
        #pragma omp barrier

        Real minibatch_factor =
            static_cast<Real>(num_examples) / training_corpus->numTokens();
        objective = regularize(global_words, global_gradient, minibatch_factor);
        
        #pragma omp critical
        global_objective += objective;

        // Clear gradients.
        gradient->clear(words, false);
        global_gradient->clear(global_words, true);

        // Wait the regularization update to finish and make sure the global
        // words are reset only after the global gradient is fully cleared.
        #pragma omp barrier

        // don't evaluate before end of iteration
        if ((minibatch_counter % 100000 == 0) && (iter == 0)) {
          evaluate(test_corpus, iteration_start, minibatch_counter,
                   test_objective, best_perplexity);
        } 

        ++minibatch_counter;
        start = end;
      }

      evaluate(test_corpus, iteration_start, minibatch_counter, test_objective,
               best_perplexity);
      #pragma omp master
      {
        Real iteration_time = get_duration(iteration_start, get_time());
        std::cerr << "Iteration: " << iter << ", "
                  << "Time: " << iteration_time << " seconds, "
                  << "  Likelihood: " << global_objective
                  << "  Size: " << training_corpus->numTokens()
                  << "  Perplexity with EOS: "
                  << perplexity(global_objective, training_corpus->numTokensS())
                  << "  Perplexity without EOS: "
                  << perplexity(global_objective, training_corpus->numTokens())
                  << "  Objective: "
                  << global_objective / training_corpus->numTokens() << endl;
        std::cerr << endl;
      }
    }
  }

  std::cerr << "Overall minimum perplexity: " << best_perplexity << endl;

  std::vector<boost::shared_ptr<Parser>> generated_list;
  for (int i = 0; i < config->generate_samples; ++i) {
    generated_list.push_back(boost::make_shared<Parser>(
        ngram_model->generateSentence(weights, eng)));
  }

  std::sort(generated_list.begin(), generated_list.end(), Parser::cmp_weights);
  for (int i = 0; i < config->generate_samples; ++i) {
    generated_list[i]->print_sentence(dict);
  }
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
void LblModel<GlobalWeights, MinibatchWeights, Metadata>::update(
    const MinibatchWords& global_words,
    const boost::shared_ptr<MinibatchWeights>& global_gradient,
    const boost::shared_ptr<GlobalWeights>& adagrad) {
  adagrad->updateSquared(global_words, global_gradient);
  weights->updateAdaGrad(global_words, global_gradient, adagrad);
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
Real LblModel<GlobalWeights, MinibatchWeights, Metadata>::regularize(
    const MinibatchWords& global_words,
    const boost::shared_ptr<MinibatchWeights>& global_gradient,
    Real minibatch_factor) {
  return weights->regularizerUpdate(global_words, global_gradient,
                                    minibatch_factor);
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
void LblModel<GlobalWeights, MinibatchWeights, Metadata>::evaluate(
    const boost::shared_ptr<SentenceCorpus>& test_corpus,
    Real& accumulator) const {
  if (test_corpus != nullptr) {
    #pragma omp master
    {
      std::cerr << "Calculating perplexity for " << test_corpus->numTokens()
                << " tokens..." << endl;
      accumulator = 0;
    }

    // Each thread must wait until the perplexity is set to 0.
    // Otherwise, partial results might get overwritten.
    #pragma omp barrier

    vector<int> indices(test_corpus->size());
    iota(indices.begin(), indices.end(), 0);
    size_t start = 0;
    while (start < test_corpus->size()) {
      size_t end = min(start + config->minibatch_size, test_corpus->size());
      vector<int> minibatch(indices.begin() + start,
                            min(indices.begin() + end, indices.end()));

      minibatch = scatterMinibatch(minibatch);
      Real objective = 0;

      for (int j : minibatch) {
        Real likelihood =
            ngram_model->evaluateSentence(test_corpus->sentence_at(j), weights);
        objective += likelihood;
      }

      #pragma omp critical
      accumulator += objective;

      start = end;
    }
    std::cerr << std::endl;

    // Wait for all the threads to compute the perplexity for their slice of
    // test data.
    #pragma omp barrier
    weights->clearCache();
  }
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
void LblModel<GlobalWeights, MinibatchWeights, Metadata>::evaluate(
    const boost::shared_ptr<SentenceCorpus>& test_corpus,
    const Time& iteration_start, int minibatch_counter, Real& log_likelihood,
    Real& best_perplexity) const {
  if (test_corpus != nullptr) {
    evaluate(test_corpus, log_likelihood);

    #pragma omp master
    {
      Real test_perplexity =
          perplexity(log_likelihood, test_corpus->numTokens());
      Real test_perplexity_s =
          perplexity(log_likelihood, test_corpus->numTokensS());
      Real iteration_time = get_duration(iteration_start, get_time());
      std::cerr << "\tMinibatch " << minibatch_counter << ", "
                << "Time: " << get_duration(iteration_start, get_time())
                << " seconds, "
                << "  Test Likelihood: " << log_likelihood
                << "  Test Size: " << test_corpus->numTokens()
                << "  Test Perplexity with EOS: " << test_perplexity_s
                << std::endl << "  Test Perplexity no EOS: " << test_perplexity
                << std::endl;

      if (test_perplexity < best_perplexity) {
        best_perplexity = test_perplexity;
        save();
      }
    }
  } else {
    #pragma omp master
    save();
  }
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
Real LblModel<GlobalWeights, MinibatchWeights, Metadata>::predict(
    int word_id, const vector<int>& context) const {
  return weights->predict(word_id, context);
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
void LblModel<GlobalWeights, MinibatchWeights, Metadata>::save() const {
  if (config->model_output_file.size()) {
    cout << "Writing model to " << config->model_output_file << "..." << endl;
    ofstream fout(config->model_output_file);
    boost::archive::binary_oarchive oar(fout);
    oar << config;
    oar << dict;
    oar << weights;
    oar << metadata;
    cout << "Done..." << endl;
  }
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
void LblModel<GlobalWeights, MinibatchWeights, Metadata>::load(
    const string& filename) {
  if (filename.size() > 0) {
    auto start_time = get_time();
    cerr << "Loading model from " << filename << "..." << endl;
    ifstream fin(filename);
    boost::archive::binary_iarchive iar(fin);
    iar >> config;
    iar >> dict;
    iar >> weights;
    iar >> metadata;
    cerr << "Reading model took " << get_duration(start_time, get_time())
         << " seconds..." << endl;
  }
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
void LblModel<GlobalWeights, MinibatchWeights, Metadata>::clearCache() {
  weights->clearCache();
}

template <class GlobalWeights, class MinibatchWeights, class Metadata>
bool LblModel<GlobalWeights, MinibatchWeights, Metadata>::operator==(
    const LblModel<GlobalWeights, MinibatchWeights, Metadata>& other) const {
  return *config == *other.config && *metadata == *other.metadata &&
         *weights == *other.weights;
}

template class LblModel<Weights, Weights, Metadata>;
template class LblModel<FactoredWeights, FactoredWeights, FactoredMetadata>;

}  // namespace oxlm

