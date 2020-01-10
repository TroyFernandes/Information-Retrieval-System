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
#include <cmath>
#include <numeric>
#include <thread>

#include "serialization.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/program_options.hpp>


#include "invert.h"
#include "porter_stemmer.h"

bool STOPWORDS = 0;
bool PORTERSTEMMING = 0;
bool DECAY = 1;
bool NORMALIZE = 0;
std::string stopwordsPath;

double decay;
int ITERATIONS;

int main(int argc, const char *argv[])
{
	try
	{
		boost::program_options::options_description desc{ "Options" };
		desc.add_options()
			("help,h", "Help screen")
			("stopwords", "Enable the removal of stopwords")
			("ps", "Enable porter stemming")
			("decay", boost::program_options::value<double>(), "specify the decay value for the random surfer model (decimal num between [0, 1])")
			("iterations", boost::program_options::value<double>(), "set the number of iterations for the PageRank step")
			("norandomsurfer", "disable the randomsurfer model")
			("normalize", "turn on normalization");


		boost::program_options::variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		boost::program_options::notify(vm);

		if (vm.count("help")) {
			std::cout << desc << '\n';
			return 0;
		}

		if (vm.count("stopwords")) {
			STOPWORDS = true;
		}
		if (vm.count("ps")) {
			PORTERSTEMMING = true;
		}
		if (vm.count("norandomsurfer")) {
			DECAY = false;
		}
		if (vm.count("normalize")) {
			NORMALIZE = true;
		}
		if (vm.count("decay")) {
			decay = vm["decay"].as<double>();
		}
		else {
			decay = 0.85;
		}
		if (vm.count("iterations")) {
			ITERATIONS = vm["iterations"].as<double>();
		}
		else {
			ITERATIONS = 20;
		}
	}
	catch (const boost::program_options::error &ex)
	{
		std::cerr << ex.what() << '\n';
		return 0;
	}

	std::cout << "PageRank Iterations: " << ITERATIONS << std::endl;
	if (PORTERSTEMMING) {
		std::cout << "Porter Stemming: ON" << std::endl;
	}
	else {
		std::cout << "Porter Stemming: OFF" << std::endl;
	}

	if (NORMALIZE) {
		std::cout << "Normalization: ON" << std::endl;
	}
	else {
		std::cout << "Normalization: OFF" << std::endl;
	}

	if (DECAY) {
		std::cout << "Random Surfer Model: ON" << std::endl;
		std::cout << "Decay Value: " << decay << std::endl;
	}
	else {
		std::cout << "Random Surfer Model: OFF" << std::endl;
	}

	//Check if the CACM file exists
	if (!doesFileExist("./cacm.all")) {
		std::cout << "The CACM file does not exist. Please place the file in the current directory. Press Enter to exit" << std::endl;
		std::cin.ignore();
		return 0;
	}

	std::string cacmFilePath = "./cacm.all";

	//Thread for the inverting step
	std::thread invThread(invert, cacmFilePath);


	//Thread for the pagerank calculation
	std::thread prThread(calculatePageRank, cacmFilePath);

	//Join the threads
	invThread.join();
	prThread.join();

	std::cout << "*** Finished creating output files. Press Enter to exit ***" << std::endl;
	std::cin.ignore();
	return 0;
}

bool readStopWords(std::string file, std::unordered_set<std::string> &stopwords) {

	// Open the File
	std::ifstream in(file.c_str());

	// Check if object is valid
	if (!in)
	{
		std::cerr << "Cannot open the File : " << file << std::endl;
		return false;
	}

	std::string str;
	while (std::getline(in, str))
	{
		if (str.size() > 0) {
			stopwords.insert(str);
		}
	}
	//Close The File
	in.close();
	return true;
}


void serializeMap(std::map < std::string, std::tuple<int, std::vector<std::tuple<int, int, std::string, std::string, int, std::string>>>> & database) {
	std::ofstream ofs("postings.dat");
	boost::archive::text_oarchive ar(ofs);
	// Save the data
	ar &database;
}


//https://stackoverflow.com/questions/46292764/check-for-file-existence-in-c-without-creating-file
bool doesFileExist(const std::string& name) {
	std::ifstream f(name.c_str());
	return f.good();
}


