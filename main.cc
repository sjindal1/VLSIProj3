// ESE-549 Project 3 Reference Solution
// Updating for Spring 2018
// Peter Milder

/** @file */

// For you to do:

// Part 1: 
//    - Write the simFullCircuit() function (based on your project 2 code)
//    - Write the getObjective() function
//    - Write the updateDFrontier() function
//    - Write the backtrace() function
//    - Write the podemRecursion() function
// Then your basic PODEM implementation should be finished.
// Test this code carefully.
// Note there is also a "checkTest" function (enabled by default) that 
// will use your simulator to run the test after you generated it and 
// check that it correctly detects the faults.
//
// Part 2:
//    - Write the eventDrivenSim() function.
//    - Change the simulation calls in your podemRecursion() function
//      from using the old simFullCircuit() function to the new
//      event-driven simulator when mode == 2.
//      (Make sure your original code still works correctly when 
//       mode == 1.)
// Then, your PODEM implementation should run considerably faster
// (probably 8 to 10x faster for large circuits).
// Test everything and then evaluate the speedup.
// A quick way to figure out how long it takes to run something in
// Linux is:
//   time ./atpg ... type other params...
//
// Part 3:
//    - Write code to perform equivalence fault collapsing
//    - Test and evaluate your result
//
// Part 4:
//    - Apply optimizations as discussed in project description

// I have marked several places with TODO for you.

#include <iostream>
#include <fstream> 
#include <vector>
#include <queue>
#include <time.h>
#include <stdio.h>
#include "parse_bench.tab.h"
#include "ClassCircuit.h"
#include "ClassGate.h"
#include "ClassFaultEquiv.h"
#include <limits>
#include <stdlib.h>
#include <time.h>

using namespace std;

/**  @brief Just for the parser. Don't touch. */
extern "C" int yyparse();

/**  Input file for parser. Don't touch. */
extern FILE *yyin;

/** Our circuit. We declare this external so the parser can use it more easily. */
extern Circuit* myCircuit;

//--------------------------
// Helper functions
void printUsage();
bool checkTest(Circuit* myCircuit);
string printPIValue(char v);
void setValueCheckFault(Gate* g, char gateValue);
//--------------------------

//----------------------------
// Functions for logic simulation
void simFullCircuit(Circuit* myCircuit);
void eventDrivenSim(Circuit* myCircuit, queue<Gate*> q);
//----------------------------

//----------------------------
// Functions for PODEM:
bool podemRecursion(Circuit* myCircuit);
bool getObjective(Gate* &g, char &v, Circuit* myCircuit);
void updateDFrontier(Circuit* myCircuit);
void backtrace(Gate* &pi, char &piVal, Gate* objGate, char objVal, Circuit* myCircuit);
bool d_dbar_on_PO(Circuit*);
//--------------------------

//----------------------------
// If you add functions, please add the prototypes here.
void setGateOutputs(vector<Gate*>&);
int findGateValue(vector<Gate*>&, int); 
bool isControllingValue(int, int);
int controllingOutput(int);
int nonControllingOutput(int);
int nonControllingValue(int);
int handleExclusiveGates(vector<Gate*>&, int);
int handleNot(vector<Gate*>&);
int handleBuffFanout(vector<Gate*>&);
void setValueForError(int, Gate*);
void setPIFanouts(Circuit*);
void setAllEquivalentNodes(vector<Gate*>&, FaultEquiv&);
bool isValidEquivGate(Gate*);
void setEquivForGate(Gate*, FaultEquiv&);
//-----------------------------



///////////////////////////////////////////////////////////
// Global variables
// These are made global to make your life slightly easier.

/** Global variable: a vector of Gate pointers for storing the D-Frontier. */
vector<Gate*> dFrontier;

/** Global variable: holds a pointer to the gate with stuck-at fault on its output location. */
Gate* faultLocation;     

/** Global variable: holds the logic value you will need to activate the stuck-at fault. */
char faultActivationVal;

/** Global variable: which part of the project are you running? */
int mode = -1;


/** @brief The main function.
 * 
 * You do not need to change anything in the main function,
 * although you should understand what is happening
 * here.
 */
