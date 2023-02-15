#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

using namespace std;

ifstream g_input_file;
char *g_curr_ptr;
string g_curr_line;
int g_line_no=0;
int g_final_offset;

struct Symbol {
    string symbol;
    int addr;
    int moduleNo;
    bool multipleDefinition = false;
    bool symbolExists = true;
    bool used = false;
};
vector<Symbol> symbolTable;


// STEP-1: Tokenizer
struct Token {
    char* token;
    int line_no;
    int start_pos;
    bool tokenFound;
};

Token getToken() {
    if (!g_input_file.is_open()) {
        cout << "No open file to get token from!" << endl;
        exit(-1);
    }
    char delimiters[] = " \t\n";
    g_curr_ptr = strtok(NULL, delimiters);
    while (g_curr_ptr==NULL && getline(g_input_file, g_curr_line)) {
        g_curr_ptr = strtok((char*)g_curr_line.c_str(), delimiters);
        g_final_offset = g_curr_line.size();
        if (!g_input_file.eof()) {
            g_final_offset += 1;
        }
        g_line_no += 1;
    }

    Token t;
    t.token = g_curr_ptr;
    t.line_no = g_line_no;
    t.start_pos = (g_curr_ptr==NULL) ? g_final_offset : (g_curr_ptr - g_curr_line.c_str() + 1);
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
    static string errstr[] = {
            "NUM_EXPECTED", // Number expect, anything >= 2^30 is not a number either
            "SYM_EXPECTED", // Symbol Expected
            "ADDR_EXPECTED", // Addressing Expected which is A/E/I/R
            "SYM_TOO_LONG", // Symbol Name is too long
            "TOO_MANY_DEF_IN_MODULE", // > 16
            "TOO_MANY_USE_IN_MODULE", // > 16
            "TOO_MANY_INSTR", // total num_instr exceeds memory size (512)
    };
    printf("Parse Error line %d offset %d: %s\n", line_no, offset, errstr[errcode].c_str());
}

struct IntToken {
    int token;
    int line_no;
    int start_pos;
    bool tokenFound;
};
IntToken readInt(bool strict=true) {
    Token t = getToken();
    IntToken result;
    result.line_no = t.line_no;
    result.start_pos = t.start_pos;
    char* tokenText = t.token;
    if (!t.tokenFound) {
        if (strict) {
            __parseerror(0, t.line_no, t.start_pos);
            exit(0);
        } else {
            result.tokenFound = false;
            return result;
        }
    }
    int i = 0;
    while(tokenText[i]) {
        if (!isdigit(tokenText[i])) {
            __parseerror(0, t.line_no, t.start_pos);
            exit(0);
        }
        i += 1;
    }
    result.token = atoi(tokenText);
    if (result.token == -1 || result.token >= pow(2, 30)) {
        __parseerror(0, t.line_no, t.start_pos);
        exit(0);
    }
    result.tokenFound = true;
    return result;
}

string readSymbol() {
    Token t = getToken();
    if (!t.tokenFound) {
        __parseerror(1, t.line_no, t.start_pos);
        exit(0);
    }
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
    if (strlen(token)>1) {
        __parseerror(2, t.line_no, t.start_pos);
        exit(0);
    }
    switch(token[0]) {
        case 'I':
            return I;
        case 'A':
            return A;
        case 'R':
            return R;
        case 'E':
            return E;
        default:
            __parseerror(2, t.line_no, t.start_pos);
            exit(0);
    }
}


// STEP-3: Parser
Symbol& getSymbol(string& symbol) {
    for (Symbol& s: symbolTable) {
        if (s.symbol == symbol) {
            return s;
        }
    }
    Symbol* s = new Symbol();
    s->symbolExists = false;
    return *s;
}

void buildSymbolTable() {
    int moduleBaseAddress = 0;
    int defCount;
    int useCount;
    int codeCount;
    int totalInstrCount = 0;
    int moduleCount = 0;
    while(true) {
        moduleCount += 1;
        IntToken defCountToken = readInt(false);
        if (!defCountToken.tokenFound) {
            break;
        }
        defCount = defCountToken.token;
        if (defCount > 16) {
            __parseerror(4, defCountToken.line_no, defCountToken.start_pos);
            exit(0);
        }
        for(int i=0; i<defCount; i++) {
            string symbol = readSymbol();
            Symbol& existingSymbol = getSymbol(symbol);
            if (existingSymbol.symbolExists) {
                existingSymbol.multipleDefinition = true;
                printf("Warning: Module %d: %s redefined and ignored\n", moduleCount, symbol.c_str());
                readInt();
            } else {
                Symbol newSymbol = Symbol();
                newSymbol.symbol = symbol;
                newSymbol.addr =  readInt().token + moduleBaseAddress;
                newSymbol.symbolExists = true;
                newSymbol.moduleNo = moduleCount;
                symbolTable.push_back(newSymbol);
            }
        }

        IntToken useCountToken = readInt();
        useCount = useCountToken.token;
        if (useCount > 16) {
            __parseerror(5, useCountToken.line_no, useCountToken.start_pos);
            exit(0);
        }

        for(int i=0; i<useCount; i++) {
            readSymbol();
        }

        IntToken codeCountToken = readInt();
        codeCount = codeCountToken.token;
        totalInstrCount += codeCount;
        if (totalInstrCount > 512) {
            __parseerror(6, codeCountToken.line_no, codeCountToken.start_pos);
            exit(0);
        }
        for(int i=0; i<codeCount; i++) {
            readIAER();
            readInt();
        }

        for (Symbol& s: symbolTable) {
            if (s.moduleNo == moduleCount && (s.addr - moduleBaseAddress) >= codeCount) {
                printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", moduleCount, s.symbol.c_str(), (s.addr - moduleBaseAddress), codeCount-1);
                s.addr = moduleBaseAddress;
            }
        }
        moduleBaseAddress += codeCount;
    }
}

