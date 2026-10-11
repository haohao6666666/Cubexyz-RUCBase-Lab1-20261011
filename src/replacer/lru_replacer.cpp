// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "lru_replacer.h"

LRUReplacer::LRUReplacer(size_t num_pages) { max_size_ = num_pages; }

LRUReplacer::~LRUReplacer() = default;

/**
 * @description: 使用LRU策略删除一个victim frame，并返回该frame的id
 * @param {frame_id_t*} frame_id 被移除的frame的id，如果没有frame被移除返回nullptr
 * @return {bool} 如果成功淘汰了一个页面则返回true，否则返回false
 */
bool LRUReplacer::victim(frame_id_t* frame_id) {
    std::scoped_lock lock(latch_);
    if (!frame_id || LRUlist_.empty()) return false;
    *frame_id = LRUlist_.back(); LRUlist_.pop_back(); LRUhash_.erase(*frame_id);
    return true;
}

/**
 * @description: 固定指定的frame，即该页面无法被淘汰
 * @param {frame_id_t} 需要固定的frame的id
 */
void LRUReplacer::pin(frame_id_t frame_id) {
    std::scoped_lock lock(latch_);
    auto it = LRUhash_.find(frame_id);
    if (it != LRUhash_.end()) { LRUlist_.erase(it->second); LRUhash_.erase(it); }
}

/**
 * @description: 取消固定一个frame，代表该页面可以被淘汰
 * @param {frame_id_t} frame_id 取消固定的frame的id
 */
void LRUReplacer::unpin(frame_id_t frame_id) {
    std::scoped_lock lock(latch_);
    if (frame_id < 0 || static_cast<size_t>(frame_id) >= max_size_ || LRUhash_.contains(frame_id) || LRUlist_.size() >= max_size_) return;
    LRUlist_.push_front(frame_id); LRUhash_.emplace(frame_id, LRUlist_.begin());
}

/**
 * @description: 获取当前replacer中可以被淘汰的页面数量
 */
size_t LRUReplacer::Size() {
    std::scoped_lock lock(latch_);
    return LRUlist_.size();
}
