
#include "corpus.h"
#include "dict.h"

namespace oxlm {

Corpus::Corpus()
  sentences_()
{
}

Words Corpus::convertWhitespaceDelimitedLine(const std::string& line, boost::shared_ptr<Dict>& dict, 
                                             bool frozen) {
  Words out;

  size_t cur = 0;
  size_t last = 0;
  int state = 0;
     
  //don't add start of sentence symbol
  //out.push_back(0); 

  while (cur < line.size()) {
    if (is_ws(line[cur++])) {
      if (state == 0) 
        continue;
      out.push_back(dict->convert(line.substr(last, cur - last - 1), frozen));
      state = 0;
    } else {
      if (state == 1) 
        continue;
      last = cur - 1;
      state = 1;
    }
  }

  if (state == 1)
    out.push_back(dict->convert(line.substr(last, cur - last), frozen));

  //add end of sentence symbol if defined 
  if (dict->eos() != -1) 
    out.push_back(dict->eos()); 

  return out;
}


void Corpus::readFile(const std::string& filename, boost::shared_ptr<Dict>& dict, bool frozen) {
  std::cerr << "Reading from " << filename << std::endl;
  std::ifstream in(filename);
  assert(in);
  std::string line;
  while(getline(in, line)) {
    Words sents = convertWhitespaceDelimitedLine(line, dict, frozen);
    sentences_.push_back(Sentence(sents));
  }
}

}

