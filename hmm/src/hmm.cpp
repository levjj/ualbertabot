/////////////////////////////////////////////////////////////////////////
//Copyright (C) 2003 Dekang Lin, lindek@cs.ualberta.ca
//
//Permission to use, copy, modify, and distribute this software for any
//purpose is hereby granted without fee, provided that the above
//copyright notice appear in all copies and that both that copyright
//notice and this permission notice appear in supporting documentation.
//No representations about the suitability of this software for any
//purpose is made. It is provided "as is" without express or implied
//warranty.
//
/////////////////////////////////////////////////////////////////////////

#include <fstream>
#include <iostream>
#include <cmath>
#include <cstdlib>

#include "hmm.h"
#include "logprobs.h"
#include "tables.h"

Transition::Transition(HmmNode* from, HmmNode* to, unsigned long obs)
{
  _from = from;
  _to = to;
  _obs = obs;
  if (_from && _to) {
    _from->outs().push_back(this);
    _to->ins().push_back(this);
  }
}

Hmm::Hmm()
{
  _minLogProb = log(0.000001);
}

double Hmm::getTransProb(Transition* trans)
{
  return _transition.get(trans->_to->state(), trans->_from->state());
}

double Hmm::getEmitProb(Transition* trans)
{
  return _emission.get(trans->_obs, trans->_to->state());
}

void Hmm::forward()
{
  // compute forward probabilities at time 0
  TimeSlot* t0 = _timeSlots[0];
  HmmNode* init = (*t0)[0];
  init->logAlpha(0);

  // compute forward probabilities at time t using the alpha values for time t-1
  for (unsigned int t = 1; t<_timeSlots.size(); t++) {
    TimeSlot* ts = _timeSlots[t];
    for (TimeSlot::iterator it = ts->begin(); it!=ts->end(); it++) {
      vector<Transition*>& ins = (*it)->ins();
      vector<double> logProbs(ins.size());
      for (unsigned int i = 0; i<ins.size(); i++) {
	Transition* trans = ins[i];
	double logProb = trans->_from->logAlpha()+getTransProb(trans)+getEmitProb(trans);
	logProbs[i] = logProb;
      }
      (*it)->logAlpha(sumLogProb(logProbs));
    }
  }
}

double Hmm::viterbi(vector<Transition*>& path)
{
  // set nodes at time 0 according to initial probabilities.
  TimeSlot* ts = _timeSlots[0];
  HmmNode* init = (*ts)[0];
  init->logAlpha(0);

  // find the best path up to path t;
  for (unsigned int t = 1; t<_timeSlots.size(); t++) {
    ts = _timeSlots[t];
    for (TimeSlot::iterator it = ts->begin(); it!=ts->end(); it++) {
      HmmNode* node = *it;
      vector<Transition*>& ins = node->ins();
      double maxProb = log(0.0);
      Transition* bestTrans = 0;
      for (unsigned int i = 0; i<ins.size(); i++) {
	Transition* trans = ins[i];
	double logProb = trans->_from->logAlpha()+getTransProb(trans)+getEmitProb(trans);
	if (bestTrans==0 || maxProb<logProb) {
	  bestTrans = trans;
	  maxProb = logProb;
	}
      }
      node->logAlpha(maxProb); // store the highest probability in logAlpha
      node->psi(bestTrans); // store the best transition in psi
    }
  }
  // Find the best node at time T. It will be the last node in the best path
  ts = _timeSlots[_timeSlots.size()-1];
  HmmNode* best = 0;
  for (TimeSlot::iterator it = ts->begin(); it!=ts->end(); it++) {
    HmmNode* node = *it;
    if (best==0 || best->logAlpha()<node->logAlpha())
      best = node;
  }

  // retrieve the nodes in the best path
  for (HmmNode* nd = best; nd;) {
    if (nd->psi()) {
      path.push_back(nd->psi());
      nd = nd->psi()->_from;
    }
    else
      nd = 0;
  } 

  // reverse the path
  for (int i = 0, j=path.size()-1; i<j; i++, j--) {
    Transition* tmp = path[i];
    path[i] = path[j];
    path[j] = tmp;
  }
  // Done
  return best->logAlpha();
}

