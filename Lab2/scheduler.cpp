#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <list>
#include <unistd.h>

using namespace std;

int g_randval_offset = 0;
vector<int> randvals;
enum State {CREATED, READY, RUNNING, BLOCKED};
const string StateStrings[] = {"CREATED", "READY", "RUNNG", "BLOCK"};
enum Transition {TRANS_TO_READY, TRANS_TO_PREEMPT, TRANS_TO_RUN, TRANS_TO_BLOCK};
const string FCFS = "FCFS";
const string LCFS = "LCFS";
const string SRTF = "SRTF";
const string RR = "RR";
const string PRIO = "PRIO";
const string PREPRIO = "PREPRIO";
bool print_verbose = false;
bool print_trace_event_exec = false;
bool print_event_queue = false;
bool print_preprio_info = false;
bool single_step_mode = false;

int DEFAULT_QUANTUM = 10000;
int DEFAULT_MAX_PRIOS = 4;

// Some metadata variable used for summary stats
int num_io_procs = 0;
int io_start_time = 0;
int io_time = 0;

class Process {

public:
    int pid;
    int arrival_time;
    int total_cpu_time;
    int max_cpu_burst;
    int current_cpu_burst;
    int max_io_burst;
    int current_io_burst;
    int rem_exec_time;
    int state_start_time;
    int static_priority;
    int dynamic_priority;

    int finish_time;
    int io_time;
    int cpu_wait_time;

    Process(int pid1, int AT, int TC, int CB, int IO, int SP)
        : pid(pid1), arrival_time(AT), total_cpu_time(TC), max_cpu_burst(CB), max_io_burst(IO), static_priority(SP) { }

};

vector<Process*> processes;
int CURRENT_TIME = 0;
Process* CURRENT_RUNNING_PROCESS = nullptr;

class Event {
public:
    int time_stamp;
    Process* process;
    State current_state;
    State next_state;
    Transition transition;

    Event(int time_stamp, Process* process, State current_state, State next_state, Transition transition)
        : time_stamp(time_stamp), process(process), current_state(current_state), next_state(next_state), transition(transition) { }
};

class DES {
    list<Event*> event_queue;

public:
    void add_event(Event* e) {
        if (event_queue.empty() || e->time_stamp >= (*event_queue.back()).time_stamp) {
            event_queue.push_back(e);
        } else {
            // Iterate through the event queue and insert at correct position
            list<Event*>::iterator it = event_queue.begin();
            while( (it != event_queue.end()) && ((*(*it)).time_stamp <= e->time_stamp) ) {
                it++;
            }
            event_queue.insert(it, e);
        }
    }

    Event* get_event() {
        if (event_queue.empty()) {
            return nullptr;
        } else {
            Event* returnEvent = event_queue.front();
            event_queue.pop_front();
            return returnEvent;
        }
    }

    int get_next_event_timestamp() {
        if (event_queue.empty()) {
            return -1;
        } else {
            return (*event_queue.front()).time_stamp;
        }
    }

    bool check_exists_event(int pid, int check_time) {
        // Function to check if event exists for a given process at a given timestamp,
        // Useful for PREPRIO simulation
        if (event_queue.empty() || get_next_event_timestamp() > check_time) {
            return false;
        } else {
            list<Event*>::iterator it = event_queue.begin();
            while (it!=event_queue.end() && (*it)->time_stamp==check_time) {
                if ((*it)->process->pid == pid) {
                    return true;
                }
                it++;
            }
        }
        return false;
    }

    void remove_event(int pid) {
        // Function to remove events for a given process,
        // Useful for PREPRIO simulation
        if (event_queue.empty()) {
            return;
        }
        list<Event*>::iterator it = event_queue.begin();
        while (it!=event_queue.end() && (*it)->process->pid != pid) {
            it++;
        }
        if (it!=event_queue.end() && (*it)->process->pid == pid) {
            event_queue.remove(*it);
        }
    }
};

class Scheduler {
    int quantum;

public:
    Scheduler(int quantum1): quantum(quantum1) {}

