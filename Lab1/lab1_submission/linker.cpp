#include <iostream>
#include <fstream>

using namespace std;

ifstream g_input_file;
char *g_curr_ptr;
string g_curr_line;
int g_line_no=0;
int g_final_offset;

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

int main(int argc, char* argv[]) {
    if(argc<=1) {
        cout << "Usage: linker input_file" << endl;
        return 0;
    }
    string input_filename = argv[1];
    g_input_file.open(input_filename);

    Token t;
    t = getToken();
    while (t.token != NULL) {
        cout << "Token: " << t.line_no << ":" << t.start_pos << " : " << t.token << endl;
        t = getToken();
    }
    cout << "Final Spot in File : line=" << t.line_no << " offset=" << t.start_pos << endl;
    return 0;
}
