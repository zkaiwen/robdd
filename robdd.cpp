////////////////////////////////////////////////////////////////////////
// File: robdd.cpp
// Kevin Zeng
// 2012-03-02
// EDA
// PROJECT 2
////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string> 
#include <cstring>
#include <list>
#include <map>
#include <queue>
#include <fstream>

/******************************************************************************************
##########################################################################################

   Circuit Class:
		Circuit netlist data structure

##########################################################################################
*******************************************************************************************/
enum
{
   G_JUNK,         /* 0 */
   G_INPUT,        /* 1 */
   G_OUTPUT,       /* 2 */
   G_XOR,          /* 3 */
   G_XNOR,         /* 4 */
   G_DFF,          /* 5 */
   G_AND,          /* 6 */
   G_NAND,         /* 7 */
   G_OR,           /* 8 */
   G_NOR,          /* 9 */
   G_NOT,          /* 10 */
   G_BUF,          /* 11 */
};

class circuit
{
    // circuit information
    int numgates;       // total number of gates (faulty included)
    int maxlevels;      // number of levels in gate level ckt
    int numInputs;		// number of inputs
    int numOutputs;		// number of outputs
    unsigned char *gtype;	// gate type
    short *fanin;		// number of fanins of gate
    short *fanout;		// number of fanouts of gate
    int *levelNum;		// level number of gate
    int **faninlist;	// fanin list of gate
    int **fanoutlist;	// fanout list of gate
	std::vector<int> outputGate;

	//ITE INFORMATION Members
	std::vector<int> gateNode;					// List of BDD Node Index given gate number
	std::vector<int*> uniqueTable;				// BDD Table
	std::map<std::vector<int>, int> hashUT;		// Unique Hash Table
	std::map<std::vector<int>, int> hashCT
	;		// Compute Hash Table

public:
    circuit(char *);    // constructor
	~circuit();

	//ITE Function
	int ITE(int i, int t, int e);	
	bool terminal(int i, int t, int e, int& terminalResult);
	void getBDD();
	int topVariable(int i, int t, int e);
	void printUT();
	int function(int pointer, int top, bool tf);
	int min(int i, int t, int e);
	int min(int i, int t);
	int insertUT(int i, int t, int e);
	void toString(std::string & ostr);
	bool iteExists(int i, int t, int e, int &CTIndex);

};

