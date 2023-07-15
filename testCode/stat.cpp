#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

#define TRACE_LINE_ITEM_NUM 9
#define MD5_SIZE 16
#define BLOCK_SIZE 4096
#define BLKPARSE_NUM 21

enum TraceType{
	    HOMES,
        WEB, 
        MAIL,
        NOT
};

class Stater{
public:
    Stater(){
        max_write_sector_address = 0;
        min_write_sector_address = INT64_MAX;
        max_read_sector_address = 0;
        min_read_sector_address = INT64_MAX;
        tt = NOT;
    }

    void processOneTraceFile(string trace_file_path){
        ifstream  trace_file(trace_file_path.c_str());
        if(!trace_file){
            printf("Trace file open failed: %s\n", trace_file_path.c_str());
            exit(-1);
        }
        printf("Process trace file %s\n", trace_file_path.c_str());

        long long trace_ns;
        int trace_pid;
        string trace_pname;
        long long trace_sector_id;
        int trace_sector_num;
        bool trace_is_write;
        int trace_major;
        int trace_minor;
        string trace_md5;
        
        string line_data;
        while(std::getline(trace_file, line_data)){
            std::istringstream iss(line_data);
            std::string item;
            // ns pid pname
            iss >> item;
            iss >> item;
            iss >> item;

            // sector id
            iss >> item;
            try{
                trace_sector_id = stoll(item);
            }catch(std::invalid_argument){
                printf("Exception: Invalid line data %s\n", item.c_str());
            }
        
            // sector number
            iss >> item;
            
            // operation
            iss >> item;
            if(item[0] == 'W'){
                trace_is_write = true;
            }else{
                trace_is_write = false;
            }

            // major minor
            iss >> item;
            iss >> item;

            // md5
            iss >> item;
            trace_md5 = item;

            this->rangeStat(trace_is_write, trace_sector_id);
        }
    }

    void setTraceType(enum TraceType _tt){
        this->tt = _tt;
    }

    enum TraceType getTraceType(){
        return this->tt;
    }

    void showStatistic(){
        printf(" ---- ---- Trace Statistic ---- ----\n");
        if(this->tt==WEB){
            printf("Trace Type: web\n");
        }else if(this->tt==MAIL){
            printf("Trace Type: mail\n");
        }else if(this->tt==HOMES){
            printf("Trace Type:homes\n");
        }

        printf("Write max sector num: %lld\n", this->max_write_sector_address);
        printf("Write min sector num: %lld\n", this->min_write_sector_address);
        printf("Write sector range: %lld\n", this->max_write_sector_address - this->min_write_sector_address);
        printf("Read max sector num: %lld\n", this->max_read_sector_address);
        printf("Read min sector num: %lld\n", this->min_read_sector_address);
        printf("Read sector range: %lld\n", this->max_read_sector_address - this->min_read_sector_address);
        printf("Access max sector num: %lld\n", this->max_access_sector_address);
        printf("Access min sector num: %lld\n", this->min_access_sector_address);
        printf("Access sector range: %lld\n", this->max_access_sector_address - this->min_access_sector_address);
    }

private:
    long long max_write_sector_address;
    long long min_write_sector_address;
    long long max_read_sector_address;
    long long min_read_sector_address;
    long long max_access_sector_address;
    long long min_access_sector_address;
    TraceType tt;

    void rangeStat(bool write, long long sector_address){
        // 读写范围
        if(write){
            this->max_write_sector_address = 
                sector_address > this->max_write_sector_address ? 
                sector_address : this->max_write_sector_address;
            
            this->min_write_sector_address = 
                sector_address < this->min_write_sector_address ? 
                sector_address : this->min_write_sector_address;
        }else{
            this->max_read_sector_address = 
                sector_address > this->max_read_sector_address ? 
                sector_address : this->max_read_sector_address;
            
            this->min_read_sector_address = 
                sector_address < this->min_read_sector_address ? 
                sector_address : this->min_read_sector_address;
        }

        // 访问范围
        this->max_access_sector_address = 
            sector_address > this->max_access_sector_address ? 
            sector_address : this->max_access_sector_address;
            
        this->min_access_sector_address = 
            sector_address < this->min_access_sector_address ? 
            sector_address : this->min_access_sector_address;
    }
};

int main(int argc, char *argv[]) {
    Stater stater;
    if(strcmp(argv[1], "homes") == 0){
        printf("Choose homes\n");
        stater.setTraceType(HOMES);
    }else if(strcmp(argv[1], "web") == 0){
        printf("Choose web\n");
        stater.setTraceType(WEB);
    }else if(strcmp(argv[1], "mail") == 0){
        printf("Choose mail\n");
        stater.setTraceType(MAIL);
    }else{
        printf("Not Support homes now\n");
        exit(-1);
    }

    for(int i=1; i<=BLKPARSE_NUM; i++){
        string trace_file_path = "";
        if(stater.getTraceType() == MAIL)
            trace_file_path = "../trace/blkparse/cheetah.cs.fiu.edu-110108-113008.";
        else if(stater.getTraceType() == WEB){
            trace_file_path = "../trace/blkparse/webmail+online.cs.fiu.edu-110108-113008.";
        }else if(stater.getTraceType() == HOMES){
            trace_file_path = "../trace/blkparse/homes-110108-112108.";
        }else{
            printf("Not support\n");
            exit(-1);
        }
        trace_file_path.append(to_string(i));
        trace_file_path.append(".blkparse");
        stater.processOneTraceFile(trace_file_path);
    }

    stater.showStatistic();
    return 0;
}
