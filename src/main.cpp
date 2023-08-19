#include <iostream>
#include <list>
#include "partitioning.h"

using namespace std;
int main(int argc, char **argv){
    if(argc < 2){
        cout << "Usage: " << argv[0] << " <input file>"  << endl;
        return 1;
    }

    auto g = loadFromFile(argv[1], 4);
    int requestedPartitions = 100;
    auto start = std::chrono::high_resolution_clock::now();
    partitioning(g, requestedPartitions);
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
    cout << "Time elapsed: " << elapsed.count() << " ms" << endl;

    delete g;
}
