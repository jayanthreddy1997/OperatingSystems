#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

ifstream g_input_file;
char *g_curr_ptr;
string g_curr_line;
int g_line_no=0;
int g_final_offset;
vector<pair<string, int> > symbolTable; // Symbol modeled as a pair, variable name and its absolute address


// STEP-1: Tokenizer
struct Token {
    char* token;
    int line_no;
    int start_pos;
    bool tokenFound;
};

Token getToken() {
    if (!g_input_file.is_open()) {
        throw runtime_error("No open file to get token from!");
    }
    char delimiters[] = " \t\n";
    g_curr_ptr = strtok(NULL, delimiters);
    while (g_curr_ptr==NULL && getline(g_input_file, g_curr_line)) {
        g_curr_ptr = strtok((char*)g_curr_line.c_str(), delimiters);
        g_final_offset = g_curr_line.size();
        g_line_no += 1;
    }

    Token t;
    t.token = g_curr_ptr;
    t.line_no = g_line_no;
    t.start_pos = (g_curr_ptr==NULL) ? (g_final_offset + 1) : (g_curr_ptr - g_curr_line.c_str() + 1);
    t.tokenFound = g_curr_ptr!=NULL;
    return t;
}

void test_tokenizer() {
    // Test getToken function
    Token t;
    t = getToken();
    while (t.token != NULL) {
        cout << "Token: " << t.line_no << ":" << t.start_pos << " : " << t.token << endl;
        t = getToken();
    }
    cout << "Final Spot in File : line=" << t.line_no << " offset=" << t.start_pos << endl;
}


// STEP-2: Basic token functions
enum AddressMode { I, A, R, E };

void __parseerror(int errcode, int line_no, int offset) {
    static char* errstr[] = {
            "NUM_EXPECTED", // Number expect, anything >= 2^30 is not a number either
            "SYM_EXPECTED", // Symbol Expected
            "ADDR_EXPECTED", // Addressing Expected which is A/E/I/R
            "SYM_TOO_LONG", // Symbol Name is too long
            "TOO_MANY_DEF_IN_MODULE", // > 16
            "TOO_MANY_USE_IN_MODULE", // > 16
            "TOO_MANY_INSTR", // total num_instr exceeds memory size (512)
    };
    printf("Parse Error line %d offset %d: %s\n", line_no, offset, errstr[errcode]);
}

int readInt() {
    Token t = getToken();
//    int intToken;
//    try {
//        intToken = atoi(t.token);
//    } catch (invalid_argument const &ex) {
//
//    }
//    return intToken;
    return (t.token==NULL) ? NULL : atoi(t.token); // TODO: add error handling
}

string readSymbol() {
    Token t = getToken();
    char* tokenText = t.token;
    int i = 0;
    if (!isalpha(tokenText[i])) {
        __parseerror(1, t.line_no, t.start_pos);
        exit(0);
    }
    i+=1;
    while(tokenText[i]) {
        if (!isalnum(tokenText[i])) {
            __parseerror(1, t.line_no, t.start_pos);
            exit(0);
        }
        i += 1;
    }
    if (i>16) {
        __parseerror(3, t.line_no, t.start_pos);
    }
    return tokenText;
}

AddressMode readIAER() {
    Token t = getToken();
    if (!t.tokenFound) {
        __parseerror(2, t.line_no, t.start_pos);
        exit(0);
    }
    char* token = t.token;
    switch(token[0]) {
        case 'I':
            return I;
        case 'A':
            return A;
        case 'R':
            return R;
        case 'E':
            return E;
    }
}


void test_tokens() {
    // test readInt, readSymbol and readIAER function using input-1/2
    if (g_input_file.is_open()) {
        g_input_file.close();
    }
    g_input_file.open("lab1_assign/input-2");
    // Definition List
    assert(readInt()==1);
    assert(readSymbol()=="xy");
    assert(readInt()==2);
    // Use list
    assert(readInt()==2);
    assert(readSymbol()=="z");
    assert(readSymbol()=="xy");
    // Program text
    assert(readInt()==5);
    assert(readIAER()==R);
    assert(readInt()==1004);
    assert(readIAER()==I);
    assert(readInt()==5678);
    assert(readIAER()==E);
    assert(readInt()==2000);
    assert(readIAER()==R);
    assert(readInt()==8002);
    assert(readIAER()==E);
    assert(readInt()==7001);
}


// STEP-3: Parser
void buildSymbolTable() {
    int moduleBaseAddress = 0;
    int defCount;
    int useCount;
    int codeCount;
    while(!g_input_file.eof()) { // TODO: check condition
        defCount = readInt();
        for(int i=0; i<defCount; i++) {
            pair<string, int> newSymbol(readSymbol(), readInt()+moduleBaseAddress);
            symbolTable.push_back(newSymbol);
        }
        useCount = readInt();
        for(int i=0; i<useCount; i++) {
            readSymbol();
        }
        codeCount = readInt();
        for(int i=0; i<codeCount; i++) {
            readIAER();
            readInt();
            moduleBaseAddress += 1;
        }
    }
}

int getSymbolAddress(string symbol) {
    for (auto s: symbolTable) {
        if (s.first == symbol) {
            return s.second;
        }
    }
    return -1;
}

void buildMemoryMap() {
    int addr = 0;
    int moduleBaseAddr = 0;
    int defCount;
    int useCount;
    int codeCount;

    while(!g_input_file.eof()) { // TODO: check condition
        moduleBaseAddr = addr;
        defCount = readInt();
        for(int i=0; i<defCount; i++) {
            readSymbol();
            readInt();
        }
        useCount = readInt();
        vector<string> useList;
        for(int i=0; i<useCount; i++) {
            useList.push_back(readSymbol());
        }
        codeCount = readInt();
        for(int i=0; i<codeCount; i++) {
            AddressMode addrMode = readIAER();
            int op = readInt();
            int opcode = op / 1000;
            int operand = op % 1000;

            if (addrMode == I) {
                printf("%03d: %04d\n", addr, op);
            } else if (addrMode == A) {
                printf("%03d: %04d\n", addr, op);
            } else if (addrMode == R) {
                int newOperand = moduleBaseAddr + operand;
                int newOp = opcode * 1000 + newOperand;
                printf("%03d: %04d\n", addr, newOp);
            } else if (addrMode == E) {
                int newOperand = getSymbolAddress(useList[operand]);
                int newOp = opcode * 1000 + newOperand;
                printf("%03d: %04d\n", addr, newOp);
            }
            addr += 1;
        }
    }
}


int main(int argc, char* argv[]) {
    if(argc<=1) {
        cout << "Usage: linker input_file" << endl;
        return 0;
    }
    string input_filename = argv[1];
    g_input_file.open(input_filename);

    // PASS 1
    buildSymbolTable();
    cout << "Symbol Table" << endl;
    for(auto s: symbolTable) {
        cout << s.first << "=" << s.second << endl;
    }

    // PASS 2
    if (g_input_file.is_open()) {
        g_input_file.close();
    }
    g_input_file.open(input_filename);

    cout << endl << "Memory Map" << endl;
    buildMemoryMap();

    if (g_input_file.is_open()) {
        g_input_file.close();
    }
    return 0;
}
