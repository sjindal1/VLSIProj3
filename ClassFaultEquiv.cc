 /** \class FaultEquiv
 * \brief Contains the graph structure for fault collapsing.
 *
 * Internally, the FaultEquiv includes a vector of faultEquivNode, which
 * represent nodes within the graph. Each node "n" holds a list of equivalent fault,
 * and it has pointers to other nodes which "n" dominates or are dominated by "n".

 * The API for interacting with a FaultEquiv includes:
 * - \a mergeFaultEquivNode, a function you will use to indicate that two faults are equivalent (and 
 * thus their nodes should be merged)
 * - \a addDominance, a function to add a dominance relationship to the graph
 * - \a getCollapsedFaultList(), which returns a vector of faults, minimized based on
 * the equivalence relationships you have set
 * - \a printFaultEquiv, which prints a text representation of the graph
 *
 * Please see the individual documentation for each function for more information.
 */


#include "ClassFaultEquiv.h"

/** \brief Construct a new empty FaultEquiv */
FaultEquiv::FaultEquiv() {}

/** \brief Construct a new FaultEquiv and initialize it
 *  \param f a vector of faultStructs that indicates all faults that must be considered
 */
FaultEquiv::FaultEquiv(vector<faultStruct> f) {
	init(f);
}

/** \brief Initialize a FaultEquiv
 *  \param f a vector of faultStructs that indicates all faults that must be considered
 */
void FaultEquiv::init(vector<faultStruct> f) {
	int count=0;
	allFaultEquivNodes.clear();

	for (vector<faultStruct>::iterator i = f.begin(); i < f.end(); ++i) {
		vector<faultStruct> v;
		v.push_back(*i);

		vector<faultEquivNode*> d1, d2;

		faultEquivNode* n = new faultEquivNode({v, d1, d2, count++});

		allFaultEquivNodes.push_back(n);
	}

}

/** \brief Print information about a faultEquivNode to an ostream
 *  \param os an ostream where the information should be printed
 *  \param n the faultEquivNode whose information you want to print
 *  \note If you want to print node information to stdout, use cout for os
 */
void FaultEquiv::printEquivNode(ostream& os, faultEquivNode n) {
	os << "Fault equivalence node ID " << n.idNumber << ": ";
	sort(n.equivFaults.begin(), n.equivFaults.end());
	os << "[" << (n.equivFaults.begin())->loc->get_outputName() << "/" << (int)((n.equivFaults.begin())->val);
	for (vector<faultStruct>::iterator i = n.equivFaults.begin()+1; i < n.equivFaults.end(); ++i) {
		os << " == " << (*i).loc->get_outputName() << "/" << (int)((*i).val);
	}
	os << "]; ";

	if (n.dominates.size() > 0) {

		// sort
		sort(n.dominates.begin(), n.dominates.end(), compareFaultEquivNodes);
		
		if (n.dominates.size() > 1) 
			os << "Dominates nodes: [";
		else
			os << "Dominates node: [";
		os << (*(n.dominates.begin()))->idNumber;
		for (vector<faultEquivNode*>::iterator i = n.dominates.begin()+1; i < n.dominates.end(); ++i) {
			os << ", " << (*i)->idNumber;
		}
		os << "]; ";
	}
	if (n.dominatedBy.size() > 0) {

		// sort
		sort(n.dominatedBy.begin(), n.dominatedBy.end(), compareFaultEquivNodes);

		if (n.dominatedBy.size() > 1)
			os << "Dominated by nodes: [";
		else
			os << "Dominated by node: [";
		os << (*(n.dominatedBy.begin()))->idNumber;
		for (vector<faultEquivNode*>::iterator i = n.dominatedBy.begin()+1; i < n.dominatedBy.end(); ++i) {
			os << ", " << (*i)->idNumber;
		}
		os << "]";
	}
	
	os << endl;
}

/** \brief Print information about all faultEquivNodes in this FaultEquiv to an ostream
 *  \param os an ostream where the information should be printed
 *  \note If you want to print node information to stdout, use cout for os
 */
void FaultEquiv::printFaultEquiv(ostream& os) {
	for (vector<faultEquivNode*>::iterator i = allFaultEquivNodes.begin(); i < allFaultEquivNodes.end(); ++i) {
		printEquivNode(os, **i);
	}
}

/** \brief Finds the faultEquivNode that includes a given fault
 *  \param fs the fault you are searching for
 *  \return pointer to the faultEquivNode you are searching for, or NULL if not found
 */
faultEquivNode* FaultEquiv::findFaultEquivNode(faultStruct fs) {
	Gate* g = fs.loc;
	char v = fs.val;
	for (vector<faultEquivNode*>::iterator i = allFaultEquivNodes.begin(); i < allFaultEquivNodes.end(); ++i) {
		vector<faultStruct> equivFaultList = (*i)->equivFaults;
		for (vector<faultStruct>::iterator j = equivFaultList.begin(); j < equivFaultList.end(); ++j) {
			if (((*j).loc == g) && ((*j).val == v))
				return *i;
		}

	}
	
	// Return null of this fault is not in the structure.
	// This may not indicate an error: it is possible the user didn't start
	// with all possible faults in the initial fault list.
	return NULL;
}