int main(int argc, char* argv[]) {

	// Check the command line input and usage
	if (argc != 5) {
		printUsage();    
		return 1;
	}
	
	mode = atoi(argv[1]);
	if ((mode < 1) || (mode > 5)) {
		printUsage();    
		return 1;   
	}

	// Parse the bench file and initialize the circuit.
	FILE *benchFile = fopen(argv[2], "r");
	if (benchFile == NULL) {
		cout << "ERROR: Cannot read file " << argv[2] << " for input" << endl;
		return 1;
	}
	yyin=benchFile;
	yyparse();
	fclose(benchFile);

	myCircuit->setupCircuit(); 
	cout << endl;

	// Setup the output text files
	ofstream outputStream, equivStream;
	string dotOutFile = argv[4];
	dotOutFile += ".out";
	outputStream.open(dotOutFile);
	if (!outputStream.is_open()) {
		cout << "ERROR: Cannot open file " << dotOutFile << " for output" << endl;
		return 1;
	}
	if (mode >= 3) {
		string dotFCFile = argv[4];
		dotFCFile += ".fc";
		equivStream.open(dotFCFile);
		if (!equivStream.is_open()) {
			cout << "ERROR: Cannot open file " << dotFCFile << " for output" << endl;
			return 1;
		}

	}
	 
	// Open the fault file.
	ifstream faultStream;
	string faultLocStr;
	faultStream.open(argv[3]);
	if (!faultStream.is_open()) {
		cout << "ERROR: Cannot open fault file " << argv[3] << " for input" << endl;
		return 1;
	}
	
	// This vector will hold all of the faults you want to generate tests for. 
	// For Parts 1 and 2, you will simply go through this vector in order. 
	// For Part 3, you will start from this list and apply equivalence fault collapsing.
	// For Part 4, you can try other techniques.
	vector<faultStruct> faultList;


	// This code goes through each line in the fault file and adds the faults to 
	// faultList.
	while(getline(faultStream, faultLocStr)) {
		string faultTypeStr;
			
		if (!(getline(faultStream, faultTypeStr))) {
			break;
		}
			
		char faultType = atoi(faultTypeStr.c_str());
		Gate* fl = myCircuit->findGateByName(faultLocStr);

		faultStruct fs = {fl, faultType};
		faultList.push_back(fs);
	}
	faultStream.close();


	// --------- Equivalence Fault Collapsing (Part 3) ---------------
	// For Part 3 and Part 4, you will do equivalence based
	// fault collapsing here. 

	// The FaultEquiv hold the fault collapsing graph. (See description below
	// and in project handout.)
	FaultEquiv myFaultEquivGraph;

	vector<faultStruct> origFaultList;
	if (mode >= 3) {

		// Keep the uncollapsed list here. You can use this to test your Part 3 code later.
		origFaultList = faultList;

		// Create a new FaultEquiv, which stores the Fault Equivalence
		// relationships in your circuit.
		myFaultEquivGraph.init(faultList);

		// -------------- your Part 3 code starts here ----------------
		// TODO

		// Perform equivalence-based fault collapsing here. (You probably want to
		// write and call one or more other functions here.

		// The basic idea behind he FaultEquiv data structure:
		//   - A FaultEquiv holds a vector of faultEquivNode.
		//   - Each faultEquivNode holds three things:
		//        (1) a vector of equivalent faults
		//        (2) a vector of pointers to other faultEquivNodes that this one 
		//            dominates
		//        (3) a vector of pointers to other faultEquivNodes that are 
		//            dominated by this node

		// For Part 3, you will just use equivalence. (Later, one of the options 
		// for Part 4 will be to exploit dominance also.)

		// When you initialize a FaultEquiv, it takes faultList (the list of all
		// faults you are currently considering, which comes from the .fault file you
		// supply at the command line). Then, it initializes the structure so that
		// each fault gets its own faultEquivNode, with no equivalence or dominance set up.

		// Then, your goal is to go through the circuit, and based on the topology and
		// gate types, tell the FaultEquiv structure which nodes to set as equivalent.
		// (And later, you can choose to do the same with dominance in Part 4 if you 
		//  want.)

		// Start by reading the documentation and looking through ClassFaultEquiv.h and .cc.

		// --------- Example -----------------------
		// Example for working with FaultEquiv:
		//    Imagine that g is a Gate* pointing to a 2-input AND gate.
		//    Therefore you know that the output and both inputs of that 
		//    gate SA0 are equivalent.
		//    You could then run:

		//       Gate* a = g->get_gateInputs()[0];
		//       Gate* b = g->get_gateInputs()[1];
		//       FaultEquiv.mergeFaultEquivNodes(a, FAULT_SA0, b, FAULT_SA0);
		//       FaultEquiv.mergeFaultEquivNodes(a, FAULT_SA0, g, FAULT_SA0);

		//    Each time you run mergeFaultEquivNodes, the FaultEquiv structure
		//    looks for nodes that match the specified faults (e.g. a sa0 and b sa0),
		//    and then merges them into a single node.

		//    You can run myFaultEquivGraph.printFaultEquiv(cout); which will 
		//    print the status of the fault equivalence node. When you are 
		//    getting started, I recommend starting with a very small circuit 
		//    and use the print function frequently. Follow along by hand.
	
		// If you want to print the current state of the FaultEquiv, run this:
		//    myFaultEquivGraph.printFaultEquiv(cout);

		// Your algorithm should walk through each gate, and figure out which
		// faults to make equivalent. Note that if you get to a "fanout" gate,
		// *none of its faults are equivalent!*


		// One other possibly helpful note: I added a public bool value called 
		// "visited" to ClassGate. When a gate is created, visited is set to 
		// false. You may use this to make sure when you are performing fault 
		// equivalence, that you don't include the same gate multiple times. 
		// When you look at a Gate, check that visited == false. Then, after 
		// you perform fault collapsing on that gate, set visited to true.
		// (This may or may not help, depending on how you choose to structure 
		// your fault equivalence code.)
                
                vector<Gate*> myCircuitPOs = myCircuit->getPOGates();
                setAllEquivalentNodes(myCircuitPOs, myFaultEquivGraph);
                
		// end of your equivalence fault collapsing code
		/////////////////////////////////////////////////

		// This will print the fault equivalence result to the .fc output file
		myFaultEquivGraph.printFaultEquiv(equivStream);
		equivStream.close();

		// Update the faultList to now be the reduced list
		// Your PODEM code will now find tests only for the collapsed faults.
		faultList = myFaultEquivGraph.getCollapsedFaultList();

		// Just for your information
		cout << "Original list length: " << origFaultList.size() << endl;
		cout << "Collapsed length: " << faultList.size() << endl;

	}
	// ------------- End of Equivalence Fault Collapsing -------------


	// ------------------------- Part 4 -----------------------------
	if (mode == 4) {
		// TODO
		// You can put mode == 4 code here if you want
		// (or maybe you will just include these optimizations inside of your
		// main PODEM code, and you can remove this)


	}
	if (mode == 5) {
		// TODO
		// Here you should start your code for mode 5, test set size reduction


	}
	// -----------End of Part 4 ---------------------------------


	// ------------- PODEM code ----------------------------------
	// This will run in all parts by default. 

	// We will use this to keep track of any undetectable faults. This may
	// be useful for you depending on what you do in Part 4.
	vector<faultStruct> undetectableFaults;

	// We will use this to store all the tests that your algorithm
	// finds. You may want to use this in checking correctness of
	// your program.
	vector<vector<char>> allTests;


	// This is the main loop that performs PODEM.
	// It iterates over all faults in the faultList. For each one,
	// it will set up the fault and call your podemRecursion() function.
	// You should not have to change this, but you should understand how 
	// it works.
	for (int faultNum = 0; faultNum < faultList.size(); faultNum++) {

		// Clear the old fault in your circuit, if any.
		myCircuit->clearFaults();
		 
		// Set up the fault we are trying to detect
		faultLocation  = faultList[faultNum].loc;
		char faultType = faultList[faultNum].val;
		faultLocation->set_faultType(faultType);      
		faultActivationVal = (faultType == FAULT_SA0) ? LOGIC_ONE : LOGIC_ZERO;
		 
		// Set all gate values to X
		for (int i=0; i < myCircuit->getNumberGates(); i++) {
			myCircuit->getGate(i)->setValue(LOGIC_X);
		}

		// initialize the D frontier.
		dFrontier.clear();
		
		// call PODEM recursion function
		bool res = podemRecursion(myCircuit);

		// If we succeed, print the test we found to the output file, and 
		// store the test in the allTests vector.
		if (res == true) {
			vector<Gate*> piGates = myCircuit->getPIGates();
			vector<char> thisTest;
			for (int i=0; i < piGates.size(); i++) {
				// Print PI value to output file
				outputStream << printPIValue(piGates[i]->getValue());

				// Store PI value for later
				char v = piGates[i]->getValue();
				if (v == LOGIC_D)
					v = LOGIC_ONE;
				else if (v == LOGIC_DBAR)
					v = LOGIC_ZERO;
				thisTest.push_back(v);			

			}
			outputStream << endl;
			allTests.push_back(thisTest);
		}

		// If we failed to find a test, print a message to the output file
		else 
			outputStream << "none found" << endl;
		
		// Lastly, you can use this to test that your PODEM-generated test
		// correctly detects the already-set fault.
		// Of course, this assumes that your simulation code is correct.
		// Comment this out when you are evaluating the runtime of your
		// ATPG system because it will add extra time.
		if (res == true) {
			if (!checkTest(myCircuit)) {
				cout << "ERROR: PODEM returned true, but generated test does not detect this fault on PO." << endl;
				//myCircuit->printAllGates(); // uncomment if you want to see what is going on here
				assert(false);
			}
		}
		
		// Just printing to screen to let you monitor progress. You can comment this
		// out if you like.
		cout << "Fault = " << faultLocation->get_outputName() << " / " << (int)(faultType) << ";";
		if (res == true) 
			cout << " test found; " << endl;
		else {
			cout << " no test found; " << endl;
			faultStruct f = {faultLocation, faultType};
			undetectableFaults.push_back(f);
		}
	}

	// clean up and close the output stream
	delete myCircuit;
	outputStream.close();

	return 0;
}


