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

#ifndef HW3_DOCIDTABLEREADER_H_
#define HW3_DOCIDTABLEREADER_H_

#include <list>      // for std::list
#include <cstdio>    // for (FILE*)

#include "./HashTableReader.h"
#include "./LayoutStructs.h"

namespace hw3 {

// A DocIDTableReader reads a docid->positions table within an index file.
class DocIDTableReader : public HashTableReader {
 public:
  // Construct a new DocIDTableReader at a specific offset with an
  // index file.

  // Arguments:
  // - f: an open (FILE*) for the underlying index file.  The
  //   constructed object takes ownership of the (FILE*) and will
  //   fclose() it  on destruction.
  // - offset: the `docIDtable`'s byte offset within the file.
  DocIDTableReader(FILE* f, IndexFileOffset_t offset);
  ~DocIDTableReader() { }

  // Lookup a docID and get back a `std::list<DocPositionOffset_t>`
  // containing the positions listed for that docID.
  //
  // Arguments:
  // - doc_id:  the docID to look for within the `docIDtable`.
  // - ret_val: (output parameter) a `list<DocPositionOffset_t>` with the
  //   positions of the word in the document. If docID is not found, nothing
  //   is saved into `*ret_val`.
  //
  // Returns:
  // - true if the docID is found, false otherwise.
  bool LookupDocID(const DocID_t& doc_id,
                   std::list<DocPositionOffset_t>* const ret_val) const;

  // Reads all docID's from the docIDtable and builds a `DocIDElementHeader`
  // for each docID and the number of word positions a docID has.
  //
  // Returns:
  // - A list<DocIDElementHeader> including all docID's from the table
  std::list<DocIDElementHeader> GetDocIDList() const;

 private:
  // This friend declaration is here so that the Test_DocIDTableReader
  // unit test fixture can access protected member variables of
  // DocIDTableReader.
  friend class Test_DocIDTableReader;

  DISALLOW_COPY_AND_ASSIGN(DocIDTableReader);
};
}  // namespace hw3

#endif  // HW3_DOCIDTABLEREADER_H_
