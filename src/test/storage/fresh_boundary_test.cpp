// SPDX-License-Identifier: MulanPSL-2.0
#include <gtest/gtest.h>
#include <array>
#include <algorithm>
#include <set>
#include <map>
#include <random>
#include <thread>
#include <vector>
#include "record/rm_manager.h"
#include "record/rm_scan.h"

struct FreshStorage : testing::Test {
    DiskManager disk; BufferPoolManager pool{3, &disk}; RmManager records{&disk, &pool};
    void TearDown() override {
        for (auto f : {"records.bin", "pages.bin", "second.bin"}) if (disk.is_file(f)) disk.destroy_file(f);
    }
};
TEST_F(FreshStorage, PartialIoAndInvalidArguments) {
    disk.create_file("pages.bin"); int fd=disk.open_file("pages.bin");
    std::array<char,19> in{},out{}; in.fill('x'); disk.write_page(fd,2,in.data(),in.size());
    disk.read_page(fd,2,out.data(),out.size()); EXPECT_EQ(in,out);
    EXPECT_THROW(disk.read_page(fd,-1,out.data(),1),InternalError);
    EXPECT_THROW(disk.read_page(fd,0,out.data(),PAGE_SIZE+1),InternalError);
    EXPECT_THROW(disk.read_page(fd,3,out.data(),1),InternalError);
    EXPECT_THROW(disk.destroy_file("pages.bin"),FileNotClosedError);
    disk.close_file(fd); EXPECT_THROW(disk.close_file(fd),FileNotOpenError);
}
TEST_F(FreshStorage, SharedDescriptorPageIo) {
    disk.create_file("pages.bin"); int fd=disk.open_file("pages.bin"); std::vector<std::thread> threads;
    for(int t=0;t<4;++t) threads.emplace_back([&,t]{
        std::array<char,PAGE_SIZE> in{},out{};in.fill(static_cast<char>(t+1));
        for(int i=0;i<50;++i){disk.write_page(fd,t,in.data(),PAGE_SIZE);disk.read_page(fd,t,out.data(),PAGE_SIZE);EXPECT_EQ(in,out);}
    });
    for(auto& t:threads)t.join(); disk.close_file(fd);
}
TEST_F(FreshStorage, ExhaustionDoesNotAllocateAndDirtySurvives) {
    disk.create_file("pages.bin");int fd=disk.open_file("pages.bin");std::vector<PageId> ids;
    for(int i=0;i<3;++i){PageId id{.fd=fd};auto* page=pool.new_page(&id);ASSERT_NE(page,nullptr);page->get_data()[0]=static_cast<char>(i+1);ids.push_back(id);}
    auto before=disk.get_fd2pageno(fd);PageId fail{.fd=fd};EXPECT_EQ(pool.new_page(&fail),nullptr);EXPECT_EQ(disk.get_fd2pageno(fd),before);
    for(auto id:ids){EXPECT_FALSE(pool.delete_page(id));pool.unpin_page(id,true);}
    PageId extra{.fd=fd};ASSERT_NE(pool.new_page(&extra),nullptr);pool.unpin_page(extra,false);
    auto* page=pool.fetch_page(ids[0]);ASSERT_NE(page,nullptr);EXPECT_EQ(page->get_data()[0],1);pool.unpin_page(ids[0],false);
    pool.discard_file_pages(fd);disk.close_file(fd);
}
TEST_F(FreshStorage, FailedFetchRestoresCandidate) {
    disk.create_file("pages.bin");int fd=disk.open_file("pages.bin");
    for(int i=0;i<3;++i){PageId id{.fd=fd};ASSERT_NE(pool.new_page(&id),nullptr);pool.unpin_page(id,false);}
    EXPECT_THROW(pool.fetch_page(PageId{.fd=fd,.page_no=100}),InternalError);
    PageId id{.fd=fd};EXPECT_NE(pool.new_page(&id),nullptr);pool.unpin_page(id,false);
    EXPECT_TRUE(pool.get_pin_snapshot().empty());pool.discard_file_pages(fd);disk.close_file(fd);
}
TEST_F(FreshStorage, ZeroPagePersistsAndDescriptorReuseIsSafe) {
    records.create_file("records.bin",64);auto f=records.open_file("records.bin");
    std::array<char,64> data{};data.fill('a');auto rid=f->insert_record(data.data(),nullptr);int old_fd=f->fd();records.close_file(f.get());
    records.create_file("second.bin",64);auto g=records.open_file("second.bin");data.fill('b');auto other=g->insert_record(data.data(),nullptr);
    auto got=g->get_record(other,nullptr);EXPECT_EQ(got->data[0],'b');records.close_file(g.get());
    f=records.open_file("records.bin");EXPECT_EQ(f->get_record(rid,nullptr)->data[0],'a');records.close_file(f.get());
    // Descriptor reuse itself is OS-dependent; data must be correct in either case.
    (void)old_fd;
    disk.create_file("pages.bin");int fd=disk.open_file("pages.bin");PageId id{.fd=fd};ASSERT_NE(pool.new_page(&id),nullptr);
    pool.unpin_page(id,false);pool.discard_file_pages(fd);disk.close_file(fd);fd=disk.open_file("pages.bin");
    std::array<char,PAGE_SIZE> out{};out.fill('z');disk.read_page(fd,id.page_no,out.data(),PAGE_SIZE);
    EXPECT_TRUE(std::all_of(out.begin(),out.end(),[](char x){return x==0;}));disk.close_file(fd);
}
TEST_F(FreshStorage, InvalidRidAndEmptyScanDoNotLeakPins) {
    records.create_file("records.bin",64);auto f=records.open_file("records.bin");RmScan empty(f.get());EXPECT_TRUE(empty.is_end());empty.next();EXPECT_TRUE(empty.is_end());
    std::array<char,64> data{};auto rid=f->insert_record(data.data(),nullptr);
    EXPECT_THROW(f->get_record(Rid{.page_no=rid.page_no,.slot_no=-1},nullptr),RecordNotFoundError);
    f->delete_record(rid,nullptr);EXPECT_THROW(f->get_record(rid,nullptr),RecordNotFoundError);
    EXPECT_THROW(f->update_record(rid,data.data(),nullptr),RecordNotFoundError);
    EXPECT_TRUE(pool.get_pin_snapshot().empty());records.close_file(f.get());
}
TEST_F(FreshStorage, RandomizedRecordsAgainstIndependentMap) {
    records.create_file("records.bin",64);auto f=records.open_file("records.bin");
    std::map<std::pair<int,int>,std::array<char,64>> expected;std::mt19937 rng(20261011);
    for(int step=0;step<10000;++step){
        auto op=rng()%4;std::array<char,64> data{};for(auto& c:data)c=static_cast<char>(rng()%127);
        if(expected.empty()||op==0){auto rid=f->insert_record(data.data(),nullptr);ASSERT_TRUE(expected.emplace(std::make_pair(rid.page_no,rid.slot_no),data).second);}
        else {auto it=expected.begin();std::advance(it,rng()%expected.size());Rid rid{.page_no=it->first.first,.slot_no=it->first.second};
            if(op==1){f->delete_record(rid,nullptr);expected.erase(it);}else if(op==2){f->update_record(rid,data.data(),nullptr);it->second=data;}
            else{auto got=f->get_record(rid,nullptr);EXPECT_EQ(std::memcmp(got->data,it->second.data(),64),0);}}
        if(step%1000==0){records.close_file(f.get());f=records.open_file("records.bin");}
    }
    std::map<std::pair<int,int>,std::array<char,64>> actual;
    for(RmScan scan(f.get());!scan.is_end();scan.next()){auto rid=scan.rid();auto got=f->get_record(rid,nullptr);std::array<char,64> data{};std::memcpy(data.data(),got->data,64);actual.emplace(std::make_pair(rid.page_no,rid.slot_no),data);}
    EXPECT_EQ(actual,expected);EXPECT_TRUE(pool.get_pin_snapshot().empty());records.close_file(f.get());
}
TEST(FreshLru, DuplicateReleaseAndConcurrentMembership) {
    LRUReplacer lru(32);lru.unpin(1);lru.unpin(2);lru.unpin(1);frame_id_t victim=-1;ASSERT_TRUE(lru.victim(&victim));EXPECT_EQ(victim,1);
    lru.pin(2);std::vector<std::thread> threads;for(int t=0;t<4;++t)threads.emplace_back([&,t]{for(int i=0;i<1000;++i){lru.unpin(t);lru.pin(t);}lru.unpin(t);});
    for(auto& t:threads)t.join();EXPECT_EQ(lru.Size(),4u);std::set<int> found;while(lru.victim(&victim))EXPECT_TRUE(found.insert(victim).second);EXPECT_EQ(found.size(),4u);
}
TEST_F(FreshStorage, EmptyMiddlePageAndSlotReuse) {
    records.create_file("records.bin",512);auto file=records.open_file("records.bin");std::array<char,512> data{};
    int per=file->get_file_hdr().num_records_per_page;std::vector<Rid> ids;
    for(int i=0;i<per*3;++i){data[0]=static_cast<char>(i);ids.push_back(file->insert_record(data.data(),nullptr));}
    int middle=ids[per].page_no;for(int i=per;i<2*per;++i)file->delete_record(ids[i],nullptr);
    int count=0;for(RmScan scan(file.get());!scan.is_end();scan.next()){EXPECT_NE(scan.rid().page_no,middle);++count;}EXPECT_EQ(count,2*per);
    auto reused=file->insert_record(data.data(),nullptr);EXPECT_EQ(reused.page_no,middle);EXPECT_TRUE(pool.get_pin_snapshot().empty());records.close_file(file.get());
}
TEST_F(FreshStorage, RecordAllocationFailureKeepsHeader) {
    records.create_file("records.bin",64);auto file=records.open_file("records.bin");int fd=file->fd();std::vector<PageId> pinned;
    for(int i=0;i<3;++i){PageId id{.fd=fd};ASSERT_NE(pool.new_page(&id),nullptr);pinned.push_back(id);}
    auto before=file->get_file_hdr();std::array<char,64> data{};
    EXPECT_THROW(file->insert_record(data.data(),nullptr),InternalError);
    EXPECT_EQ(file->get_file_hdr().num_pages,before.num_pages);EXPECT_EQ(file->get_file_hdr().first_free_page_no,before.first_free_page_no);
    for(auto id:pinned)pool.unpin_page(id,false);records.close_file(file.get());
}
