#ifndef _PYP_PARSE_LEX_WEIGHTS_H_
#define _PYP_PARSE_LEX_WEIGHTS_H_

#include "pyp/parsed_pyp_weights.h"

namespace oxlm {

//this is the lexicalized model, with tags and words
template<unsigned wOrder, unsigned tOrder, unsigned aOrder>
class ParsedLexPypWeights: public ParsedPypWeights<tOrder, aOrder> {

  public:
  ParsedLexPypWeights(size_t vocab_size, size_t num_tags, size_t num_actions);

  Real predict(WordId word, Words context) const override;

  Reals predict(Words context) const override;

  Real predictWord(WordId word, Words context) const override;
  
  Reals predictWord(Words context) const override;
  
  Real likelihood() const override;

  Real wordLikelihood() const override;

  void resampleHyperparameters(MT19937& eng) override;

  void updateInsert(const boost::shared_ptr<ParseDataSet>& examples, MT19937& eng) override;

  void updateRemove(const boost::shared_ptr<ParseDataSet>& examples, MT19937& eng) override;
  
  int numWords() const override;

  int vocabSize() const override;

  private:
  PYPLM<wOrder> lex_lm_;
  int lex_vocab_size_;
};

}

#endif
