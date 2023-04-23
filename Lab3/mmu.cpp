#include <iostream>
#include <fstream>
#include <deque>
#include <unistd.h>
#include <cctype>

using namespace std;

const int MAX_VPAGES = 64;
int max_frames = 128;

int g_randval_offset = 0;
vector<int> randvals;

// Costs
unsigned int C_READ_WRITE = 1;
unsigned int C_CONTEXT_SWITCH = 130;
unsigned int C_PROCESS_EXIT = 1230;
unsigned int C_MAPS = 350;
unsigned int C_UNMAPS = 410;
unsigned int C_INS = 3200;
unsigned int C_OUTS = 2750;
unsigned int C_FINS = 2350;
unsigned int C_FOUTS = 2800;
unsigned int C_ZEROS = 150;
unsigned int C_SEGV = 440;
unsigned int C_SEGPROT = 410;

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
vector<pair<char, int> > instructions; // TODO: remove this and process instructions as we read them

struct ProcessStats {
    unsigned long unmaps, maps, ins, outs, fins, fouts, zeros, segv, segprot;

    ProcessStats() {
        unmaps = maps = ins = outs = fins = fouts = zeros = segv = segprot = 0;
    }
};
vector<ProcessStats*> pstats;


// Overall stats
unsigned long long cost = 0.0;
unsigned long ctx_switches = 0;
unsigned long process_exits = 0;

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
        cost += C_UNMAPS;
        pstats[frame->pid]->unmaps += 1;
        if (del_pte->modified) {
            del_pte->paged_out = 1;
            if (del_pte->file_mapped) {
                if (curr_options.O)
                    printf(" FOUT\n");
                cost += C_FOUTS;
                pstats[frame->pid]->fouts += 1;
            } else {
                if (curr_options.O)
                    printf(" OUT\n");
                cost += C_OUTS;
                pstats[frame->pid]->outs += 1;
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
        cost += C_SEGV;
        pstats[curr_proc->pid]->segv += 1;
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
            cost += C_CONTEXT_SWITCH;
            ctx_switches += 1;
            continue;
        }
        else if (operation == 'e') {
            if (curr_options.O)
                printf("EXIT current process %d", curr_proc->pid);
            // TODO: remove unnecessary comments
            // Traverse the active process's page table and for each valid entry UNMAP the page and FOUT modified filemapped pages.
            // Note that dirty non-fmapped (anonymous) pages are not written back (OUT) as the process exits.
            // The used frame has to be returned to the free pool.
            for (int i=0; i<MAX_VPAGES; i++) {
                if (curr_proc->page_table[i].valid) {
                    if (curr_options.O)
                        printf(" UNMAP %d:%d\n", curr_proc->pid, i);
                    cost += C_UNMAPS;
                    pstats[curr_proc->pid]->unmaps += 1;
                    if (curr_proc->page_table[i].file_mapped && curr_proc->page_table[i].modified) {
                        if (curr_options.O)
                            printf(" FOUT\n");
                    }
                    free_pool.push_back(frame_table + curr_proc->page_table[i].page_frame_num);
                }
            }
            process_exits += 1;
            cost += C_PROCESS_EXIT;
            curr_proc = NULL;
            continue;
        }

        cost += C_READ_WRITE;
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
                    cost += C_FINS;
                    pstats[curr_proc->pid]->fins += 1;
                } else {
                    if (curr_options.O)
                        printf(" IN\n");
                    cost += C_INS;
                    pstats[curr_proc->pid]->ins += 1;
                }
            }
            if (!pte->paged_out && !pte->file_mapped) {
                if (curr_options.O)
                    printf(" ZERO\n");
                cost += C_ZEROS;
                pstats[curr_proc->pid]->zeros += 1;
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
            cost += C_MAPS;
            pstats[curr_proc->pid]->maps += 1;
        }
        // now the page is definitely present
        // check write protection
        // simulate instruction execution by hardware by updating the R/M PTE bits
        if (pte->write_protect && operation=='w') {
            if (curr_options.O)
                printf(" SEGPROT\n");
            cost += C_SEGPROT;
            pstats[curr_proc->pid]->segprot += 1;
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
        pstats.push_back(new ProcessStats());
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
    int c;
    // Parse input flags
    while ((c = getopt(argc, argv, "f:a:o:")) != -1) {
        switch (c) {
            case 'f': {
                sscanf(optarg, "%d", &max_frames);
                if (max_frames > 128) {
                    throw runtime_error("Number of frames can not be greater than 128\n");
                }
                break;
            }
            case 'o': {
                char *print_opt = optarg;
                while (*print_opt) {
                    switch (*print_opt) {
                        case 'O':
                            curr_options.O = true;
                            break;
                        case 'P':
                            curr_options.P = true;
                            break;
                        case 'F':
                            curr_options.F = true;
                            break;
                        case 'S':
                            curr_options.S = true;
                            break;
                        case 'x':
                            curr_options.x = true;
                            break;
                        case 'y':
                            curr_options.y = true;
                            break;
                        case 'f':
                            curr_options.f = true;
                            break;
                        case 'a':
                            curr_options.a = true;
                            break;
                        default:
                            cout << "Provide valid options" << endl;
                            abort();
                    }
                    print_opt++;
                }
                break;
            }
            case 'a': {
                switch (tolower(optarg[0])) {
                    case 'f':
                        CURR_PAGER = new FIFO_Pager();
                        break;
                    case 'r':
                        CURR_PAGER = new FIFO_Pager(); // TODO: fix
                        break;
                    case 'c':
                        CURR_PAGER = new FIFO_Pager(); // TODO: fix
                        break;
                    case 'e':
                        CURR_PAGER = new FIFO_Pager(); // TODO: fix
                        break;
                    case 'a':
                        CURR_PAGER = new FIFO_Pager(); // TODO: fix
                        break;
                    case 'w':
                        CURR_PAGER = new FIFO_Pager(); // TODO: fix
                        break;
                    default:
                        cout << "Provide valid scheduler option" << endl;
                        abort();
                }
                break;
            }
            default:
                cout << "Please provide valid input" << endl;
                abort();
        }
    }

    string infile = argv[optind];
    optind++;
    string rfile = argv[optind];

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
            printf("PT[%d]:", processes[i]->pid);
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
        for (int i=0; i<processes.size(); i++) {
            ProcessStats *pstat = pstats[i];
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
                   processes[i]->pid,
                   pstat->unmaps, pstat->maps, pstat->ins, pstat->outs,
                   pstat->fins, pstat->fouts, pstat->zeros,
                   pstat->segv, pstat->segprot);
            printf("TOTALCOST %lu %lu %lu %llu %lu\n",
                   instructions.size(), ctx_switches, process_exits, cost, sizeof(PTE));
        }
    }
}
