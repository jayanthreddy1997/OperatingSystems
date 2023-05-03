#include <iostream>
#include <vector>
#include <unistd.h>
#include <fstream>
#include <list>

using namespace std;
int curr_time = 0; // TODO: should this start from 1?
int curr_track = 0;
int track_dir = +1; // +1: inc track num, -1: dec track num

bool debug_mode = false;
bool debug_queue_mode = false;
bool debug_flook_mode = false;

// variables for summary statistics
int stat_tot_movement = 0;
int stat_io_util_time = 0;
int stat_acc_turnaround_time = 0;
int stat_acc_waittime = 0;
int max_waittime = 0;

struct IO_Request {
    int req_time; // Request arrival time
    int track; // Track number that the request accesses

    int start_time;
    int end_time;

    IO_Request(int req_time, int track): req_time(req_time), track(track) {}
};

vector<IO_Request> io_requests;
list<IO_Request*> io_queue;
IO_Request *curr_running = nullptr;

class IO_Scheduler {
public:
    virtual IO_Request *get_request() = 0;
};

class FIFO_IO_Scheduler: public IO_Scheduler {
public:
    virtual IO_Request *get_request() {
        if (io_queue.empty())
            return nullptr;
        IO_Request *ret = io_queue.front();
        io_queue.pop_front();
        track_dir = (ret->track - curr_track)>=0 ? +1 : -1; //TODO: maybe move this out?
        return ret;
    }
};

class SSTF_IO_Scheduler: public IO_Scheduler {
public:
    virtual IO_Request *get_request() {
        IO_Request *ret = nullptr;
        int curr_seek_time;
        int min_seek_time = INT_MAX;
        for(auto req: io_queue) {
            curr_seek_time = abs(curr_track - req->track);
            if (curr_seek_time < min_seek_time) {
                min_seek_time = curr_seek_time;
                ret = req;
            }
        }
        io_queue.remove(ret);
        track_dir = (ret->track - curr_track)>=0 ? +1 : -1;
        return ret;
    }
};

class LOOK_IO_Scheduler: public IO_Scheduler {
public:
    virtual IO_Request *get_request() {
        IO_Request *ret = nullptr;
        if (io_queue.empty())
            return ret;

        int curr_seek_time;
        int min_seek_time = INT_MAX;
        for(auto req: io_queue) {
            if ( (track_dir > 0 && req->track >= curr_track) || (track_dir < 0 && req->track <= curr_track) ) {
                curr_seek_time = abs(req->track - curr_track);
                if (curr_seek_time < min_seek_time) {
                    min_seek_time = curr_seek_time;
                    ret = req;
                }
            }
        }
        // If no request was found, flip direction and search
        if (ret == nullptr) {
            track_dir = -track_dir;
            return get_request();
        }

        io_queue.remove(ret);
        if (ret->track != curr_track)
            track_dir = (ret->track - curr_track)>=0 ? +1 : -1;
        return ret;
    }
};

IO_Scheduler *SCH;

void run_simulation() {
    int i = 0;
    int curr_waittime = 0;
    while (true) {
        if (curr_time==716) {
            int x=1;
        }
        while (i<io_requests.size() && io_requests[i].req_time == curr_time) {
            io_queue.push_back(&io_requests[i]);
            i++;
        }

        if (curr_running!=nullptr && curr_running->track==curr_track) {
            curr_running->end_time = curr_time;
            stat_io_util_time += curr_running->end_time - curr_running->start_time;
            stat_acc_turnaround_time += curr_running->end_time - curr_running->req_time;
            curr_running = nullptr;
        }

        while (curr_running == nullptr && !io_queue.empty()) {
            curr_running = SCH->get_request();
            curr_running->start_time = curr_time;
            curr_waittime = curr_time - curr_running->req_time;
            stat_acc_waittime += curr_waittime;
            max_waittime = max(max_waittime, curr_waittime);
            if (curr_running->track == curr_track) {
                curr_running->end_time = curr_time;
                stat_io_util_time += curr_running->end_time - curr_running->start_time;
                stat_acc_turnaround_time += curr_running->end_time - curr_running->req_time;
                curr_running = nullptr;
            }
        }

        if (curr_running == nullptr && i>=io_requests.size())
            break;

        if (curr_running != nullptr) {
            curr_track += track_dir;
            stat_tot_movement += 1;
        }

        curr_time += 1;
    }
}

int main(int argc, char** argv) {
    int c;
    while ((c = getopt(argc, argv, "s:vqf")) != -1) {
        switch (c) {
            case 's': {
                switch (toupper(optarg[0])) {
                    case 'N': {
                        SCH = new FIFO_IO_Scheduler();
                        break;
                    }
                    case 'S': {
                        SCH = new SSTF_IO_Scheduler();
                        break;
                    }
                    case 'L': {
                        SCH = new LOOK_IO_Scheduler();
                        break;
                    }
                    case 'C': {
                        SCH = new FIFO_IO_Scheduler();
                        break;
                    }
                    case 'F': {
                        SCH = new FIFO_IO_Scheduler();
                        break;
                    }
                    default: {
                        cout << "Please provide a valid scheduler option." << endl;
                        abort();
                    }
                }
                break;
            }
            case 'v': {
                debug_mode = true;
                break;
            }
            case 'q': {
                debug_queue_mode = true;
                break;
            }
            case 'f': {
                debug_flook_mode = true;
                break;
            }
            default: {
                cout << "Please provide valid input" << endl;
                abort();
            }
        }
    }
    string input_filename = argv[optind];

    ifstream input_file;
    input_file.open(input_filename);
    int req_time;
    int req_track;
    string in;
    while (getline(input_file, in)) {
        if (in[0] == '#')
            continue;
        sscanf(in.c_str(), "%d %d", &req_time, &req_track);
        io_requests.push_back(IO_Request(req_time, req_track));
    }

    run_simulation();

    // Print summary stats
    for (int i=0; i<io_requests.size(); i++) {
        printf("%5d: %5d %5d %5d\n", i, io_requests[i].req_time, io_requests[i].start_time, io_requests[i].end_time);
    }
    double io_utilization = double(stat_io_util_time) / double(curr_time);
    double avg_turnaround = double(stat_acc_turnaround_time) / double(io_requests.size());
    double avg_waittime = double(stat_acc_waittime) / double(io_requests.size());
    printf("SUM: %d %d %.4lf %.2lf %.2lf %d\n", curr_time, stat_tot_movement, io_utilization,
           avg_turnaround, avg_waittime, max_waittime);
    return 0;
}