#############################################
# Libraries
#############################################

add_libraries(lbl
  class_distribution.cc
  context_cache.cc
  context_processor.cc
  factored_metadata.cc
  factored_weights.cc
  metadata.cc
  minibatch_words.cc
  model_utils.cc
  parsed_factored_metadata.cc
  parsed_factored_weights.cc
  tagged_parsed_factored_metadata.cc
  tagged_parsed_factored_weights.cc
  weights.cc
  word_distributions.cc
  word_to_class_index.cc
)

target_link_libs(lbl corpus rt ${Boost_LIBRARIES})