void printVec(std::string description, std::vector < double >& vec) {

	std::cout << description << std::endl;
	for (std::size_t i = 0; i != vec.size(); ++i) {

		std::cout << i << ": " << vec[i] << std::endl;

	}

}

void calculatePageRank(std::string filePath) {
	std::cout << "Calculating PageRank scores" << std::endl;
	//Load CACM File
	std::ifstream resource(filePath);

	std::map< int, std::set<int>> citations;

	std::string line;
	while (std::getline(resource, line)) {

		if (line.rfind(".X", 0) == 0 && line.rfind(".X", 0) != std::string::npos) {

			std::getline(resource, line);

			//Keep on reading the lines until you find the line starting with .B since this is where the abstract ends
			while (!(line.rfind(".I", 0) == 0 && line.rfind(".I", 0) != std::string::npos) && !resource.eof()) {

				std::string s = line;
				std::istringstream iss(s);

				std::vector<int> result((std::istream_iterator<int>(iss)), std::istream_iterator<int>());

				std::map < int, std::set<int> > ::iterator it = citations.find(result[2]);

				if (result[1] == 5 && result[0] != result[2]) {

					// If the key's already present in the map...
					if (it != citations.end()) {
						it->second.insert(result[0]);
					}
					//Else create a new entry for the new word
					else {
						std::set<int> temp;
						temp.insert(result[0]);

						citations.insert({ result[2],temp });

					}

				}

				std::getline(resource, line);
			}

		}

	}


	//TESTING
	//citations.clear();
	//citations.insert({ 1, {2, 3} });
	//citations.insert({ 2, {4} });
	//citations.insert({ 3, {1, 2, 4} });
	//citations.insert({ 4, {} });
	//TESTING

	//We need to fill the empty spots aka the disconnected documents in the graph representation
	int maxDocNum = citations.rbegin()->first;
	for (int i = 1; i <= maxDocNum; i++) {
		citations.insert({ i, {} });
	}


	std::map<int, int> lookupMap;
	std::map<int, int> lookupMapRev;
	int lookupCounter = 0;
	for (auto const& val : citations) {
		lookupMapRev.insert({ lookupCounter, val.first });
		lookupMap.insert({ val.first, lookupCounter });
		lookupCounter++;
	}

	auto citationsCopy = citations;
	citations.clear();
	for (auto const& x : citationsCopy) {
		std::set<int> temp;
		for (auto y : x.second) {
			temp.insert({ lookupMap.at(y) });
		}
		citations.insert({ lookupMap.at(x.first), temp });
	}

	//Print the lookup map relations
	//for (auto const& x : lookupMap) {
	//	std::cout << x.first << " -> " << x.second << std::endl;
	//}


	//Save the lookupmap to disk
	serializeLookupMap(lookupMap);

	//Iteration Matrix
	std::vector<std::vector<double>> iteration(ITERATIONS + 1, std::vector<double>(citations.size()));

	iteration[0] = std::vector<double>(citations.size(), 1.0 / citations.size());
	//printVec("Iteration 0", iteration[0]);



	for (int i = 1; i < ITERATIONS + 1; i++) {
		//Choose a random node
		int node = 0;
		for (auto const& nodeVal : citations) {
			node = nodeVal.first;
			//Find all the nodes pointing to the chosen node
			std::vector<int> links;
			for (auto const& x : citations)
			{
				for (auto edge : x.second) {
					if (edge == node) {
						links.push_back(x.first);
					}
				}
			}

			//In the list of the links, find how many outgoing links there are
			std::vector < std::pair<int, int>> outgoing;
			for (auto edge : links) {
				outgoing.push_back(std::make_pair(edge, citations.at(edge).size()));
			}

			//Update the vector
			if (outgoing.empty()) {
				iteration[i][node] = 1.0 / citations.size();
			}
			for (auto pair : outgoing) {
				//iteration[i][node] += decay * (iteration[i - 1][pair.first] / (double)pair.second);
				iteration[i][node] += (iteration[i - 1][pair.first] / (double)pair.second);

			}

			//Decay (Random Surfer)
			if (DECAY) {
				iteration[i][node] = (decay / citations.size()) + (1 - decay)*(iteration[i][node]);
			}

		}
		//std::string currentIter = "Iteration " + std::to_string(i);
		//printVec(currentIter, iteration[i]);

	}

	//Get the total of the pagerank scores (Should equal 1)
	double sum = 0;
	std::for_each(iteration.back().begin(), iteration.back().end(), [&](double n) {
		sum += n;
	});

	//Rank the documents
	std::vector<double> result;

	if (NORMALIZE) {
		result = normalize(iteration.back());
	}
	else {
		result = iteration.back();
	}

	serializePageRankScore(result);

	//Write to file
	writeResults(result);


	//printVec("Final Score", result);


	std::cout << "PageRank Total = " << sum << std::endl;

	std::cout << "*** Finished Calculating Page Rank Scores ***" << std::endl;



}

