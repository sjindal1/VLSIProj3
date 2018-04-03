#ifndef CLASSFAULTEQUIV_H
#define CLASSFAULTEQUIV_H

#include "ClassGate.h"
#include <vector>
#include <algorithm>

/** \brief Encodes a single stuck-at fault as a location and a stuck-at value
 */
struct faultStruct {
	/** A pointer to the gate whose output is the fault site */
	Gate* loc; 

	/** The stuck at value (FAULT_SA0, FAULT_SA1 or NOFAULT) */
	char val;  
};

// Defining the operator for comparing two faultStructs; used if we want
// to sort them
inline bool operator<(const faultStruct& a, const faultStruct& b) {
	return (a.loc->get_outputName() < b.loc->get_outputName());
}

// Defining the equality operator for two faultStructs; used in the
// the find function
inline bool operator==(const faultStruct& a, const faultStruct& b) {
	return ((a.loc == b.loc) && (a.val == b.val));
}


/** \brief A node in the fault collapsing graph.
 */
struct faultEquivNode {		
	/** Indicates equivalent faults */
	vector<faultStruct> equivFaults; 

	/** Points to nodes that this node dominates */
	vector<faultEquivNode*> dominates; 

	/** Points to nodes that dominate this node */
	vector<faultEquivNode*> dominatedBy; 

	/** A unique ID number; just used for printing */	
	int idNumber; 
};

// Defining the equality operator for two faultEquivNodes; used in the
// the find function
inline bool operator==(const faultEquivNode& fen1, const faultEquivNode& fen2) {
	return (fen1.idNumber == fen2.idNumber);
}

// Defining the operator for comparing two faultEquivNodes; used if we want
// to sort them
inline bool compareFaultEquivNodes(faultEquivNode* a, faultEquivNode* b) {
	return a->idNumber < b->idNumber;
}


class FaultEquiv{

private:
	vector<faultEquivNode*> allFaultEquivNodes;

public:
	FaultEquiv();
	FaultEquiv(vector<faultStruct> f);
	void init(vector<faultStruct> f);
	void printEquivNode(ostream& os, faultEquivNode n);
	void printFaultEquiv(ostream& os);
	faultEquivNode* findFaultEquivNode(faultStruct fs);

	//FIXME: change these to Gate*, char faultType, Gate*, char faultType2
	bool mergeFaultEquivNodes(Gate* g1, char v1, Gate* g2, char v2);
	bool addDominance(Gate* dominatorGate, char dominatorVal, Gate* dominatedGate, char dominatedVal);

	vector<faultStruct> getCollapsedFaultList();
	~FaultEquiv();
};

#endif
