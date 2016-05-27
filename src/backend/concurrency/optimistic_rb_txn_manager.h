//===----------------------------------------------------------------------===//
//
//                         PelotonDB
//
// optimistic_rb_txn_manager.h
//
// Identification: src/backend/concurrency/optimistic_rb_txn_manager.h
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include "backend/concurrency/transaction_manager.h"
#include "backend/storage/tile_group.h"
#include "backend/storage/rollback_segment.h"

namespace peloton {

namespace concurrency {

// Each transaction has a RollbackSegmentPool
extern thread_local storage::RollbackSegmentPool *current_segment_pool;
extern thread_local cid_t latest_read_timestamp;
//===--------------------------------------------------------------------===//
// optimistic concurrency control with rollback segment
//===--------------------------------------------------------------------===//

class OptimisticRbTxnManager : public TransactionManager {
  public:
  typedef char* RBSegType;

  OptimisticRbTxnManager() {}

  virtual ~OptimisticRbTxnManager() {}

  static OptimisticRbTxnManager &GetInstance();

  virtual VisibilityType IsVisible(
      const storage::TileGroupHeader *const tile_group_header,
      const oid_t &tuple_id);

  virtual bool IsOwner(const storage::TileGroupHeader *const tile_group_header,
                       const oid_t &tuple_id);

  virtual bool IsOwnable(
      const storage::TileGroupHeader *const tile_group_header,
      const oid_t &tuple_id);

  inline bool IsInserted(
      const storage::TileGroupHeader *const tile_grou_header,
      const oid_t &tuple_id) {
      assert(IsOwner(tile_grou_header, tuple_id));
      return tile_grou_header->GetBeginCommitId(tuple_id) == MAX_CID;
  }

  virtual bool AcquireOwnership(
      const storage::TileGroupHeader *const tile_group_header,
      const oid_t &tile_group_id, const oid_t &tuple_id);

  virtual void YieldOwnership(const oid_t &tile_group_id,
    const oid_t &tuple_id);

  bool ValidateRead( 
    const storage::TileGroupHeader *const tile_group_header,
    const oid_t &tuple_id,
    const cid_t &end_cid);


  virtual bool PerformInsert(const ItemPointer &location);

  // Get the read timestamp of the latest transaction on this thread, it is 
  // either the begin commit time of current transaction of the just committed
  // transaction.
  cid_t GetLatestReadTimestamp() {
    return latest_read_timestamp;
  }

  /**
   * Deprecated interfaces
   */
  virtual bool PerformRead(const ItemPointer &location);

  virtual void PerformUpdate(const ItemPointer &old_location __attribute__((unused)),
                             const ItemPointer &new_location __attribute__((unused))) { assert(false); }

  virtual void PerformDelete(const ItemPointer &old_location  __attribute__((unused)),
                             const ItemPointer &new_location __attribute__((unused))) { assert(false); }

  virtual void PerformUpdate(const ItemPointer &location  __attribute__((unused))) { assert(false); }

  /**
   * Interfaces for rollback segment
   */

  // Add a new rollback segment to the tuple
  void PerformUpdateWithRb(const ItemPointer &location, char *new_rb_seg);

  // Rollback the master copy of a tuple to the status at the begin of the 
  // current transaction
  void RollbackTuple(std::shared_ptr<storage::TileGroup> tile_group,
                            const oid_t tuple_id);

  // Whe a txn commits, it needs to set an end timestamp to all RBSeg it has
  // created in order to make them invisible to future transactions
  void InstallRollbackSegments(storage::TileGroupHeader *tile_group_header,
                                const oid_t tuple_id, const cid_t end_cid);

  /**
   * @brief Test if a reader with read timestamp @read_ts should follow on the
   * rb chain started from rb_set
   */
  inline bool IsRBVisible(char *rb_seg, cid_t read_ts) {
    // Check if we actually have a rollback segment
    if (rb_seg == nullptr) {
      return false;
    }

    cid_t rb_ts = storage::RollbackSegmentPool::GetTimeStamp(rb_seg);

    return read_ts < rb_ts;
  }

