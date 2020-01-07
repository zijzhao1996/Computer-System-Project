/**
 * File: mapreduce-reducer.cc
 * --------------------------
 * Presents the implementation of the MapReduceReducer class,
 * which is charged with the responsibility of collating all of the
 * intermediate files for a given hash number, sorting that collation,
 * grouping the sorted collation by key, and then pressing that result
 * through the reducer executable.
 *
 * See the documentation in mapreduce-reducer.h for more information.
 */
#include "mr-names.h"
#include "mapreduce-reducer.h"
#include <sstream>
#include <iostream>
using namespace std;

MapReduceReducer::MapReduceReducer(const string& serverHost, unsigned short serverPort,
                                   const string& cwd, const string& executable, const string& outputPath) : 
  MapReduceWorker(serverHost, serverPort, cwd, executable, outputPath) {}

void MapReduceReducer::reduce() const {
    while(true) {
        string name;
        if (!requestInput(name)) break;
        alertServerOfProgress("About to process \"" + name + "\".");
        string base = extractBase(name);
        string input = base.substr(2);
        string outputName = changeExtension(input,"mapped","mid");
        string command = "cat " + name + " | sort | ";
        command += "python " + cwd + "/group-by-key.py > " + outputPath + "/" + changeExtension(base, "mapped", "mid");
        bool midpass = sortandgroup(command);
        if(!midpass) {
            notifyServer(name,midpass);
            return;
        }
        alertServerOfProgress("About to process \"" + name + "\".");
        string mid = outputPath + "/" + changeExtension(base, "mapped", "mid");
        string out = outputPath + "/" + changeExtension(base, "mapped", "output");
        bool success = processInput(mid, out);
        remove(mid.c_str());
        notifyServer(name,success);
    }

}
bool MapReduceReducer::sortandgroup(string command) const {
    ostringstream oss;
    oss<<command;
    string exe = oss.str();
    return system(exe.c_str()) == 0;
}
