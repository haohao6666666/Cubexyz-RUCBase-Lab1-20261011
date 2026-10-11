// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0
#include "buffer_pool_manager.h"
#include "disk_manager.h"
#include "common/errors.h"

#include <array>
#include <cstring>

bool BufferPoolManager::find_victim_page(frame_id_t* frame_id) {
    if (!free_list_.empty()) { *frame_id = free_list_.front(); free_list_.pop_front(); return true; }
    return replacer_->victim(frame_id);
}

// Commit a frame transition only after the old dirty contents have been saved.
void BufferPoolManager::update_page(Page* page, PageId id, frame_id_t frame) {
    if (page->is_dirty_) disk_manager_->write_page(page->id_.fd, page->id_.page_no, page->data_, PAGE_SIZE);
    page_table_.erase(page->id_);
    page->reset_memory(); page->id_ = id; page->is_dirty_ = false; page->pin_count_ = 0;
    page_table_[id] = frame;
}

Page* BufferPoolManager::fetch_page(PageId id) {
    std::scoped_lock lock(latch_);
    if (auto it = page_table_.find(id); it != page_table_.end()) {
        auto& page = pages_[it->second]; replacer_->pin(it->second); ++page.pin_count_; return &page;
    }
    frame_id_t frame;
    if (!find_victim_page(&frame)) return nullptr;
    auto& page = pages_[frame];
    try {
        std::array<char, PAGE_SIZE> contents{};
        disk_manager_->read_page(id.fd, id.page_no, contents.data(), PAGE_SIZE);
        update_page(&page, id, frame);
        std::memcpy(page.data_, contents.data(), PAGE_SIZE); page.pin_count_ = 1;
        replacer_->pin(frame); return &page;
    } catch (...) {
        if (page.id_.page_no == INVALID_PAGE_ID) free_list_.push_front(frame);
        else replacer_->unpin(frame);
        throw;
    }
}

bool BufferPoolManager::unpin_page(PageId id, bool dirty) {
    std::scoped_lock lock(latch_);
    auto it = page_table_.find(id);
    if (it == page_table_.end()) return false;
    auto& page = pages_[it->second];
    if (page.pin_count_ <= 0) return false;
    page.is_dirty_ = page.is_dirty_ || dirty;
    if (--page.pin_count_ == 0) replacer_->unpin(it->second);
    return true;
}

bool BufferPoolManager::flush_page(PageId id) {
    std::scoped_lock lock(latch_);
    auto it = page_table_.find(id); if (it == page_table_.end()) return false;
    auto& page = pages_[it->second];
    disk_manager_->write_page(id.fd, id.page_no, page.data_, PAGE_SIZE); page.is_dirty_ = false;
    return true;
}

Page* BufferPoolManager::new_page(PageId* id) {
    if (!id) throw InternalError("null output page id");
    std::scoped_lock lock(latch_);
    disk_manager_->get_file_name(id->fd);
    frame_id_t frame; if (!find_victim_page(&frame)) return nullptr;
    auto& page = pages_[frame];
    try {
        // Write failure must not consume the next page number.
        if (page.is_dirty_) {
            disk_manager_->write_page(page.id_.fd, page.id_.page_no, page.data_, PAGE_SIZE);
            page.is_dirty_ = false;
        }
        PageId next{.fd=id->fd, .page_no=disk_manager_->allocate_page(id->fd)};
        update_page(&page, next, frame); page.pin_count_ = 1; page.is_dirty_ = true;
        replacer_->pin(frame); *id = next; return &page;
    } catch (...) {
        if (page.id_.page_no == INVALID_PAGE_ID) free_list_.push_front(frame);
        else replacer_->unpin(frame);
        throw;
    }
}

bool BufferPoolManager::delete_page(PageId id) {
    std::scoped_lock lock(latch_);
    auto it = page_table_.find(id); if (it == page_table_.end()) return true;
    auto frame = it->second; auto& page = pages_[frame];
    if (page.pin_count_ != 0) return false;
    if (page.is_dirty_) disk_manager_->write_page(id.fd, id.page_no, page.data_, PAGE_SIZE);
    page_table_.erase(it); replacer_->pin(frame); page.reset_memory();
    page.id_ = PageId{}; page.is_dirty_ = false; page.pin_count_ = 0; free_list_.push_back(frame);
    return true;
}

void BufferPoolManager::flush_all_pages(int fd) {
    std::scoped_lock lock(latch_);
    for (const auto& [id, frame] : page_table_) if (id.fd == fd) {
        auto& page = pages_[frame]; disk_manager_->write_page(fd, id.page_no, page.data_, PAGE_SIZE);
        page.is_dirty_ = false;
    }
}
