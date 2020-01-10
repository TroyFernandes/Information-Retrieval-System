#include <fstream>
#include <stdio.h>
#include <string>
#include <iostream>
#include <unordered_set>
#include <istream>
#include <iterator>
#include <sstream>
#include <regex>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <numeric>

#include "serialization.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
#include "boost/timer/timer.hpp"
#include <boost/program_options.hpp>


#include "eval.h"
#include "porter_stemmer.h"

bool PORTERSTEMMING = 0;


std::vector<std::vector<double>> documentVec;
std::vector<double> idfVec;
std::map<double, std::string> documentTitles;
std::map < std::string, std::tuple<int, std::vector<std::tuple<int, int, std::string, std::string, int, std::string>>>> db;
std::map<int, std::string > authorMap;
std::map<int, int> lookupMap;
//pagerank scores
std::vector<double> scores;

double averageMAP = 0.0;
double averageRP = 0.0;

double w1;
double w2;

int main(int argc, const char *argv[]) {

	try
	{
		boost::program_options::options_description desc{ "Options" };
		desc.add_options()
			("help,h", "Help screen")
			("ps", "Enable porter stemming")
			("w1", boost::program_options::value<double>(), "w1 Value")
			("w2", boost::program_options::value<double>(), "w2 Value");


		boost::program_options::variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		boost::program_options::notify(vm);

		if (vm.count("help")) {
			std::cout << desc << '\n';
			return 0;
		}

		if (vm.count("ps")) {
			PORTERSTEMMING = true;
		}

		if (vm.count("w1")) {
			w1 = vm["w1"].as<double>();
		}
		else {
			w1 = 0.5;
		}

		if (vm.count("w2")) {
			w2 = vm["w2"].as<double>();
		}
		else {
			w2 = 0.5;
		}

	}
	catch (const boost::program_options::error &ex)
	{
		std::cerr << ex.what() << '\n';
		return 0;
	}






	{
		if (!doesFileExist("./postings.dat") || !doesFileExist("./lookup_table.dat") || !doesFileExist("./qrels.text") || !doesFileExist("./query.text")) {
			std::cout << "Missing one or more files [\"postings.dat\", \"lookup_table.dat\", \"qrels.text\", \"query.text\"]" << std::endl;
			std::cout << "Please copy these file to the current directory" << std::endl;

			return 0;
		}

		std::cout << "W1 Value = " << w1 << std::endl;
		std::cout << "W2 Value = " << w2 << std::endl;

		std::cout << "Reading the postings file..." << std::endl;

		std::ifstream ifs("./postings.dat");
		boost::archive::text_iarchive ar(ifs);
		// Load the data
		ar &db;

		std::cout << "Finished reading the postings file" << std::endl << std::endl;

		//Read the pagerank scores
		readPRScores(scores);

		//Read the lookup table
		readLookupTable(lookupMap);


		//Grab the last element of the map since thats where we saved the number of documents
		std::string x = db.rbegin()->first;

		//Remove the last element from the map
		db.erase(std::prev(db.end()));

		x = x.erase(0, 1);

		//Build the TFIDF 
		documentVec = buildTFIDF(db, std::stoi(x));

	}


	auto qMap = buildQueryMap();
	auto rdMap = buildRelevantList();

	evaluate(qMap, rdMap);

	std::cout << "Finished. Press Enter to exit" << std::endl;
	//Wait for keypress before exiting
	std::cin.ignore();

	return 0;

}



std::map<double, int> search(std::string originalQ, std::string query, std::map < std::string, std::tuple<int, std::vector<std::tuple<int, int, std::string, std::string, int, std::string>>>> & database) {

	std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) { return std::tolower(c); });

	auto queryVec = tfIDFQuery(query, database.size(), database);

	auto simMap = cosineSimilarity(documentVec, queryVec);

	return simMap;
}

//https://stackoverflow.com/questions/46292764/check-for-file-existence-in-c-without-creating-file
bool doesFileExist(const std::string& name) {
	std::ifstream f(name.c_str());
	return f.good();
}