////////////////////////////////////////////////////////////////////////
// constructor: reads in the *.lev file and builds basic data structure
// 		for the gate-level ckt
////////////////////////////////////////////////////////////////////////
circuit::circuit(char *cktName)
{
    FILE *inFile;
    char fName[40];
    int i, j, count;
    char c;
    int gatenum, junk;
    int f1, f2, f3;
    strcpy(fName, cktName);
    strcat(fName, ".lev");

    inFile = fopen(fName, "r");
    if (inFile == NULL)
    {
	fprintf(stderr, "Can't open .lev file\n");
	exit(-1);
    }

    numgates = maxlevels = 0;
    numInputs = numOutputs = 0;

    fscanf(inFile, "%d", &count);	// number of gates
    fscanf(inFile, "%d", &junk);	// skip the second line

	printf("Reading in gate information...\n");
    // allocate space for gates data structure
    gtype = new unsigned char[count];
    fanin = new short[count];
    fanout = new short[count];
    levelNum = new int[count];
    faninlist = new int * [count];
    fanoutlist = new int * [count];

    // now read in the circuit
    for (i=1; i<count; i++)
	{
		fscanf(inFile, "%d", &gatenum);
		fscanf(inFile, "%d", &f1);
		fscanf(inFile, "%d", &f2);
		fscanf(inFile, "%d", &f3);
	
		numgates++;
		gtype[gatenum] = (unsigned char) f1;

		if (gtype[gatenum] == G_INPUT)
		    numInputs++;
		else if (gtype[gatenum] == G_OUTPUT){
		    numOutputs++;
			outputGate.push_back(gatenum);
		}
		else if (gtype[gatenum] > 13)
		    printf("gate %d is an unimplemented gate type\n", gatenum);
	
		f2 = (int) f2;
		levelNum[gatenum] = f2;
	
		if (f2 >= (maxlevels))
		    maxlevels = f2 + 5;
	
		fanin[gatenum] = (int) f3;

		// now read in the faninlist
		faninlist[gatenum] = new int[fanin[gatenum]];

		for (j=0; j<fanin[gatenum]; j++)
		{
		    fscanf(inFile, "%d", &f1);
		    faninlist[gatenum][j] = (int) f1;
		}
		for (j=0; j<fanin[gatenum]; j++) // followed by samethings
		    fscanf(inFile, "%d", &junk);
	
		// read in the fanout list
		fscanf(inFile, "%d", &f1);
		fanout[gatenum] = (int) f1;
	
		fanoutlist[gatenum] = new int[fanout[gatenum]];
		for (j=0; j<fanout[gatenum]; j++)
		{
		    fscanf(inFile, "%d", &f1);
		    fanoutlist[gatenum][j] = (int) f1;
		}
	
		// skip till end of line
		while ((c = getc(inFile)) != '\n' && c != EOF);
    }	// for (i...)
    fclose(inFile);

 	printf("Successfully read in circuit:\n");
    printf("\t%d PIs.\n", numInputs);
    printf("\t%d POs.\n", numOutputs);
    printf("\t%d total number of gates\n", numgates);
    printf("\t%d levels in the circuit.\n", maxlevels / 5);

	//SET ITE OPERATIONS
	//Adding Terminal Nodes to Unique Table
	uniqueTable.push_back(0);
	uniqueTable.push_back(new int[3]);
	uniqueTable[1][0] = 1;
	uniqueTable[1][1] = 1;
	uniqueTable[1][2] = 1;
	uniqueTable.push_back( new int[3]);
	uniqueTable[2][0] = 2;
	uniqueTable[2][1] = 2;
	uniqueTable[2][2] = 2;
	std::vector<int> key0 (3,1);
	std::vector<int> key1 (3,2);
	hashUT[key0] = 1;
	hashUT[key1] = 2;	

	//Offset Gate by 1
	gateNode.push_back(0);
		
	//Adding Inputs to Unique Table
	for(int i = 3; i < numInputs+3; i++){
		uniqueTable.push_back(new int[3]);
		uniqueTable[i][2] = i-2;
		uniqueTable[i][1] = 2;
		uniqueTable[i][0] = 1;

		int keys[] = {i-2, 2, 1};
		std::vector<int> key (keys, keys + sizeof(keys) / sizeof(int));
		hashUT[key] = i;	
		gateNode.push_back(i);
	}



   
}


//Destructor for Circuit
circuit::~circuit(){
	//Free Allocated Memory
	for(int i = 0; i < uniqueTable.size(); i++)
		delete uniqueTable[i];

    delete gtype;
    delete fanin;
    delete fanout;
    delete levelNum;
    delete faninlist;
    delete fanoutlist;
}


////////////////////////////////////////////////////////////////////////
// Extra member functions here
////////////////////////////////////////////////////////////////////////

//Extracts BDD From circuit file
void circuit::getBDD(){

	//Cycle through every gate (Gate is ordered in level fashion)
	for(int i = numInputs+1; i < numgates; i++){
		int gate = gtype[i];			// Get the gate type
		int index = 0;
		int f,g,h;
		int *in = faninlist[i];			// Get the fanin list for the gate

		//Check which gate we are dealing with and call ITE
		switch(gate){
			case 3:		//XOR
				index = ITE(gateNode[in[0]], -1 * gateNode[in[1]], gateNode[in[1]]);
				break;

			case 4:		//XNOR
				index = ITE(gateNode[in[0]], gateNode[in[1]],  -1 * gateNode[in[1]]);
				break;

			case 6:		//AND
				index = ITE(gateNode[in[0]], gateNode[in[1]], 1);
	
				//Check for multi input
				for(int j = 2; j < (fanin[i]); j++)
					index = ITE(gateNode[in[j]], index, 1); 					
				
				break;
				
			case 7:		//NAND
				//Check for multi input. If more and 2 input, gates are and gates with the last one being a nand gate
				if(fanin[i] < 3)
					index = ITE(gateNode[in[0]], -1 * (gateNode[in[1]]), 2);
				
				else{
					index = ITE(gateNode[in[0]], gateNode[in[1]], 1);
					for(int j = 2; j < (fanin[i] - 1); j++)
						index = ITE(index, gateNode[in[j]], 1); 					
					index = ITE(index, -1 * gateNode[in[fanin[i]-1]], 2);
				}
				break;

			case 8:		//OR
				index = ITE(gateNode[in[0]], 2, gateNode[in[1]]);
				for(int j = 2; j < (fanin[i]); j++)
					index = ITE(gateNode[in[j]], 2, index);				

				break;

			case 9:		//NOR
				//Check for multi input. If more and 2 input, gates are and gates with the last one being a nor gate
				if(fanin[i] < 3)
					index = ITE(gateNode[in[0]], 1, -1 * (gateNode[in[1]]));
				else{
					index = ITE(gateNode[in[0]], 2, gateNode[in[1]]);
					for(int j = 2; j < (fanin[i] - 1); j++)
						index = ITE(gateNode[in[j]], 2, index); 					

					index = ITE(index, 1, -1 * gateNode[in[fanin[i]-1]]);
				}
				
				break;
			case 10:	//NOT	
				index = ITE(gateNode[in[0]], 1, 2);
				break;
		};

		//Add BDD node for that gate number
		gateNode.push_back(index);
	}

}


