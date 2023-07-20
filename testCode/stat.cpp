#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <openssl/sha.h>

using namespace std;

#define BLKPARSE_NUM 21
#define SECTOR_SIZE 512
#define GB (1024*1024*1024)

enum TraceType{
	    HOMES,
        WEB, 
        MAIL,
        TEST,
        NOT
};

struct __attribute__ ((__packed__)) SHA1FP {
    // 20 bytes
    uint64_t fp1;
    uint32_t fp2, fp3, fp4;

    void print() {
        printf("%lu:%d:%d:%d\n", fp1, fp2, fp3, fp4);
    }
};

struct TupleHasher {
    std::size_t operator()(const SHA1FP &key) const {
        return key.fp1;
    }
};

struct TupleEqualer {
    bool operator()(const SHA1FP &lhs, const SHA1FP &rhs) const {
        return lhs.fp1 == rhs.fp1 && lhs.fp2 == rhs.fp2 && lhs.fp3 == rhs.fp3 && lhs.fp4 == rhs.fp4;
    }
};

class Stater{
public:
    Stater(){
        init();

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

            this->rangeStat(trace_is_write, trace_sector_id);
            this->readWriteLenStat(trace_is_write, trace_sector_num);
            this->writeDedupStat(trace_is_write, trace_md5);
            this->reqDedupStat(trace_md5);
            this->writeGeneratorStat(trace_is_write, trace_md5, trace_sector_num);
            this->writeUniqueOffsetStat(trace_is_write, trace_sector_id);
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
        printf("Write sector range: %.2f GiB\n", 
                                    float(this->max_write_sector_address - this->min_write_sector_address)*SECTOR_SIZE/GB);
        printf("Read max sector num: %lld\n", this->max_read_sector_address);
        printf("Read min sector num: %lld\n", this->min_read_sector_address);
        printf("Read sector range: %.2f GiB\n", 
                                    float(this->max_read_sector_address - this->min_read_sector_address)*SECTOR_SIZE/GB);
        printf("Access max sector num: %lld\n", this->max_access_sector_address);
        printf("Access min sector num: %lld\n", this->min_access_sector_address);
        printf("Access sector range: %.2f GiB\n", 
                                    float(this->max_access_sector_address - this->min_access_sector_address)*SECTOR_SIZE/GB);
        printf("Written unique by offset: %.2f GiB\n",  
                                    float(this->write_sector_id.size())*SECTOR_SIZE*8/GB);

        printf("Write 8 num: %lld, Write 16 num: %lld, ", this->write8, this->write16);
        printf("Write 24 num: %lld, Write 32 num: %lld, ", this->write24, this->write32);
        printf("Write 40 num: %lld, Write 40 plus: %lld\n", this->write40, this->write40plus);
        printf("Read 8 num: %lld, Read 16 num: %lld, ", this->read8, this->read16);
        printf("Read 24 num: %lld, Read 32 num: %lld, ", this->read24, this->read32);
        printf("Read 40 num: %lld, Read 40 plus: %lld\n", this->read40, this->read40plus);

        printf("Writes num: %llu, unique writes num: %lu ", this->writes_num, this->fp_set.size());
        printf("Write Dedup Ratio: %.2f\n", float(this->writes_num)/float(this->fp_set.size()));

        printf("Req num: %llu, unique req num: %lu ", this->req_num, this->fp_set_req.size());
        printf("Req Dedup Ratio: %.2f\n", float(this->req_num)/float(this->fp_set_req.size()));
        
        printf("Generator Write Dedup Ratio: %.2f\n", 
                                    float(this->writes_num)/float(this->fp_gen_sha1.size()));
    }

private:
    long long write8, write16, write24, write32, write40;
    long long read8, read16, read24, read32, read40;
    long long write40plus;
    long long read40plus;
    long long max_write_sector_address;
    long long min_write_sector_address;
    long long max_read_sector_address;
    long long min_read_sector_address;
    long long max_access_sector_address;
    long long min_access_sector_address;
    TraceType tt;

    unsigned long long writes_num;
    unsigned long long req_num;
    unordered_set<string> fp_set;
    unordered_set<string> fp_set_req;
    unordered_set<SHA1FP, TupleHasher, TupleEqualer> fp_gen_sha1;
    unordered_set<long long> write_sector_id;
    
private:
    void init(){
        max_write_sector_address = 0;
        min_write_sector_address = INT64_MAX;
        max_read_sector_address = 0;
        min_read_sector_address = INT64_MAX;
        
        tt = NOT;

        write8 = write16 = write24 = write32 = write40 = 0;
        read8 = read16 = read24 = read32 = read40 = 0;
        write40plus = read40plus = 0;

        writes_num = 0;
        req_num = 0;
    }

    void writeUniqueOffsetStat(bool write, long long sector_id){
        if(write){
            this->write_sector_id.insert(sector_id);
        }
    }

    void writeGeneratorStat(bool write, const string& md5, int sector_num){
        if(write){
            string data = data_generated_from_md5(md5, sector_num);
            struct SHA1FP sha1;
            SHA1((unsigned char *)data.c_str(), data.size(), (uint8_t*)&sha1);
            fp_gen_sha1.emplace(sha1);
        }
    }

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

    void readWriteLenStat(bool write, int sector_number){
        if(write){
            if(sector_number == 8) write8++;
            else if(sector_number == 16) write16++;
            else if(sector_number == 24) write24++;
            else if(sector_number == 32) write32++;
            else if(sector_number == 40) write40++;
            else if(sector_number > 40) write40plus++;
        }else{
            if(sector_number == 8) read8++;
            else if(sector_number == 16) read16++;
            else if(sector_number == 24) read24++;
            else if(sector_number == 32) read32++;
            else if(sector_number == 40) read40++;
            else if(sector_number > 40) read40plus++;
        }
    }

    void writeDedupStat(bool is_write, const string& fp){
        if(is_write){
            this->writes_num++;
            this->fp_set.insert(fp);
        }
    }

    void reqDedupStat(const string& fp){
        this->req_num++;
        this->fp_set_req.insert(fp);
    }

    string data_generated_from_md5(const string& trace_md5, int sector_num){
        string ans;
        ans.reserve(sector_num*SECTOR_SIZE);
        for(int i=1; i<=ans.capacity()/trace_md5.size(); i++)
            ans.append(trace_md5);
        return ans;
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
    }else if(strcmp(argv[1], "test") == 0){
        printf("Choose test\n");
        stater.setTraceType(TEST);
    }else{
        printf("Not Support homes now\n");
        exit(-1);
    }

    if(stater.getTraceType() == TEST){
        stater.processOneTraceFile("../trace/blkparse/test.blkparse");
        stater.showStatistic();
        return 0;
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