void buildMemoryMap() {
    int addr = 0;
    int moduleBaseAddr = 0;
    int defCount;
    int useCount;
    int codeCount;
    int moduleCount = 0;

    while(!g_input_file.eof()) {
        moduleCount += 1;
        moduleBaseAddr = addr;
        IntToken intToken = readInt(false);
        if (!intToken.tokenFound) {
            break;
        }
        defCount = intToken.token;
        for(int i=0; i<defCount; i++) {
            readSymbol();
            readInt();
        }

        useCount = readInt().token;
        vector<pair<string, bool> > useList; // pair indicating symbol name and whether it is used
        for(int i=0; i<useCount; i++) {
            pair<string, bool> newEntry(readSymbol(), false);
            useList.push_back(newEntry);
        }

        codeCount = readInt().token;
        for(int i=0; i<codeCount; i++) {
            AddressMode addrMode = readIAER();
            int op = readInt().token;
            int opcode = op / 1000;
            int operand = op % 1000;

            if (addrMode == I) {
                if (op>=10000) {
                    printf("%03d: 9999 Error: Illegal immediate value; treated as 9999\n", addr);
                } else {
                    printf("%03d: %04d\n", addr, op);
                }
            } else if (opcode>=10) {
                printf("%03d: 9999 Error: Illegal opcode; treated as 9999\n", addr);
            } else if (addrMode == A) {
                if(operand >= 512) {
                    printf("%03d: %04d Error: Absolute address exceeds machine size; zero used\n", addr, opcode*1000);
                } else {
                    printf("%03d: %04d\n", addr, op);
                }
            } else if (addrMode == R) {
                int newOperand;
                int newOp;
                if (operand >= codeCount) {
                    newOperand = moduleBaseAddr;
                    newOp = opcode * 1000 + newOperand;
                    printf("%03d: %04d Error: Relative address exceeds module size; zero used\n", addr, newOp);
                } else {
                    newOperand = moduleBaseAddr + operand;
                    newOp = opcode * 1000 + newOperand;
                    printf("%03d: %04d\n", addr, newOp);
                }
            } else if (addrMode == E) {
                if (operand >= useList.size()) {
                    printf("%03d: %04d Error: External address exceeds length of uselist; treated as immediate\n", addr, op);
                } else {
                    Symbol& s = getSymbol(useList[operand].first);
                    useList[operand].second = true;
                    if (s.symbolExists) {
                        s.used = true;
                        int newOperand = s.addr;
                        int newOp = opcode * 1000 + newOperand;
                        printf("%03d: %04d\n", addr, newOp);
                    } else {
                        int newOp = opcode * 1000;
                        printf("%03d: %04d Error: %s is not defined; zero used\n", addr, newOp, useList[operand].first.c_str());
                    }
                }
            }
            addr += 1;
        }
        for (auto x: useList) {
            if (!x.second) {
                printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", moduleCount, x.first.c_str());
            }
        }
    }
}


int main(int argc, char* argv[]) {
    if(argc<=1) {
        cout << "Expected argument after options" << endl;
        return 0;
    }
    string input_filename = argv[1];
    g_input_file.open(input_filename);
    if (!g_input_file.is_open()) {
        printf("Not a valid inputfile <%s>\n", input_filename.c_str());
        exit(1);
    }
    // PASS 1
    buildSymbolTable();
    cout << "Symbol Table" << endl;
    for(int i=0; i<symbolTable.size(); i++) {
        Symbol s = symbolTable.at(i);
        if (s.multipleDefinition) {
            cout << s.symbol << "=" << s.addr << " Error: This variable is multiple times defined; first value used" << endl;
        } else {
            cout << s.symbol << "=" << s.addr << endl;
        }
    }

    // PASS 2
    if (g_input_file.is_open()) {
        g_input_file.close();
    }
    g_input_file.open(input_filename);

    cout << endl << "Memory Map" << endl;
    buildMemoryMap();

    cout << endl;
    for(int i=0; i<symbolTable.size(); i++) {
        Symbol s = symbolTable.at(i);
        if (!s.used) {
            printf("Warning: Module %d: %s was defined but never used\n", s.moduleNo, (s.symbol).c_str());
        }
    }
    if (g_input_file.is_open()) {
        g_input_file.close();
    }
    return 0;
}
