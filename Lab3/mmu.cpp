#include <iostream>
#include <fstream>
#include <deque>

using namespace std;

const int MAX_VPAGES = 64;
int max_frames = 128;

int g_randval_offset = 0;
vector<int> randvals;

enum PageReplacementAlgo {FIFO, RANDOM, CLOCK, NRU, AGING, WORKING_SET};

struct Options{
    bool O, P, F, S, x, y, f, a;
};
Options curr_options;

typedef struct {
    unsigned int valid: 1;
    unsigned int referenced: 1;
    unsigned int modified: 1;
    unsigned int write_protect: 1;
    unsigned int paged_out: 1;
    unsigned int page_frame_num: 7;

    unsigned int is_vma_mapped: 1;
    unsigned int file_mapped: 1;
    // TODO: check is we need to initialize to 0
} PTE;

// TODO: Optimize storage
typedef struct {
    int pid;
    int page_id;
    bool is_mapped;
} Frame;

Frame* frame_table;
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
    virtual Frame *select_victim_frame() = 0;
};

class FIFO_Pager: public Pager {
    int head;
public:
    FIFO_Pager() {
        head = 0;
    }

    virtual Frame *select_victim_frame() {
        Frame *f = frame_table + head;
        head = (head + 1)%max_frames;
        return f;
    }
};

Pager *CURR_PAGER;

Frame *allocate_frame_from_free_list() {
    if (free_pool.empty())
        return NULL;
    Frame *f = free_pool.front();
    free_pool.pop_front();
    return f;
}

Frame *get_frame() {
    Frame *frame = allocate_frame_from_free_list();
    if (frame == NULL) {
        frame = CURR_PAGER->select_victim_frame();
        PTE *del_pte = &processes[frame->pid]->page_table[frame->page_id];
        del_pte->valid = 0;
        if (curr_options.O)
            printf(" UNMAP %d:%d\n", frame->pid, frame->page_id);
        if (del_pte->modified) {
            del_pte->paged_out = 1;
            if (del_pte->file_mapped) {
                if (curr_options.O)
                    printf(" FOUT\n");
            } else {
                if (curr_options.O)
                    printf(" OUT\n");
            }
        }
    }
    return frame;
}

int INSTR_POS = -1;
bool get_next_instruction(char &operation, int &vpage) {
    INSTR_POS += 1;
    if (INSTR_POS >= instructions.size()) {
        return false;
    } else {
        operation = instructions[INSTR_POS].first;
        vpage = instructions[INSTR_POS].second;
        return true;
    }
}

bool page_fault_handler(int vpage, VMA *vma) {
    // Check if in VMA, else raise error
    if (curr_proc->page_table[vpage].is_vma_mapped) {
        return true;
    }
    bool vpage_exists = false;
    for (auto &curr_vma: curr_proc->vma_list) {
        if (vpage >= curr_vma.start_vpage && vpage <= curr_vma.end_vpage) {
            vpage_exists = true;
            vma = &curr_vma;
            break;
        }
    }
    if (!vpage_exists) {
        if (curr_options.O)
            printf(" SEGV\n");
    }
    return vpage_exists;
}

