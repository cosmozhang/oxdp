#include "gdp/arc_standard_parse_model.h"

namespace oxlm {

void ArcStandardParseModel::resampleParticles(AsParserList* beam_stack, MT19937& eng,
        unsigned num_particles) {
  //assume (beam_stack->at(pi)->num_particles() > 0)
  std::vector<double> importance_w(beam_stack->size(), L_MAX); 
  for (unsigned i = 0; i < importance_w.size(); ++i) 
    importance_w[i] = beam_stack->at(i)->weighted_importance_weight();

  //resample according to importance weight
  multinomial_distribution_log part_mult(importance_w); 
  std::vector<int> sample_counts(beam_stack->size(), 0);
  for (unsigned i = 0; i < num_particles;) {
    unsigned pi = part_mult(eng);
    ++sample_counts[pi];
    ++i;
  }

  for (unsigned i = 0; i < beam_stack->size(); ++i) {
    beam_stack->at(i)->set_num_particles(sample_counts[i]);
    beam_stack->at(i)->reset_importance_weight();
  }
}

ArcStandardParser ArcStandardParseModel::beamParseSentence(const ParsedSentence& sent, 
                               const boost::shared_ptr<ParsedWeightsInterface>& weights, unsigned beam_size) {
  std::vector<AsParserList> beam_chart; 
  beam_chart.push_back(AsParserList());
  beam_chart[0].push_back(boost::make_shared<ArcStandardParser>(static_cast<TaggedSentence>(sent))); 

  //std::cout << "gold arcs: ";
  //sent.print_arcs();

  //shift ROOT symbol (probability 1)
  beam_chart[0][0]->shift(); 

  //add reduce actions, then shift word k (expect for last iteration) 
  for (unsigned k = 1; k <= sent.size(); ++k) {
    //there are k beam lists. perform reduces down to list 1

    for (unsigned i = k - 1; i > 0; --i) { 
      //prune if size exceeds beam_size
      if (beam_chart[i].size() > beam_size) {
        std::sort(beam_chart[i].begin(), beam_chart[i].end(), TransitionParser::cmp_particle_weights); 
        //remove items with worst scores
        for (unsigned j = beam_chart[i].size(); j > beam_size; --j)
          beam_chart[i].pop_back();
      }

      //for every item in the list, add valid reduce actions to list i - 1 
      for (unsigned j = 0; (j < beam_chart[i].size()); ++j) {
        double reduceleftarcp = weights->predictAction(static_cast<WordId>(kAction::la), beam_chart[i][j]->actionContext());
        double reducerightarcp = weights->predictAction(static_cast<WordId>(kAction::ra), beam_chart[i][j]->actionContext());
        //std::cout << "(la: " << reduceleftarcp << ", ra: " << reducerightarcp << ")" << " ";
        double reducep = neg_log_sum_exp(reduceleftarcp, reducerightarcp);
       
        //TODO have option to make la/ra choice deterministic
        beam_chart[i-1].push_back(boost::make_shared<ArcStandardParser>(*beam_chart[i][j]));
        if (i > 1) { //left arc only invalid when stack size is 2 **
          beam_chart[i-1].push_back(boost::make_shared<ArcStandardParser>(*beam_chart[i][j]));

          beam_chart[i-1].back()->leftArc();
          beam_chart[i-1].back()->add_particle_weight(reduceleftarcp);
          beam_chart[i-1].rbegin()[1]->rightArc();
          beam_chart[i-1].rbegin()[1]->add_particle_weight(reducerightarcp); 

          if (k == sent.size()) {  
            beam_chart[i-1].back()->add_importance_weight(reducep); 
            beam_chart[i-1].rbegin()[1]->add_importance_weight(reducep); 
          } 
        } else {
          beam_chart[i-1].back()->rightArc();
          beam_chart[i-1].back()->add_particle_weight(reducerightarcp); 
          
          if (k == sent.size()) 
            beam_chart[i-1].back()->add_importance_weight(reducerightarcp - reducep); 
        }
      }
    }

    if ((beam_chart[0].size() > beam_size) || (k == sent.size())) {
        std::sort(beam_chart[0].begin(), beam_chart[0].end(), TransitionParser::cmp_particle_weights); 
        //remove items with worst scores
        for (unsigned j = beam_chart[0].size(); j > beam_size; --j)
          beam_chart[0].pop_back();
    }

    //perform shifts
    if (k < sent.size()) {
      for (unsigned i = 0; (i < k); ++i) { 
        for (unsigned j = 0; j < beam_chart[i].size(); ++j) {
          double shiftp = weights->predictAction(static_cast<WordId>(kAction::sh), 
                                                      beam_chart[i][j]->actionContext());
          double tagp = weights->predictTag(beam_chart[i][j]->next_tag(), 
                                           beam_chart[i][j]->tagContext());
          double wordp = weights->predictWord(beam_chart[i][j]->next_word(), 
                                             beam_chart[i][j]->wordContext());

          beam_chart[i][j]->shift();
          beam_chart[i][j]->add_particle_weight(shiftp); 
          beam_chart[i][j]->add_importance_weight(tagp); 
          beam_chart[i][j]->add_importance_weight(wordp); 
          beam_chart[i][j]->add_particle_weight(tagp); 
          beam_chart[i][j]->add_particle_weight(wordp); 
        }
      }
      //insert new beam_chart[0] to increment indexes
      beam_chart.insert(beam_chart.begin(), AsParserList());
    } 
    //std::cout << std::endl; 
  }
 
  //TODO sum over identical parses in final beam 
  unsigned n = 0; //index to final beam
  for (unsigned i = 0; (i < beam_chart[n].size()); ++i) 
    beam_chart[n][0]->add_beam_weight(beam_chart[n][i]->particle_weight());

  //print parses
  //add verbose option?
  /* for (unsigned i = 0; (i < 5) && (i < beam_chart[n].size()); ++i) {
    std::cout << beam_chart[n][i]->particle_weight() << " ";
    beam_chart[n][i]->print_arcs();
    beam_chart[n][i]->print_actions();

    //can't do this now, but add if needed later
    //float dir_acc = (beam_chart[n][i]->directed_accuracy_count(gold_dep) + 0.0)/(sent.size()-1);
    //std::cout << "  Dir Accuracy: " << dir_acc;
  } */

  if (beam_chart[n].size()==0) {
    std::cout << "no parse found" << std::endl;
    return ArcStandardParser(static_cast<TaggedSentence>(sent));  
  } else
    return ArcStandardParser(*beam_chart[n][0]); 
}


ArcStandardParser ArcStandardParseModel::particleParseSentence(const ParsedSentence& sent, 
        const boost::shared_ptr<ParsedWeightsInterface>& weights, MT19937& eng, unsigned num_particles,
        bool resample) {
    //Follow approach similar to per-word beam-search, but also keep track of number of particles that is equal to given state
  //perform sampling and resampling to update these counts, and remove 0 count states

  AsParserList beam_stack; 
  beam_stack.push_back(boost::make_shared<ArcStandardParser>(static_cast<TaggedSentence>(sent), static_cast<int>(num_particles))); 

  //shift ROOT symbol (probability 1)
  beam_stack[0]->shift(); 

  for (unsigned i = 1; i < sent.size(); ++i) {
    for (unsigned j = 0; j < beam_stack.size(); ++j) { 
      if (beam_stack[j]->num_particles()==0)
        continue;
       
      //sample a sequence of possible actions leading up to the next shift

      int num_samples = beam_stack[j]->num_particles();

      Words r_ctx = beam_stack[j]->actionContext();
      double shiftp = weights->predictAction(static_cast<WordId>(kAction::sh), r_ctx);
      double reduceleftarcp = weights->predictAction(static_cast<WordId>(kAction::la), r_ctx);
      double reducerightarcp = weights->predictAction(static_cast<WordId>(kAction::ra), r_ctx);
      double reducep = neg_log_sum_exp(reduceleftarcp, reducerightarcp); 

      std::vector<int> sample_counts = {0, 0, 0}; //shift, reduceleftarc, reducerightarc

      if (beam_stack[j]->stack_depth() < 2) {
        //only shift is allowed
        sample_counts[0] += num_samples;
      } else {
        if (beam_stack[j]->stack_depth() == 2) {
          //left arc disallowed
          reduceleftarcp = L_MAX;
          reducerightarcp = reducep;
        }

        std::vector<double> distr = {shiftp, reduceleftarcp, reducerightarcp};
        multinomial_distribution_log mult(distr); 
        for (int k = 0; k < num_samples; k++) {
          WordId act = mult(eng);
          ++sample_counts[act];
        }
      }        
     
      if ((sample_counts[1] > 0) && (sample_counts[2] > 0)) {
        beam_stack.push_back(boost::make_shared<ArcStandardParser>(*beam_stack[j]));
        beam_stack.push_back(boost::make_shared<ArcStandardParser>(*beam_stack[j]));

        beam_stack.back()->leftArc();
        beam_stack.back()->add_particle_weight(reduceleftarcp);
        beam_stack.back()->set_num_particles(sample_counts[1]); 

        beam_stack.rbegin()[1]->rightArc();
        beam_stack.rbegin()[1]->add_particle_weight(reducerightarcp); 
        beam_stack.rbegin()[1]->set_num_particles(sample_counts[2]); 

      } else if (sample_counts[2] > 0) {
        beam_stack.push_back(boost::make_shared<ArcStandardParser>(*beam_stack[j]));
        beam_stack.back()->rightArc();
        beam_stack.back()->add_particle_weight(reducerightarcp); 
        beam_stack.back()->set_num_particles(sample_counts[2]); 
      } else if (sample_counts[1] > 0) {
        beam_stack.push_back(boost::make_shared<ArcStandardParser>(*beam_stack[j]));
        beam_stack.back()->leftArc();
        beam_stack.back()->add_particle_weight(reduceleftarcp); 
        beam_stack.back()->set_num_particles(sample_counts[1]); 
      }

      //perform shift if > 0 samples
      if (sample_counts[0] == 0)
        beam_stack[j]->set_num_particles(0);
      else {
        double tagp = weights->predictTag(beam_stack[j]->next_tag(), beam_stack[j]->tagContext());
        double wordp = weights->predictWord(beam_stack[j]->next_word(), beam_stack[j]->wordContext());

        beam_stack[j]->shift();
        beam_stack[j]->add_particle_weight(shiftp); 
        beam_stack[j]->add_importance_weight(wordp); 
        beam_stack[j]->add_importance_weight(tagp); 
        beam_stack[j]->add_particle_weight(wordp); 
        beam_stack[j]->add_particle_weight(tagp); 
        beam_stack[j]->set_num_particles(sample_counts[0]);
      }
    }
 
    if (resample) {
      //sort and remove 
      std::sort(beam_stack.begin(), beam_stack.end(), TransitionParser::cmp_weighted_importance_weights); 
      for (int j = beam_stack.size()- 1; ((j >= 0) && (beam_stack[j]->num_particles() == 0)); --j)
        beam_stack.pop_back();
      //std::cout << "Beam size: " << beam_stack.size();
      resampleParticles(&beam_stack, eng, num_particles);
      /* int active_particle_count = 0;
       for (int j = 0; j < beam_stack.size(); ++j)
        if (beam_stack[j]->num_particles() > 0)
         ++active_particle_count;
      std::cout << " -> " << active_particle_count << " without null \n"; */
    }
  }
     
  ///completion
  AsParserList final_beam; 
  bool has_more_states = true;

  while (has_more_states) {
    has_more_states = false;
    unsigned cur_beam_size = beam_stack.size();

    for (unsigned j = 0; j < cur_beam_size; ++j) { 
      if ((beam_stack[j]->num_particles() > 0) && !beam_stack[j]->inTerminalConfiguration()) {
        //add paths for reduce actions
        has_more_states = true; 
        Words r_ctx = beam_stack[j]->actionContext();
        double reduceleftarcp = weights->predictAction(static_cast<WordId>(kAction::la), r_ctx);
        double reducerightarcp = weights->predictAction(static_cast<WordId>(kAction::ra), r_ctx);
        double reducep = neg_log_sum_exp(reduceleftarcp, reducerightarcp); 
        
        int num_samples = beam_stack[j]->num_particles();
        std::vector<int> sample_counts = {0, 0}; //reduceleftarc, reducerightarc

        if (beam_stack[j]->stack_depth() == 2) {
          //only allow right arc
          sample_counts[1] = num_samples;
        } else {
          std::vector<double> distr = {reduceleftarcp, reducerightarcp};
          multinomial_distribution_log mult(distr); 
          for (int k = 0; k < num_samples; k++) {
            WordId act = mult(eng);
            ++sample_counts[act];
          }
        }

        if ((sample_counts[0] > 0) && (sample_counts[1] > 0)) {
          beam_stack.push_back(boost::make_shared<ArcStandardParser>(*beam_stack[j]));

          beam_stack.back()->leftArc();
          beam_stack.back()->add_particle_weight(reduceleftarcp);
          beam_stack.back()->set_num_particles(sample_counts[0]); 
          beam_stack.back()->add_importance_weight(reducep); 

          beam_stack[j]->rightArc();
          beam_stack[j]->add_particle_weight(reducerightarcp); 
          beam_stack[j]->set_num_particles(sample_counts[1]); 
          beam_stack[j]->add_importance_weight(reducep); 
          
        } else if (sample_counts[1] > 0) {
          beam_stack[j]->rightArc();
          beam_stack[j]->add_particle_weight(reducerightarcp); 
          beam_stack[j]->set_num_particles(sample_counts[1]); 
          beam_stack[j]->add_importance_weight(reducep); 

        } else if (sample_counts[0] > 0) {
          beam_stack[j]->leftArc();
          beam_stack[j]->add_particle_weight(reduceleftarcp); 
          beam_stack[j]->set_num_particles(sample_counts[0]); 
          beam_stack[j]->add_importance_weight(reducep); 
        }
      }
    }

    if (resample) {
      //sort and remove 
      std::sort(beam_stack.begin(), beam_stack.end(), TransitionParser::cmp_weighted_importance_weights); 
      for (int j = beam_stack.size()- 1; ((j >= 0) && (beam_stack[j]->num_particles() == 0)); --j)
        beam_stack.pop_back();
      //std::cout << "Beam size: " << beam_stack.size();
      resampleParticles(&beam_stack, eng, num_particles);
      //int active_particle_count = 0;
      //for (int j = 0; j < beam_stack.size(); ++j)
      //  if (beam_stack[j]->num_particles() > 0)
      //    ++active_particle_count;
      //std::cout << " -> " << active_particle_count << " without null \n";
    }
  }

  //alternatively, sort according to particle weight 
  //std::sort(final_beam.begin(), final_beam.end(), cmp_particle_ptr_weights); //handle pointers
 
  std::sort(beam_stack.begin(), beam_stack.end(), TransitionParser::cmp_weighted_importance_weights); 
  for (int j = beam_stack.size()- 1; ((j >= 0) && (beam_stack[j]->num_particles() == 0)); --j)
    beam_stack.pop_back();
  //std::cout << "Final beam size: " << beam_stack.size();

  /* if ((beam_stack.size() > 0) && take_max) {
    //resampleParticles(&beam_stack, eng, num_particles);
    std::sort(beam_stack.begin(), beam_stack.end(), TransitionParser::cmp_particle_weights); 
    return ArcStandardParser(*beam_stack[0]);
  } */
  if (beam_stack.size() > 0) {
    //just take 1 sample
    resampleParticles(&beam_stack, eng, 1);
    for (unsigned i = 0; i < beam_stack.size(); ++i) {
      if (beam_stack[i]->num_particles() == 1) {
        //beam_stack[i]->print_arcs();
        //float dir_acc = (beam_stack[i]->directed_accuracy_count(gold_dep) + 0.0)/(sent.size()-1);
        //std::cout << "  Dir Accuracy: " << dir_acc;
        //std::cout << "  Sample weight: " << (beam_stack[i]->particle_weight()) << std::endl;
        return ArcStandardParser(*beam_stack[i]); 
      }
    }
  }

  std::cout << "no parse found" << std::endl;
  return ArcStandardParser(static_cast<TaggedSentence>(sent));  
}

//sample a derivation for the gold parse, given the current model
//three-way decisions
ArcStandardParser ArcStandardParseModel::particleGoldParseSentence(const ParsedSentence& sent, 
          const boost::shared_ptr<ParsedWeightsInterface>& weights, MT19937& eng, unsigned num_particles, bool resample) {
  //Follow approach similar to per-word beam-search, but also keep track of number of particles that is equal to given state
  //perform sampling and resampling to update these counts, and remove 0 count states
  
  AsParserList beam_stack; 
  beam_stack.push_back(boost::make_shared<ArcStandardParser>(static_cast<TaggedSentence>(sent), static_cast<int>(num_particles))); 

  //shift ROOT symbol (probability 1)
  beam_stack[0]->shift(); 

  for (unsigned i = 1; i < sent.size(); ++i) {
    for (unsigned j = 0; j < beam_stack.size(); ++j) { 
      if (beam_stack[j]->num_particles()==0)
        continue;
       
      //sample a sequence of possible actions leading up to the next shift

      int num_samples = beam_stack[j]->num_particles();
      std::vector<int> sample_counts = {0, 0, 0}; //shift, reduceleftarc, reducerightarc

      Words r_ctx = beam_stack[j]->actionContext();
      double shiftp = weights->predictAction(static_cast<WordId>(kAction::sh), r_ctx);
      double reduceleftarcp = weights->predictAction(static_cast<WordId>(kAction::la), r_ctx);
      double reducerightarcp = weights->predictAction(static_cast<WordId>(kAction::ra), r_ctx);
      double reducep = neg_log_sum_exp(reduceleftarcp, reducerightarcp); 
      
      kAction oracle_next = beam_stack[j]->oracleNext(sent);

      if (oracle_next==kAction::sh) {
        //only shift is allowed
        sample_counts[0] += num_samples;
        if (beam_stack[j]->stack_depth() >= 2)
          beam_stack[j]->add_importance_weight(shiftp);  
      } else {
        //enforce at least one particle to reduce
        std::vector<double> distr; //= {shiftp, reduceleftarcp, reducerightarcp};
        if (oracle_next==kAction::la) {
          distr = {shiftp, reducep, L_MAX};
          sample_counts[1] =  1;            
        }
        if (oracle_next==kAction::ra) {
          distr = {shiftp, L_MAX, reducep};
          sample_counts[2] =  1;            
        }

        multinomial_distribution_log mult(distr); 
        for (int k = 1; k < num_samples; k++) {
          WordId act = mult(eng);
          ++sample_counts[act];
        }
      }
      
     if (sample_counts[2] > 0) {
        beam_stack.push_back(boost::make_shared<ArcStandardParser>(*beam_stack[j]));
        beam_stack.back()->rightArc();
        beam_stack.back()->add_particle_weight(reducerightarcp); 
        beam_stack.back()->add_importance_weight(reducerightarcp - reducep); 
        beam_stack.back()->set_num_particles(sample_counts[2]); 
      } else if (sample_counts[1] > 0) {
        beam_stack.push_back(boost::make_shared<ArcStandardParser>(*beam_stack[j]));
        beam_stack.back()->leftArc();
        beam_stack.back()->add_particle_weight(reduceleftarcp); 
        beam_stack.back()->add_importance_weight(reduceleftarcp - reducep); 
        beam_stack.back()->set_num_particles(sample_counts[1]); 
      }

      //perform shift if > 0 samples
      if (sample_counts[0] == 0)
        beam_stack[j]->set_num_particles(0);
      else {
        double tagp = weights->predictTag(beam_stack[j]->next_tag(), beam_stack[j]->tagContext());
        double wordp = weights->predictWord(beam_stack[j]->next_word(), beam_stack[j]->wordContext());
        
        beam_stack[j]->shift();
        beam_stack[j]->add_importance_weight(wordp); 
        beam_stack[j]->add_importance_weight(tagp);
        beam_stack[j]->add_particle_weight(shiftp); 
        beam_stack[j]->add_particle_weight(wordp); 
        beam_stack[j]->add_particle_weight(tagp); 
        beam_stack[j]->set_num_particles(sample_counts[0]);
      }
    }
 
    if (resample) {
      //sort and remove 
      std::sort(beam_stack.begin(), beam_stack.end(), TransitionParser::cmp_weighted_importance_weights); 
      for (int j = beam_stack.size()- 1; ((j >= 0) && (beam_stack[j]->num_particles() == 0)); --j)
        beam_stack.pop_back();
      //std::cout << "Beam size: " << beam_stack.size();
      resampleParticles(&beam_stack, eng, num_particles);
      //int active_particle_count = 0;
      //for (int j = 0; j < beam_stack.size(); ++j)
      //  if (beam_stack[j]->num_particles() > 0)
      //   ++active_particle_count;
      //std::cout << " -> " << active_particle_count << " without null \n";
    }
  }
     
  ///completion
  bool has_more_states = true;

  while (has_more_states) {
    has_more_states = false;
    //unsigned cur_beam_size = beam_stack.size();
    //std::cerr << cur_beam_size << ": ";

    for (unsigned j = 0; j < beam_stack.size(); ++j) { 
      if ((beam_stack[j]->num_particles() > 0) && !beam_stack[j]->inTerminalConfiguration()) {
        //add paths for reduce actions
        has_more_states = true; 
        Words r_ctx = beam_stack[j]->actionContext();
        double reduceleftarcp = weights->predictAction(static_cast<WordId>(kAction::la), r_ctx);
        double reducerightarcp = weights->predictAction(static_cast<WordId>(kAction::ra), r_ctx);
        
        kAction oracle_next = beam_stack[j]->oracleNext(sent);
        //std::cerr << " (" << beam_stack[j]->num_particles() << ") " << static_cast<WordId>(oracle_next);
        if (oracle_next==kAction::re) {
          //invalid, so let particles die (else all particles are moved on)
          beam_stack[j]->set_num_particles(0);
        } else if (oracle_next == kAction::ra) {
          beam_stack[j]->rightArc();
          beam_stack[j]->add_particle_weight(reducerightarcp); 
          beam_stack[j]->add_importance_weight(reducerightarcp); 
        } else if (oracle_next == kAction::la) {
          beam_stack[j]->leftArc();
          beam_stack[j]->add_particle_weight(reduceleftarcp); 
          beam_stack[j]->add_importance_weight(reduceleftarcp); 
        }
      }
    }
    //std::cerr << std::endl;

    if (resample) {
      //sort and remove 
      std::sort(beam_stack.begin(), beam_stack.end(), TransitionParser::cmp_weighted_importance_weights); 
      for (int j = beam_stack.size()- 1; ((j >= 0) && (beam_stack[j]->num_particles() == 0)); --j)
        beam_stack.pop_back();
      //std::cout << "Beam size: " << beam_stack.size();
      resampleParticles(&beam_stack, eng, num_particles);
      //int active_particle_count = 0;
      //for (int j = 0; j < beam_stack.size(); ++j)
      //  if (beam_stack[j]->num_particles() > 0)
      //    ++active_particle_count;
      //std::cout << " -> " << active_particle_count << " without null \n";
    }
  }

  //alternatively, sort according to particle weight 
  //std::sort(final_beam.begin(), final_beam.end(), cmp_particle_ptr_weights); //handle pointers
 
  std::sort(beam_stack.begin(), beam_stack.end(), TransitionParser::cmp_weighted_importance_weights); 
  for (int j = beam_stack.size()- 1; ((j >= 0) && (beam_stack[j]->num_particles() == 0)); --j)
    beam_stack.pop_back();
  //std::cerr << beam_stack.size() << " ";

  //just take 1 sample
  if (beam_stack.size() > 0)
    resampleParticles(&beam_stack, eng, 1);
  for (unsigned i = 0; i < beam_stack.size(); ++i) {
    if (beam_stack[i]->num_particles() == 1) {
      //beam_stack[i]->print_arcs();
      //float dir_acc = (beam_stack[i]->directed_accuracy_count(sent) + 0.0)/(sent.size()-1);
      //std::cout << "  Dir Accuracy: " << dir_acc;
      //std::cout << "  Sample weight: " << (beam_stack[i]->particle_weight()) << std::endl;

      return ArcStandardParser(*beam_stack[i]); 
    }
  }

  std::cout << "no parse found" << std::endl;
  return ArcStandardParser(static_cast<TaggedSentence>(sent));  
}


ArcStandardParser ArcStandardParseModel::staticGoldParseSentence(const ParsedSentence& sent, 
                                    const boost::shared_ptr<ParsedWeightsInterface>& weights) {
  ArcStandardParser parser(static_cast<TaggedSentence>(sent));
  
  kAction a = kAction::sh;
  while (!parser.inTerminalConfiguration() && (a != kAction::re)) {
    a = parser.oracleNext(sent);  
    if (a != kAction::re) {
      //update particle weight
      double actionp = weights->predictAction(static_cast<WordId>(a), parser.actionContext());
      parser.add_particle_weight(actionp);

      if (a == kAction::sh) {
        double tagp = weights->predictTag(parser.next_tag(), parser.tagContext());
        double wordp = weights->predictWord(parser.next_word(), parser.wordContext());
        parser.add_particle_weight(tagp);
        parser.add_particle_weight(wordp);
      }

      parser.executeAction(a);
    } 
  }

  return parser;
}
    
ArcStandardParser ArcStandardParseModel::staticGoldParseSentence(const ParsedSentence& sent) {
  ArcStandardParser parser(static_cast<TaggedSentence>(sent));
  
  kAction a = kAction::sh;
  while (!parser.inTerminalConfiguration() && (a != kAction::re)) {
    a = parser.oracleNext(sent);  
    if (a != kAction::re) 
      parser.executeAction(a);
  }

  return parser;
}

//generate a sentence: ternary decisions
ArcStandardParser ArcStandardParseModel::generateSentence(const boost::shared_ptr<ParsedWeightsInterface>& weights, 
        MT19937& eng) {
  unsigned sent_limit = 100;
  ArcStandardParser parser;
  bool terminate_shift = false;
  parser.push_tag(0);
  parser.shift(0);
    
  do {
    kAction a = kAction::sh; //placeholder action
    //std::cerr << "arcs: " << parser.arcs().size() << std::endl;
    
    if (parser.stack_depth() < 2) {
      a = kAction::sh;
    } else if (parser.size() >= sent_limit) {
        // check to upper bound sentence length
        //if (!terminate_shift)
        //  std::cout << " LENGTH LIMITED ";
        terminate_shift = true;
        a = kAction::re;
    } else {
      Words r_ctx = parser.actionContext();
      double shiftp = weights->predictAction(static_cast<WordId>(kAction::sh), r_ctx);
      double leftarcreducep = weights->predictAction(static_cast<WordId>(kAction::la), r_ctx);
      double rightarcreducep = weights->predictAction(static_cast<WordId>(kAction::ra), r_ctx);

      if (parser.stack_depth() == 2)
        leftarcreducep = L_MAX;

      //sample an action
      std::vector<double> distr = {shiftp, leftarcreducep, rightarcreducep};
      multinomial_distribution_log mult(distr); 
      WordId act = mult(eng);
      //std::cout << "(" << parser.stack_depth() << ") ";
      //std::cout << act << " ";
      parser.add_particle_weight(distr[act]);
      
      if (act==0) {
        a = kAction::sh;
      } else if (act==1) {
        a = kAction::la; 
        parser.leftArc();
      } else {
        a = kAction::ra;
        parser.rightArc();
      }
    } 

    if (a == kAction::sh) {
      //sample a tag - disallow root tag
      Words t_ctx = parser.tagContext();
      std::vector<double> t_distr(weights->numTags() - 1, L_MAX);
      for (WordId w = 1; w < weights->numTags(); ++w) 
        t_distr[w-1] = weights->predictTag(w, t_ctx); 
      multinomial_distribution_log t_mult(t_distr);
      WordId tag = t_mult(eng) + 1;

      double tagp = weights->predictTag(tag, t_ctx); 
      parser.push_tag(tag);
      parser.add_particle_weight(tagp);

      //sample a word 
      Words w_ctx = parser.wordContext();
      std::vector<double> w_distr(weights->numWords(), 0);

      w_distr[0] = weights->predictWord(-1, w_ctx); //unk probability
      for (WordId w = 1; w < weights->numWords(); ++w) 
        w_distr[w] = weights->predictWord(w, w_ctx); 
      multinomial_distribution_log w_mult(w_distr);
      WordId word = w_mult(eng);
      if (word==0)
        word = -1;

      double wordp = weights->predictWord(word, w_ctx); 
      parser.shift(word);
      parser.add_particle_weight(wordp);
    }
  } while (!parser.inTerminalConfiguration() && !terminate_shift);
  //std::cout << std::endl;

  //std::cout << std::endl;
  return parser;
}

void ArcStandardParseModel::extractSentence(const ParsedSentence& sent, 
          const boost::shared_ptr<ParseDataSet>& examples) {
  ArcStandardParser parse = staticGoldParseSentence(sent); 
  parse.extractExamples(examples);
}

void ArcStandardParseModel::extractSentence(ParsedSentence& sent, 
          const boost::shared_ptr<ParsedWeightsInterface>& weights, 
          const boost::shared_ptr<ParseDataSet>& examples) {
  ArcStandardParser parse = staticGoldParseSentence(sent, weights);
  //std::cout << "Gold actions: ";
  //parse.print_actions();
  parse.extractExamples(examples);
}

double ArcStandardParseModel::evaluateSentence(const ParsedSentence& sent, 
          const boost::shared_ptr<ParsedWeightsInterface>& weights, 
          const boost::shared_ptr<AccuracyCounts>& acc_counts,
          size_t beam_size) {
  ArcStandardParser parse = beamParseSentence(sent, weights, beam_size);
  acc_counts->countAccuracy(parse, sent);
  return parse.particle_weight();
}

}