/////////////////////////////////////////////////////////////////////
// Functions in this section are helper functions.
// You should not need to change these, except if you want
// to enable the checkTest function (which will use your simulator
// to attempt to check the test vector computed by PODEM.)


/** @brief Print usage information (if user provides incorrect input).
 * 
 * You don't need to touch this.
 */
void printUsage() {
	cout << "Usage: ./atpg [mode] [bench_file] [fault_file] [output_base]" << endl << endl;
	cout << "   mode:        1 through 5" << endl;
	cout << "   bench_file:  the target circuit in .bench format" << endl;
	cout << "   fault_file:  faults to be considered" << endl;
	cout << "   output_base: basename for output file" << endl;
	cout << endl;
	cout << "   The system will generate a test pattern for each fault listed" << endl;
	cout << "   in fault_file and store the result in output_loc.out" << endl;
	cout << "   If you are running Part 3 or 4, it will also print the result" << endl;
	cout << "   of your equivalence fault collapsing in file output_loc.fc" << endl << endl;
	cout << "   Example: ./atpg 3 test/c17.bench test/c17.fault myc17" << endl;
	cout << "      --> This will run your Part 3 code and produce two output files:" << endl;
	cout << "          1: myc17.out - contains the test vectors you generated for these faults" << endl;
	cout << "          2: myc17.fc  - contains the FaultEquiv data structure that shows your" << endl;
	cout << "                         equivalence classes" << endl;
	cout << endl;	
}