std::vector<std::vector<double>> buildTFIDF(std::map < std::string, std::tuple<int, std::vector<std::tuple<int, int, std::string, std::string, int, std::string>>>> dict, int docNum) {

	std::map < int, std::vector<std::tuple<std::string, int>> > collection;

	for (auto it = dict.begin(); it != dict.end(); it++)
	{
		int documentID;
		int frequency;
		std::string word;

		std::vector<std::tuple<std::string, int>> tempWordFreq;
		word = it->first;
		for (auto tup : std::get<1>(it->second)) {

			tempWordFreq.clear();

			//std::tuple<int, int, std::string, std::string, int>
			documentID = std::get<0>(tup);
			frequency = std::get<4>(tup);

			//Add the document with the title to the documentTitle Map
			documentTitles.insert({ documentID, std::get<2>(tup) });

			authorMap.insert({ documentID, std::get<5>(tup) });

			std::tuple<std::string, int> temp = std::make_tuple<>(word, frequency);
			tempWordFreq.push_back(temp);

			std::sort(tempWordFreq.begin(), tempWordFreq.end());
			tempWordFreq.erase(std::unique(tempWordFreq.begin(), tempWordFreq.end()), tempWordFreq.end());

			std::map < int, std::vector<std::tuple<std::string, int>> >::iterator it2 = collection.find(documentID);


			// If the key's already present in the map...
			if (it2 != collection.end()) {
				(it2->second).insert((it2->second).end(), tempWordFreq.begin(), tempWordFreq.end());
			}
			//Else create a new entry for the new word
			else {
				collection.insert({ documentID, tempWordFreq });

			}
		}

	}



	std::string w;
	int position;

	std::vector<std::vector<double>> documents;

	for (auto x : collection) {
		std::vector<double> test(dict.size(), 0);

		//std::cout << "DOCUMENT: " << x.first;
		for (auto y : x.second) {
			w = std::get<0>(y);
			position = std::distance(dict.begin(), dict.find(w));

			test[position] = std::get<1>(y);

		}
		documents.push_back(test);
	}


	int totWords = 1;
	//Calculate frequency
	for (auto & x : documents) {
		//Count non zero elements
		totWords = countNonZero(x);

		for (std::size_t i = 0; i != x.size(); ++i) {
			// access element as v[i]
			x[i] = (x[i]) / totWords;
			//x[i] = (x[i]);
		}

	}


	//Calculate the IDF
	std::vector<double> idf(dict.size(), 0);
	double nonZero = 0;
	double idfVal;
	for (size_t j = 0; j < dict.size(); j++) {
		for (size_t i = 0; i < documents.size(); i++) {

			if (documents[i][j] > 0) {
				nonZero++;
			}

		}

		//idfVal = (log2(documents.size() / nonZero));
		idf[j] = 1.0 + (log2(documents.size() / nonZero));


		nonZero = 0;
		//std::cout << std::endl;

	}

	idfVec = idf;

	//Print the IDF vectors
	//std::cout << "IDF Vector" << std::endl;
	//for (auto x : idf) {
	//	std::cout << " " << x;
	//}
	//std::cout << std::endl;


	//Multiply the frequency vector with the IDF vector
	for (size_t i = 0; i < documents.size(); i++) {
		for (size_t j = 0; j < documents[i].size(); j++) {
			documents[i][j] = idf[j] * documents[i][j];
		}
	}

	return documents;
}

int countNonZero(const std::vector<double>& elems) {
	return std::count_if(elems.begin(), elems.end(), [](double c) {return c > 0; });
}

std::vector<double> tfIDFQuery(std::string query, int wordCount, std::map < std::string, std::tuple<int, std::vector<std::tuple<int, int, std::string, std::string, int, std::string>>>> &dict) {

	std::vector<double> queryVec(wordCount, 0);

	int totWords = 0;
	int index;
	std::stringstream s(query);
	std::string word;

	for (int i = 0; s >> word; i++)
	{

		index = std::distance(dict.begin(), dict.find(word));
		if (index != dict.size()) {
			queryVec[index]++;
			//std::cout << word << " Index: " << index << std::endl;
		}
	}

	//Count the total number of words in the query
	//This counts maximum frequency, i.e if a words appears more than once in the query, don't count it multiple times
	for (auto x : queryVec) {
		if (x != 0.0) {
			totWords++;
		}
	}


	for (std::size_t i = 0; i != queryVec.size(); ++i) {
		queryVec[i] = queryVec[i] / totWords;
	}

	//std::cout << "Query Vector:";
	//for (auto x : queryVec) {
	//	std::cout << " " << x;
	//}
	//std::cout << std::endl;

	return queryVec;
}