void run_simulation() {
    char operation;
    int vpage;

    while (get_next_instruction(operation, vpage)) {
        if (curr_options.O)
            printf("%d: ==> %c %d\n", INSTR_POS, operation, vpage);
        if (INSTR_POS==14) {
            int x  = 1;
        }
        if (operation == 'c') {
            // Here the vpage actually points to proc id
            curr_proc = processes[vpage];
            continue;
        }
        else if (operation == 'e') {
            if (curr_options.O)
                printf("EXIT current process %d", curr_proc->pid);
            curr_proc = NULL;
            // TODO: remove unnecessary comments
            // Traverse the active process's page table and for each valid entry UNMAP the page and FOUT modified filemapped pages.
            // Note that dirty non-fmapped (anonymous) pages are not written back (OUT) as the process exits.
            // The used frame has to be returned to the free pool.
            for (int i=0; i<MAX_VPAGES; i++) {
                if (curr_proc->page_table[i].valid) {
                    if (curr_options.O)
                        printf(" UNMAP %d:%d\n", curr_proc->pid, i);
                    if (curr_proc->page_table[i].file_mapped && curr_proc->page_table[i].modified) {
                        if (curr_options.O)
                            printf(" FOUT\n");
                    }
                    free_pool.push_back(frame_table + curr_proc->page_table[i].page_frame_num);
                }
            }
            continue;
        }

        PTE *pte = &curr_proc->page_table[vpage];
        if ( ! pte->valid) {
            // this in reality generates the page fault exception and now you execute
            // verify this is actually a valid page in a vma if not raise error and next inst
            VMA *vma;
            if(!page_fault_handler(vpage, vma)) {
                continue;
            }

            Frame *new_frame = get_frame();
            // TODO: figure out if/what to do with old frame if it was mapped
            // see general outline in MM-slides under Lab3 header and writeup below
            // see whether and how to bring in the content of the access page.
            if(pte->paged_out) {
                if(pte->file_mapped) { //TODO: check if this is right or we need to take vma->file_mapped
                    if (curr_options.O)
                        printf(" FIN\n");
                } else {
                    if (curr_options.O)
                        printf(" IN\n");
                }
            }
            if (!pte->paged_out && !pte->file_mapped) {
                if (curr_options.O)
                    printf(" ZERO\n");
            }
            pte->page_frame_num = new_frame - frame_table;
            pte->valid = 1;
            pte->is_vma_mapped = 1;
            pte->modified = 0;
            pte->referenced = 0;
            pte->file_mapped = vma->file_mapped ;
            pte->paged_out = false;
            pte->write_protect = vma->write_protected;

            new_frame->page_id = pte - curr_proc->page_table;
            new_frame->pid = curr_proc->pid;
            new_frame->is_mapped = true;

            if (curr_options.O)
                printf(" MAP %d\n", pte->page_frame_num);
        }
        // now the page is definitely present
        // check write protection
        // simulate instruction execution by hardware by updating the R/M PTE bits
        if (pte->write_protect && operation=='w') {
            if (curr_options.O)
                printf(" SEGPROT\n");
            pte->referenced = 1;
            continue;
        }
        if (operation == 'w') {
            pte->referenced = 1;
            pte->modified = 1;
        }
        if (operation == 'r') {
            pte->referenced = 1;
        }
    }
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
    // TODO: change all inputs to cli
    max_frames = 16;
    if (max_frames > 128) {
        throw runtime_error("Number of frames can not be greater than 128\n");
    }
    PageReplacementAlgo algo = FIFO;
    switch(algo) {
        case FIFO:
            CURR_PAGER = new FIFO_Pager();
            break;
        case RANDOM:
            break;
        case CLOCK:
            break;
        case NRU:
            break;
        case AGING:
            break;
        case WORKING_SET:
            break;
    }

    string infile = "problem/in1";
    string rfile = "problem/rfile";
    curr_options.O = curr_options.P = curr_options.F = curr_options.S = true;

    read_input_files(infile, rfile);

    // Initialize Frame Table
    frame_table = (Frame*) malloc(max_frames * sizeof(Frame));
    // Initialize free pool
    for (int i=0; i<max_frames; i++) {
        free_pool.push_back(frame_table + i);
    }

    run_simulation();

    if (curr_options.P) {
        PTE *p;
        for (int i=0; i<processes.size(); i++) {
            printf("PT[%d]:", i);
            for (int j=0; j<MAX_VPAGES; j++) {
                p = &processes[i]->page_table[j];
                if (!p->valid) {
                    if (p->paged_out) {
                        printf(" #");
                    } else {
                        printf(" *");
                    }
                } else {
                    // TODO: check if it is S if swapped to file!
                    printf(" %d:%c%c%c", j, p->referenced?'R':'-', p->modified?'M':'-', p->paged_out?'S':'-');
                }
            }
            printf("\n");
        }
    }

    if (curr_options.F) {
        printf("FT:");
        for (int i=0; i<max_frames; i++) {
            if(frame_table[i].is_mapped) {
                printf(" %d:%d", frame_table[i].pid, frame_table[i].page_id);
            } else {
                printf(" *");
            }
        }
        printf("\n");
    }

    if (curr_options.S) {
        // TODO
//        printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
//               proc->pid,
//               pstats->unmaps, pstats->maps, pstats->ins, pstats->outs,
//               pstats->fins, pstats->fouts, pstats->zeros,
//               pstats->segv, pstats->segprot);
//        printf("TOTALCOST %lu %lu %lu %llu %lu\n",
//               inst_count, ctx_switches, process_exits, cost, sizeof(pte_t));
    }
}
