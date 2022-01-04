#include <stdio.h>
#include <string>
#include <getopt.h>
#include "stream.hpp"
#include "global.hpp"


using namespace std;
using namespace mediakit;

int discovery_options(int argc, char** argv, string& input, string& output){
    int ret = 0;
    
    static option long_options[] = {
        {"input", no_argument, 0, 'i'},
        {"output", no_argument, 0, 'o'},
        {0, 0, 0, 0}
    };
    
    int opt = 0;
    int option_index = 0;
    while((opt = getopt_long(argc, argv, "i:o:", long_options, &option_index)) != -1){
        switch(opt){
            case 'i':
                input = optarg;
                break;
            case 'o':
                output = optarg;
                break;
            default:
                break;
        }
    }
    return ret;
}

int main(int argc, char** argv){
    printf("hello\n");

    int ret = 0;
    // string input;

    if((ret = discovery_options(argc, argv, inputfile, outputfile)) != 0){
        printf("discovery options failed. ret=%d", ret);
        return ret;
    }
    
    char *data=NULL;
    long size = 0;

    StreamClient *client = new StreamClient();
    printf("read_file\n");
    client->read_file(inputfile, &data, &size);


    printf("on_stream\n");
    client->on_stream(data, size);
    
    
    return 0;
}