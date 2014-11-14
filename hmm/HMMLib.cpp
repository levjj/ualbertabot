#include "HMMlib/hmm_table.hpp"
#include "HMMlib/hmm_vector.hpp"
#include "HMMlib/hmm.hpp"
using namespace hmmlib;

#include <iostream>

const int K = 6; // number of states
const int M = 4; // alphabet size
const int R = 1; // rounds of training

boost::shared_ptr<HMMVector<double> > sample(HMM<double> &hmm, sequence &obs, int n) {
	HMMVector<double> I(K);
	if (obs.size() == 0) {
		for (int i = 0; i < K; i++) {
			I(i) = hmm.get_initial_probs()(i);
			std::cout << "(" << I(i) << ")" << std::endl;
		}
	} else {
		// Running forward
		HMMMatrix<double> F(obs.size(), K);
		HMMVector<double> scales(obs.size());
		hmm.forward(obs, scales, F);

		// Running backward
		HMMMatrix<double> B(obs.size(), K);
		hmm.backward(obs, scales, B);

		HMMMatrix<double> post(obs.size(), K);
		hmm.posterior_decoding(obs, F, B, scales, post);

		for (int i = 0; i < K; i++) {
			I(i) = post(i, obs.size() - 1);
		}
	}
	for (int i = 0; i < n; i++) {
		HMMVector<double> F(K);
		for (unsigned int x = 0; x < K; x++) {
			F(x) = 0.0;
			for (int y = 0; y < K; y++) {
				F(x) += I(y) * hmm.get_trans_probs()(y, x);
			}
		}
		for (unsigned int x = 0; x < K; x++) {
			I(x) = F(x);
		}
	}
	boost::shared_ptr<HMMVector<double> > emission(new HMMVector<double>(M));
	for (int z = 0; z < M; z++) {
		(*emission)(z) = 0.0;
		for (int y = 0; y < K; y++) {
			(*emission)(z) += I(y) * hmm.get_emission_probs()(y, z);
		}
		std::cout << "(" << (*emission)(z) << ")" << std::endl;
	}
	return emission;
}