    virtual bool does_preempt() {
        // returns true for ‘E’ scheds, else false.
        return false;
    }

    virtual void add_process(Process *p) {}

    virtual Process* get_next_process() {
        return nullptr;
    }

    virtual int get_quantum() {
        return quantum;
    }

    virtual void print_scheduler_name() {}
};

class FCFS_Scheduler: public Scheduler {
    list<Process*> ready_queue;

public:
    FCFS_Scheduler(): Scheduler(DEFAULT_QUANTUM) {}

    virtual bool does_preempt() {
        return false;
    }

    virtual void add_process(Process* p) {
        ready_queue.push_back(p);
    }

    virtual Process* get_next_process() {
        if (ready_queue.empty()) {
            return nullptr;
        }
        Process* p = ready_queue.front();
        ready_queue.pop_front();
        return p;
    }

    virtual void print_scheduler_name() {
        cout << FCFS << endl;
    }
};

class LCFS_Scheduler: public Scheduler {
    list<Process*> ready_queue;

public:
    LCFS_Scheduler(): Scheduler(DEFAULT_QUANTUM) {}

    virtual bool does_preempt() {
        return false;
    }

    virtual void add_process(Process* p) {
        ready_queue.push_back(p);
    }

    virtual Process* get_next_process() {
        if (ready_queue.empty()) {
            return nullptr;
        }
        Process* p = ready_queue.back();
        ready_queue.pop_back();
        return p;
    }

    virtual void print_scheduler_name() {
        cout << LCFS << endl;
    }
};

class SRTF_Scheduler: public Scheduler {
    list<Process*> ready_queue;

public:
    SRTF_Scheduler(): Scheduler(DEFAULT_QUANTUM) {}

    virtual bool does_preempt() {
        return false;
    }

    virtual void add_process(Process* p) {
        ready_queue.push_back(p);
    }

    virtual Process* get_next_process() {
        if (ready_queue.empty()) {
            return nullptr;
        }
        Process* p = ready_queue.front();
        std::list<Process*>::iterator it;
        for (it = ready_queue.begin(); it != ready_queue.end(); ++it){
            if ((*it)->rem_exec_time < p->rem_exec_time) {
                p = *it;
            }
        }
        ready_queue.remove(p);
        return p;
    }

    virtual void print_scheduler_name() {
        cout << SRTF << endl;
    }
};

class RR_Scheduler: public Scheduler {
    list<Process*> ready_queue;

public:
    RR_Scheduler(int quantum): Scheduler(quantum){}

    virtual bool does_preempt() {
        return false;
    }

    virtual void add_process(Process* p) {
        p->dynamic_priority = p->static_priority - 1;
        ready_queue.push_back(p);
    }

    virtual Process* get_next_process() {
        if (ready_queue.empty()) {
            return nullptr;
        }
        Process* p = ready_queue.front();
        ready_queue.pop_front();
        return p;
    }

    virtual void print_scheduler_name() {
        cout << RR << " " << get_quantum() << endl;
    }
};

class PRIO_Scheduler: public Scheduler {
    list<Process*> *active_ready_queue;
    list<Process*> *expired_ready_queue;

    int num_prios;

public:
    PRIO_Scheduler(int quantum, int num_prios): Scheduler(quantum), num_prios(num_prios) {
        active_ready_queue = new list<Process*> [num_prios];
        expired_ready_queue = new list<Process*> [num_prios];
    }

    virtual bool does_preempt() {
        return false;
    }

    virtual void add_process(Process* p) {
        if (p->dynamic_priority == -1) {
            p->dynamic_priority = p->static_priority - 1;
            expired_ready_queue[p->dynamic_priority].push_back(p);
        } else {
            active_ready_queue[p->dynamic_priority].push_back(p);
        }
    }

