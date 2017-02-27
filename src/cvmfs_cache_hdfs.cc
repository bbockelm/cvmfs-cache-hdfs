
#include <map>
#include <string>
#include <sstream>
#include <cstdint>
#include <cinttypes>

#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <pwd.h>

#include "libcvmfs_cache.h"
#include "hdfs.h"

struct cvmcache_context *ctx;
std::string g_hdfs_base;
hdfsFS g_fs = nullptr;

struct TxnTransient {
  TxnTransient() : size_(0), fp_(NULL) { memset(&id_, 0, sizeof(id_)); }
  explicit TxnTransient(const struct cvmcache_hash &hash, hdfsFile fp) : size_(0), id_(hash), fp_(fp) { }

  uint64_t size_;
  struct cvmcache_hash id_;
  hdfsFile fp_;
};

std::map<uint64_t, TxnTransient> transactions;

static std::string hdfs_file_name(const struct cvmcache_hash &id)
{
  std::stringstream ss;
  char tmp_path[sizeof(id.digest) + 2];
  memcpy(tmp_path, id.digest, 2);
  tmp_path[2] = '/';
  memcpy(tmp_path+3, id.digest+2, sizeof(id.digest)-2);
  tmp_path[sizeof(tmp_path)-1] = '\0';
  ss << g_hdfs_base << "/" << tmp_path;
  return ss.str();
}

static int hdfs_pread(struct cvmcache_hash *id,
                    uint64_t offset,
                    uint32_t *size,
                    unsigned char *buffer)
{
  std::string fname = hdfs_file_name(*id);
  hdfsFile fp = hdfsOpenFile(g_fs, fname.c_str(), O_RDONLY, 0, 0, 0);
  if (fp == nullptr)
  {
    return CVMCACHE_STATUS_NOENTRY;
  }
  tSize nbytes = hdfsPread(g_fs, fp, offset, buffer, *size);
  if (-1 == nbytes)
  {
    hdfsCloseFile(g_fs, fp);
    return CVMCACHE_STATUS_IOERR;
  }
  *size = nbytes;
  return CVMCACHE_STATUS_OK;
}


static int hdfs_start_txn(struct cvmcache_hash *id,
                        uint64_t txn_id,
                        struct cvmcache_object_info *info)
{
  std::string fname = hdfs_file_name(*id);
  std::string new_fname = fname + ".inprogress";

  hdfsFile fp = hdfsOpenFile(g_fs, fname.c_str(), O_RDONLY, 0, 0, 0);
  if (fp != nullptr)
  {
    hdfsCloseFile(g_fs, fp);
    return CVMCACHE_STATUS_OK;
  }
  fp = hdfsOpenFile(g_fs, fname.c_str(), O_WRONLY, 0, 1, 0);

  TxnTransient txn(*id, fp);
  transactions[txn_id] = txn;
  return CVMCACHE_STATUS_OK;
}


static int hdfs_write_txn(uint64_t txn_id,
                 unsigned char *buffer,
                 uint32_t size)
{
  TxnTransient &txn = transactions[txn_id];

  tSize nbytes = hdfsWrite(g_fs, txn.fp_, buffer, size);
  if (nbytes == -1)
  {
    return CVMCACHE_STATUS_IOERR;
  }

  txn.size_ += size;
  return CVMCACHE_STATUS_OK;
}