/** @brief Uses *your* simulator to check validity of your test.
 * 
 * This function can be called after your PODEM algorithm finishes.
 * If you enable this, it will clear the circuit's internal values,
 * and re-simulate the vector PODEM found to test your result.
 
 * This is helpful when you are developing and debugging, but will just
 * slow things down once you know things are correct.
 
 * Important: this function of course assumes that your simulation code 
 * is correct. If your simulation code is incorrect, then this is not
 * helpful to you.
*/
bool checkTest(Circuit* myCircuit) {

	simFullCircuit(myCircuit);

	// look for D or D' on an output
	vector<Gate*> poGates = myCircuit->getPOGates();
	for (int i=0; i<poGates.size(); i++) {
		char v = poGates[i]->getValue();
		if ((v == LOGIC_D) || (v == LOGIC_DBAR)) {
			return true;
		}
	}

	// If we didn't find D or D' on any PO, then our test was not successful.
	return false;

}

/** @brief Prints a PI value. 
 * 
 * This is just a helper function used when storing the final test you computed.
 *  You don't need to run or modify this.
 */
string printPIValue(char v) {
	switch(v) {
		case LOGIC_ZERO: return "0";
		case LOGIC_ONE: return "1";
		case LOGIC_UNSET: return "U";
		case LOGIC_X: return "X";
		case LOGIC_D: return "1";
		case LOGIC_DBAR: return "0";
	}
	return "";
}

/** @brief Set the value of Gate* g to value gateValue, accounting for any fault on g.
		\note You will not need to modify this.
 */
void setValueCheckFault(Gate* g, char gateValue) {
	if ((g->get_faultType() == FAULT_SA0) && (gateValue == LOGIC_ONE)) 
		g->setValue(LOGIC_D);
	else if ((g->get_faultType() == FAULT_SA0) && (gateValue == LOGIC_DBAR)) 
		g->setValue(LOGIC_ZERO);
	else if ((g->get_faultType() == FAULT_SA1) && (gateValue == LOGIC_ZERO)) 
		g->setValue(LOGIC_DBAR);
	else if ((g->get_faultType() == FAULT_SA1) && (gateValue == LOGIC_D)) 
		g->setValue(LOGIC_ONE);
	else
		g->setValue(gateValue);
}

// end of helper functions
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Start of functions for circuit simulation.

/** @brief Runs full circuit simulation
 *
 * Adapt your Project 2 code for this!
 */
void simFullCircuit(Circuit* myCircuit) {

	// Clear all the non-PI gate values
	for (int i=0; i<myCircuit->getNumberGates(); i++) {
		Gate* g = myCircuit->getGate(i);
		if (g->get_gateType() != GATE_PI)
			g->setValue(LOGIC_UNSET);      
	}  

	// TODO
	// Write your simulation code here. Use your Project 2 code.
	// Also: don't forget to check for faults on the PIs; you either
	// need to do this here *or* in the PODEM recursion when you set
	// the value.
        
        vector<Gate*> circuitPIs = myCircuit->getPIGates();
        for(Gate* g:circuitPIs){
          if(g->getValue() == LOGIC_UNSET) g->setValue(LOGIC_X);
          else{
            if(g->get_faultType() == FAULT_SA0 && g->getValue() == LOGIC_ONE) g->setValue(LOGIC_D);
            if(g->get_faultType() == FAULT_SA1 && g->getValue() == LOGIC_ZERO) g->setValue(LOGIC_DBAR);
          }
          //cout << "gate name : " << g->get_outputName() << " value :" << g->printValue() << endl;
        }
        vector<Gate*> circuitPOs = myCircuit->getPOGates();
        setGateOutputs(circuitPOs);
}

/** @brief Perform event-driven simulation.
 * \note You will write this function in Part 2.
 * 
 * Please see the project handout for a description of what
 * we are doing here and why.

 * This function takes as input the Circuit* and a queue<Gate*>
 * indicating the remaining gates that need to be evaluated.
 */