    virtual Process* get_next_process() {
        for (int i=num_prios-1; i>=0; i--) {
            if (active_ready_queue[i].empty()) {
                continue;
            }
            Process *p = active_ready_queue[i].front();
            active_ready_queue[i].pop_front();
            return p;
        }
        // active queue was empty, swap active and expired queues
        list<Process*> *t;
        t = active_ready_queue;
        active_ready_queue = expired_ready_queue;
        expired_ready_queue = t;
        for (int i=num_prios-1; i>=0; i--) {
            if (active_ready_queue[i].empty()) {
                continue;
            }
            Process *p = active_ready_queue[i].front();
            active_ready_queue[i].pop_front();
            return p;
        }

        return nullptr; // Both active and expired queue are empty
    }

    virtual void print_scheduler_name() {
        cout << PRIO << " " << get_quantum() << endl;
    }
};

class PREPRIO_Scheduler: public PRIO_Scheduler {
public:
    PREPRIO_Scheduler(int quantum, int num_prios): PRIO_Scheduler(quantum, num_prios) {}

    virtual void print_scheduler_name() {
        cout << PREPRIO << " " << get_quantum() << endl;
    }

    virtual bool does_preempt() {
        return true;
    }
};

int myrandom(int burst) {
    int next_rand_num = 1 + (randvals[g_randval_offset] % burst);
    g_randval_offset = (g_randval_offset + 1) % randvals.size();
    return next_rand_num;
}

