#include <iostream>
#include <unordered_map>
#include <cstdlib>

#include "hpyplm.h"
#include "corpus/corpus.h"
#include "utils/m.h"
#include "utils/random.h"
#include "pyp/crp.h"
#include "pyp/tied_parameter_resampler.h"

#define kORDER 3  //default 4
#define nSAMPLES 200

using namespace std;
using namespace oxlm;

Dict dict;

int main(int argc, char** argv) {
  if (argc != 3) {
    cerr << argv[0] << " <training.txt> <nsamples>\n\nEstimate a " 
         << kORDER << "-gram HPYP LM and report perplexity\n100 is usually sufficient for <nsamples>\n";
    return 1;
  }
  MT19937 eng;
  string train_file = argv[1];
  int samples = atoi(argv[2]);
  
  vector<vector<WordId> > corpuse;
  set<WordId> vocabe, tv;
  const WordId kSOS = dict.Convert("<s>");
  const WordId kEOS = dict.Convert("</s>");
  cerr << "Reading corpus...\n";
  ReadFromFile(train_file, &dict, &corpuse, &vocabe);
  cerr << "E-corpus size: " << corpuse.size() << " sentences\t (" << vocabe.size() << " word types)\n";
  
  PYPLM<kORDER> lm(vocabe.size(), 1, 1, 1, 1);
  vector<WordId> ctx(kORDER - 1, kSOS);
  for (int sample=0; sample < samples; ++sample) {
    for (const auto& s : corpuse) {
      ctx.resize(kORDER - 1);
      for (unsigned i = 0; i <= s.size(); ++i) {
        WordId w = (i < s.size() ? s[i] : kEOS);
        if (sample > 0) lm.decrement(w, ctx, eng);
        lm.increment(w, ctx, eng);
        ctx.push_back(w);
      }
    }
    if (sample % 10 == 9) {
      cerr << " [LLH=" << lm.log_likelihood() << "]" << endl;
      if (sample % 30u == 29) lm.resample_hyperparameters(eng);
    } else { cerr << '.' << flush; }
  }
//  lm.print(cerr);
  double llh = 0;
  unsigned cnt = 0;
  
  vector<vector<WordId> > particles;
  vector<int> length_dist;

  //generate sentences
  for (int i = 0; i < nSAMPLES; ++i) {
    ctx.resize(kORDER - 1);
    WordId w = kSOS;
    
    while (w != kEOS) {    
      //cout << "context: ";
      //for (auto w: ctx)
      //  cout << w << " ";
      
      w = lm.generate(ctx, vocabe.size(), 0, eng); 
      //cout << "word: " << dict.Convert(w) << endl;
      ctx.push_back(w);
    
      double lp = log(lm.prob(w, ctx)) / log(2);
      llh -= lp;
      cnt++;
    } 

    //print the sentence
    cout << (ctx.size() - 2) << " ";
    for (auto w: ctx)
        cout << dict.Convert(w) << " ";
    cout << endl;
    particles.push_back(ctx);
    length_dist.push_back(ctx.size()-2);
  } 

  sort(length_dist.begin(), length_dist.end());
  for (auto l: length_dist)
    cout << l << " ";
  cout << endl;

  cerr << "  Log_10 prob: " << (-llh * log(2) / log(10)) << endl;
  cerr << "        Count: " << cnt << endl;
  cerr << "Cross-entropy: " << (llh / cnt) << endl;
  cerr << "   Perplexity: " << pow(2, llh / cnt) << endl;
  return 0;
}