std::map<double, int> cosineSimilarity(std::vector<std::vector<double>> docVec, std::vector<double> qVec) {


	//Multiply the idf vector with the query vector
	for (size_t i = 0; i < idfVec.size(); i++) {
		qVec[i] = qVec[i] * idfVec[i];
	}

	std::cout << std::endl;
	//Caculate the length of the documents
	std::vector<double> docLength;
	for (auto x : docVec) {
		double length = 0;
		for (auto y : x) {
			if (y != 0.0) {
				length += pow(y, 2);
			}
		}
		length = sqrt(length);
		docLength.push_back(length);
	}

	//Calculate the length of the query vector
	double qLength = 0;
	for (auto y : qVec) {
		if (y != 0.0) {
			qLength += pow(y, 2);
		}
	}
	qLength = sqrt(qLength);

	//<Cosine Score, Doc ID>
	std::map<double, int> cosineSimMap;

	double mostRelevant = 0.0;
	int a = 0;
	for (size_t i = 0; i < docVec.size(); i++) {
		double cosineSim;
		cosineSim = std::inner_product(std::begin(docVec[i]), std::end(docVec[i]), std::begin(qVec), 0.0);
		cosineSim = cosineSim / (docLength[i] * qLength);
		//std::cout << "Cosine Sim (D" << i + 1 << ", q): " << cosineSim << std::endl;

		double score = (w1 * cosineSim) + (w2*scores[i]);

		cosineSimMap.insert({ score, i + 1 });

	}

	auto results = topK(cosineSimMap, 10);

	return results;
}

std::map<double, int> topK(std::map<double, int> cosineSimMap, int K) {

	std::map<double, int> results;

	int counter = 0;
	std::cout << std::endl;
	std::cout << "Document Ranking:" << std::endl;

	for (auto iter = cosineSimMap.rbegin(); iter != cosineSimMap.rend(); ++iter) {
		if (K == 0) {
			break;
		}
		std::cout << std::endl << ++counter << ")" << std::endl;
		std::cout << "DocID: " << iter->second << std::endl;
		try
		{
			int scoreIndex;
			scoreIndex = lookupMap.at(iter->second);
			std::cout << "PageRank Score:" << scores[scoreIndex] << std::endl;
		}
		catch (std::out_of_range& const e)
		{
			std::cout << "PageRank Score: N/A" << std::endl;
		}

		std::map<double, std::string>::const_iterator pos = documentTitles.find(iter->second);

		if (!(pos == documentTitles.end())) {
			std::cout << "Title: " << pos->second << std::endl;
		}
		std::cout << "Score: " << iter->first << std::endl;
		std::cout << "Author: " << authorMap.at(iter->second) << std::endl;

		results.insert({ iter->first,iter->second });

		K--;
	}

	return results;

}

std::map<int, std::string> buildQueryMap() {

	std::map<int, std::string> queryMap;

	std::string docID, title;


	//Load CACM File
	std::ifstream data_store("./query.text");

	//Go line by line through the CACM file
	for (std::string line; std::getline(data_store, line); ) {
		//Find the line contiaing the document ID
		std::string temporary;
		if (line.rfind(".I", 0) == 0 && line.rfind(".I", 0) != std::string::npos) {



			//std::cout << docID << std::endl;
			//std::cout << title << std::endl;

			//String cleaning below
			char chars[] = "()[]-,:''\".+;$!&*><=/^|`0123456789%{}";
			for (unsigned int i = 0; i < strlen(chars); ++i)
			{
				title.erase(std::remove(title.begin(), title.end(), chars[i]), title.end());
			}


			if (!docID.compare("") == 0) {
				queryMap.insert({ std::stoi(docID), title });

			}



			//This is the reset point where every variable gets reset
			title.clear();


			//We know the doc ID is contained in a line where the first 3 charachters are ".I "
			docID = (line.erase(0, 3));
		}

		//Find the line containg the abstract text
		if (line.rfind(".W", 0) == 0 && line.rfind(".W", 0) != std::string::npos) {
			//Keep getting new lines until you reach any of the .W whatever
			std::getline(data_store, line);

			//Keep on reading the lines until you find the line starting with .B since this is where the abstract ends
			while (!(line.rfind(".N", 0) == 0 && line.rfind(".N", 0) != std::string::npos)) {
				title += " " + line;
				std::getline(data_store, line);
			}

		}



	}


	return queryMap;
}