void Simulation(DES* des, Scheduler* sch) {

    Event* evt;
    bool CALL_SCHEDULER = false;

    while( (evt = des->get_event()) ) {
        Process* proc = evt->process;
        CURRENT_TIME = evt->time_stamp;
        Transition transition = evt->transition;
        State currentState = evt->current_state;
        State nextState = evt->next_state;
        int timeInPrevState = CURRENT_TIME - proc->state_start_time; // for accounting
        delete evt;
        evt = nullptr;

        if (currentState==BLOCKED) {
            proc->io_time += timeInPrevState;
        }
        if (currentState==READY) {
            proc->cpu_wait_time += timeInPrevState;
        }
        if (currentState==BLOCKED && nextState==READY) {
            num_io_procs -= 1;
            if (num_io_procs == 0) {
                io_time += (CURRENT_TIME - io_start_time);
            }
        }


        proc->state_start_time = CURRENT_TIME;
        switch (transition) {  // encodes where we come from and where we go
            case TRANS_TO_READY: {
                // must come from BLOCKED or CREATED
                // add to run queue, no event created
                proc->dynamic_priority = proc->static_priority-1;

                if (CURRENT_RUNNING_PROCESS!=nullptr && sch->does_preempt()) {
                    bool preempt_cond1 = proc->dynamic_priority > CURRENT_RUNNING_PROCESS->dynamic_priority;
                    bool preempt_cond2 = !des->check_exists_event(CURRENT_RUNNING_PROCESS->pid, CURRENT_TIME);

                    if (print_preprio_info)
                        printf("    --> PrioPreempt Cond1=%d Cond2=%d (%d) --> %s\n",
                               preempt_cond1, preempt_cond2, CURRENT_RUNNING_PROCESS->pid, ((preempt_cond1 && preempt_cond2)?"YES":"NO"));
                    if (preempt_cond1 && preempt_cond2) {
                        des->remove_event(CURRENT_RUNNING_PROCESS->pid);
                        Event *newEvent = new Event(CURRENT_TIME, CURRENT_RUNNING_PROCESS, RUNNING, READY,
                                                    TRANS_TO_PREEMPT);
                        des->add_event(newEvent);
                    }
                }

                sch->add_process(proc);
                CALL_SCHEDULER = true;
                break;
            }
            case TRANS_TO_PREEMPT: {// similar to TRANS_TO_READY
                // must come from RUNNING (preemption)
                // add to runqueue (no event is generated)
                proc->rem_exec_time -= timeInPrevState;
                proc->current_cpu_burst -= timeInPrevState;
                proc->dynamic_priority -= 1;
                sch->add_process(proc);
                CALL_SCHEDULER = true;
                CURRENT_RUNNING_PROCESS = nullptr;
                break;
            }
            case TRANS_TO_RUN: {
                // create event for either preemption or blocking
                int run_time = std::min(proc->rem_exec_time, proc->current_cpu_burst);
                Event *newEvent;
                if (sch->get_quantum() < run_time) {
                    // Create event for preemption
                    newEvent = new Event(CURRENT_TIME + sch->get_quantum(), proc, RUNNING, READY, TRANS_TO_PREEMPT);
                } else {
                    // Create event for blocking
                    newEvent = new Event(CURRENT_TIME + run_time, proc, RUNNING, BLOCKED, TRANS_TO_BLOCK);
                }
                des->add_event(newEvent);
                break;
            }
            case TRANS_TO_BLOCK: {
                //create an event for when process becomes READY again
                proc->rem_exec_time -= timeInPrevState;
                proc->current_cpu_burst -= timeInPrevState;
                if (proc->rem_exec_time == 0) {
                    // Process completed execution
                    proc->finish_time = CURRENT_TIME;
                } else {
                    proc->current_io_burst = myrandom(proc->max_io_burst);
                    num_io_procs += 1;
                    if(num_io_procs==1) {
                        io_start_time = CURRENT_TIME;
                    }
                    Event *newEvent = new Event(CURRENT_TIME + proc->current_io_burst, proc, BLOCKED, READY, TRANS_TO_READY);
                    des->add_event(newEvent);
                }
                CALL_SCHEDULER = true;
                CURRENT_RUNNING_PROCESS = nullptr;
                break;
            }
        }
        if (print_verbose) {
            printf("%d %d %d: ", CURRENT_TIME, proc->pid, timeInPrevState);
            if (transition==TRANS_TO_BLOCK && proc->rem_exec_time == 0) {
                printf("Done");
            } else {
                printf("%s -> %s",StateStrings[currentState].c_str(), StateStrings[nextState].c_str());
                if (transition==TRANS_TO_BLOCK) {
                    printf("  ib=%d rem=%d", proc->current_io_burst, proc->rem_exec_time);
                }
            }

            if (currentState==READY && nextState==RUNNING) {
                printf(" cb=%d rem=%d prio=%d", proc->current_cpu_burst, proc->rem_exec_time, proc->dynamic_priority);
            }
            if (currentState==RUNNING && nextState==READY) {
                int print_prio = (proc->dynamic_priority + 1)%proc->static_priority;
                printf("  cb=%d rem=%d prio=%d", proc->current_cpu_burst, proc->rem_exec_time, print_prio);
            }
            printf("\n");
        }
        if(CALL_SCHEDULER) {
            if (des->get_next_event_timestamp() == CURRENT_TIME)
                continue; //process next event from Event queue
            CALL_SCHEDULER = false; // reset global flag
            if (CURRENT_RUNNING_PROCESS == nullptr) {
                CURRENT_RUNNING_PROCESS = sch->get_next_process();
                if (CURRENT_RUNNING_PROCESS == nullptr)
                    continue;
                if (CURRENT_RUNNING_PROCESS->current_cpu_burst <= 0) {
                    CURRENT_RUNNING_PROCESS->current_cpu_burst = std::min(CURRENT_RUNNING_PROCESS->rem_exec_time,
                                                                          myrandom(CURRENT_RUNNING_PROCESS->max_cpu_burst));
                }
                // create event to make this process runnable for same time.
                Event* e = new Event(CURRENT_TIME, CURRENT_RUNNING_PROCESS, READY, RUNNING, TRANS_TO_RUN);
                des->add_event(e);
            }
        }
    }
}

