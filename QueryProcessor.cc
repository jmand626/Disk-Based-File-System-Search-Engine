/*
 * Copyright Â©2024 Hannah C. Tang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Autumn Quarter 2024 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include "./QueryProcessor.h"

#include <algorithm>
#include <iostream>
#include <list>
#include <string>
#include <vector>

extern "C" {
#include "./libhw1/CSE333.h"
}

using std::list;
using std::sort;
using std::string;
using std::vector;

namespace hw3 {

// This structure is used to store a index-file-specific query result.
typedef struct {
  DocID_t doc_id;  // The document ID within the index file.
  int rank;        // The rank of the result so far.
} IdxQueryResult;


// Declaration before defintion
// A helper function that will return either nothing or a vector full of query
// results, given a vector of strings as the querys, a indextablereader,
// and a doctable reader
static vector<QueryProcessor::QueryResult> ProcessQueryHelper(
    const vector<string>& query, IndexTableReader* itr, DocTableReader* dtr);


QueryProcessor::QueryProcessor(const list<string>& index_list, bool validate) {
  // Stash away a copy of the index list.
  index_list_ = index_list;
  array_len_ = index_list_.size();
  Verify333(array_len_ > 0);

  // Create the arrays of DocTableReader*'s. and IndexTableReader*'s.
  dtr_array_ = new DocTableReader*[array_len_];
  itr_array_ = new IndexTableReader*[array_len_];

  // Populate the arrays with heap-allocated DocTableReader and
  // IndexTableReader object instances.
  list<string>::const_iterator idx_iterator = index_list_.begin();
  for (int i = 0; i < array_len_; i++) {
    FileIndexReader fir(*idx_iterator, validate);
    dtr_array_[i] = fir.NewDocTableReader();
    itr_array_[i] = fir.NewIndexTableReader();
    idx_iterator++;
  }
}

QueryProcessor::~QueryProcessor() {
  // Delete the heap-allocated DocTableReader and IndexTableReader
  // object instances.
  Verify333(dtr_array_ != nullptr);
  Verify333(itr_array_ != nullptr);
  for (int i = 0; i < array_len_; i++) {
    delete dtr_array_[i];
    delete itr_array_[i];
  }

  // Delete the arrays of DocTableReader*'s and IndexTableReader*'s.
  delete[] dtr_array_;
  delete[] itr_array_;
  dtr_array_ = nullptr;
  itr_array_ = nullptr;
}

vector<QueryProcessor::QueryResult> QueryProcessor::ProcessQuery(
    const vector<string>& query) const {
  Verify333(query.size() > 0);

  // STEP 1.
  // (the only step in this file)
  vector<QueryProcessor::QueryResult> final_result;
  int size = array_len_;

  if (query.size() <= 0) return final_result;
  // We need to ensure that our vector has words before we start querying


  for (int i = 0; i < size; i++) {
    vector<QueryProcessor::QueryResult> resultHelper =
    ProcessQueryHelper(query, itr_array_[i], dtr_array_[i]);
    // Here we take our total query, and our corresponding doctable and index
    // table to process our query
    // This just moves from result to final_result
    for (QueryProcessor::QueryResult& res : resultHelper) {
      final_result.push_back(res);
    }
  }

  // Sort the final results.
  sort(final_result.begin(), final_result.end());
  return final_result;
}

static vector<QueryProcessor::QueryResult> ProcessQueryHelper(
    const vector<string>& query, IndexTableReader* itr, DocTableReader* dtr) {
  vector<IdxQueryResult> current_result;
  // current result is our running result storage
  // The final piece of this function is to take each of the query that we
  // individually isolated and parsed, lookup their doc id, and use that to
  // properly recalculate its name and rank
  vector<QueryProcessor::QueryResult> final_results;

  DocIDTableReader* docID_table_reader = itr->LookupWord(query[0]);
  if (docID_table_reader == nullptr) return final_results;
  // A doc id is obviously useful because it contains word posistions, but we
  // want to check for the first query to ensure that there is something there
  // at all
  list<DocIDElementHeader> docIDheader_list =
  docID_table_reader->GetDocIDList();
  for (DocIDElementHeader& head : docIDheader_list) {
    // For each docid head, there is a docid table, and there is a word too
    IdxQueryResult idxQResult;

    idxQResult.rank = head.num_positions;
    idxQResult.doc_id = head.doc_id;
    current_result.push_back(idxQResult);
    // We use this idQResult later to build up the return values
  }
  delete docID_table_reader;
  // Since we dont need this struct anymore, it was really only there to help
  // us get the data, not use it

  if (query.size() == 1) {
    // You will see this for loop at the end of this function too. Why? Because
    // it serves two distinct purposes -> Here, its for cases where only one
    // query is sent in. Later, its for multiple word queries
    for (IdxQueryResult& r : current_result) {
      string doc_name;
      QueryProcessor::QueryResult queryRes;
      // We need another QueryResult here to build up our data, and we use a
      // string as a return parameter

      dtr->LookupDocID(r.doc_id, &doc_name);
      queryRes.document_name = doc_name;
      queryRes.rank = r.rank;
      // Building up the new object we made and sending it out
      final_results.push_back(queryRes);
    }
    return final_results;
  }

  for (size_t i = 1; i < query.size(); i++) {
    docID_table_reader = itr->LookupWord(query[i]);
    // Now this is where the 'meat' of ranking and calculating how many times
    // a word appears goes.

    if (docID_table_reader == nullptr) {
      current_result.clear();
      break;
    }
    // Also pretty much the same as last time, checking for any values is always
    // a good safeguard

    for (auto it = current_result.begin(); it != current_result.end();) {
      // Here we use the weird C++ std style iterators to go through our
      // current set of queries
      list<DocPositionOffset_t> pos;
      // now we can finally add the amount of occurences we actually get!
      if (docID_table_reader->LookupDocID(it->doc_id, &pos)) {
        it->rank += pos.size();
        // Its not a increment to rank, since we dont go in sequentially and
        // count occurences. The value has already been stored for us
        it++;
      } else {
        it = current_result.erase(it);
        // We just move on. Read syntax, from lecture code
      }
    }

    delete docID_table_reader;
    // As we did with the previous case, clean up our memory here too
  }

  for (IdxQueryResult& r : current_result) {
    string doc_name;
    QueryProcessor::QueryResult queryRes;
    // And now we see this exact same loop here. Why? This is the case for when
    // a query has multiple words in its quueries
    dtr->LookupDocID(r.doc_id, &doc_name);
    queryRes.document_name = doc_name;
    queryRes.rank = r.rank;
    final_results.push_back(queryRes);
  }

  return final_results;
}



}  // namespace hw3
