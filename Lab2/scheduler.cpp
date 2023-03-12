#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <list>

using namespace std;

int g_randval_offset = 0;
vector<int> randvals;
int CURRENT_TIME = 0;
enum State {CREATED, READY, RUNNING, BLOCKED};
const string StateStrings[] = {"CREATED", "READY", "RUNNG", "BLOCK"};
enum Transition {TRANS_TO_READY, TRANS_TO_PREEMPT, TRANS_TO_RUN, TRANS_TO_BLOCK};
int DEFAULT_MAX_PRIOS = 4;

class Process {

public:
    int pid;
    int arrival_time;
    int total_cpu_time;
    int max_cpu_burst; // Change this to max cpu burst
    int current_cpu_burst;
    int max_io_burst;
    int current_io_burst; // TODO: maybe remove this?
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
};

class Scheduler {
public:
    virtual bool does_preempt() {
        // returns true for ‘E’ scheds, else false.
        return false;
    }

    virtual void add_process(Process *p) {}

    virtual Process* get_next_process() {
        return nullptr;
    }
};

class FCFS_Scheduler: public Scheduler {
    static const int quantum = 10000;
    list<Process*> ready_queue;

public:

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
};

class LCFS_Scheduler: public Scheduler {
    static const int quantum = 10000;
    list<Process*> ready_queue;

public:
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
};

class SRTF_Scheduler: public Scheduler {
    static const int quantum = 10000;
    list<Process*> ready_queue;

public:
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
};

int myrandom(int burst) {
    int next_rand_num = 1 + (randvals[g_randval_offset] % burst);
    g_randval_offset = (g_randval_offset + 1) % randvals.size();
    return next_rand_num;
}

void Simulation(DES* des, Scheduler* sch) {

    Event* evt;
    bool CALL_SCHEDULER;
    Process* CURRENT_RUNNING_PROCESS = nullptr;
    CALL_SCHEDULER = false;

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

        bool print_verbose = true;
        if (print_verbose) {
            printf("%d %d %d: %s -> %s",
                   CURRENT_TIME, proc->pid, timeInPrevState, StateStrings[currentState].c_str(), StateStrings[nextState].c_str());
            if (currentState==READY && nextState==RUNNING) {
                printf(" cb=%d rem=%d prio=%d", proc->current_cpu_burst, proc->rem_exec_time, proc->dynamic_priority);
            }
        }

        proc->state_start_time = CURRENT_TIME;
        switch (transition) {  // encodes where we come from and where we go
            case TRANS_TO_READY:
                // must come from BLOCKED or CREATED
                // add to run queue, no event created
                sch->add_process(proc);
                CALL_SCHEDULER = true;
                break;
            case TRANS_TO_PREEMPT: // similar to TRANS_TO_READY
                // must come from RUNNING (preemption)
                // add to runqueue (no event is generated)
                // TODO: Check logic
                sch->add_process(proc);
                CALL_SCHEDULER = true;
                break;
            case TRANS_TO_RUN:
                // create event for either preemption or blocking
                // TODO: Handle preemption
                Event* newEvent;
                if (proc->rem_exec_time < proc->current_cpu_burst) { //TODO: cleanup using max
                    newEvent = new Event(CURRENT_TIME + proc->rem_exec_time, proc, RUNNING, BLOCKED, TRANS_TO_BLOCK);
                    proc->rem_exec_time = 0;
                } else {
                    proc->rem_exec_time -= proc->current_cpu_burst;
                    newEvent = new Event(CURRENT_TIME+proc->current_cpu_burst, proc, RUNNING, BLOCKED, TRANS_TO_BLOCK);
                }
                des->add_event(newEvent);
                break;
            case TRANS_TO_BLOCK:
                //create an event for when process becomes READY again
                if (proc->rem_exec_time == 0) {
                    if(print_verbose) {
                        printf(" Done!");
                    }
                    proc->finish_time = CURRENT_TIME;
                } else {
                    int io_burst = myrandom(proc->max_io_burst);
                    if(print_verbose) {
                        printf("  ib=%d rem=%d", io_burst, proc->rem_exec_time);
                    }
                    Event* newEvent = new Event(CURRENT_TIME+io_burst, proc, BLOCKED, READY, TRANS_TO_READY);
                    des->add_event(newEvent);
                }
                CALL_SCHEDULER = true;
                CURRENT_RUNNING_PROCESS = nullptr;
                break;
        }
        if (print_verbose) {
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
                CURRENT_RUNNING_PROCESS->current_cpu_burst = myrandom(CURRENT_RUNNING_PROCESS->max_cpu_burst);
                // create event to make this process runnable for same time.
                Event* e = new Event(CURRENT_TIME, CURRENT_RUNNING_PROCESS, READY, RUNNING, TRANS_TO_RUN);
                des->add_event(e);
            }
        }
    }
}

int main() {
    // TODO: Change to take input from cli
    string input_filename = "problem/lab2_assign/input0";
    string randval_input_filename = "problem/lab2_assign/rfile";
//    string scheduler_mode = "F";
//    string scheduler_mode = "L";
    string scheduler_mode = "S";

    ifstream randval_input_file;
    randval_input_file.open(randval_input_filename);
    int rand_num;
    int rand_num_count;
    randval_input_file >> rand_num_count; // TODO: dyn. allocate randvals of size rand_num_count at once
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
    Scheduler* sch;
    if (scheduler_mode=="F") {
        sch = new FCFS_Scheduler();
    } else if (scheduler_mode=="L") {
        sch = new LCFS_Scheduler();
    } else if (scheduler_mode=="S") {
        sch = new SRTF_Scheduler();
    }

    int pid = 0;
    Process* p;
    Event* e;
    while (input_file >> arrival_time >> total_cpu_time >> cpu_burst >> io_burst) {
        p = new Process(pid, arrival_time, total_cpu_time, cpu_burst, io_burst, myrandom(DEFAULT_MAX_PRIOS));
        p->rem_exec_time = total_cpu_time;
        p->dynamic_priority = p->static_priority - 1;
        e = new Event(arrival_time, p, CREATED, READY, TRANS_TO_READY);
        des.add_event(e);
        processes.push_back(p);
        pid++;
    }
    input_file.close();

    Simulation(&des, sch);

    for (Process* p: processes) {
        printf("%04d: %4d %4d %4d %1d | %5d %5d %5d %5d\n", p->pid, p->arrival_time, p->max_cpu_burst, p->max_io_burst,
               p->static_priority, p->finish_time, p->finish_time - p->arrival_time, p->io_time, p->cpu_wait_time);
    }

    // TODO: summary stats
//    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.2lf",
//           processes[processes.size()-1]->finish_time);
}
