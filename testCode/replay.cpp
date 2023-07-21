#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/time.h>
#include <thread>
#include <atomic>
#include <set>

#include "thread_pool.h"

using namespace std;

#define TRACE_LINE_ITEM_NUM 9
#define MD5_SIZE 16
#define BLOCK_SIZE 4096
#define SECTOR_SIZE 512LL

#define BLKPARSE_NUM 21

enum TraceType{
    HOMES,
    WEB, 
    MAIL,
    TEST,
    NOT
};

class TraceReplayer{
public:
    TraceReplayer(string _device_path):
    device_path(_device_path)
    {
        deviceOpen();
        replaying_trace_num = 0;
    }
    ~TraceReplayer(){
        deviceClose();
    }
    
    void over(){
        cv.notify_all();
        delete threadPool;
    }

    void processOneTraceFile(string trace_file_path){
        replaying_trace_num = 0;

        ifstream  trace_file(trace_file_path.c_str());
        if(!trace_file){
            printf("Trace file open failed: %s\n", trace_file_path.c_str());
            exit(-1);
        }
        printf("Process trace file %s\n", trace_file_path.c_str());

        long long trace_ns;
        int trace_pid;
        string trace_pname;
        off_t trace_sector_id;
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
            trace_sector_num = stoi(item);
            
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
            
            /*
                replay
            */
            if(replaying_trace_num % 10000 == 0){
                printf("req %d\n", replaying_trace_num);
            }
            replaying_trace_num++;

            string data = data_generated_from_md5(trace_md5, trace_sector_num);
            threadPool->doJob([this, data, trace_sector_id, trace_sector_num,trace_is_write]() {
                {
                    std::unique_lock<std::mutex> l(mut);
                    while (accessSet.find(trace_sector_id) != accessSet.end()) {
                        cv.wait(l);
                    }
                    accessSet.insert(trace_sector_id);
                }
                
                if(trace_is_write)
                    writeToDevice(this->fd_device, data, trace_sector_id, trace_sector_num);
                else
                    readFromDevice(this->fd_device, trace_sector_id, trace_sector_num);

                {
                    std::unique_lock<std::mutex> l(mut);
                    accessSet.erase(trace_sector_id);
                    cv.notify_all();
                }
            });
        }
        
    }

    // setters
    void setTraceType(enum TraceType _tt){this->tt = _tt;}
    void setThreadNum(int num){
        this->thread_num = num;
        threadPool = new AThreadPool(num);
    }
    
    // getters
    enum TraceType getTraceType(){return this->tt;}
    int getThreadNum(){return this->thread_num;}
    int getDeviceFD(){return this->fd_device;}

private:
    string data_generated_from_md5(const string& trace_md5, int sector_num){
        string ans;
        ans.reserve(sector_num*SECTOR_SIZE);
        for(int i=1; i<=ans.capacity()/trace_md5.size(); i++)
            ans.append(trace_md5);
        return ans;
    }

    void writeToDevice(int fd, const string &s, off_t sector_offset, int sector_num){
        if(sector_num > 40)
            return ;
        alignas(SECTOR_SIZE) uint8_t buff[40*SECTOR_SIZE];
        memcpy(buff, s.c_str(), s.size());
        int write_num = pwrite(fd, reinterpret_cast<void*>(buff), 
                               sector_num*SECTOR_SIZE, sector_offset*SECTOR_SIZE);
        if(write_num < 0){
            printf("Write error %d\n", errno);
            exit(-1);
        }
    }

    void readFromDevice(int fd, int sector_offset, int sector_num){
        if(sector_num > 40)
            return ;

        alignas(SECTOR_SIZE) uint8_t buff[40*SECTOR_SIZE];
        int read_num = pread(fd, reinterpret_cast<void*>(buff),
                             sector_num*SECTOR_SIZE, sector_offset*SECTOR_SIZE);

        if(read_num < 0){
            printf("Read error %s", strerror(errno));
            exit(-1);
        }
    }
    
    void deviceOpen(){
        fd_device = open(device_path.c_str(), O_RDWR | O_DIRECT);
        if (fd_device == -1) {
            fprintf(stderr, "Failed to open device: %s, %s\n", strerror(errno), device_path.c_str());
            exit(-1);
        }
    }

    void deviceClose(){
        close(this->fd_device);
    }

private:
    int fd_device;
    string device_path;
    TraceType tt;
    int replaying_trace_num;
    int thread_num;

    std::mutex mut;
    std::condition_variable cv;
    AThreadPool *threadPool;
    std::set<off_t> accessSet;
};


int main(int argc, char* argv[]) {
    TraceReplayer player("/dev/mapper/mydedup");
    
    if(argc < 3){
        cout<<"You need 2 arg at least\n";
        exit(-1);
    }

    /*
        Parsing and setting args
    */
    if(strcmp(argv[1], "homes") == 0){
        printf("Choose homes\n");
        player.setTraceType(HOMES);
    }else if(strcmp(argv[1], "web") == 0){
        printf("Choose web\n");
        player.setTraceType(WEB);
    }else if(strcmp(argv[1], "mail") == 0){
        printf("Choose mail\n");
        player.setTraceType(MAIL);
    }else if(strcmp(argv[1], "test") == 0){
        printf("Choose test\n");
        player.setTraceType(TEST);
    }else{
        printf("Not Support now\n");
        exit(-1);
    }
    
    player.setThreadNum(atoi(argv[2]));
    
    /*
        Replay
    */
    struct timeval t1, t2;
    if(player.getTraceType() == TEST){
        gettimeofday(&t1, NULL);
        player.processOneTraceFile("../trace/blkparse/test.blkparse");
        gettimeofday(&t2, NULL);
        goto over;
    }

    for(int i=1; i<=BLKPARSE_NUM; i++){
        string trace_file_path = "";
        if(player.getTraceType() == MAIL)
            trace_file_path = "../trace/blkparse/cheetah.cs.fiu.edu-110108-113008.";
        else if(player.getTraceType() == WEB){
            trace_file_path = "../trace/blkparse/webmail+online.cs.fiu.edu-110108-113008.";
        }else if(player.getTraceType() == HOMES){
            trace_file_path = "../trace/blkparse/homes-110108-112108.";
        }else{
            printf("Not support\n");
            exit(-1);
        }
        trace_file_path.append(to_string(i));
        trace_file_path.append(".blkparse");

        gettimeofday(&t1, NULL);
        player.processOneTraceFile(trace_file_path);
        gettimeofday(&t2, NULL);
    }
    
over:
    player.over();
    printf("Cost time %ld us\n", (t2.tv_sec-t1.tv_sec)*1000000 + t2.tv_usec-t1.tv_usec);
    return 0;
}