void Hmm::backward()
{
  int T = _timeSlots.size()-1;
  if (T<1) // no observation
    return;
  for (int t = T; t>=0; t--) {
    TimeSlot* ts = _timeSlots[t];
    for (TimeSlot::iterator it = ts->begin(); it!=ts->end(); it++) {
      HmmNode* node = *it;
      if (t==T)
	node->logBeta(0);
      else {
	vector<Transition*>& outs = node->outs();
	vector<double> logProbs(outs.size());
	for (unsigned int i = 0; i<outs.size(); i++) {
	  Transition* trans = outs[i];
	  double logProb = trans->_to->logBeta()+getTransProb(trans)+getEmitProb(trans);
	  logProbs[i] =logProb;
	}
	node->logBeta(sumLogProb(logProbs));
      }
    }
  }
}

// Compute P(e_1:T) = sum_s P(e_1:T, x_T=s) = sum_s alpha_s(T);
double Hmm::obsProb()
{
  if (_timeSlots.size()<1)
    return 1; // no observations

  forward();
  TimeSlot* last = _timeSlots[_timeSlots.size()-1];
  vector<double> alphaT;
  for (TimeSlot::iterator it = last->begin(); it!=last->end(); it++) {
    alphaT.push_back((*it)->logAlpha());
  }
  return sumLogProb(alphaT);  
}

double Hmm::getPseudoCounts(PseudoCounts& counts)
{
  double PofObs = obsProb(); // this call includes a forward() call.
  backward();
  //  print();

  // Compute the pseudo counts of transitions, emissions, and initializations
  for (unsigned int t = 0; t<_timeSlots.size(); t++) {
    TimeSlot* ts = _timeSlots[t];
    TimeSlot::iterator it = ts->begin();

    // P(X_t=s|e_1:T) = alpha_s(t)*beta_s(t)/P(e_t+1:T|e_1:t)
    // The value sum below is log P(e_t+1:T|e_1:t)
    vector<double> logprobs;
    for (; it!=ts->end(); it++) {
      logprobs.push_back((*it)->logAlpha()+(*it)->logBeta());
    }
    double sum = sumLogProb(logprobs);

    // add the pseudo counts into counts
    for (it = ts->begin(); it!=ts->end(); it++) {
      HmmNode* node = *it;
      
      //stateCount=P(X_t=s|e_1:T) 
      double stateCount = node->logAlpha()+node->logBeta()-sum; 

      counts.stateCount().add(node->state(), stateCount);
      vector<Transition*>& ins = node->ins();
      unsigned int k;
      for (k = 0; k<ins.size(); k++) {
	Transition* trans = ins[k];
	HmmNode* from = trans->_from;
	double transCount = from->logAlpha()+getTransProb(trans)
	  +getEmitProb(trans)+node->logBeta()-PofObs;
//	cerr << _str2id.getStr(node->state()) << '\t' 
//	     << _str2id.getStr(trans->_obs) << '\t'
//	     << exp(transCount) << endl;
	counts.emitCount().add(node->state(), trans->_obs, transCount);
      }
      vector<Transition*>& outs = node->outs();
      for (k = 0; k<outs.size(); k++) {
	Transition* trans = outs[k];
	HmmNode* to = trans->_to;
	double transCount = node->logAlpha()+getTransProb(trans)
	  +getEmitProb(trans)+to->logBeta()-PofObs;
	counts.transCount().add(node->state(), to->state(), transCount);
      }
    }
  }
  //  counts.print(_str2id);
  return PofObs;
}

void Hmm::baumWelch(vector<vector<unsigned long>*>& sequences, int iterations)
{
  cerr << "Training with Baum-Welch for up to " << iterations << " iterations, using "
       << sequences.size() << " sequences." << endl;
  double prevTotalLogProb = 0;
  for (int k = 0; k<iterations; k++) {
    PseudoCounts counts;
    double totalLogProb = 0;
    for (unsigned int i=0; i<sequences.size(); i++) {
      vector<unsigned long>& seq = *sequences[i];
      for (unsigned int j=0; j<seq.size(); j++) {
	addObservation(seq[j]);
      }
      // accumulate the pseudo counts
      totalLogProb += getPseudoCounts(counts);
      reset();
    }
    cerr << "Iteration " << k+1 << ' ' << "totalLogProb=" << totalLogProb << endl;
    if (prevTotalLogProb!=0 && (totalLogProb - prevTotalLogProb<0.1))
      break;
    else
      prevTotalLogProb = totalLogProb;
    updateProbs(counts);
  }
}