static int hdfs_commit_txn(uint64_t txn_id) {
  TxnTransient &txn(transactions[txn_id]);
  printf("commit txn ID %" PRIu64 ", size %" PRIu64 "\n", txn_id, txn.size_);

  if (hdfsHFlush(g_fs, txn.fp_) == -1)
  {
    fprintf(stderr, "Commit txn ID %" PRIu64 " failed: %s (errno=%d)\n", txn_id, strerror(errno), errno);
    hdfsCloseFile(g_fs, txn.fp_);
    return CVMCACHE_STATUS_IOERR;
  }
  if (-1 == hdfsCloseFile(g_fs, txn.fp_))
  {
    fprintf(stderr, "Commit txn ID %" PRIu64 " failed: %s (errno=%d)\n", txn_id, strerror(errno), errno);
    return CVMCACHE_STATUS_IOERR;
  }
  std::string fname = hdfs_file_name(txn.id_);
  std::string new_fname = fname + ".inprogress";

  if ((-1 == hdfsRename(g_fs, new_fname.c_str(), fname.c_str())) &&  (-1 == hdfsExists(g_fs, fname.c_str())))
  {
    fprintf(stderr, "Commit of txn ID %" PRIu64 ", filename %s failed: %s (errno=%d)\n", txn_id, fname.c_str(), strerror(errno), errno);
    return CVMCACHE_STATUS_IOERR;
  }

  transactions.erase(txn_id);
  return CVMCACHE_STATUS_OK;
}

static int hdfs_abort_txn(uint64_t txn_id) {
  printf("Abort transaction %" PRIu64 "\n", txn_id);

  TxnTransient &txn = transactions[txn_id];

  if (-1 == hdfsCloseFile(g_fs, txn.fp_))
  {
    fprintf(stderr, "Abort txn ID %" PRIu64 " failed: %s (errno=%d)\n", txn_id, strerror(errno), errno);
    transactions.erase(txn_id);
    return CVMCACHE_STATUS_IOERR;
  }
  std::string fname = hdfs_file_name(txn.id_) + ".inprogress";
  if (-1 == hdfsDelete(g_fs, fname.c_str(), 0))
  {
    fprintf(stderr, "Delete failed for %s: %s (errno=%d)\n", fname.c_str(), strerror(errno), errno);
    transactions.erase(txn_id);
    return CVMCACHE_STATUS_IOERR;
  }

  transactions.erase(txn_id);
  return CVMCACHE_STATUS_OK;
}

void Usage(const char *progname) {
  printf("%s <HDFS base directory> <Cvmfs cache locator>\n", progname);
}


int main(int argc, char **argv) {
  if (argc < 3) {
    Usage(argv[0]);
    return 1;
  }
  printf("Will use HDFS directory %s\n", argv[1]);

  int euid = geteuid();
  struct passwd * user_info = getpwuid(euid);
  if ((user_info == nullptr) || (user_info->pw_name == nullptr))
  {
    fprintf(stderr, "Failed to determine current username: %s (errno=%d).\n", strerror(errno), errno);
    return 1;
  }
  g_fs = hdfsConnectAsUser("default", 0, user_info->pw_name);
  if (g_fs == nullptr)
  {
    fprintf(stderr, "Failed to connect to HDFS: %s (errno=%d).\n", strerror(errno), errno);
    return 1;
  }

  struct cvmcache_callbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.cvmcache_chrefcnt = nullptr;
  callbacks.cvmcache_obj_info = nullptr;
  callbacks.cvmcache_pread = hdfs_pread;
  callbacks.cvmcache_start_txn = hdfs_start_txn;
  callbacks.cvmcache_write_txn = hdfs_write_txn;
  callbacks.cvmcache_commit_txn = hdfs_commit_txn;
  callbacks.cvmcache_abort_txn = hdfs_abort_txn;
  callbacks.cvmcache_info = nullptr;
  callbacks.cvmcache_listing_begin = nullptr;
  callbacks.cvmcache_listing_next = nullptr;
  callbacks.cvmcache_listing_end = nullptr;
  callbacks.capabilities = CVMCACHE_CAP_WRITE;

  ctx = cvmcache_init(&callbacks);
  assert(ctx != NULL);
  //int part_size = cvmcache_max_object_size(ctx);
  int retval = cvmcache_listen(ctx, argv[2]);
  assert(retval);
  printf("Listening for cvmfs clients on %s\n", argv[2]);
  cvmcache_process_requests(ctx, 0);
  while (true) {
    sleep(1);
  }
  hdfsDisconnect(g_fs);
  return 0;
}
