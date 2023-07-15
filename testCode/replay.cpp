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
#define SECTOR_SIZE 512

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
    TraceReplayer(string _device_path):device_path(_device_path){
        buffAllocation();
        deviceOpen();
        replaying_trace_num = 0;
    }
    ~TraceReplayer(){
        buffDeallocation();
        deviceClose();
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

            if(trace_is_write){
                string data = this->data_generated_from_md5(trace_md5, trace_sector_num);
                this->writeToDevice(data, trace_sector_id, trace_sector_num);
            }else{
                this->readFromDevice(trace_sector_id, trace_sector_num);
            }
        }
    }

    void setTraceType(enum TraceType _tt){
        this->tt = _tt;
    }

    enum TraceType getTraceType(){
        return this->tt;
    }


private:
    string data_generated_from_md5(const string& trace_md5, int sector_num){
        string ans(trace_md5.c_str(), sector_num*SECTOR_SIZE/trace_md5.size());
        return ans;
    }

    void writeToDevice(const string& s, int sector_offset, int sector_num){
        lseek(this->fd_device, sector_offset*512, SEEK_SET);
        int write_num;
        switch (sector_num)
        {
        case 8:
            memcpy(buff8, s.c_str(), 8*512);
            write_num = write(this->fd_device, buff8, 8*512);
            break;
        case 16:
            memcpy(buff16, s.c_str(), 16*512);
            write_num = write(this->fd_device, buff16, 16*512);
            break;
        case 24:
            memcpy(buff24, s.c_str(), 24*512);   
            write_num = write(this->fd_device, buff24, 24*512);
            break;
        case 32:
            memcpy(buff32, s.c_str(), 32*512);
            write_num = write(this->fd_device, buff32, 32*512);
            break;
        case 40:
            memcpy(buff40, s.c_str(), 40*512); 
            write_num = write(this->fd_device, buff40, 40*512);
            break;
        default:
            break;
        }

        if(write_num < 0){
            printf("Write error %s", strerror(errno));
            exit(-1);
        }
    }

    void readFromDevice(int sector_offset, int sector_num){
        lseek(this->fd_device, sector_offset*512, SEEK_SET);
        int read_num;
        switch (sector_num)
        {
        case 8:
            read_num = read(this->fd_device, buff8, 8*512);
            break;
        case 16:
            read_num = read(this->fd_device, buff16, 16*512);
            break;
        case 24:
            read_num = read(this->fd_device, buff24, 24*512);
            break;
        case 32:
            read_num = read(this->fd_device, buff32, 32*512);
            break;
        case 40: 
            read_num = read(this->fd_device, buff40, 40*512);
            break;
        default:
            break;
        }

        if(read_num < 0){
            printf("Read error %s, offset:%d, len:%d", strerror(errno), sector_offset, sector_num);
            exit(-1);
        }
    }

    void buffAllocation(){
        this->buff8 = (char*)malloc(8*512);
        int result = posix_memalign(&buff8, 4096, 512*8);
        if (result != 0) printf("buff8 align failed\n");

        this->buff16 = (char*)malloc(512*16);
        result = posix_memalign(&buff16, 4096, 512*16);
        if (result != 0) printf("buff16 align failed\n");

        this->buff24 = (char*)malloc(512*24);
        result = posix_memalign(&buff24, 4096, 512*24);
        if (result != 0) printf("buff24 align failed\n");

        this->buff32 = (char*)malloc(512*32);
        result = posix_memalign(&buff32, 4096, 512*32);
        if (result != 0) printf("buff32 align failed\n");

        this->buff40 = (char*)malloc(512*40);
        result = posix_memalign(&buff40, 4096, 512*40);
        if (result != 0) printf("buff40 align failed\n");
    }

    void buffDeallocation(){
        free(buff8);
        free(buff16);
        free(buff24);
        free(buff32);
        free(buff40);
    }
    
    void deviceOpen(){
        fd_device = open(device_path.c_str(), O_RDWR | O_DIRECT);
        if (fd_device == -1) {
            fprintf(stderr, "Failed to open device: %s\n", strerror(errno));
            exit(-1);
        }
    }

    void deviceClose(){
        close(this->fd_device);
    }

private:
    void * buff8;
    void * buff16;
    void * buff24;
    void * buff32;
    void * buff40;
    int fd_device;
    string device_path;
    TraceType tt;
    int replaying_trace_num;
};


int main(int argc, char* argv[]) {
    string device_path = "/dev/mapper/mydedup"; // replace with your device path
    TraceReplayer player(device_path);

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
        printf("Not Support homes now\n");
        exit(-1);
    }

    if(player.getTraceType() == TEST){
        player.processOneTraceFile("../trace/blkparse/test.blkparse");
        return 0;
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
        player.processOneTraceFile(trace_file_path);
    }
    return 0;
}