void Hmm::updateProbs(PseudoCounts& counts)
{
  _transition.clear();
  _emission.clear();
  for (TwoDTable::iterator i = counts.transCount().begin(); i!=counts.transCount().end(); i++) {
    unsigned long from = i->first;
    double fromCount = counts.stateCount().get(from);
    OneDTable& cnts = *i->second;
    for (OneDTable::iterator j = cnts.begin(); j!=cnts.end(); j++) {
      //      if (j->second-fromCount>=_minLogProb)
	_transition.add(from, j->first, j->second-fromCount);
    }
  }
  for (TwoDTable::iterator s = counts.emitCount().begin(); s!=counts.emitCount().end(); s++) {
    unsigned long state = s->first;
    double stateCount = counts.stateCount().get(state);
    OneDTable& cnts = *s->second;
    for (OneDTable::iterator o = cnts.begin(); o!=cnts.end(); o++) {
      //      if (o->second-stateCount>_minLogProb)
	_emission.add(state, o->first, o->second-stateCount);
    }
  }
  //  saveProbs("");
}

void HmmNode::print()
{
  cerr << _hmm->getStr(state()) << '\t'
       << "alpha=" << exp(logAlpha()) << '\t'
       << "beta=" << exp(logBeta());
  unsigned int i;
  cerr << " (in";
  for (i = 0; i<_ins.size(); i++) {
    cerr << ' ' << _hmm->getStr(_ins[i]->_from->state());
  }
  cerr << ") ";
  cerr << " (out";
  for (i = 0; i<_outs.size(); i++) {
    cerr << ' ' << _hmm->getStr(_outs[i]->_to->state());
  }
  cerr << ")";
  cerr << endl;
}

HmmNode::~HmmNode()
{
  for (unsigned int i = 0; i<_outs.size(); i++) {
    delete _outs[i];
  }
}

TimeSlot::~TimeSlot()
{
  for (TimeSlot::iterator it = begin(); it!=end(); it++) {
    delete (*it);
  }
}

void Hmm::reset()
{
  for (unsigned int t = 0; t<_timeSlots.size(); t++){
    delete _timeSlots[t];
  }
  _timeSlots.clear();
}

Hmm::~Hmm()
{
  reset();
}

void Hmm::addObservation(unsigned long o)
{
  vector<unsigned long> stateIds;
  ULSet* cntx = _emission.getCntx(o);
  if (cntx==0) {
    for (TwoDTable::iterator it = _emission.begin(); it!=_emission.end(); it++) {
      stateIds.push_back(it->first);
    }
  }
  else {
    for (ULSet::iterator it = cntx->begin(); it!=cntx->end(); it++) {
      stateIds.push_back(*it);
    }
  }

  if (_timeSlots.empty()) {
    // create a special state for time 0;
    TimeSlot* t0 = new TimeSlot;
    t0->push_back(new HmmNode(0, _initState, this));
    _timeSlots.push_back(t0);
  }

  TimeSlot* ts = new TimeSlot;
  int time = _timeSlots.size();
  for (unsigned int i = 0; i<stateIds.size(); i++) {
    HmmNode* node = new HmmNode(time, stateIds[i], this);
    ts->push_back(node);
    TimeSlot* prev = _timeSlots[time-1];
    for (TimeSlot::iterator it = prev->begin(); it!=prev->end();it++) {
      ULSet* possibleSrc = _transition.getCntx(node->state());
	if (possibleSrc && possibleSrc->find((*it)->state())!=possibleSrc->end())
	  new Transition(*it, node, o);
    }
  }
  _timeSlots.push_back(ts);
}

void Hmm::loadProbs(string name)
{
  string s = name+".trans";
  ifstream transProb(s.c_str());
  string initState;
  transProb >> initState;
  _initState = _str2id.getId(initState);
  _transition.load(transProb, _str2id);

  s = name+".emit";
  ifstream emitProb(s.c_str());
  _emission.load(emitProb, _str2id);
}

void Hmm::saveProbs(string name)
{
  if (name=="") {
    cerr << "transition probabilities:" << endl;
    _transition.save(cerr, _str2id);
    cerr << "---------------------------------" << endl;
    cerr << "emission probabilities:" << endl;
    _emission.save(cerr, _str2id);
    cerr << "---------------------------------" << endl;
  }
  else {
    string s = name+".trans";
    ofstream transProb(s.c_str());
    transProb << _str2id.getStr(_initState) << endl;
    _transition.save(transProb, _str2id);

    s = name+".emit";
    ofstream emitProb(s.c_str());
    _emission.save(emitProb, _str2id);
  }
}

