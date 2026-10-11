// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0
#include "rm_file_handle.h"

namespace {
// Owns exactly the pin acquired by fetch_page_handle, including exceptional exits.
class RecordPin {
    BufferPoolManager* pool_; PageId id_; bool dirty_ = false;
public:
    RecordPin(BufferPoolManager* pool, Page* page) : pool_(pool), id_(page->get_page_id()) {}
    ~RecordPin() { pool_->unpin_page(id_, dirty_); }
    void modified() { dirty_ = true; }
};
void check_slot(const RmFileHdr& hdr, const Rid& rid) {
    if (rid.slot_no < 0 || rid.slot_no >= hdr.num_records_per_page) throw RecordNotFoundError(rid.page_no, rid.slot_no);
}
}

std::unique_ptr<RmRecord> RmFileHandle::get_record(const Rid& rid, Context*) const {
    check_slot(file_hdr_, rid); auto page = fetch_page_handle(rid.page_no);
    RecordPin pin(buffer_pool_manager_, page.page);
    if (!Bitmap::is_set(page.bitmap, rid.slot_no)) throw RecordNotFoundError(rid.page_no, rid.slot_no);
    return std::make_unique<RmRecord>(file_hdr_.record_size, page.get_slot(rid.slot_no));
}

Rid RmFileHandle::insert_record(const char* buf, Context*) {
    if (!buf) throw InternalError("null record input");
    auto page = create_page_handle(); RecordPin pin(buffer_pool_manager_, page.page);
    int slot = Bitmap::first_bit(false, page.bitmap, file_hdr_.num_records_per_page);
    if (slot >= file_hdr_.num_records_per_page) throw InternalError("free-page chain contains a full page");
    std::memcpy(page.get_slot(slot), buf, file_hdr_.record_size); Bitmap::set(page.bitmap, slot);
    ++page.page_hdr->num_records;
    if (page.page_hdr->num_records == file_hdr_.num_records_per_page) {
        file_hdr_.first_free_page_no = page.page_hdr->next_free_page_no;
        page.page_hdr->next_free_page_no = RM_NO_PAGE;
    }
    pin.modified(); return Rid{.page_no=page.page->get_page_id().page_no, .slot_no=slot};
}

void RmFileHandle::insert_record(const Rid&, const char*) {
    // Reserved for the later recovery lab; not part of Lab1.
}

void RmFileHandle::delete_record(const Rid& rid, Context*) {
    check_slot(file_hdr_, rid); auto page = fetch_page_handle(rid.page_no);
    RecordPin pin(buffer_pool_manager_, page.page);
    if (!Bitmap::is_set(page.bitmap, rid.slot_no)) throw RecordNotFoundError(rid.page_no, rid.slot_no);
    bool was_full = page.page_hdr->num_records == file_hdr_.num_records_per_page;
    Bitmap::reset(page.bitmap, rid.slot_no); --page.page_hdr->num_records;
    if (was_full) release_page_handle(page);
    pin.modified();
}

void RmFileHandle::update_record(const Rid& rid, const char* buf, Context*) {
    if (!buf) throw InternalError("null record input");
    check_slot(file_hdr_, rid); auto page = fetch_page_handle(rid.page_no);
    RecordPin pin(buffer_pool_manager_, page.page);
    if (!Bitmap::is_set(page.bitmap, rid.slot_no)) throw RecordNotFoundError(rid.page_no, rid.slot_no);
    std::memcpy(page.get_slot(rid.slot_no), buf, file_hdr_.record_size); pin.modified();
}

RmPageHandle RmFileHandle::fetch_page_handle(int page_no) const {
    if (page_no < RM_FIRST_RECORD_PAGE || page_no >= file_hdr_.num_pages)
        throw PageNotExistError(disk_manager_->get_file_name(fd_), page_no);
    Page* page = buffer_pool_manager_->fetch_page(PageId{.fd=fd_, .page_no=page_no});
    if (!page) throw InternalError("record buffer pool exhausted");
    return {&file_hdr_, page};
}

RmPageHandle RmFileHandle::create_new_page_handle() {
    PageId id{.fd=fd_, .page_no=INVALID_PAGE_ID};
    Page* page = buffer_pool_manager_->new_page(&id);
    if (!page) throw InternalError("record buffer pool exhausted");
    RmPageHandle handle(&file_hdr_, page);
    handle.page_hdr->num_records = 0; handle.page_hdr->next_free_page_no = file_hdr_.first_free_page_no;
    std::memset(handle.bitmap, 0, file_hdr_.bitmap_size);
    ++file_hdr_.num_pages; file_hdr_.first_free_page_no = id.page_no;
    return handle;
}

RmPageHandle RmFileHandle::create_page_handle() {
    return file_hdr_.first_free_page_no == RM_NO_PAGE ? create_new_page_handle() : fetch_page_handle(file_hdr_.first_free_page_no);
}

void RmFileHandle::release_page_handle(RmPageHandle& page) {
    page.page_hdr->next_free_page_no = file_hdr_.first_free_page_no;
    file_hdr_.first_free_page_no = page.page->get_page_id().page_no;
}