void eventDrivenSim(Circuit* myCircuit, queue<Gate*> q) {
	
        Gate* pi = q.front();
        setValueForError(pi->getValue(), pi);
        //cout << "input name " << pi->get_outputName() << " value " << pi->printValue() << endl;
        while(!q.empty()){
          Gate* input = q.front();
          q.pop();
          vector<Gate*> outputs = input->get_gateOutputs();
          vector<char> outputValues;
          for(Gate* gate:outputs){
            //cout << "output name " << gate->get_outputName() << " value " << gate->printValue() << endl;
            outputValues.push_back(gate->getValue());
            gate->setValue(LOGIC_UNSET);
          }
          setGateOutputs(outputs);
          int i = 0;
          for(Gate* gate:outputs){
            if(outputValues[i++] != gate->getValue()) q.push(gate);
          }
        }
}

// End of functions for circuit simulation
////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////
// Begin functions for PODEM.

/** @brief PODEM recursion.
 *
 * \note For Part 1, you must write this, following the pseudocode from class. 
 * Make use of the getObjective and backtrace functions.
 * For Part 2, you will add code to this that calls your eventDrivenSim() function.
 * For Parts 3 and 4, use the eventDrivenSim() version.
 */
bool podemRecursion(Circuit* myCircuit) {

	// TODO

	// Write the PODEM recursion function here. It should:
	//   - Check for D or D' on any PO and return true if so
	//   - Call getObjective
	//   - Call backtrace
	//   - Set the PI indicated by your backtrace function.
	//       - Be careful you are doing the right thing if there is a fault on a PI
	//   - Recurse
	//   - If recursion succeeds, return true.
	//   - If recursion fails, reverse value you set on the PI and recurse again.
  	//   - If recursion succeeds, return true.
  	//   - If neither recursive call returns true, imply the PI = X and return false
        //cout << "fault location name : " << faultLocation->get_outputName() << " value " << faultLocation->printValue();
        if(d_dbar_on_PO(myCircuit)) return true;
        Gate* target_gate = (Gate*)malloc(sizeof(Gate));
        char gateVal = LOGIC_UNSET;
        if(getObjective(target_gate, gateVal, myCircuit)){
          if(!target_gate) return false;
          Gate* input = (Gate*)malloc(sizeof(Gate));
          char inputVal = LOGIC_UNSET;
          backtrace(input, inputVal, target_gate, gateVal, myCircuit);
          if(!input) return false;
          
          input->setValue(inputVal);
          //Imply function based on the mode 
          if(mode == 1){
            simFullCircuit(myCircuit);
          }else{
            queue<Gate*> q;
            q.push(input);
            eventDrivenSim(myCircuit, q);
          } 
          //recurse
          bool result = podemRecursion(myCircuit);
          if(result) return true;
          if(input->get_faultType() != NOFAULT) return false;
          
          //set opposite value if recursion fails
          if(inputVal == LOGIC_ONE){
            input->setValue(LOGIC_ZERO);
          }else if(inputVal == LOGIC_ZERO){
            input->setValue(LOGIC_ONE);
          } 
          //Impy function based on the mode
          if(mode == 1){
            simFullCircuit(myCircuit);
          }else{
            queue<Gate*> q;
            q.push(input);
            eventDrivenSim(myCircuit, q);
          }
          //recurse
          result = podemRecursion(myCircuit);
          if(result) return true;
          
          //IF both fail set PI value as X and retun false 
          input->setValue(LOGIC_X);
          if(mode == 1){
            simFullCircuit(myCircuit);
          }else{
            queue<Gate*> q;
            q.push(input);
            eventDrivenSim(myCircuit, q);
          } 
          return false; 
        }
        return false;
}

// Find the objective for myCircuit. The objective is stored in g, v.
// 
// class or your textbook.
/** @brief PODEM objective function.
 *  \param g Use this pointer to store the objective Gate your function picks.
 *  \param v Use this char to store the objective value your function picks.
 *  \param myCircuit A pointer to the Circuit being considered
 *  \returns True if the function is able to determine an objective, and false if it fails.
 * \note For Part 1, you must write this, following the pseudocode in class and the code's comments.
 */
bool getObjective(Gate* &g, char &v, Circuit* myCircuit) {

	// TODO

	// Note: you have a global variable Gate* faultLocation which represents
	// the gate with the fault. Use this when you need to check if the 
	// fault has been excited yet.

	// Another note: if the fault is not excited but the fault 
	// location value is not X, then we have failed to activate 
	// the fault. In this case getObjective should fail and Return false.  
	// Otherwise, use the D-frontier to find an objective.

	// Don't forget to call updateDFrontier() to make sure your dFrontier 
	// (a global variable) is up to date.

	// Remember, for Parts 1/2 if you want to match my reference solution exactly,
	// you should choose the first gate in the D frontier (dFrontier[0]), and pick
	// its first approrpriate input.
	
        //If fault location is 1 or 0 we have failed to excite the fault
        if(faultLocation->getValue() == LOGIC_ONE || faultLocation->getValue() == LOGIC_ZERO){
          return false;
        }
        //If fault location is X we need to excite the fault
        if(faultLocation->getValue() == LOGIC_X){
          g = faultLocation;
          if(faultLocation->get_faultType() == FAULT_SA0) v = LOGIC_ONE;
          else v = LOGIC_ZERO;
          return true;
        } 
        //Once fault is excited we need to set 
        //the remaining gate inputs to the controlling value
        updateDFrontier(myCircuit);
        if(dFrontier.size() == 0) return false;
        Gate* dGate = dFrontier[0];
        vector<Gate*> gInputs = dGate->get_gateInputs();
        for(Gate* gInput:gInputs){
          if(gInput->getValue() == LOGIC_UNSET || gInput->getValue() == LOGIC_X){
            g = gInput;
            char gateType = dGate->get_gateType();
            // We treat XOR as OR and XNOR as NOR
            if(gateType == GATE_XOR){
              gateType = GATE_OR;
            }else if(gateType == GATE_XNOR) {
              gateType = GATE_NOR;
            }
            v = nonControllingValue(gateType); 
            return true;
          }
        }             

	return false;
}


