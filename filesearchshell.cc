/*
 * Copyright ©2024 Hannah C. Tang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Autumn Quarter 2024 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <cstdlib>    // for EXIT_SUCCESS, EXIT_FAILURE
#include <iostream>   // for std::cout, std::cerr, etc.
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#include "./QueryProcessor.h"

using std::cerr;
using std::endl;
using std::cout;
using std::cin;
using std::string;
using std::vector;
using std::stringstream;
using std::list;

// Error usage message for the client to see
// Arguments:
// - prog_name: Name of the program
static void Usage(char* prog_name);

// Reads input from cin and splits it into lowercase Splits stored in a vector
// It takes a pointer to a std vector of std strings. We use it here to avoid
// copying the query into our helper
static bool QueryHelper(vector<string>* query);

// Your job is to implement the entire filesearchshell.cc
// functionality. We're essentially giving you a blank screen to work
// with; you need to figure out an appropriate design, to decompose
// the problem into multiple functions or classes if that will help,
// to pick good interfaces to those functions/classes, and to make
// sure that you don't leak any memory.
//
// Here are the requirements for a working solution:
//
// The user must be able to run the program using a command like:
//
//   ./filesearchshell ./foo.idx ./bar/baz.idx /tmp/blah.idx [etc]
//
// i.e., to pass a set of filenames of indices as command inputLine
// arguments. Then, your program needs to implement a loop where
// each loop iteration it:
//
//  (a) prints to the console a prompt telling the user to input the
//      next query.
//
//  (b) reads a white-space separated list of query Splits from
//      std::cin, converts them to lowercase, and constructs
//      a vector of c++ strings out of them.
//
//  (c) uses QueryProcessor.cc/.h's QueryProcessor class to
//      process the query against the indices and get back a set of
//      query results.  Note that you should instantiate a single
//      QueryProcessor  object for the lifetime of the program, rather
//      than  instantiating a new one for every query.
//
//  (d) print the query results to std::cout in the format shown in
//      the transcript on the hw3 web page.
//
// Also, you're required to quit out of the loop when std::cin
// experiences EOF, which a user passes by pressing "control-D"
// on the console.  As well, users should be able to type in an
// arbitrarily long query -- you shouldn't assume anything about
// a maximum inputLine length.  Finally, when you break out of the
// loop and quit the program, you need to make sure you deallocate
// all dynamically allocated memory.  We will be running valgrind
// on your filesearchshell implementation to verify there are no
// leaks or errors.
//
// You might find the following technique useful, but you aren't
// required to use it if you have a different way of getting the
// job done.  To split a std::string into a vector of Splits, you
// can use a std::stringstream to get the job done and the ">>"
// operator. See, for example, "gnomed"'s post on stackoverflow for
// their example on how to do this:
//
//   http://stackoverflow.com/questions/236129/c-how-to-split-a-string
//
// (Search for "gnomed" on that page. They use an istringstream, but
// a stringstream gets the job done too.)
//
// Good luck, and write beautiful code!
int main(int argc, char** argv) {
  if (argc < 2) {
    Usage(argv[0]);
  }

  // STEP 1:
  // Implement filesearchshell!
  // Probably want to write some helper methods ...
  list<string> indexlist;


  for (int i = 1; i < argc; i++) {
    // Since argc is a count. We can use argv as an array here because its a
    // a char**
    indexlist.push_back(argv[i]);
  }

  hw3::QueryProcessor qprocess(indexlist);
  // We have one processor for this entire loop, AND NOT for each query
  // seperately

  while (1) {
    vector<string> query_input;
    // Remember when we declared that the helper needed a pointer to a vector
    // with a string in it. Well, we are almost there!

    cout << "Enter Query:" << endl;
    // pay special attention to the formatting, its needs be exact

    if (!QueryHelper(&query_input)) break;
    // The helper returns false if its gets no more values, so we need to do
    // the same here

    if (!query_input.empty()) {
      // Process the query and fetch results
      vector<hw3::QueryProcessor::QueryResult> results =
      qprocess.ProcessQuery(query_input);

      // Print results directly here
      if (results.empty()) {
        cout << "  [no results]" << endl;
        // Format gleamed from transcript, when submitted a qquery such as:
        // "tiny dust particles humidifier". There are two spaces in the begin-
        // ing of the response
      } else {
        for (size_t i = 0; i < results.size(); ++i) {
          cout << "  " << results[i].document_name << " (" << results[i].rank
          << ")" << endl;
          // There is not much here to say because this is more about just
          // looking at the sample transcript. We need a for loop because
          // results can have multiple Split-in-file return queries
        }
      }
    }
  }

  return EXIT_SUCCESS;
}

static bool QueryHelper(vector<string>* query) {
  string inputLine;
  if (!getline(cin, inputLine)) return false;

  stringstream ss(inputLine);
  string wordSplit;

  // Transform input to lowercase in place
  for (size_t i = 0; i < inputLine.length(); ++i) {
    inputLine[i] = tolower(inputLine[i]);
    // We have chosen to do this to NORMALIZE our inputs and compare them
  }

  // Use a stringstream to split the inputLine into words
  query->clear();
  while (ss >> wordSplit) {
    // It turns out this is a quick and easy way to split a string into words.
    // All we needed to do was this quick conversion to a stream

    // You might ask why the hell this loop is here, didnt we do alreadu have
    // this code? No, the above the code put the whole line to lowercase, which
    // apparently is not enough for filesearchshell to work
    for (size_t i = 0; i < wordSplit.length(); ++i) {
      wordSplit[i] = tolower(wordSplit[i]);
    }
    query->push_back(wordSplit);
  }

  return true;
}

// Already written by staff
static void Usage(char* prog_name) {
  cerr << "Usage: " << prog_name << " [index files+]" << endl;
  exit(EXIT_FAILURE);
}
