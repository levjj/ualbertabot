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

	for (int st = 0; st < K; st++) {
		for (int e = 0; e < M; e++) {
			std::cout << (*E_counts_ptr)(st, e) << "," << std::endl;
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;

	boost::shared_ptr<HMM<double> > nhmm(new HMM<double>(initial, T_counts_ptr, E_counts_ptr));
	return nhmm;
}

int main(int argc, char *args[]) {
	int n = 4; // length of observed sequence
	int p = 2; // number of observations

	boost::shared_ptr<HMMVector<double> > I_ptr(new HMMVector<double>(K));
    boost::shared_ptr<HMMMatrix<double> > T_ptr(new HMMMatrix<double>(K, K));
    boost::shared_ptr<HMMMatrix<double> > E_ptr(new HMMMatrix<double>(M, K));

    HMMVector<double> &I = *I_ptr;
    HMMMatrix<double> &T = *T_ptr;
    HMMMatrix<double> &E = *E_ptr;

	// initial probability distribution
	I(0) = 1.0;
	for (int i = 1; i < K; i++) {
		I(i) = 0.8;
	}

	// transition probability distribution
	for (int i = 1; i < K; i++) {
		for (int j = 1; j < K; j++) {
			if (j < i) {
				T(i, j) = 0;
			}
			else {
				T(i, j) = 1.0 / (K - i + 1);
			}
		}
	}

	// emission probability distribution
	for (int i = 1; i < K; i++) {
		for (int j = 1; j < M; j++) {
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
			hmm.swap(train(I_ptr, hmm, obs[i]));
		}
	}

	sequence robs(0);
	//robs[0] = new sequence(n);
	boost::shared_ptr<HMMVector<double> > E_probs = sample(*hmm, robs, 1);

	for (int q = 0; q < M; q++) {
		std::cout << q << "(" << (*E_probs)(q) << "),";
	}
	std::cout << std::endl;
	system("pause");
}