void Hmm::print()
{
  for (unsigned int i = 0; i<_timeSlots.size(); i++) {
    TimeSlot* ts = _timeSlots[i];
    cerr << "TIME=" << i << endl;
    for (unsigned int s = 0; s<ts->size(); s++) {
      (*ts)[s]->print();
    }
  }
}

void Hmm::readSeqs(istream& istrm, vector<vector<unsigned long>*>& sequences)
{
  string line;
  const string delims(" ,");
  while (getline(istrm, line)) {
    vector<unsigned long>* seq = new vector<unsigned long>;
    string::size_type begIdx, endIdx;
    begIdx = line.find_first_not_of(delims);
    while (begIdx!=string::npos) {
      if (line[begIdx]=='#') // the rest of the line are comments
	break;
      endIdx = line.find_first_of(delims, begIdx);
      if (endIdx==string::npos) {
	endIdx = line.length();
      }
      string word = line.substr(begIdx, endIdx-begIdx);
      seq->push_back(getId(word));
      begIdx = line.find_first_not_of(delims, endIdx);
    }
    if (seq->size()>0)
      sequences.push_back(seq);
    else
      delete seq;
  }
}


void Hmm::genSeqs(ostream& ostrm, int seqs)
{
  vector<unsigned long> seq;
  for (int i = 0; i<seqs; i++) {
    genSeq(seq);
    for (unsigned int k = 0; k<seq.size(); k++) {
      if (k)
	ostrm << ' ';
      ostrm << _str2id.getStr(seq[k]);
    }
    ostrm << endl;
    seq.clear();
  }
}


void Hmm::genSeq(vector<unsigned long>& seq)
{
  unsigned long state = _initState, next, obs;
  while (true) {
    if (!_transition.rand(state, next) || !_emission.rand(next, obs))
      break;
    state = next;
    seq.push_back(obs);
  }
}

// Returns the number of observations in the stats.csv file
int Hmm::numObservations(string race) {
	ifstream stats((race + "/stats.csv").c_str());
	string line;
	int result = 0;
	while (getline(stats, line))
	{
		result++;
	}
	return result;
}

void Hmm::makeEmitAndTransFiles(string race, int num_states) {
	int num_emits = numObservations(race);
	ofstream transfile((race + "/hmm.trans").c_str());
	transfile << "S1" << endl;
	for (int i = 1; i <= num_states; ++i) {
		for (int j = i; j <= num_states; ++j) {
			double prob = (i == j) ? 0.5 : 0.5 / (num_states - i);
			transfile << "S" << i << " S" << j << " " << prob << endl;
		}
	}
	transfile.close();

	ofstream emitfile((race + "/hmm.emit").c_str());
	for (int i = 1; i <= num_states; ++i) {
		for (int j = 1; j <= num_emits; ++j) {
			double prob = 1.0 / num_emits;
			emitfile << "S" << i << " " << j << " " << prob << endl;
		}
	}
	emitfile.close();
}

// Returns a new HMM with the saved probabilities
// Use create=true to reset the files 
void Hmm::loadFromRace(string race) {
	this->loadProbs(race + "/hmm");
}

vector<double>* Hmm::getCurrentPD() {
	vector<double>* result = new vector<double>();
	if (_timeSlots.empty()) {
		result->push_back(0.0);
		for (unsigned int i = 1; i < _transition.size(); i++) {
			result->push_back(-DBL_MAX);
		}
	}
	else {
		this->forward();
		TimeSlot* last = _timeSlots[_timeSlots.size() - 1];

		for (TimeSlot::iterator it = last->begin(); it != last->end(); it++) {
			result->push_back((*it)->logAlpha());
		}
	}
	return result;
}

void Hmm::observe(unsigned long state) {
    stringstream ss;
    string s;
    ss << state;
    ss >> s;
	this->addObservation(s);
}

vector<double>* Hmm::predict(unsigned int t) {
	vector<double>* current = this->getCurrentPD();
	// Make t transitions

	//for (int i = 0; i < t; i++) { }

	// Predict with the emission vector E
	vector<double>* result = new vector<double>();
	for (unsigned int i = 0; i < _emission[0]->size(); i++) {
		double em = 0;
		for (unsigned int st = 0; st < current->size(); st++) {
			em += exp(current->at(st) + _emission.get(_transition.size() + i, st));
		}
		result->push_back(em);
	}
	delete current;
	return result;
}

