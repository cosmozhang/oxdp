#include <fstream>
#include <string>

#include <boost/program_options.hpp>

#include "gdp/lbl_model.h"
#include "gdp/lbl_dp_model.h"

using namespace boost::program_options;
using namespace oxlm;
using namespace std;

template <class Model, class Weights, class Metadata>
void extract_word_vectors(const string& model_file, const string& vocab_file,
                          const string& vectors_file) {
  // TODO Extend to include LblModel.
  LblDpModel<Model, Weights, Metadata> model;
  model.load(model_file);

  boost::shared_ptr<Dict> dict = model.getDict();
  MatrixReal word_vectors = model.getWordVectors();

  ofstream fout(vocab_file);
  for (size_t i = 0; i < word_vectors.cols(); ++i) {
    fout << dict->lookup(i) << endl;
  }

  ofstream vout(vectors_file);
  for (size_t i = 0; i < word_vectors.cols(); ++i) {
    vout << word_vectors.col(i).transpose() << endl;
  }
}

// Extracts word vectors from a model and write out to a file.
int main(int argc, char** argv) {
  options_description desc("Command line options");
  desc.add_options()("help,h", "Print available options")(
      "model,m", value<string>()->required(), "File containing the model")(
      "type,t", value<int>()->required(), "Model type")(
      "vocab", value<string>()->required(), "Output file for model vocabulary")(
      "vectors", value<string>()->required(), "Output file for word vectors");

  variables_map vm;
  store(parse_command_line(argc, argv, desc), vm);
  if (vm.count("help")) {
    cout << desc << endl;
  }

  notify(vm);

  string model_file = vm["model"].as<string>();
  int model_type = vm["type"].as<int>();
  string vocab_file = vm["vocab"].as<string>();
  string vectors_file = vm["vectors"].as<string>();

  switch (model_type) {
    //TODO
    /*case NLM:
      extract_word_vectors<LblLM>(model_file, vocab_file, vectors_file);
      return 0;
    case FACTORED_NLM:
      extract_word_vectors<ArcStandardLabelledParseModel<ParsedFactoredWeights>,
    ParsedFactoredWeights, ParsedFactoredMetadata>(model_file, vocab_file,
    vectors_file);
      extract_word_vectors<FactoredLblLM>(model_file, vocab_file, vectors_file);
      return 0; */
    case 2:
      extract_word_vectors<ArcStandardLabelledParseModel<ParsedFactoredWeights>,
                           ParsedFactoredWeights, ParsedFactoredMetadata>(
          model_file, vocab_file, vectors_file);
    case 3:
      extract_word_vectors<
          ArcStandardLabelledParseModel<TaggedParsedFactoredWeights>,
          TaggedParsedFactoredWeights, TaggedParsedFactoredMetadata>(
          model_file, vocab_file, vectors_file);
      return 0;
    default:
      cout << "Unknown model type" << endl;
      return 1;
  }

  return 0;
}