  // Return nullptr if the tuple is not activated to current txn.
  // Otherwise return the evident that current tuple is activated, the evidence
  // is either a RB or a pointer to the master version
  inline char* GetActivatedEvidence(const storage::TileGroupHeader *tile_group_header, const oid_t tuple_slot_id) {
    cid_t txn_begin_cid = current_txn->GetBeginCommitId();
    cid_t tuple_begin_cid = tile_group_header->GetBeginCommitId(tuple_slot_id);

    // The tuple is still valid
    assert(tuple_begin_cid != MAX_CID);
    // Owner can not call this function
    assert(IsOwner(tile_group_header, tuple_slot_id) == false);

    
    bool master_activated = (txn_begin_cid >= tuple_begin_cid);
    char *prev_visible = master_activated ? 
      tile_group_header->GetReservedFieldRef(tuple_slot_id) : nullptr;

    RBSegType rb_seg = GetRbSeg(tile_group_header, tuple_slot_id);
  
    while (IsRBVisible(rb_seg, txn_begin_cid)) {
      prev_visible = rb_seg;
      rb_seg = storage::RollbackSegmentPool::GetNextPtr(rb_seg);
    }

    return prev_visible;
  }

  virtual void PerformDelete(const ItemPointer &location);

  virtual Result CommitTransaction();

  virtual Result AbortTransaction();

  virtual Transaction *BeginTransaction() {
    // Set current transaction
    txn_id_t txn_id = GetNextTransactionId();
    cid_t begin_cid = GetNextCommitId();

    LOG_INFO("Beginning transaction %lu", txn_id);


    Transaction *txn = new Transaction(txn_id, begin_cid);
    current_txn = txn;

    auto eid = EpochManagerFactory::GetInstance().EnterEpoch(begin_cid);
    txn->SetEpochId(eid);

    latest_read_timestamp = begin_cid;
    // Create current transaction poll
    current_segment_pool = new storage::RollbackSegmentPool(BACKEND_TYPE_MM);

    return txn;
  }

  virtual void EndTransaction() {
    auto result = current_txn->GetResult();
    auto end_cid = current_txn->GetEndCommitId();

    if (result == RESULT_SUCCESS) {
      // Committed
      if (current_txn->IsReadOnly()) {
        // read only txn, just delete the segment pool because it's empty
        delete current_segment_pool;
      } else {
        // It's not read only txn
        current_segment_pool->SetPoolTimestamp(end_cid);
        living_pools_[end_cid] = std::shared_ptr<peloton::storage::RollbackSegmentPool>(current_segment_pool);
      }
    } else {
      // Aborted
      // TODO: Add coperative GC
      current_segment_pool->MarkedAsGarbage();
      garbage_pools_[current_txn->GetBeginCommitId()] = std::shared_ptr<peloton::storage::RollbackSegmentPool>(current_segment_pool);
    }

    EpochManagerFactory::GetInstance().ExitEpoch(current_txn->GetEpochId());

    delete current_txn;
    current_txn = nullptr;
    current_segment_pool = nullptr;
  }

  // A helper function to validate correctness
  void ValidateRbSegChain(storage::TileGroupHeader *tile_group_header, const oid_t &tuple_id) {
    RBSegType rb_seg = GetRbSeg(tile_group_header, tuple_id);
    cid_t begin_ts = tile_group_header->GetBeginCommitId(tuple_id);
    bool no_max_cid = false;

    while (rb_seg != nullptr) {
      cid_t rb_ts = storage::RollbackSegmentPool::GetTimeStamp(rb_seg);
      if (rb_ts == MAX_CID) {
        CHECK_M(no_max_cid == false, "Should not have a max cid on rb seg");
      } else {
        no_max_cid = true;
        CHECK_M(rb_ts <= begin_ts, "RB has a TS that is bigger than previous TS");
      }
      rb_seg = storage::RollbackSegmentPool::GetNextPtr(rb_seg);
      begin_ts = rb_ts;
    }
  }

