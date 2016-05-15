//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// optimistic_txn_manager.h
//
// Identification: src/backend/concurrency/optimistic_txn_manager.h
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include "backend/concurrency/transaction_manager.h"
#include "backend/storage/tile_group.h"
#include "backend/catalog/manager.h"

namespace peloton {
namespace concurrency {

extern thread_local std::unordered_map<oid_t, storage::TileGroup *> tile_group_cache;

//===--------------------------------------------------------------------===//
// optimistic concurrency control
//===--------------------------------------------------------------------===//

class OptimisticTxnManager : public TransactionManager {
 public:
  OptimisticTxnManager()  {}

  virtual ~OptimisticTxnManager() {}

  static OptimisticTxnManager &GetInstance();

  virtual VisibilityType IsVisible(
      const storage::TileGroupHeader *const tile_group_header,
      const oid_t &tuple_id);

  virtual bool IsOwner(const storage::TileGroupHeader *const tile_group_header,
                       const oid_t &tuple_id);

  virtual bool IsOwnable(
      const storage::TileGroupHeader *const tile_group_header,
      const oid_t &tuple_id);

  virtual bool AcquireOwnership(
      const storage::TileGroupHeader *const tile_group_header,
      const oid_t &tile_group_id, const oid_t &tuple_id);

  virtual void YieldOwnership(const oid_t &tile_group_id,
    const oid_t &tuple_id);

  virtual bool PerformInsert(const ItemPointer &location);

  virtual bool PerformRead(const ItemPointer &location);

  virtual void PerformUpdate(const ItemPointer &old_location,
                             const ItemPointer &new_location);

  virtual void PerformDelete(const ItemPointer &old_location,
                             const ItemPointer &new_location);

  virtual void PerformUpdate(const ItemPointer &location);

  virtual void PerformDelete(const ItemPointer &location);

  virtual Result CommitTransaction();

  virtual Result AbortTransaction();

  virtual Transaction *BeginTransaction() {
    txn_id_t txn_id = GetNextTransactionId();
    cid_t begin_cid = GetNextCommitId();
    Transaction *txn = new Transaction(txn_id, begin_cid);

    auto eid = EpochManagerFactory::GetInstance().EnterEpoch(begin_cid);
    txn->SetEpochId(eid);

    current_txn = txn;

    return txn;
  }

  virtual void EndTransaction() {

    EpochManagerFactory::GetInstance().ExitEpoch(current_txn->GetEpochId());

    delete current_txn;
    current_txn = nullptr;
  }

  virtual storage::TileGroup *GetTileGroupFromCache(oid_t tile_group_id) {
    auto itr = tile_group_cache.find(tile_group_id);
    if (itr != tile_group_cache.end()) {
      return itr->second;
    } else {
      storage::TileGroup *tile_group = catalog::Manager::GetInstance().GetTileGroup(tile_group_id).get();
      tile_group_cache.emplace(tile_group_id, tile_group);
      return tile_group;
    }
  }
};
}
}