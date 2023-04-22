#include <iostream>
#include <fstream>
#include <deque>

using namespace std;

const int MAX_FRAMES = 128;
const int MAX_VPAGES = 64;

int g_randval_offset = 0;
vector<int> randvals;

enum PageReplacementAlgo {FIFO, RANDOM, CLOCK, NRU, AGING, WORKING_SET};

struct Options {
    bool O, P, F, S, x, y, f, a;

    Options(bool O=false, bool P=false, bool F=false, bool S=false, bool x=false, bool y=false, bool f=false, bool a=false):
    O(O), P(P), F(F), S(S), x(x), y(y), f(f), a(a) {}
};
Options curr_options;

typedef struct {
    unsigned int valid: 1;
    unsigned int referenced: 1;
    unsigned int modified: 1;
    unsigned int write_protect: 1;
    unsigned int paged_out: 1;
    unsigned int page_frame_num: 7;
} PTE;

// TODO: Optimize storage
typedef struct {
    int pid;
    int page_id;
    bool is_mapped;
} Frame;

Frame frame_table[MAX_FRAMES];
deque<Frame*> free_pool;

struct VMA {
    unsigned int start_vpage: 6;
    unsigned int end_vpage: 6;
    unsigned int write_protected: 1;
    unsigned int file_mapped: 1;

    VMA(int start_vpage, int end_vpage, int write_protected, int file_mapped):
        start_vpage(start_vpage), end_vpage(end_vpage), write_protected(write_protected),
        file_mapped(file_mapped) {}
};

class Process {
public:
    int pid;
    vector<VMA> vma_list;
    PTE page_table[MAX_VPAGES];

    Process(int pid): pid(pid){}
};

vector<Process*> processes;
Process *curr_proc;
vector<pair<char, int> > instructions;

class Pager {
public:
    virtual Frame* select_victim_frame() { return NULL; }
};
Pager *CURR_PAGER;

Frame *allocate_frame_from_free_list() {
    // TODO
    return NULL;
}

Frame *get_frame() {
    Frame *frame = allocate_frame_from_free_list();
    if (frame == NULL) frame = CURR_PAGER->select_victim_frame();
    return frame;
}

void run_simulation() {
//    while (get_next_instruction(&operation, &vpage)) {
//    // handle special case of “c” and “e” instruction // now the real instructions for read and write pte_t *pte = &current_process->page_table[vpage]; if ( ! pte->present) {
//    // this in reality generates the page fault exception and now you execute
//    // verify this is actually a valid page in a vma if not raise error and next inst frame_t *newframe = get_frame();
//    //-> figure out if/what to do with old frame if it was mapped
//    // see general outline in MM-slides under Lab3 header and writeup below
//    // see whether and how to bring in the content of the access page.
//        }
//    // now the page is definitely present
//    // check write protection
//    // simulate instruction execution by hardware by updating the R/M PTE bits update_pte(read/modify) bits based on operations.
//    }
}

void read_input_files(string &infile, string &rfile) {
    ifstream randval_input_file;
    randval_input_file.open(rfile);
    int rand_num;
    int rand_num_count;
    randval_input_file >> rand_num_count;
    for(int i=0; i<rand_num_count; i++) {
        randval_input_file >> rand_num;
        randvals.push_back(rand_num);
    }

    int n_procs;
    int n_vmas;
    int start_vpage;
    int end_vpage;
    int write_protected;
    int file_mapped;

    ifstream input_file;
    input_file.open(infile);
    string in;
    getline(input_file, in);
    while (in[0] == '#') {
        getline(input_file, in);
    }
    n_procs = atoi(in.c_str());

    for (int i=0; i<n_procs; i++) {
        Process* p = new Process(i);
        getline(input_file, in);
        while (in[0] == '#') {
            getline(input_file, in);
        }
        n_vmas = atoi(in.c_str());

        for (int j=0; j<n_vmas; j++) {
            getline(input_file, in);
            while (in[0] == '#') {
                getline(input_file, in);
            }
            sscanf(in.c_str(), "%d %d %d %d", &start_vpage, &end_vpage, &write_protected, &file_mapped);
            p->vma_list.emplace_back(start_vpage, end_vpage, write_protected, file_mapped);
        }
        processes.push_back(p);
    }
    char cmd;
    int id;
    while (getline(input_file, in)) {
        if (in[0] == '#') {
            continue;
        }
        sscanf(in.c_str(), "%c %d", &cmd, &id);
        instructions.emplace_back(cmd, id);
    }
}

int main(int argc, char **argv) {
    int n_frames = 16;
    PageReplacementAlgo algo = FIFO;
    string infile = "problem/in1";
    string rfile = "problem/rfile";
    curr_options = new Options(true);

    read_input_files(infile, rfile);

    // Initialize free pool
    for (int i=0; i<MAX_FRAMES; i++) {
        free_pool.push_back(&frame_table[i]);
    }

    run_simulation();

}