  // Get current segment pool of the transaction manager
  inline storage::RollbackSegmentPool *GetSegmentPool() {
    return current_segment_pool;
  }

  // Get the head of RB Seg of a tuple
  inline RBSegType GetRbSeg(const storage::TileGroupHeader *tile_group_header, const oid_t tuple_id) {
    RBSegType *rb_seg_ptr = (RBSegType *)(tile_group_header->GetReservedFieldRef(tuple_id) + rb_seg_offset);
    return *rb_seg_ptr;
  }

private:
  static const size_t lock_offset = 0;
  static const size_t rb_seg_offset = lock_offset + 8;
  static const size_t delete_flag_offset = rb_seg_offset + sizeof(RBSegType);
  // TODO: add cooperative GC
  // The RB segment pool that is activlely being used
  cuckoohash_map<cid_t, std::shared_ptr<storage::RollbackSegmentPool>> living_pools_;
  // The RB segment pool that has been marked as garbage
  cuckoohash_map<cid_t, std::shared_ptr<storage::RollbackSegmentPool>> garbage_pools_;

   // Init reserved area of a tuple
  // delete_flag is used to mark that the transaction that owns the tuple
  // has deleted the tuple
  // Spinlock (8 bytes) | RB seg pointer (8 bytes) | delete_flag (1 bytes)
  void InitTupleReserved(const storage::TileGroupHeader *tile_group_header, const oid_t tuple_id) {
    auto reserved_area = tile_group_header->GetReservedFieldRef(tuple_id);
    new (reserved_area + lock_offset) Spinlock();
    SetRbSeg(tile_group_header, tuple_id, nullptr);
    *(reinterpret_cast<bool*>(reserved_area + delete_flag_offset)) = false;
  }

  // Set the head of RB Seg of a tuple
  inline void SetRbSeg(const storage::TileGroupHeader *tile_group_header, const oid_t tuple_id,
                       const RBSegType new_rb_seg) {
    RBSegType *rb_seg_ptr = (RBSegType *)(tile_group_header->GetReservedFieldRef(tuple_id) + rb_seg_offset);
    if (new_rb_seg != nullptr) {
      assert(storage::RollbackSegmentPool::GetNextPtr(new_rb_seg) == *rb_seg_ptr);  
    }
    *rb_seg_ptr = new_rb_seg;
  }

  // Delete flag
  inline bool GetDeleteFlag(const storage::TileGroupHeader *tile_group_header, const oid_t tuple_id) {
    return *(reinterpret_cast<bool*>(tile_group_header->GetReservedFieldRef(tuple_id) + delete_flag_offset));
  }

  inline void SetDeleteFlag(const storage::TileGroupHeader *tile_group_header, const oid_t tuple_id) {
    *(reinterpret_cast<bool*>(tile_group_header->GetReservedFieldRef(tuple_id) + delete_flag_offset)) = true;
  }

  inline void ClearDeleteFlag(const storage::TileGroupHeader *tile_group_header, const oid_t tuple_id) {
    *(reinterpret_cast<bool*>(tile_group_header->GetReservedFieldRef(tuple_id) + delete_flag_offset)) = false;
  }

  // Lock a tuple
  inline void LockTuple(const storage::TileGroupHeader *tile_group_header, const oid_t tuple_id) {
    auto lock = (Spinlock *)(tile_group_header->GetReservedFieldRef(tuple_id) + lock_offset);
    lock->Lock();
  }

  // Unlock a tuple
  inline void UnlockTuple(const storage::TileGroupHeader *tile_group_header, const oid_t tuple_id) {
    auto lock = (Spinlock *)(tile_group_header->GetReservedFieldRef(tuple_id) + lock_offset);
    lock->Unlock();
  }
};
}
}