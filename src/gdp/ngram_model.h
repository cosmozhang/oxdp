#ifndef _CORPUS_NGRAM_MODEL_H_
#define _CORPUS_NGRAM_MODEL_H_

#include "corpus/utils.h"
#include "corpus/sentence.h"
#include "corpus/data_set.h"
#include "corpus/corpus.h"

#include "pyp/pyp_weights.h"
#include "lbl/weights.h"
#include "lbl/factored_weights.h"

namespace oxlm {

template<class Weights>
class NGramModel {
  public:
  NGramModel(unsigned order, WordId sos, WordId eos);

  Words extractContext(const boost::shared_ptr<Corpus> corpus, int position);

  void extract(const boost::shared_ptr<Corpus> corpus, int position,     
          const boost::shared_ptr<DataSet>& examples);

  Real evaluate(const boost::shared_ptr<Corpus> corpus, int position, 
          const boost::shared_ptr<Weights>& weights); 

  void extractSentence(const Sentence& sent, 
          const boost::shared_ptr<DataSet>& examples);

  //return likelihood
  Real evaluateSentence(const Sentence& sent, 
          const boost::shared_ptr<Weights>& weights);

  private:
  unsigned order_;
  WordId sos_;
  WordId eos_;
          
};

}

#endif