// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0
#include "rm_scan.h"
#include "rm_file_handle.h"

RmScan::RmScan(const RmFileHandle* file) : file_handle_(file), rid_{.page_no=RM_FIRST_RECORD_PAGE, .slot_no=-1} {
    if (!file) throw InternalError("null scan file");
    next();
}

void RmScan::next() {
    if (is_end()) return;
    while (rid_.page_no < file_handle_->file_hdr_.num_pages) {
        auto page = file_handle_->fetch_page_handle(rid_.page_no);
        int slot = Bitmap::next_bit(true, page.bitmap, file_handle_->file_hdr_.num_records_per_page, rid_.slot_no);
        file_handle_->buffer_pool_manager_->unpin_page(page.page->get_page_id(), false);
        if (slot < file_handle_->file_hdr_.num_records_per_page) { rid_.slot_no = slot; return; }
        ++rid_.page_no; rid_.slot_no = -1;
    }
    rid_ = Rid{.page_no=RM_NO_PAGE, .slot_no=-1};
}
bool RmScan::is_end() const { return rid_.page_no == RM_NO_PAGE; }
Rid RmScan::rid() const { return rid_; }
