#include <fstream>
#include <iostream>
using namespace std; 

#include "hmm.h"
#include <cmath>

void test();

void makeEmitAndTransFiles(string race, int num_states, int num_emits) {
    ofstream transfile(race + "/hmm.trans");
    transfile << "S1" << endl;
    for (int i = 1; i <= num_states; ++i) {
        for (int j = i; j <= num_states; ++j) {
			float prob = 1.0 / (num_states - i + 1);
            transfile << "S" << i << " S" << j << " " << prob << endl;
        }
    }
    transfile.close();

    ofstream emitfile(race + "/hmm.emit");
    for (int i = 1; i <= num_states; ++i) {
        for (int j = 1; j <= num_emits; ++j) {
			float prob = 1.0 / num_emits;
            emitfile << "S" << i << " " << j << " " << prob << endl;
        }
    }
    emitfile.close();
}

// Returns the number of observations in the stats.csv file
int numObservations(string race) {
	ifstream stats(race + "/stats.csv");
	string line;
	int result = 0;
	while (getline(stats, line))
	{
		result++;
	}
	return result;
}

// Returns a new HMM with the saved probabilities
// Use create=true to reset the files 
Hmm* hmmForRace(string race, bool create) {
	if (create) {
		makeEmitAndTransFiles(race, 10, numObservations(race));
	}
	Hmm* hmm = new Hmm();
	hmm->loadProbs(race + "/hmm");
	return hmm;
}

void trainhmm(string race)
{
	Hmm* hmm = hmmForRace(race, true);
	ifstream istrm(race + "/data.csv");
	int maxIterations = 10;

	vector<vector<unsigned long>*> trainingSequences;
	hmm->readSeqs(istrm, trainingSequences);
	hmm->baumWelch(trainingSequences, maxIterations);
	hmm->saveProbs(race + "/hmm");
}

int main(int argc, char* argv[])
{
	trainhmm("P");
	trainhmm("T");
	trainhmm("Z");

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