/** \brief Finds and merges two notes indicating equivalent faults
 *  \param g1 pointer to gate 1
 *  \param v1 stuck-at value of gate 1
 *  \param g2 pointer to gate 2
 *  \param v2 stuck-at value of gate 2
 *  \return true if successful or false if nodes cannot be found or nodes are already merged
 *
 * This function will merge g1 stuck-at-v1 and g2 stuck-at-v2, indicating
 * the two faults are equivalent.
 */
bool FaultEquiv::mergeFaultEquivNodes(Gate* g1, char v1, Gate* g2, char v2) {

	faultStruct fs1 = {g1, v1};
	faultStruct fs2 = {g2, v2};

	faultEquivNode* node1 = findFaultEquivNode(fs1);
	faultEquivNode* node2 = findFaultEquivNode(fs2);

	// checking both are non-null
	if ((node1 != NULL) && (node2 != NULL)) { 

		// already merged
		if (node1 == node2) {
			return false;
		}

		// We will choose to copy everything from node 2 into node 1.

		// 1. Move all faults from node 2's equivalence fault list
		vector<faultStruct> faultsToMove = node2->equivFaults;
		for (vector<faultStruct>::iterator thisFault = faultsToMove.begin(); thisFault < faultsToMove.end(); ++thisFault) {
			node1->equivFaults.push_back(*thisFault);
		}

		// Any faults that node2 dominates now need to be copied to node 1
		for (vector<faultEquivNode*>::iterator thisNode = node2->dominates.begin(); thisNode < node2->dominates.end(); ++thisNode) {
			// if thisNode is not already in node1->dominates, add it			
			if (find(node1->dominates.begin(), node1->dominates.end(), *thisNode) == node1->dominates.end())
				node1->dominates.push_back(*thisNode);
		}

		// Any faults that are dominated by node 2 now need to point to node 1
		for (vector<faultEquivNode*>::iterator thisNode = node2->dominatedBy.begin(); thisNode < node2->dominatedBy.end(); ++thisNode) {
			(*thisNode)->dominates.erase(remove((*thisNode)->dominates.begin(), (*thisNode)->dominates.end(), node2), (*thisNode)->dominates.end());
			(*thisNode)->dominates.push_back(node1);			
		}			

		// 3. Delete node2 (freeing its memory and remove node2 from this list of all fault equivalent nodes
		delete node2;
		allFaultEquivNodes.erase(remove(allFaultEquivNodes.begin(), allFaultEquivNodes.end(), node2), allFaultEquivNodes.end());

		return true;
	}

	return false;
}


// a dominates b
// therefore any test for b will also detect a
/** \brief Records fault dominance relationship between two faults
 *  \param dominatorGate the location of the dominating fault
 *  \param dominatorVal  the stuck-at value of the dominating fault
 *  \param dominatedGate the location of the dominated fault
 *  \param dominatedVal  the stuck-at value of the dominated fault
 *  \return true if successful or false if nodes cannot be found or this dominance was already performed
 
 *  This function encodes the dominance relationship where fault
 *  "dominatorGate stuck-at-dominatorVal" dominates "dominatedGate stuck-at dominatedVal"
 *  Therefore we know that any test for the latter also will detect the former.
 */

bool FaultEquiv::addDominance(Gate* dominatorGate, char dominatorVal, Gate* dominatedGate, char dominatedVal) {
	faultStruct a = {dominatorGate, dominatorVal};
	faultStruct b = {dominatedGate, dominatedVal};

	faultEquivNode* aNode = findFaultEquivNode(a);
	faultEquivNode* bNode = findFaultEquivNode(b);

	// cout << "Adding: " << a.loc->get_outputName() << "/" << (int)a.val << " dominates " << b.loc->get_outputName() << "/" << (int)b.val << endl;

	// A fault cannot dominate itself or an equivalent fault
	if (aNode == bNode)
		return false; 

	if ((aNode != NULL) && (bNode != NULL)) {

		// aNode dominates bNode
		if (find(aNode->dominates.begin(), aNode->dominates.end(), bNode) != aNode->dominates.end())
			return false; // it was already there
		aNode->dominates.push_back(bNode);


		// bNode isDominated by aNode
		if (find(bNode->dominatedBy.begin(), bNode->dominatedBy.end(), aNode) != bNode->dominatedBy.end())
			return false; // it was already there
		bNode->dominatedBy.push_back(aNode);

		return true;

	}

	return false;

}

/** \brief Gets the collapsed fault list implied by the fault equivalences encoded in this FaultEquiv
 *  \return a vector of faultStructs that shows the minimized fault list
 *  \note This function does not include any dominance relationships; please see the proejct hadnout if you are interested in using dominacne in Part 4
 */
vector<faultStruct> FaultEquiv::getCollapsedFaultList() {
	vector<faultStruct> allCollapsedFaults;

	for (vector<faultEquivNode*>::iterator i = allFaultEquivNodes.begin(); i < allFaultEquivNodes.end(); ++i) {
		allCollapsedFaults.push_back((*i)->equivFaults[0]);
	}
	return allCollapsedFaults;
}

/** \brief Deconstructor */
FaultEquiv::~FaultEquiv() {
	for (vector<faultEquivNode*>::iterator i = allFaultEquivNodes.begin(); i < allFaultEquivNodes.end(); ++i)
		delete *i;
}
