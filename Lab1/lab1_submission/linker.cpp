#include <iostream>
#include <fstream>

using namespace std;

ifstream g_input_file;
char *g_curr_ptr;
string g_curr_line;
int g_line_no=0;
int g_final_offset;


// STEP-1: Tokenizer
struct Token {
    char* token;
    int line_no;
    int start_pos;
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
    t.start_pos = (g_curr_ptr==NULL) ? (g_final_offset + 2) : (g_curr_ptr - g_curr_line.c_str() + 1);
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

int readInt() {
    Token t = getToken();
    return atoi(t.token); // TODO: add error handling
}

string readSymbol() {
    return getToken().token;
}

AddressMode readIAER() {
    // TODO: add error handling
    char* token = getToken().token;
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

bool isdigit() {
    // TODO
    return true;
}
bool isalpha() {
    // TODO
    return true;
}
bool isalnum() {
    // TODO
    return true;
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



int main(int argc, char* argv[]) {
    if(argc<=1) {
        cout << "Usage: linker input_file" << endl;
        return 0;
    }
    string input_filename = argv[1];
    g_input_file.open(input_filename);



    g_input_file.close();
    return 0;
}
