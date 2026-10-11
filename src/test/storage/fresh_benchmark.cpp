// SPDX-License-Identifier: MulanPSL-2.0
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
#include "record/rm_manager.h"
#include "record/rm_scan.h"
using Clock=std::chrono::steady_clock;
void require(bool ok){if(!ok)throw std::runtime_error("benchmark validation failed");}
double elapsed(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
int main(){
    std::cout<<"kind,operation,pool_frames,size,repetition,ms,checksum\n";
    for(auto mode:{"sequential","uniform","hotspot"}){
        std::mt19937 rng(20261011);std::vector<int> trace(100000);uint64_t expected=0;
        for(size_t i=0;i<trace.size();++i){int n=std::strcmp(mode,"sequential")==0?i%2048:std::strcmp(mode,"uniform")==0?rng()%2048:(rng()%10<9?rng()%64:rng()%2048);trace[i]=n;expected+=n;}
        for(int frames:{32,128,512})for(int rep=1;rep<=5;++rep){
            DiskManager disk;BufferPoolManager pool(frames,&disk);disk.create_file("bench_pages.bin");int fd=disk.open_file("bench_pages.bin");
            std::array<char,PAGE_SIZE> data{};for(int n=0;n<2048;++n){std::memcpy(data.data(),&n,sizeof(n));disk.write_page(fd,n,data.data(),PAGE_SIZE);}
            uint64_t sum=0;auto start=Clock::now();
            for(int n:trace){PageId id{.fd=fd,.page_no=n};auto* page=pool.fetch_page(id);require(page!=nullptr);int v;std::memcpy(&v,page->get_data(),sizeof(v));sum+=v;require(pool.unpin_page(id,false));}
            double ms=elapsed(start);require(sum==expected);require(pool.get_pin_snapshot().empty());pool.discard_file_pages(fd);disk.close_file(fd);disk.destroy_file("bench_pages.bin");
            std::cout<<"page,"<<mode<<","<<frames<<",100000,"<<rep<<","<<ms<<","<<sum<<"\n";
        }
    }
    for(int count:{10000,50000,100000})for(auto op:{"insert","scan","update","delete"})for(int rep=1;rep<=5;++rep){
        DiskManager disk;BufferPoolManager pool(128,&disk);RmManager manager(&disk,&pool);manager.create_file("bench_records.bin",64);auto file=manager.open_file("bench_records.bin");
        std::array<char,64> data{};std::vector<Rid> ids;ids.reserve(count);
        if(std::strcmp(op,"insert")!=0)for(int i=0;i<count;++i){std::memcpy(data.data(),&i,sizeof(i));ids.push_back(file->insert_record(data.data(),nullptr));}
        uint64_t checksum=0;auto start=Clock::now();
        if(std::strcmp(op,"insert")==0)for(int i=0;i<count;++i){std::memcpy(data.data(),&i,sizeof(i));ids.push_back(file->insert_record(data.data(),nullptr));}
        else if(std::strcmp(op,"scan")==0)for(RmScan scan(file.get());!scan.is_end();scan.next()){auto record=file->get_record(scan.rid(),nullptr);int value;std::memcpy(&value,record->data,sizeof(value));checksum+=value;}
        else if(std::strcmp(op,"update")==0)for(int i=0;i<count;++i){int value=i+1;std::memcpy(data.data(),&value,sizeof(value));file->update_record(ids[i],data.data(),nullptr);}
        else for(auto id:ids)file->delete_record(id,nullptr);
        double ms=elapsed(start);
        if(std::strcmp(op,"scan")==0)require(checksum==static_cast<uint64_t>(count)*(count-1)/2);
        else if(std::strcmp(op,"delete")==0){RmScan scan(file.get());require(scan.is_end());checksum=count;}
        else for(int i=0;i<count;++i){auto record=file->get_record(ids[i],nullptr);int value;std::memcpy(&value,record->data,sizeof(value));require(value==i+(std::strcmp(op,"update")==0));checksum+=value;}
        require(pool.get_pin_snapshot().empty());manager.close_file(file.get());manager.destroy_file("bench_records.bin");
        std::cout<<"record,"<<op<<",128,"<<count<<","<<rep<<","<<ms<<","<<checksum<<"\n";
    }
}
