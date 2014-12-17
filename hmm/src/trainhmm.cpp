#include <fstream>
#include <iostream>
#include <sstream>
using namespace std; 

#include "hmm.h"
#include <cmath>
#include <windows.h>

// helper class
class color {
public:
	color(WORD val) : m_val(val) { }

	void set() const {
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), m_val);
	}

private:
	WORD m_val;
};

static const color red(4);
static const color green(2);
static const color white(7);

// overload operator<< to get manipulator to work
inline std::ostream& operator<<(std::ostream& os, const color& c) {
	c.set();
	return os;
}

void trainhmm(string race, int numStates, int maxIterations)
{
	Hmm hmm;
	hmm.makeEmitAndTransFiles(race, numStates);
	hmm.loadFromRace(race);
	ifstream istrm((race + "/data.csv").c_str());

	vector<vector<unsigned long>*> trainingSequences;
	hmm.readSeqs(istrm, trainingSequences);
	hmm.baumWelch(trainingSequences, maxIterations);
	hmm.saveProbs(race + "/hmm");
}

vector<unsigned long>* loadReplayData(string race, int idx) {
	vector<unsigned long>* result = new vector<unsigned long>();
    ifstream istrm((race + "/data.csv").c_str());
    string line;
	for (int i = 0; i <= idx; i++) {
		getline(istrm, line);
	}
	string::size_type begIdx, endIdx;
	begIdx = line.find_first_not_of(",");
	while (begIdx != string::npos) {
		endIdx = line.find_first_of(",", begIdx);
		if (endIdx == string::npos) {
			endIdx = line.length();
		}
		string word = line.substr(begIdx, endIdx - begIdx);
		result->push_back(atol(word.c_str()));
		begIdx = line.find_first_not_of(",", endIdx);
	}
	return result;
}

void testdata(string race, int numLines, int *trials, int *correct) {
	Hmm hmm;
	hmm.loadFromRace(race);

	string filename = race + "/data.csv";
	ifstream infile(filename.c_str());
	string line;
	for (int z = 0; z < numLines; z++) {
		vector<unsigned long>* replay = loadReplayData(race, z);
		for (unsigned int i = 0; i < replay->size(); i++) {
			unsigned int missed = 0, j;
			for (j = 0; j < i; j++) {
				hmm.observe(replay->at(j));
			}
			vector<unsigned long> *seq = hmm.predictMaxSeq(10);
			for (unsigned int k = j; k < replay->size() && k < j+10; k++) {
				unsigned long prediction = seq->at(k - j);
				if (prediction == replay->at(k)){
					correct[k - j]++;
				}
				trials[k - j]++;
			}
			hmm.reset();
			delete seq;
		}
		cerr << "Ran " << z << " from " << numLines << endl;
	}
}

void testhmm(string race, int index)
{
	cout << "Testing with replay " << index << endl;
	Hmm hmm;
	hmm.loadFromRace(race);
	vector<unsigned long>* replay = loadReplayData(race, index);
	cout << "Replay length: " << replay->size() << endl << "Rep:";
	for (vector<unsigned long>::iterator it = replay->begin(); it != replay->end(); it++) {
		cout.width(3); cout << (*it);
	}
	for (unsigned int i = 0; i < replay->size(); i++) {
		unsigned int missed = 0, j;
		cout << endl << "Pre:";
		for (j = 0; j < i; j++) {
			cout.width(3);
			cout << replay->at(j);
			hmm.observe(replay->at(j));
		}
		vector<unsigned long> *seq = hmm.predictMaxSeq(replay->size() - i);
		for (unsigned int k = j; k < replay->size(); k++) {
			cout.width(3);
			unsigned long prediction = seq->at(k - j);
			if (prediction != replay->at(k)){
				missed++;
				cout << red << prediction;
			}
			else {
				cout << green << prediction;
			}
		}
		cout << white << "  (" << (1.0 - missed / (double)(replay->size() - i)) << ")" << endl;
		hmm.reset();
		delete seq;
	}
}

void testBuildingStats() {
	Hmm hmm;
	hmm.loadFromRace("P");
	BuildingStats stats;

    stats.readStatsFile("Z/stats.csv");
    stats.readOurStatsFile("P/stats.csv");
    stats.readRepliesFile("Z/replies.csv");
    int rep_state = stats.getReplyState(11);
    set<string> search;
    search.insert("Reaver");
    search.insert("Observer");
    int state = stats.getClosestState(search);
    int state2 = stats.getClosestState2(search);
    cout << state << " " << state2 << endl;
    set<string>* unitTypes = stats.decodeState(state);
    for (set<string>::iterator it = unitTypes->begin(); it != unitTypes->end(); it++) {
        cout << *it << ",";
    }
    cout << endl << endl;;
    set<string>* unitTypes2 = stats.decodeState(state2);
    for (set<string>::iterator it = unitTypes2->begin(); it != unitTypes2->end(); it++) {
        cout << *it << ",";
    }
    cout << endl;
}

int main(int argc, char* argv[])
{
	if (argc == 1) {
		testBuildingStats();
	}
	else if (argc == 3) {
		if (argv[2][0] == '0') {
			int trials[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			int correct[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			testdata("P", 100, &trials[0], &correct[0]);
			testdata("T", 100, &trials[0], &correct[0]);
			testdata("Z", 100, &trials[0], &correct[0]);
			cout << "Steps;Trials;Correct;Accurary" << endl;
			for (unsigned int p = 0; p < 10; p++) {
				double acc = ((double)correct[p]) / ((double)trials[p]);
				cout << (p + 1) << ";" << trials[p] << "; " << correct[p] << "; " << acc << endl;
			}
		}
		else {
			testhmm(argv[1], atoi(argv[2]));
		}
	}
	else {
		system("time /t");
		trainhmm(argv[1], 48, 256);
		system("time /t");
	}

    system("pause");
}