/*unsigned long Hmm::predictMax(unsigned int t) {
	vector<double>* dist = predict(t);
	unsigned long max = 0;
	for (int i = 1; i < dist->size(); i++) {
		if (dist->at(i) > dist->at(max)) max = i;
	}
	delete dist;
	return atoi(getStr(max).c_str()) + 1;
}*/

vector<unsigned long>* Hmm::predictMaxSeq(unsigned int t) {
	vector<unsigned long>* result = new vector<unsigned long>();
	vector<Transition*> transitions;
	if (_timeSlots.empty()) {
		transitions.push_back(new Transition(NULL, new HmmNode(0, 0, this), 0));
	}
	else {
		viterbi(transitions);
	}
	unsigned long state = transitions.back()->_to->state(), obs;
	vector<double> *stateDistribution = new vector<double>(_transition.size(), 0), *next;
	(*stateDistribution)[state] = 1.0;
	// Initial state distribution is 0, 0, ... 0, 1, 0, ... 0, 0
	//                                            ^
	//                                            State returned by viterbi
	for (unsigned int i = 0; i < t; i++) {
		next = _transition.dist(stateDistribution);
		delete stateDistribution;
		stateDistribution = next;
		obs = _emission.max(stateDistribution);
		result->push_back(atoi(getStr(obs).c_str()));
	}
	return result;
}

unsigned long Hmm::predictMax(unsigned int t) {
	vector<unsigned long>* obs = predictMaxSeq(t);
	unsigned long pred = obs->at(obs->size() - 1);
	delete obs;
	return pred;
}

void PseudoCounts::print(Str2IdMap& str2id)
{
  cerr << "TRANSITION"<<endl;
  _transCount.save(cerr, str2id);
  cerr << "*********************" << endl;
  cerr << "EMISSION"<<endl;
  _emitCount.save(cerr, str2id);
  cerr << "*********************" << endl;
  cerr << "STATE"<<endl;
  _stateCount.save(cerr, str2id);
  cerr << "*********************" << endl;
  cerr << "INIT-PROBS"<<endl;
}

void BuildingStats::readStatsFile(const string& filename) {
    sets.clear();
    ifstream infile(filename.c_str());
    string line;
    if (infile.is_open()) {
        while (getline(infile, line)) {
            set<string> buildings;
            size_t start = line.find('[');
            size_t end = line.find(']');
            if (end == start + 1) {
				sets_by_size.insert(pair<int, int>(sets.size(), buildings.size()));
				sets.push_back(buildings);
                continue;
            }
            line = line.substr(start + 1, end - start - 1);
            do {
                start = line.find('\'');
                end = line.find('\'', start + 1);
                string building = line.substr(start + 1, end - start - 1);
                buildings.insert(building);
                line = line.substr(end + 1);
            } while (line.find(',') != string::npos);
			sets_by_size.insert(pair<int, int>(sets.size(), buildings.size()));
			sets.push_back(buildings);
        }
        infile.close();
    }
}

// returns the closest state number given a set of buildings
int BuildingStats::getClosestState(const set<string>& unitTypes) {
	set <pair<int, int>, compare_by_statesize>::iterator it = sets_by_size.begin();
	for (; it != sets_by_size.end(); ++it) {
		set<string>::const_iterator unitType = unitTypes.begin();
		for (; unitType != unitTypes.end() && sets[it->first].count(*unitType); ++unitType);
		if (unitType == unitTypes.end()) {
			return it->first + 1; // found state that has all the unit types
		}
	}
	return (--it)->first + 1; // not found any state that has all the unit types
}

// returns the closest state number given a set of buildings
set<string>* BuildingStats::decodeState(int state) {
	return &sets[state - 1];
}

int BuildingStats::getClosestState2(const set<string>& unitTypes) {
    vector<int> misses;
    misses.resize(sets.size());
    // count number of units not in state
    for (unsigned int state = 0; state != sets.size(); ++state)
        for (set<string>::const_iterator target_iter = unitTypes.begin(); target_iter != unitTypes.end(); ++target_iter)
            if (!sets[state].count(*target_iter))
                ++misses[state];
    // find lowest miss count
    int min_misses = 999;
    for (int i = 0; i != misses.size(); ++i)
        if (misses[i] < min_misses)
            min_misses = misses[i];

    int smallest_state_index = 0;
    unsigned int smallest_state_size = 999;
    for (int i = 0; i != misses.size(); ++i)
        if (misses[i] == min_misses && sets[i].size() < smallest_state_size) {
            smallest_state_index = i;
            smallest_state_size = sets[i].size();
        }
    return smallest_state_index + 1;
}