//ITE FUNCTION
int circuit::ITE(int i, int t, int e){
	int terminalResult = 0;

	//Check if ITE is a terminal node
	if(terminal(i, t, e, terminalResult)){
		return terminalResult;		
	}
	
	//Check computed Table to see if ITE is already computed
	if(iteExists(i,t,e, terminalResult))
		return terminalResult;

	//Get top variable give i t e 
	int top = topVariable(i, t, e);

	//Recurse ITE through true and false edge
	int TE = ITE(function(i, top, true), function(t, top, true), function(e, top, true));
	int FE = ITE(function(i, top, false), function(t, top, false), function(e, top, false));

	//Check if TE and FE are the same. 
	if(TE == FE)
		return TE;
	
	//Add node into table if unique
	int label = insertUT(top, TE, FE);

	//Add node to CT;
	int keys[] = {i, t, e};
	std::vector<int> key (keys, keys+sizeof(keys)/sizeof(int));
	hashCT[key] = label;

	return label;
}


//FIND/INSERT ITE INTO UNIQUE TABLE
int circuit::insertUT(int i, int t, int e){
	//Extract KEY
	int keys[] = {i, t, e};
	std::vector<int> key (keys, keys+sizeof(keys)/sizeof(int));

	//Check to see if uniqueTable consist of the ITE
	if(hashUT.find(key) != hashUT.end())
		return hashUT[key];

	//Add ite into unique table
	int index = uniqueTable.size();
	uniqueTable.push_back(new int[3]);
	uniqueTable[index][2] = i;
	uniqueTable[index][1] = t;
	uniqueTable[index][0] = e;
	hashUT[key] = index;

	return index;
}


//FIND ITE IN COMPUTE TABLE
bool circuit::iteExists(int i, int t, int e, int &CTIndex){
	//Extract KEY
	int keys[] = {i, t, e};
	std::vector<int> key (keys, keys+sizeof(keys)/sizeof(int));

	//Check to see if compute table has index for ITE already
	if(hashCT.find(key) == hashCT.end())
		return false;
	else{
		CTIndex = hashCT[key];
		return true;
	}
}


//Check to see if node is terminal
bool circuit::terminal(int i, int t, int e, int& terminalResult){
	if(i == 2)
		terminalResult = t;
	else if (i == 1)
		terminalResult = e;
	else if (t == e)
		terminalResult = t;
	else if (t == 2 && e == 1)
		terminalResult = i;
	else if (t == 1 && e == 2)
		terminalResult = -1 * i;
	else if(i == e && t == 2)
		terminalResult  = e;
	else if(i == e && t == 1)
		terminalResult = 1;
	else if (i == t && e == 1)
		terminalResult = i;
	else if ( i == t && e == 2)
		terminalResult = 2;
	else
		return false;
	
	return true;
		
}