// A very simple method to update the D frontier.
/** @brief A simple method to compute the set of gates on the D frontier.
 *
 * \note For Part 1, you must write this. The simplest form follows the pseudocode included in the comments.
 */

void updateDFrontier(Circuit* myCircuit) {

	// TODO

	// Procedure:
	//  - clear the dFrontier vector (stored as the global variable dFrontier -- see the 
	//    top of the file)
	//  - loop over all gates in the circuit; for each gate, check if it should be on 
	//    D-frontier; if it is, add it to the dFrontier vector.

	// If you want to match my output exactly, go through the gates in this order:
	//     for (int i=0; i < myCircuit->getNumberGates(); i++) {    
	//         Gate* g = myCircuit->getGate(i);

	// One way to improve speed for Part 4 would be to improve D-frontier management. 
	// You can add/remove gates from the D frontier during simulation, instead of adding 
	// an entire pass over all the gates like this.
        
        dFrontier.clear();
        //setPIFanouts(myCircuit);
        //We add a gate to the DFrontier if the gate has a D or a DBAR at the input but the 
        //output value of the gate is X.
        for(int i=0; i < myCircuit->getNumberGates(); i++){
          Gate* g = myCircuit->getGate(i);
          if(g && (g->getValue() == LOGIC_X)){
            vector<Gate*> gInputs = g->get_gateInputs();
            for(Gate* gInput:gInputs){
              if(gInput->getValue() == LOGIC_D || gInput->getValue() == LOGIC_DBAR){
                dFrontier.push_back(g);
                break;
              }
            }
          }
        }
        
        return;

}


// Backtrace: given objective objGate and objVal, then figure out which input (pi) to set 
// and which value (piVal) to set it to.

/** @brief PODEM backtrace function
 * \param pi Output: A Gate pointer to the primary input your backtrace function found.
 * \param piVal Output: The value you want to set that primary input to
 * \param objGate Input: The objective Gate (computed by getObjective)
 * \param objVal Input: the objective value (computed by getObjective)
 * \param myCircuit Input: pointer to the Circuit being considered
 * \note Write this function based on the psuedocode from class.
 */
void backtrace(Gate* &pi, char &piVal, Gate* objGate, char objVal, Circuit* myCircuit) {

	// TODO

	// Write your backtrace code here.

	// If you want your solution to match mine exactly:
	//   1. Go through the gate inputs in order; pick the first X input.
	//   2. Treat XOR gates like they are OR and XNOR like they are NOR.
	
        Gate* i = objGate;
        int numInversions = 0;

        while(i->get_gateType() != GATE_PI){
          char gateType = i->get_gateType();
          if(gateType == GATE_NOT || gateType == GATE_NOR || gateType == GATE_NAND || gateType == GATE_XNOR) numInversions++;
          vector<Gate*> gInputs = i->get_gateInputs();
          for(Gate* gInput:gInputs){
            if(gInput->getValue() == LOGIC_X){
              i = gInput;
              break;
            }
          } 
        }
        
        pi = i;
        if(numInversions%2 == 0) piVal = objVal;
        else if(objVal == LOGIC_ZERO) piVal = LOGIC_ONE;
        else if(objVal == LOGIC_ONE) piVal = LOGIC_ZERO;
        
        return;
}

