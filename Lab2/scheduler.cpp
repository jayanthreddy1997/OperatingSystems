#include <string.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

using namespace std;

int g_randval_offset = 0;
vector<int> randvals;

enum State {CREATED, READY, RUNNING, BLOCKED};

class Process {
    int pid;
    int arrival_time;
    int total_cpu_time;

};

class Event {
    int time_stamp;
    Process* process;
    State current_state;
    State next_state;
    Event(int& time_stamp, Process* process, State& current_state, State& next_state) {
        this->time_stamp = time_stamp;
        this->process = process;
        this->current_state = current_state;
        this->next_state = next_state;
    }
};

class DiscreteEventSimulation {
//    vector<>
};

int myrandom(int burst) { return 1 + (randvals[g_randval_offset] % burst); }

int main() {
    // TODO: Change to take input from cli
    string input_filename = "problem/lab2_assign/input0";
    string randval_input_filename = "problem/lab2_assign/rfile";

    ifstream randval_input_file;
    randval_input_file.open(randval_input_filename);
    int rand_num;
    while(randval_input_file >> rand_num) {
        randvals.push_back(rand_num);
    }

    ifstream input_file;
    input_file.open(input_filename);


    int arrival_time;
    int total_cpu_time;
    int cpu_burst;
    int io_burst;

    while (input_file >> arrival_time >> total_cpu_time >> cpu_burst >> io_burst) {
        cout << arrival_time << total_cpu_time << cpu_burst << io_burst << endl;
    }
    input_file.close();

}
