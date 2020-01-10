# Example Information Retrieval System
This project is a sample IR System using the CACM document collection. The [Boost](https://www.boost.org/) library is used to serialize the datastructure (map) holding the document information (the inverted index)

The document collection used is the [CACM Collection](http://ir.dcs.gla.ac.uk/resources/test_collections/cacm/)

The program is split into three different parts:

1. ``invert`` : Creates the inverted index for the document collection. The output are files which are needed for the other two programs
2. ``search`` : Provides a command line interface to search for documents within the collection. This program uses the output files produced by the ``invert`` program
3. ``eval`` : Evaluates the results of the searching algorithm using the "query.text" and "qrels.text" files found within the CACM zip

