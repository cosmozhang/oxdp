#ifndef _PYP_PARSE_WEIGHTS_H_
#define _PYP_PARSE_WEIGHTS_H_

#include "pyp/pyp_weights.h"
#include "corpus/dict.h"
#include "corpus/data_point.h"
#include "corpus/parse_data_set.h"
#include "pyp/pyplm.h"

namespace oxlm {

//NB this is the unlexicalized model, with only tags
template<unsigned tOrder, unsigned aOrder>
class ParsedPypWeights: public PypWeights<tOrder> {

  public:
  ParsedPypWeights(size_t vocab_size, size_t num_tags, size_t num_actions);

  virtual Real predictWord(WordId word, Words context) const;
  
  Real predictTag(WordId tag, Words context) const;

  Real predictAction(WordId action, Words context) const;

  virtual Real wordLikelihood() const;

  Real tagLikelihood() const;

  Real actionLikelihood() const;

  Real likelihood() const override;

  void resampleHyperparameters(MT19937& eng) override;

  virtual void updateInsert(const ParseDataSet& examples, MT19937& eng);

  virtual void updateRemove(const ParseDataSet& examples, MT19937& eng);

  virtual int numWords() const;

  int numTags() const;

  int numActions() const;

  private:
  PYPLM<aOrder> action_lm_;
  int num_actions_;
};

}

#endif