int main(int argc, char **argv) {
    // Program invocation format: <program> [-v] [-t] [-e] [-p] [-i] [-s<schedspec>] inputfile randfile
    // where scheduler specification is "–s [ FLS | R<num> | P<num>[:<maxprio>] | E<num>[:<maxprios>] ]"

    int quantum = DEFAULT_QUANTUM;
    int num_prios = DEFAULT_MAX_PRIOS;

    Scheduler* sch;
    int c;
    while ((c = getopt(argc, argv, "vtepis:")) != -1) {
        switch (c) {
            case 'v':
                print_verbose = true;
                break;
            case 't':
                print_trace_event_exec = true;
                break;
            case 'e':
                print_event_queue = true;
                break;
            case 'p':
                print_preprio_info = true;
                break;
            case 'i':
                single_step_mode = true;
                break;
            case 's':
                char temp;
                switch(optarg[0]) {
                    case 'F':
                        sch = new FCFS_Scheduler();
                        break;
                    case 'L':
                        sch = new LCFS_Scheduler();
                        break;
                    case 'S':
                        sch = new SRTF_Scheduler();
                        break;
                    case 'R':
                        sscanf(optarg, "%c%d", &temp, &quantum);
                        sch = new RR_Scheduler(quantum);
                        break;
                    case 'P':
                        sscanf(optarg, "%c%d:%d", &temp, &quantum, &num_prios);
                        sch = new PRIO_Scheduler(quantum, num_prios);
                        break;
                    case 'E':
                        sscanf(optarg, "%c%d:%d", &temp, &quantum, &num_prios);
                        sch = new PREPRIO_Scheduler(quantum, num_prios);
                        break;
                    default:
                        cout << "Provide valid scheduler option" << endl;
                        abort();
                }
                break;
            default:
                cout << "Please provide valid input" << endl;
                abort();
        }
    }
    string input_filename = argv[optind];
    optind += 1;
    string randval_input_filename = argv[optind];

    ifstream randval_input_file;
    randval_input_file.open(randval_input_filename);
    int rand_num;
    int rand_num_count;
    randval_input_file >> rand_num_count;
    for(int i=0; i<rand_num_count; i++) {
        randval_input_file >> rand_num;
        randvals.push_back(rand_num);
    }

    ifstream input_file;
    input_file.open(input_filename);

    int arrival_time;
    int total_cpu_time;
    int cpu_burst;
    int io_burst;

    DES des;
    int pid = 0;
    Process* p;
    Event* e;
    while (input_file >> arrival_time >> total_cpu_time >> cpu_burst >> io_burst) {
        p = new Process(pid, arrival_time, total_cpu_time, cpu_burst, io_burst, myrandom(num_prios));
        p->rem_exec_time = total_cpu_time;
        p->dynamic_priority = p->static_priority - 1;
        p->state_start_time = arrival_time;
        e = new Event(arrival_time, p, CREATED, READY, TRANS_TO_READY);
        des.add_event(e);
        processes.push_back(p);
        pid++;
    }
    input_file.close();

    Simulation(&des, sch);

    sch->print_scheduler_name();
    int last_event_finishing_time = 0;
    int cumulative_cpu_time = 0;
    int cumulative_cpu_wait_time = 0;
    int cumulative_turnaround_time = 0;
    for (Process* p: processes) {
        cumulative_cpu_time += p->total_cpu_time;
        last_event_finishing_time = max(last_event_finishing_time, p->finish_time);
        cumulative_turnaround_time += p->finish_time - p->arrival_time;
        cumulative_cpu_wait_time += p->cpu_wait_time;
        printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", p->pid, p->arrival_time, p->total_cpu_time, p->max_cpu_burst, p->max_io_burst,
               p->static_priority, p->finish_time, p->finish_time - p->arrival_time, p->io_time, p->cpu_wait_time);
    }

    // Summary stats
    double cpu_util = ((double)cumulative_cpu_time)*100/((double)last_event_finishing_time);
    double io_util = ((double)io_time)*100/((double)last_event_finishing_time);
    double avg_turnaround_time = ((double)cumulative_turnaround_time) / processes.size();
    double avg_cpu_wait_time = ((double)cumulative_cpu_wait_time) / processes.size();
    double throughput = ((double)100*processes.size())/((double)last_event_finishing_time);
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", last_event_finishing_time, cpu_util, io_util, avg_turnaround_time,
           avg_cpu_wait_time, throughput);

}