void serializePageRankScore(std::vector<double> &scores) {
	std::ofstream ofs("pagerank_scores.dat");
	boost::archive::text_oarchive ar(ofs);
	ar &scores;
}

void invert(std::string filePath) {

	//Load stopwords file
	std::unordered_set<std::string> stopwords;

	if (!doesFileExist("./stopwords.txt")) {
		STOPWORDS = false;
	}
	else {
		std::string filepath = "./stopwords.txt";
		readStopWords(filepath, stopwords);
	}

	std::ifstream data_store(filePath);
	std::string docID, title, abstract, author;

	std::map < std::string, std::tuple<int, std::vector<std::tuple<int, int, std::string, std::string, int, std::string>>>> dict;

	std::cout << "Building Inverted Index..." << std::endl;


	int docCount = 0;

	//Go line by line through the CACM file
	for (std::string line; std::getline(data_store, line); ) {
		//Find the line contiaing the document ID
		std::string temporary;
		if (line.rfind(".I", 0) == 0 && line.rfind(".I", 0) != std::string::npos) {

			std::string currentDoc = line;
			docCount = std::stoi(currentDoc.erase(0, 3));

			//String cleaning below
			char chars[] = "()[]-,:''\".+;$!&*><=/^|`0123456789%{}";
			for (unsigned int i = 0; i < strlen(chars); ++i)
			{
				title.erase(std::remove(title.begin(), title.end(), chars[i]), title.end());
				abstract.erase(std::remove(abstract.begin(), abstract.end(), chars[i]), abstract.end());
			}


			//convert the line to all lowercase
			std::transform(title.begin(), title.end(), title.begin(), [](unsigned char c) { return std::tolower(c); });
			std::transform(abstract.begin(), abstract.end(), abstract.begin(), [](unsigned char c) { return std::tolower(c); });

			//Add the titles into a vector for easier operations
			std::stringstream ssTitle(title);
			std::istream_iterator<std::string> beginTitle(ssTitle);
			std::istream_iterator<std::string> endTitle;
			std::vector<std::string> vstrings(beginTitle, endTitle);

			//Add the abstract into the vector
			std::stringstream ssAbstract(abstract);
			std::istream_iterator<std::string> beginAbstract(ssAbstract);
			std::istream_iterator<std::string> endAbstract;
			std::copy(beginAbstract, endAbstract, std::back_inserter(vstrings));


			//Porter Stemming
			if (PORTERSTEMMING) {
				for (std::size_t i = 0, e = vstrings.size(); i != e; ++i) {

					char *cstr = new char[vstrings[i].length() + 1];
					strcpy(cstr, vstrings[i].c_str());

					int end = stem(cstr, 0, std::strlen(cstr) - 1);
					cstr[end + 1] = 0;
					std::string stemmedWord = std::string(cstr);
					vstrings[i] = stemmedWord;
					delete[] cstr;
				}
			}

			//Print the current document being worked on
			//Add the words into a dictionary(map)
			for (std::size_t i = 0, e = vstrings.size(); i != e; ++i) {

				//Count how many times the word occurs

				int freq = std::count(vstrings.begin(), vstrings.end(), vstrings[i]);
				//std::cout << docID << ": " << vstrings[i] << " occurs " << freq << " times" << std::endl;

				std::map < std::string, std::tuple<int, std::vector<std::tuple<int, int, std::string, std::string, int, std::string>>>>::iterator it = dict.find(vstrings[i]);


				// If the key's already present in the map...
				if (it != dict.end()) {

					//increment the frequency
					std::get<0>(it->second)++;

					//Append the docID and position to the entry
					std::vector<std::tuple<int, int, std::string, std::string, int, std::string>> info;
					std::tuple<int, int, std::string, std::string, int, std::string> temp = std::make_tuple<>(static_cast<int>(std::stoi(docID)), static_cast<int>(i), title.c_str(), abstract.c_str(), static_cast<int>(freq), author);
					info.push_back(temp);
					std::get<1>(it->second).push_back(temp);
				}
				//Else create a new entry for the new word
				else {


					std::tuple<int, std::vector<std::tuple<int, int, std::string, std::string, int, std::string>>> temp;
					std::vector<std::tuple<int, int, std::string, std::string, int, std::string>> temp1;

					std::tuple<int, int, std::string, std::string, int, std::string> temp2 = std::make_tuple<>(std::stoi(docID), static_cast<int>(i), title.c_str(), abstract.c_str(), static_cast<int>(freq), author);

					//Append the docID and pos to the entry
					temp1.push_back(temp2);
					std::get<0>(temp) = 1;
					std::get<1>(temp) = temp1;

					dict.insert(std::pair<std::string, std::tuple<int, std::vector<std::tuple<int, int, std::string, std::string, int, std::string>>>>(vstrings[i], temp));

				}
			}

			//This is the reset point where every variable gets reset
			title.clear();
			abstract.clear();
			author.clear();

			//We know the doc ID is contained in a line where the first 3 charachters are ".I "
			docID = (line.erase(0, 3));
		}
		//Find the line contiaing the title
		if (line.rfind(".T", 0) == 0 && line.rfind(".T", 0) != std::string::npos) {
			std::getline(data_store, line);
			title = line;
		}
		//Find the line containg the abstract text
		if (line.rfind(".W", 0) == 0 && line.rfind(".W", 0) != std::string::npos) {
			//Keep getting new lines until you reach any of the .W whatever
			std::getline(data_store, line);

			//Keep on reading the lines until you find the line starting with .B since this is where the abstract ends
			while (!(line.rfind(".B", 0) == 0 && line.rfind(".B", 0) != std::string::npos)) {
				abstract += " " + line;
				std::getline(data_store, line);
			}

		}

		//Find the line contiaing the author
		if (line.rfind(".A", 0) == 0 && line.rfind(".A", 0) != std::string::npos) {
			std::getline(data_store, line);
			while (!(line.rfind(".N", 0) == 0 && line.rfind(".N", 0) != std::string::npos)) {
				author += " " + line;
				std::getline(data_store, line);
			}
		}

	}

	//Add on the number of documents to the end of the map
	std::tuple<int, std::vector<std::tuple<int, int, std::string, std::string, int, std::string>>> empty_temp;
	docCount = docCount - 1;
	dict.insert(std::pair<std::string, std::tuple<int, std::vector<std::tuple<int, int, std::string, std::string, int, std::string>>>>("~" + std::to_string(docCount), empty_temp));

	//Remove stopwords. The default value is to not remove stopwords
	if (STOPWORDS) {
		for (auto word : stopwords) {
			dict.erase(word);
		}
	}


	//Use boost to serialize the data structure to a file
	serializeMap(dict);

	std::cout << "There are: " << dict.size() << " Terms in the map" << std::endl;
	std::cout << "*** Finished building the Inverted Index ***" << std::endl;
}

void serializeLookupMap(std::map<int, int>& lookup) {
	std::ofstream ofs("lookup_table.dat");
	boost::archive::text_oarchive ar(ofs);
	ar &lookup;
}

std::vector<double> normalize(std::vector<double> &arr) {

	double sum = 0;
	std::for_each(arr.begin(), arr.end(), [&](double n) {
		sum += n;
	});

	double normFactor = 1 / sum;

	auto min = *std::min_element(std::begin(arr), std::end(arr));
	auto max = *std::max_element(std::begin(arr), std::end(arr));

	for (std::size_t i = 0; i != arr.size(); ++i) {
		//	arr[i] = (arr[i] - min) / (max - min);
		arr[i] = (arr[i] * normFactor);
	}

	return arr;
}

void writeResults(std::vector<double> &arr) {
	std::ofstream myfile;
	myfile.open("pagerank_scores.txt");

	for (int i = 0; i < arr.size(); i++) {
		myfile << "Document ID: " << i + 1 << ", PageRank Score: " << arr[i] << std::endl;
	}

	myfile.close();
}