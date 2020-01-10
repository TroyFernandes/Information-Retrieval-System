# Example Information Retrieval System
This project is a sample IR System using the CACM document collection. The [Boost](https://www.boost.org/) library is used to serialize the datastructure (map) holding the document information (the inverted index)

The document collection used is the [CACM Collection](http://ir.dcs.gla.ac.uk/resources/test_collections/cacm/)

The program is split into three different parts:

1. ``invert`` : Creates the inverted index for the document collection. The output are files which are needed for the other two programs
2. ``search`` : Provides a command line interface to search for documents within the collection. This program uses the output files produced by the ``invert`` program
3. ``eval`` : Evaluates the results of the searching algorithm using the "query.text" and "qrels.text" files found within the CACM zip

# How to Run

### Invert

This program generates all the files necessary for both ``search`` and ``eval``. To run this program, compile and run the .exe

1. Required Files (Must be in same directory):

- cacm.all : required
- stopwords.txt : optional

2. Command Line Options:

- **--help** : get a list of available command line options
- **--stopwords** : enable the removal of stopwords
- **--ps** : enable porter stemming
- **--decay** number : specify the decay value for the random surfer model where _number_ is a
    decimal number between 0 and 1
- **--iterations** _number_ : specify the number of iterations for the PageRank step where _number_ is
    the number of iterations
- **--norandomsurfer** : disable the random surfer model (is on by default)
- **--normalize** : turn off normalization (is off by default)

Example: ``invert.exe --nonormalize --decay 0.93 --iterations 10``


3. Output Files:

- lookup_table.dat : a helper file needed for ``eval`` and ``search``
- pagerank_scores.dat : binary formatted pagerank scores needed for ``eval`` and ``search``
- postings.dat : a binary formatted postings file needed for ``eval`` and ``search``
- pagerank_scores.txt : a readable text file with all the pagerank scores. Not needed by any other
    programs

### Eval

This program goes through the query.text file and executes the search function. To run this program,
simply run the .exe

1. Required Files (Must be in the same directory):

- lookup_table.dat : required
- pagerank_scores.dat : required
- postings.dat : required
- qrels.text : required
- query.text : required
- stopwords.txt : optional

2. Command Line Options:

- **--help** : get a list of available command line options
- **--ps** : enable porter stemming
- **--w1** _number_ : specify the w1 value where number is the value
- **--w 2** _number_ : specify the w 2 value where number is the value

Example: ``eval3.exe --w1 0.7 --w2 0``.

3. Output Files: None

### Search


This program takes the users search query and returns the relevant results. To run this program, simply
run the .exe

1. Required Files (Must be in the same directory):

- lookup_table.dat : required
- pagerank_scores.dat : required
- postings.dat : required
- stopwords.txt : optional


2. Command Line Options:

- **--help** : get a list of available command line options
- **--ps** : enable porter stemming
- **--w1** _number_ : specify the w1 value where number is the value
- **--w 2** _number_ : specify the w 2 value where number is the value

Example: ``search.exe --w1 0.7 --w2 0``.



