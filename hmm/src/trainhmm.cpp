#include <fstream>
#include <iostream>
#include <sstream>
using namespace std; 

#include "hmm.h"
#include <cmath>

void test();

void trainhmm(string race, int maxIterations)
{
	Hmm hmm;
	hmm.loadFromRace(race, true);
	ifstream istrm((race + "/data.csv").c_str());

	vector<vector<unsigned long>*> trainingSequences;
	hmm.readSeqs(istrm, trainingSequences);
	hmm.baumWelch(trainingSequences, maxIterations);
	hmm.saveProbs(race + "/hmm");
}

int main(int argc, char* argv[])
{
    Hmm hmm;
    hmm.loadFromRace("P");
    BuildingStats stats;

    stats.readStatsFile("P/stats.csv");
    vector<string> search;
    search.push_back("Assimilator");
    search.push_back("Gateway");
    int state = stats.getClosestState(search);
    
    //for (int i = 1; i <= 128; i <<= 1) {
	//	cout << "s/i=" << i << endl;
	//	system("time /t");
	//	trainhmm("P", i);
	//	system("time /t");
	//	trainhmm("T", i);
	//	system("time /t");
	//	trainhmm("Z", i);
	//	system("time /t");
	//}

    system("pause");
}

void test() {
    Hmm hmm;

    // Using a collection of observation sequences to train a HMM model using the Baum-Welch algorithm
    char* input = "phone-init1";
    char* output = "phone-result1";
    char* train = "phone.train";
    hmm.loadProbs(input);
    ifstream istrm(train);
    int maxIterations = 10;

    vector<vector<unsigned long>*> trainingSequences;
    hmm.readSeqs(istrm, trainingSequences);
    hmm.baumWelch(trainingSequences, maxIterations);
    hmm.saveProbs(output);

    // Given an HMM and an observation sequence, compute the sequence of the hidden states that has the highest probability using the Viterbi algorithm
    Hmm hmm2;

    hmm2.loadProbs("phone\0");
    ifstream infile("phone.input\0");
    vector<vector<unsigned long>*> sequences;
    hmm2.readSeqs(infile, sequences);

    for (unsigned int i = 0; i<sequences.size(); i++) {
        vector<unsigned long>& seq = *sequences[i];
        for (unsigned int j = 0; j<seq.size(); j++) {
            hmm2.addObservation(seq[j]);
        }
        vector<Transition*> path;
        double jointProb = hmm2.viterbi(path);
        cout << "P(path)=" << exp(jointProb - hmm2.obsProb()) << endl
            << "path: " << endl;
        for (unsigned int i = 0; i<path.size(); i++) {
            Transition* trans = path[i];
            if (trans == 0) continue;
            cout << hmm2.getStr(trans->_obs) << '\t'
                << hmm2.getStr(trans->_to->state()) << endl;
        }
        hmm2.reset();
    }

    // Generate an observation sequence using a HMM model
    Hmm hmm3;

    hmm3.loadProbs("phone\0");
    ofstream outfile("phone-sequences.txt\0");
    int seqs = 10;
    hmm3.genSeqs(outfile, seqs);
}