////////////////////////////////////////////////////////////////////////////
// Please place any new functions you add here, between these two bars.

  bool d_dbar_on_PO(Circuit* myCircuit){
    vector<Gate*> myPOGates = myCircuit->getPOGates();
    for(Gate* gate:myPOGates){
      //cout << "primary output : " << gate->get_outputName() << " value " << gate->printValue() << endl;
      if(gate->getValue() == LOGIC_D || gate->getValue() == LOGIC_DBAR) return true; 
    }
    return false;
  }
  
  // Function to set the output values
  // Since we have error only at output we check the gate output for 
  // stuck at fault before we set the output value. If there is an error
  // we set a D or a DBAR accordingly
  void setGateOutputs(vector<Gate*>& circuitOuts){
    int noOfGates = circuitOuts.size();
    for(int i=0; i < noOfGates; i++){
      Gate* gateOut = circuitOuts[i];
      if(gateOut->getValue() == LOGIC_UNSET){
        vector<Gate*> circuitInputs = gateOut->get_gateInputs();
        int output = findGateValue(circuitInputs, gateOut->get_gateType());
        if(gateOut->get_faultType() == NOFAULT) 
          gateOut->setValue(output);
        else
          setValueForError(output, gateOut);
      }
    }
  }
  
  // This function is used to set a D or a DBAR if 
  // there is an error at the gate output
  void setValueForError(int result, Gate* gate){
    if(gate->get_faultType() == FAULT_SA0 && result == LOGIC_ONE){
      gate->setValue(LOGIC_D);
    }else if(gate->get_faultType() == FAULT_SA1 && result == LOGIC_ZERO){
      gate->setValue(LOGIC_DBAR);
    }else{
      gate->setValue(result);
    }
  }
  
  //Find the gate output value based on the input. The function also has a 
  //recursive call if the required values are not set
  int findGateValue(vector<Gate*>& gates, int gateType){
    int noOfGates = gates.size();
    bool xValue = false, dValue = false, dbarValue = false;
    if(gateType == GATE_XOR || gateType == GATE_XNOR){
      return handleExclusiveGates(gates, gateType);
    }
    if(gateType == GATE_NOT){
      return handleNot(gates);
    }
    if(gateType == GATE_BUFF || gateType == GATE_FANOUT){
      return handleBuffFanout(gates);
    }
    
    // AND, OR, NAND, NOR gates
    for(int i=0; i<noOfGates; i++){
      if(gates[i]->getValue() == LOGIC_UNSET) setGateOutputs(gates);
      if(gates[i]->getValue() == LOGIC_X){
        xValue = true;
      } 
      else if(gates[i]->getValue() == LOGIC_D){
        dValue = true;
      }
      else if(gates[i]->getValue() == LOGIC_DBAR){
        dbarValue = true;
      }      
      else if(isControllingValue(gates[i]->getValue(), gateType)) return controllingOutput(gateType);
    }
    
    // D or DBAR means one of the values if a controlling value hence 
    // return the controlling output
    if(dValue && dbarValue ) return controllingOutput(gateType);
    
    //If there is no controlling value and an X value output is X  
    if(xValue) return LOGIC_X;
    
    // D values and non-controlling values
    if(dValue && (gateType == GATE_AND || gateType == GATE_OR)) return LOGIC_D;
    if(dbarValue && (gateType == GATE_AND || gateType == GATE_OR)) return LOGIC_DBAR;
    
    // DBAR values and non-controlling values
    if(dValue && (gateType == GATE_NAND || gateType == GATE_NOR)) return LOGIC_DBAR;
    if(dbarValue && (gateType == GATE_NAND || gateType == GATE_NOR)) return LOGIC_D;
    
    // else case where all values are non controlling values
    return nonControllingOutput(gateType);
  }
  
  //check if input value is a controlling value based on the gate
  bool isControllingValue(int input, int gateType){
    if((gateType == GATE_NAND || gateType == GATE_AND) && input == LOGIC_ZERO) return true;  
    if((gateType == GATE_NOR || gateType == GATE_OR) && input == LOGIC_ONE) return true;
    return false;
  }
  
  //Return the controlling values of the gates
  int controllingOutput(int gateType){
    if(gateType == GATE_NAND) return LOGIC_ONE;
    if(gateType == GATE_AND) return LOGIC_ZERO;
    if(gateType == GATE_NOR) return LOGIC_ZERO;
    if(gateType == GATE_OR) return LOGIC_ONE; 
  }

  //Return the non-controlling output values of the gates
  int nonControllingOutput(int gateType){
    if(gateType == GATE_NAND) return LOGIC_ZERO;
    if(gateType == GATE_AND) return LOGIC_ONE;
    if(gateType == GATE_NOR) return LOGIC_ONE;
    if(gateType == GATE_OR) return LOGIC_ZERO; 
  }
  
  //Return the non-controlling input values of the gates
  int nonControllingValue(int gateType){
    if(gateType == GATE_NAND) return LOGIC_ONE;
    if(gateType == GATE_AND) return LOGIC_ONE;
    if(gateType == GATE_NOR) return LOGIC_ZERO;
    if(gateType == GATE_OR) return LOGIC_ZERO; 
  }

  // Speacial Function to handle XOR and XNOR
  int handleExclusiveGates(vector<Gate*>& gates, int gateType){
    if(gates[0]->getValue() == LOGIC_UNSET) setGateOutputs(gates);
    if(gates[1]->getValue() == LOGIC_UNSET) setGateOutputs(gates);
    bool xValue = false, ones = false, zeros = false, dValue = false, dbarValue = false;
    for(int i=0; i<2; i++){
      if(gates[i]->getValue() == LOGIC_X) return LOGIC_X;
      if(gates[i]->getValue() == LOGIC_ONE) ones = true;
      if(gates[i]->getValue() == LOGIC_ZERO) zeros = true;
      if(gates[i]->getValue() == LOGIC_D) dValue = true;
      if(gates[i]->getValue() == LOGIC_DBAR) dbarValue = true;
    }
    // 1 & 0
    if(zeros && ones && gateType == GATE_XOR) return LOGIC_ONE;
    if(zeros && ones && gateType == GATE_XNOR) return LOGIC_ZERO;
    // 0 & D
    if(zeros && dValue && gateType == GATE_XOR) return LOGIC_D;
    if(zeros && dValue && gateType == GATE_XNOR) return LOGIC_DBAR;
    // 1 & D
    if(ones && dValue && gateType == GATE_XOR) return LOGIC_DBAR;
    if(ones && dValue && gateType == GATE_XNOR) return LOGIC_D;
    // 0 & DBAR
    if(zeros && dbarValue && gateType == GATE_XOR) return LOGIC_DBAR;
    if(zeros && dbarValue && gateType == GATE_XNOR) return LOGIC_D;
    // 1 & DBAR
    if(ones && dbarValue && gateType == GATE_XOR) return LOGIC_D;
    if(ones && dbarValue && gateType == GATE_XNOR) return LOGIC_DBAR;
    // D & DBAR
    if(dValue && dbarValue && gateType == GATE_XOR) return LOGIC_ONE;
    if(dValue && dbarValue && gateType == GATE_XNOR) return LOGIC_ZERO;
    // else both the logics are same in that case we need to return 0 for XOR and 1 XNOR
    if(gateType == GATE_XOR) return LOGIC_ZERO;
    if(gateType == GATE_XNOR) return LOGIC_ONE;
    
  }
 
  // speacial function to handle NOT gate
  // return the complement of the input value
  int handleNot(vector<Gate*>& gates){
    if(gates[0]->getValue() == LOGIC_UNSET) setGateOutputs(gates);
    if(gates[0]->getValue() == LOGIC_ONE) return LOGIC_ZERO;
    if(gates[0]->getValue() == LOGIC_ZERO) return LOGIC_ONE;
    if(gates[0]->getValue() == LOGIC_X) return LOGIC_X;
    if(gates[0]->getValue() == LOGIC_D) return LOGIC_DBAR;
    if(gates[0]->getValue() == LOGIC_DBAR) return LOGIC_D;
      
  }

  // Speacial function to handle BUFF and FANOUT gates 
  // Return the input values as is
  int handleBuffFanout(vector<Gate*>& gates){
    if(gates[0]->getValue() == LOGIC_UNSET) setGateOutputs(gates);
    return gates[0]->getValue();
  }
   
  void setPIFanouts(Circuit* myCircuit){
    vector<Gate*> myCircuitPIs = myCircuit->getPIGates();
    for(Gate* piGate:myCircuitPIs){
      if(piGate->getValue() != LOGIC_X && piGate->getValue() != LOGIC_UNSET){
        vector<Gate*> outputGates = piGate->get_gateOutputs();
        for(Gate* outGate:outputGates){
          if(outGate->get_gateType() == GATE_FANOUT){
            setValueForError(piGate->getValue(), outGate);
          }
        }
      }
    }
  }
 
  void setAllEquivalentNodes(vector<Gate*>& circuitOuts, FaultEquiv& myFaultEquivGraph){
    int noOfGates = circuitOuts.size();
    for(int i=0; i < noOfGates; i++){
      Gate* gateOut = circuitOuts[i];
      if(!gateOut->visited){
        gateOut->visited = true;
        if(isValidEquivGate(gateOut)){
          setEquivForGate(gateOut, myFaultEquivGraph);
        }
        vector<Gate*> inputs = gateOut->get_gateInputs();
        setAllEquivalentNodes(inputs, myFaultEquivGraph);
      }
    }
  }
  
  bool isValidEquivGate(Gate* gate){
    char gateType = gate->get_gateType();
    if(gateType == GATE_FANOUT || gateType == GATE_XOR || gateType == GATE_XNOR) return false;
    else return true;
  }
  
  void setEquivForGate(Gate* gate, FaultEquiv& myFaultEquivGraph){
    vector<Gate*> inputs = gate->get_gateInputs();
    char gateType = gate->get_gateType();
    if(gateType == GATE_NOT){
      myFaultEquivGraph.mergeFaultEquivNodes(gate, FAULT_SA0, inputs[0], FAULT_SA1);
      myFaultEquivGraph.mergeFaultEquivNodes(gate, FAULT_SA1, inputs[0], FAULT_SA0);
    }
    else if(gateType == GATE_BUFF){
      myFaultEquivGraph.mergeFaultEquivNodes(gate, FAULT_SA0, inputs[0], FAULT_SA0);
      myFaultEquivGraph.mergeFaultEquivNodes(gate, FAULT_SA1, inputs[0], FAULT_SA1);
    }else{
      char stuckAtOut, stuckAtIn;
      if(controllingOutput(gateType) == LOGIC_ZERO) stuckAtOut = FAULT_SA0;
      else stuckAtOut = FAULT_SA1;
      if(nonControllingValue(gateType) == LOGIC_ONE) stuckAtIn = FAULT_SA0;
      else stuckAtIn = FAULT_SA1;
      for(Gate* inGate:inputs)
        myFaultEquivGraph.mergeFaultEquivNodes(gate, stuckAtOut, inGate, stuckAtIn); 
    }
  }
////////////////////////////////////////////////////////////////////////////