std::map<int, std::vector<int>> buildRelevantList() {

	std::map<int, std::vector<int>> relDocList;

	int queryID, relevantDocID;


	//Load CACM File
	std::ifstream data_store("./qrels.text");

	//Go line by line through the CACM file
	for (std::string line; std::getline(data_store, line); ) {

		std::string tempLine;

		tempLine = line;

		queryID = std::stoi(tempLine.substr(0, 2));

		relevantDocID = std::stoi(tempLine.substr(3, 4));


		std::map<int, std::vector<int>>::iterator it2 = relDocList.find(queryID);

		// If the key's already present in the map...
		if (it2 != relDocList.end()) {
			(it2->second).push_back(relevantDocID);
		}
		//Else create a new entry 
		else {
			std::vector <int> temp({ relevantDocID });
			relDocList.insert({ queryID, temp });
		}
	}

	return relDocList;
}

void evaluate(std::map<int, std::string> qMap, std::map<int, std::vector<int>> relDocList) {
	int counter = 1;
	for (auto it = qMap.begin(); it != qMap.end(); it++)
	{
		std::cout << std::endl << "*********** QUERY ID: " << it->first << "***********" << std::endl;

		std::string query;
		query = it->second;

		//Porter Stemming
		if (PORTERSTEMMING) {

			char *cstr = new char[query.length() + 1];
			strcpy(cstr, query.c_str());

			int end = stem(cstr, 0, std::strlen(cstr) - 1);
			cstr[end + 1] = 0;
			std::string stemmedWord = std::string(cstr);
			query = stemmedWord;
			delete[] cstr;
		}

		std::map<double, int> results = search(query, query, db);
		std::vector<int> resultSet;
		for (auto it2 = results.begin(); it2 != results.end(); it2++)
		{
			resultSet.push_back(it2->second);
		}

		//The judged documents set
		std::vector<int> judgedSet = relDocList[counter];


		std::vector<int> intersection;
		std::set_intersection(resultSet.begin(), resultSet.end(), judgedSet.begin(), judgedSet.end(), std::back_inserter(intersection));

		std::cout << "Judged: ";
		for (auto x : judgedSet) {
			std::cout << x << " ";
		}
		std::cout << std::endl;

		std::cout << "Returned: ";
		for (auto x : resultSet) {
			std::cout << x << " ";
		}
		std::cout << std::endl;

		std::cout << "Common: ";
		for (auto x : intersection) {
			std::cout << x << " ";
		}
		std::cout << std::endl;

		meanAveragePrecision(judgedSet, resultSet, intersection);

		counter++;
		//break;
	}

	std::cout << std::endl << "********* RESULTS ***********" << std::endl;
	std::cout << "Number of Queries: " << qMap.size() << std::endl;
	std::cout << "Average MAP: " << averageMAP / (double)qMap.size() << std::endl;
	std::cout << "Average R-Precision: " << averageRP / (double)qMap.size() << std::endl;
	std::cout << std::endl << "*****************************" << std::endl;

}

void meanAveragePrecision(std::vector<int> knownRelevant, std::vector<int> returnedSet, std::vector<int> common) {

	double precision = 0.0;
	double RP = 0.0;

	for (size_t i = 0; i < common.size(); i++) {
		for (size_t j = 0; j < returnedSet.size(); j++) {
			if (common[i] == returnedSet[j]) {
				precision += (((double)i + 1.0) / ((double)j + 1.0));
			}
		}
	}

	precision = precision / (double)returnedSet.size();
	averageMAP += precision;
	std::cout << "MAP: " << precision << std::endl;
	if (knownRelevant.size() != 0) {
		RP = ((double)common.size() / (double)knownRelevant.size());
	}
	else {
		RP = 0;
	}
	averageRP += RP;
	std::cout << "R-Precision: " << RP << std::endl;
}

void readPRScores(std::vector<double>& scores) {
	std::ifstream ifs("./pagerank_scores.dat");
	boost::archive::text_iarchive ar(ifs);
	// Load the data
	ar &scores;
}

void readLookupTable(std::map<int, int>& lookupTable) {
	std::ifstream ifs("./lookup_table.dat");
	boost::archive::text_iarchive ar(ifs);
	// Load the data
	ar &lookupTable;
}

std::vector<double> normalize(std::vector<double> &arr) {

	auto min = *std::min_element(std::begin(arr), std::end(arr));
	auto max = *std::max_element(std::begin(arr), std::end(arr));

	for (std::size_t i = 0; i != arr.size(); ++i) {
		arr[i] = (arr[i] - min) / (max - min);
	}

	return arr;
}

