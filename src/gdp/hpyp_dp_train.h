#ifndef _CGDP_DPT_H_
#define _CGDP_DPT_H_

#include <iostream>
#include <unordered_map>
#include <cstdlib>

#include "transition_parser.h"
#include "hpyplm/hpyplm.h"
#include "corpus/corpus.h"
#include "pyp/m.h"
#include "pyp/random.h"
#include "pyp/crp.h"
#include "pyp/tied_parameter_resampler.h"

#define kORDER 3  //default 4

namespace oxlm {

//three-way action decision
void train_raw(std::string train_file, Dict& dict, std::set<WordId>& vocabs, std::vector<Words>& corpussh, std::vector<Words>& corpusre) {
  const WordId kSOS = dict.Convert("ROOT"); 
  std::vector<WordId> ctx(kORDER - 1, kSOS);
  std::vector<Words> corpuss;
  std::vector<WxList> corpusd;

  std::string sentence_file = train_file + ".sentences";
  std::cerr << "Reading corpus...\n";
  ReadFromFile(sentence_file, &dict, &corpuss, &vocabs);
  std::cerr << "E-corpus size: " << corpuss.size() << " sentences\t (" << vocabs.size() << " word types)\n";
  
  std::string dependencies_file = train_file + ".dependencies";
  std::cerr << "Reading corpus...\n";
  ReadFromDependencyFile(dependencies_file, &corpusd);
  std::cerr << "E-corpus size: " << corpusd.size() << " sentences\n";

  // apply the oracle
  for (unsigned i = 0; i < corpuss.size(); ++i) {
    ArcStandardParser parser(corpuss[i], kORDER);
    bool has_parse = parser.sentence_oracle(corpusd[i]);
    if (parser.is_projective_dependency(corpusd[i])) {
      if (has_parse) {
        //std::cout << parser.actions_str() << std::endl;
        //add the extracted training examples
        WordsList sh_ctxs = parser.shift_contexts();
        for (unsigned j = 0; j < parser.sentence_length(); ++j) {
          //corpussh.push_back(Words(1, parser.get_sentence()[j]));
          Words ins(1, parser.get_sentence()[j]);
          //corpussh.back()->insert(corpussh.end(), sh_ctxs[j].begin(), sh_ctxs[j].end());
          ins.insert(ins.end(), sh_ctxs[j].begin(), sh_ctxs[j].end());
          corpussh.push_back(ins);
        }
              
        WordsList act_ctxs = parser.action_contexts();
        for (unsigned j = 0; j < act_ctxs.size(); ++j) {
          Words ins(1, static_cast<WordId>(parser.action_predictions()[j]));
          ins.insert(ins.end(), act_ctxs[j].begin(), act_ctxs[j].end());
          //for (auto a: ins)
          //    std::cout << a << " ";
          //std::cout << std::endl;
          corpusre.push_back(ins);
        }
            
      } else
            std::cerr << "oracle failure" << std::endl;
    } //else
      //std::cerr << "non-projective parse" << std::endl;
  }
}

//with binary decisions
void train_raw(std::string train_file, Dict& dict, std::set<WordId>& vocabs, std::vector<Words>& corpussh, std::vector<Words>& corpusre, std::vector<Words>& corpusarc) {
  const WordId kSOS = dict.Convert("ROOT"); 
  std::vector<WordId> ctx(kORDER - 1, kSOS);
  std::vector<Words> corpuss;
  std::vector<WxList> corpusd;

  std::string sentence_file = train_file + ".sentences";
  std::cerr << "Reading corpus...\n";
  ReadFromFile(sentence_file, &dict, &corpuss, &vocabs);
  std::cerr << "E-corpus size: " << corpuss.size() << " sentences\t (" << vocabs.size() << " word types)\n";
  
  std::string dependencies_file = train_file + ".dependencies";
  std::cerr << "Reading corpus...\n";
  ReadFromDependencyFile(dependencies_file, &corpusd);
  std::cerr << "E-corpus size: " << corpusd.size() << " sentences\n";

  // apply the oracle
  for (unsigned i = 0; i < corpuss.size(); ++i) {
    ArcStandardParser parser(corpuss[i], kORDER);
    bool has_parse = parser.sentence_oracle(corpusd[i]);
    if (parser.is_projective_dependency(corpusd[i])) {
      if (has_parse) {
        //std::cout << parser.actions_str() << std::endl;
        //add the extracted training examples
        WordsList sh_ctxs = parser.shift_contexts();
        for (unsigned j = 0; j < parser.sentence_length(); ++j) {
          //corpussh.push_back(Words(1, parser.get_sentence()[j]));
          Words ins(1, parser.get_sentence()[j]);
          //corpussh.back()->insert(corpussh.end(), sh_ctxs[j].begin(), sh_ctxs[j].end());
          ins.insert(ins.end(), sh_ctxs[j].begin(), sh_ctxs[j].end());
          corpussh.push_back(ins);
        }
         
        //reduce contexts = action contexts
        WordsList re_ctxs = parser.action_contexts();
        for (unsigned j = 0; j < re_ctxs.size(); ++j) {
          Words ins(1, static_cast<WordId>(parser.reduce_predictions()[j]));
          ins.insert(ins.end(), re_ctxs[j].begin(), re_ctxs[j].end());
          corpusre.push_back(ins);
        }

        //left/right arc contexts
        WordsList arc_ctxs = parser.arc_contexts();
        for (unsigned j = 0; j < arc_ctxs.size(); ++j) {
          Words ins(1, static_cast<WordId>(parser.arc_predictions()[j]));
          ins.insert(ins.end(), re_ctxs[j].begin(), re_ctxs[j].end());
          corpusarc.push_back(ins);
        }
            
      } else
            std::cerr << "oracle failure" << std::endl;
    } //else
      //std::cerr << "non-projective parse" << std::endl;
  }
}

void train_lm(int samples, MT19937& eng, Dict& dict, std::vector<Words>& corpus, PYPLM<kORDER>& lm) {
  const WordId kSOS = dict.Convert("ROOT"); 
  std::vector<WordId> ctx(kORDER - 1, kSOS);

  for (int sample=0; sample < samples; ++sample) {
    for (const auto& s : corpus) {
      WordId w = s[0];
      ctx = std::vector<WordId>(s.begin()+1, s.end());
      if (sample > 0) lm.decrement(w, ctx, eng);
      lm.increment(w, ctx, eng);
    }
    if (sample % 10 == 9) {
      std::cerr << " [LLH=" << lm.log_likelihood() << "]" << std::endl;
      if (sample % 30u == 29) lm.resample_hyperparameters(eng);
    } else { std::cerr << '.' << std::flush; }
  }
}

//Old train_shift
void train_shift(int samples, std::string wc_train_file, MT19937& eng, Dict& dict, std::set<WordId>& vocabs, PYPLM<kORDER>& shift_lm) {
  const WordId kSOS = dict.Convert("ROOT"); 
  std::vector<WordId> ctx(kORDER - 1, kSOS);
  std::vector<Words> corpuss;

  std::cerr << "Reading corpus...\n";
  ReadFromFile(wc_train_file, &dict, &corpuss, &vocabs);
  std::cerr << "E-corpus size: " << corpuss.size() << " sentences\t (" << vocabs.size() << " word types)\n";
  //shift_lm = new PYPLM<kORDER>(vocabs.size(), 1, 1, 1, 1);

  for (int sample=0; sample < samples; ++sample) {
    for (const auto& s : corpuss) {
      WordId w = s[0];
      ctx = std::vector<WordId>(s.begin()+1, s.end());
      if (sample > 0) shift_lm.decrement(w, ctx, eng);
      shift_lm.increment(w, ctx, eng);
    }
    if (sample % 10 == 9) {
      std::cerr << " [LLH=" << shift_lm.log_likelihood() << "]" << std::endl;
      if (sample % 30u == 29) shift_lm.resample_hyperparameters(eng);
    } else { std::cerr << '.' << std::flush; }
  }
}

//Old train_action
void train_action(int samples, std::string ac_train_file, MT19937& eng, Dict& dict, std::set<WordId>& vocabr, PYPLM<kORDER>& action_lm) {
  const WordId kSOS = dict.Convert("ROOT"); 
  std::vector<WordId> ctx(kORDER - 1, kSOS);
  std::vector<Words> corpusr;

  std::cerr << "Reading corpus...\n";
  ReadFromActionFile(ac_train_file, &dict, &corpusr, &vocabr);
  std::cerr << "E-corpus size: " << corpusr.size() << " sentences\t (" << vocabr.size() << " word types)\n";

  for (int sample=0; sample < samples; ++sample) {
    for (const auto& s : corpusr) {
      WordId w = s[0];  //index over actions
      ctx = std::vector<WordId>(s.begin()+1, s.end());
      if (sample > 0) action_lm.decrement(w, ctx, eng);
      action_lm.increment(w, ctx, eng);
    }
    if (sample % 10 == 9) {
      std::cerr << " [LLH=" << action_lm.log_likelihood() << "]" << std::endl;
      if (sample % 30u == 29) action_lm.resample_hyperparameters(eng);
    } else { std::cerr << '.' << std::flush; }
  }
}

}
#endif