//Get the top variable given i t e
int circuit::topVariable(int i, int t, int e){
	//Handle neg edges
	if(i < 0) i = i*-1;
	if(t < 0) t = t*-1;
	if(e < 0) e = e*-1;

	//Get variable of i t and e
	int ii = uniqueTable[i][2];
	int tt = uniqueTable[t][2];
	int ee = uniqueTable[e][2];

	//Check to see which one is top
	if(i > 2){
		if(t > 2){
			if(e > 2)
				//2 2 2
				return min(ii,tt,ee);
				
			//2 2 0
			return min(ii, tt);
		}
		
		if(e > 2)
			// 2 0 2
			return min(ii, ee);

		// 2 0 0
		return ii;
	}

	if(t > 2){
		if(e > 2)
			//0 2 2
			return min(tt, ee);
			
		//0 2 0
		return tt;
	}
	
	if(e > 2)
		// 0 0 2
		return ee;
	
}

//Returns the minimum of the three values
int circuit::min(int i , int t, int e){
	int min = i;

	if(t < min)
		min = t;
	
	if(e < min)
		min = e;
	
	return min;
}

//Returns the minimum of the two values
int circuit::min(int i , int t){
	if(i >= t)	return t;
	else return i;
}


//Given variable, what does the function evaluate to 
int circuit::function(int pointer,int top, bool tf){
	//Check to see if node is negative
	if(pointer >  0){
		//Return Terminal Nodes
		if (pointer == 2 || pointer == 1)
			return pointer;
		
		//Return Corresponding edge node
		if(uniqueTable[pointer][2] == top && tf)
			return uniqueTable[pointer][1];
		else if(uniqueTable[pointer][2] == top && !tf)
			return uniqueTable[pointer][0];
		
		return pointer;
	}
	else{
		pointer *= -1;
		
		//Return the opposite edge node
		if(pointer == 2 )
			return 1;
		else if (pointer == 1)
			return 2;
		
		if(uniqueTable[pointer][2] == top && tf)
			return  -1 * uniqueTable[pointer][1];
		else if(uniqueTable[pointer][2] == top && !tf)
			return  -1 * uniqueTable[pointer][0];

		return -1 * pointer;
	}
	
}


//Prints out the UT
void circuit::printUT(){
	for(int i = 1; i < uniqueTable.size(); i++){
		printf("%d ", i);
		for( int j = 2; j >= 0; j--){
			printf("%d ", uniqueTable[i][j]);
		}
		printf("\n");
	}
}

//Converts the BDD table to string format
void circuit::toString(std::string &ostr){
	std::stringstream ss;
	std::vector<int> oGate; 
	
	//Get First Line INFO
	ss << numInputs << " " << numOutputs << " " << uniqueTable.size()  << "\n";

	//Get Variable Order
	for(int i = 1; i < (numInputs+1); i++){
		ss << i << " ";
	}
	ss << "\n"; 


	

	//Get Root/Output Nodes
	for(int i = 0; i < outputGate.size(); i++){
		int outputGateNum = faninlist[outputGate[i]][0];
		ss << gateNode[outputGateNum] << " ";
	}

	ss << "\n";

	//Extract BDD Table
	for(int i = 1; i < uniqueTable.size(); i++){
		ss << i << " ";

		for(int j = 2; j >=0; j--)
			ss << uniqueTable[i][j] << " ";

		ss << "\n";
	}
	ostr = ss.str();
}


//*************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************************







/******************************************************************************************
##########################################################################################

	MAIN FUNCTION
	 
##########################################################################################
*******************************************************************************************/
main(int argc, char *argv[])
{
    char cktName[20];
    circuit *ckt;

    if (argc != 2)
    {
		fprintf(stderr, "Usage: %s <ckt>\n", argv[0]);
        exit(-1);
    }

    strcpy(cktName, argv[1]);

    // read in the circuit and build data structure
    ckt = new circuit(cktName);

	//Extract BDD from Circuit
	ckt->getBDD();

	//Get bdd file in string format
	std::string b2d;
	ckt->toString(b2d);

    char bddfile[40];

    strcpy(bddfile, argv[1]);
    strcat(bddfile, ".bdd");

	//Output data to file
	std::ofstream ostream;
	ostream.open(bddfile, std::ios::out);
	ostream << b2d;
	ostream.close();

	//Release Memory
	delete ckt;




}