boost::shared_ptr<HMM<double> > train(boost::shared_ptr<HMMVector<double> > initial, boost::shared_ptr<HMM<double> > hmm, sequence* obs) {
	// Running forward
	HMMMatrix<double> F(obs->size(), K);
	HMMVector<double> scales(obs->size());
	hmm->forward(*obs, scales, F);

	// Running backward
	HMMMatrix<double> B(obs->size(), K);
	hmm->backward(*obs, scales, B);

	// Running Baum-Welch
	boost::shared_ptr<HMMVector<double> > pi_counts_ptr(new HMMVector<double>(K));
	boost::shared_ptr<HMMMatrix<double> > T_counts_ptr(new HMMMatrix<double>(K, K));
	boost::shared_ptr<HMMMatrix<double> > E_counts_ptr(new HMMMatrix<double>(M, K));
	hmm->baum_welch(*obs, F, B, scales, *pi_counts_ptr, *T_counts_ptr, *E_counts_ptr);

    std::cout << "train() Emissions:" << std::endl;
    for (int st = 0; st < K; st++) {
		for (int e = 0; e < M; e++) {
            std::cout.width(14);
			std::cout << (*E_counts_ptr)(st, e) << ",";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;

	boost::shared_ptr<HMM<double> > nhmm(new HMM<double>(initial, T_counts_ptr, E_counts_ptr));
	return nhmm;
}

void test();

int main(int argc, char *args[]) {
    //test();

	int n = 4; // length of observed sequence
	int p = 2; // number of observations

	// vector of initial state probabilities where [i] is probability of model initially being in state i
    //   must sum to 1.0
    boost::shared_ptr<HMMVector<double> > I_ptr(new HMMVector<double>(K));
    // matrix of transition probabilities where [i][j] is the probability of the transition from state i to state j
    //   each row[i] must sum to 1.0
    boost::shared_ptr<HMMMatrix<double> > T_ptr(new HMMMatrix<double>(K, K));
    // matrix where element [i][j] is probability of state j emitting alphabet symbol a[i]
    //   each column [j] must sum to 1.0
    boost::shared_ptr<HMMMatrix<double> > E_ptr(new HMMMatrix<double>(M, K));

    HMMVector<double> &I = *I_ptr;
    HMMMatrix<double> &T = *T_ptr;
    HMMMatrix<double> &E = *E_ptr;

	// initial probability distribution
	for (int i = 0; i < K; i++) {
		I(i) = 1.0 / K;
	}

	// transition probability distribution
	for (int i = 0; i < K; i++) {
		for (int j = 0; j < K; j++) {
			T(i, j) = 1.0 / K;
		}
	}

	// emission probability distribution
	for (int i = 0; i < K; i++) {
		for (int j = 0; j < M; j++) {
			E(i, j) = 1.0 / M;
		}
	}

    // Constructing HMM
	boost::shared_ptr<HMM<double> > hmm(new HMM<double>(I_ptr, T_ptr, E_ptr));

	std::vector<sequence*> obs(p);
	obs[0] = new sequence(n);
    (*obs[0])[0] = 1;
	(*obs[0])[1] = 2;
	(*obs[0])[2] = 2;
	(*obs[0])[3] = 3;
	obs[1] = new sequence(n);
	(*obs[1])[0] = 1;
	(*obs[1])[1] = 1;
	(*obs[1])[2] = 3;
	(*obs[1])[3] = 3;

    for (int r = 0; r < R; r++) {
		for (unsigned int i = 0; i < obs.size(); i++) {
            boost::shared_ptr<HMM<double> > new_hmm = train(I_ptr, hmm, obs[i]);
            hmm.swap(new_hmm);
		}
	}

	sequence robs(0);
	//robs[0] = new sequence(n);
	boost::shared_ptr<HMMVector<double> > E_probs = sample(*hmm, robs, 1);

    std::cout << "new probabilities:" << std::endl;
    for (int q = 0; q < M; q++) {
        std::cout << q << "(";
        std::cout.width(5);
        std::cout << (*E_probs)(q) << "),";
	}
	std::cout << std::endl;
	system("pause");
}

void test() {
    int K = 2; // number of states
    int M = 2; // alphabet size
    int n = 4; // length of observed sequence

    // vector of initial state probabilities where [i] is probability of model initially being in state i
    boost::shared_ptr<HMMVector<double> > pi_ptr(new HMMVector<double>(K));
    // matrix of transition probabilities where [i][j] is the probability of the transition from state i to state j
    boost::shared_ptr<HMMMatrix<double> > T_ptr(new HMMMatrix<double>(K, K));
    // matrix where element [i][j] is probability of state j emitting alphabet symbol a[i]
    boost::shared_ptr<HMMMatrix<double> > E_ptr(new HMMMatrix<double>(M, K));

    HMMVector<double> &pi = *pi_ptr;
    // initial probabilities
    pi(0) = 0.2; pi(1) = 0.8;

    HMMMatrix<double> &T = *T_ptr;
    // transitions from state 0
    T(0, 0) = 0.1; T(0, 1) = 0.9;
    // transitions from state 1
    T(1, 0) = 0.9; T(1, 1) = 0.1;

    HMMMatrix<double> &E = *E_ptr;
    // emissions from state 0
    E(0, 0) = 0.25; E(1, 0) = 0.75;
    // emissions from state 1
    E(0, 1) = 0.75; E(1, 1) = 0.25;

    std::cout << "Constructing HMM" << std::endl;
    HMM<double> hmm(pi_ptr, T_ptr, E_ptr);

    std::cout << "obs : [0, 1, 0, 1]" << std::endl;
    sequence obs(n);
    obs[0] = 0;
    obs[1] = 1;
    obs[2] = 0;
    obs[3] = 1;
    std::cout << "obs length: " << obs.size() << std::endl;

    std::cout << "Running viterbi" << std::endl;
    sequence hiddenseq(n);
    // Viterbi algorithm returns the most likely sequence of hidden states that matches an observed sequence
    double loglik = hmm.viterbi(obs, hiddenseq);
    std::cout << "-- hiddenseq: [" << hiddenseq[0] << ", " << hiddenseq[1] << ", " << hiddenseq[2] << ", " << hiddenseq[3] << "]" << std::endl;
    std::cout << "-- log likelihood of hiddenseq: " << loglik << std::endl;

    std::cout << "Running forward" << std::endl;
    HMMMatrix<double> F(n, K);
    HMMVector<double> scales(n);
    hmm.forward(obs, scales, F);

    std::cout << "Running likelihood" << std::endl;
    loglik = hmm.likelihood(scales);
    std::cout << "-- loglikelihood of obs: " << loglik << std::endl;

    std::cout << "Running backward" << std::endl;
    HMMMatrix<double> B(n, K);
    hmm.backward(obs, scales, B);

    std::cout << "Running posterior decoding" << std::endl;
    HMMMatrix<double> pd(n, K);
    hmm.posterior_decoding(obs, F, B, scales, pd);

    std::cout << "Running Baum-Welch" << std::endl;
    boost::shared_ptr<HMMVector<double> > pi_counts_ptr(new HMMVector<double>(K));
    boost::shared_ptr<HMMMatrix<double> > T_counts_ptr(new HMMMatrix<double>(K, K));
    boost::shared_ptr<HMMMatrix<double> > E_counts_ptr(new HMMMatrix<double>(M, K));
    // get an estimate of parameters given a set of observed feature vectors
    hmm.baum_welch(obs, F, B, scales, *pi_counts_ptr, *T_counts_ptr, *E_counts_ptr);

    std::cout << "Constructing new HMM" << std::endl;
    HMM<double> hmm2(pi_counts_ptr, T_counts_ptr, E_counts_ptr);

    std::cout << "Running forward on new HMM" << std::endl;
    hmm2.forward(obs, scales, F);
    std::cout << "Running likelihood on new HMM" << std::endl;
    loglik = hmm2.likelihood(scales);
    std::cout << "-- loglikelihood of obs in new HMM: " << loglik << std::endl;

    system("pause");
}