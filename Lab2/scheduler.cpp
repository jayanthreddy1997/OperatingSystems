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
enum Transition {TRANS_TO_READY, TRANS_TO_PREEMPT, TRANS_TO_RUN, TRANS_TO_BLOCK};
int DEFAULT_MAX_PRIOS = 4;

class Process {
    int arrival_time;
    int total_cpu_time;
    int cpu_burst;
    int io_burst;
public:
    int remaining_execution_time;
    int state_start_time;
    int pid;
    // TODO: some more bookkeeping variables needed

    Process(int pid1, int AT, int TC, int CB, int IO): pid(pid1), arrival_time(AT), total_cpu_time(TC), cpu_burst(CB), io_burst(IO) { }

};

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
    list<Process*> ready_queue;
    int quantum;

public:
    void add_process(Process *p) {

    }

    Process* get_next_process() {
        return nullptr;
    }
    bool does_preempt() {
        // returns true for ‘E’ scheds, else false.
        return false;
    }
};

class FCFS_Scheduler: public Scheduler {

};

int myrandom(int burst) {
    int next_rand_num = 1 + (randvals[g_randval_offset] % burst);
    g_randval_offset = (g_randval_offset + 1) % randvals.size();
    return next_rand_num;
}

void Simulation(DES* des, Scheduler* scheduler) {

    Event* evt;
    bool CALL_SCHEDULER;
    Process* CURRENT_RUNNING_PROCESS;
    while( (evt = des->get_event()) ) {
        CALL_SCHEDULER = false;
        Process* proc = evt->process;
        CURRENT_TIME = evt->time_stamp;
        int transition = evt->transition;
        int timeInPrevState = CURRENT_TIME - proc->state_start_time; // for accounting
        delete evt;
        evt = nullptr; // remove cur event obj and don’t touch anymore
        cout << proc->pid << endl;
//        switch (transition) {  // encodes where we come from and where we go
//            case TRANS_TO_READY:
//                // must come from BLOCKED or CREATED
//                // add to run queue, no event created
//                CALL_SCHEDULER = true;
//                break;
//            case TRANS_TO_PREEMPT: // similar to TRANS_TO_READY
//                // must come from RUNNING (preemption)
//                // add to runqueue (no event is generated)
//                CALL_SCHEDULER = true;
//                break;
//            case TRANS_TO_RUN:
//                // create event for either preemption or blocking
//                break;
//            case TRANS_TO_BLOCK:
//                //create an event for when process becomes READY again
//                CALL_SCHEDULER = true;
//                break;
//        }
//        if(CALL_SCHEDULER) {
//            if (des->get_next_event_timestamp() == CURRENT_TIME)
//                continue; //process next event from Event queue
//            CALL_SCHEDULER = false; // reset global flag
//            if (CURRENT_RUNNING_PROCESS == nullptr) {
//                CURRENT_RUNNING_PROCESS = scheduler->get_next_process();
//                if (CURRENT_RUNNING_PROCESS == nullptr)
//                    continue;
//                // create event to make this process runnable for same time.
//            }
//        }
    }
}

int main() {
    // TODO: Change to take input from cli
    string input_filename = "problem/lab2_assign/input_misc";
    string randval_input_filename = "problem/lab2_assign/rfile";
    string scheduler_mode = "F";

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
    sch = new FCFS_Scheduler();
    int pid = 0;
    Process* p;
    Event* e;
    while (input_file >> arrival_time >> total_cpu_time >> cpu_burst >> io_burst) {
//        cout << arrival_time << total_cpu_time << cpu_burst << io_burst << endl;

        p = new Process(pid, arrival_time, total_cpu_time, cpu_burst, io_burst);
        e = new Event(arrival_time, p, CREATED, READY, TRANS_TO_READY);
        des.add_event(e);
        pid++;
    }
    input_file.close();

    Simulation(&des, sch);

}
