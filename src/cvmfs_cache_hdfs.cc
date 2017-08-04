
#include <map>
#include <string>
#include <sstream>
#include <cstdint>
#include <cinttypes>
#include <ctime>

#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <pwd.h>
#include <stdarg.h>
#include <signal.h>

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

/**
 * Allows us to use a cvmcache_hash in (hash) maps.
 *
 * From implementation in cvmfs/cache_plugin/cvmfs_cache_ram.cc
 */
struct ComparableHash {
  ComparableHash() { }
  ComparableHash(const struct cvmcache_hash &h) : hash(h) { }
  bool operator ==(const ComparableHash &other) const {
    return cvmcache_hash_cmp(const_cast<cvmcache_hash *>(&(this->hash)),
                             const_cast<cvmcache_hash *>(&(other.hash))) == 0;
  }
  bool operator !=(const ComparableHash &other) const {
    return cvmcache_hash_cmp(const_cast<cvmcache_hash *>(&(this->hash)),
                             const_cast<cvmcache_hash *>(&(other.hash))) != 0;
  }
  bool operator <(const ComparableHash &other) const {
    return cvmcache_hash_cmp(const_cast<cvmcache_hash *>(&(this->hash)),
                             const_cast<cvmcache_hash *>(&(other.hash))) < 0;
  }
  bool operator >(const ComparableHash &other) const {
    return cvmcache_hash_cmp(const_cast<cvmcache_hash *>(&(this->hash)),
                             const_cast<cvmcache_hash *>(&(other.hash))) > 0;
  }

  struct cvmcache_hash hash;
};

struct FileHandle {
  explicit FileHandle(hdfsFile fp, int64_t ref) :ref_(ref), fp_(fp) {}

  int64_t ref_ = 0;
  hdfsFile fp_ = nullptr;
};

std::map<ComparableHash, FileHandle> open_files;


static bool g_logreload = true;
static int g_logfd = -1;
static std::string g_logfname;
static void log(const char *format, ... )
  __attribute__((format (printf, 1, 2)));

static void
reload_log() {
  if (g_logfname.empty()) {
    return;
  }
  g_logreload = false;

  int fd = open(g_logfname.c_str(), O_APPEND|O_CREAT|O_WRONLY, 0640);
  if (fd == -1) {
    log("Failed to open new log file descriptor: %s.", strerror(errno));
    return;
  }
  close(g_logfd);
  g_logfd = fd;
  dup2(g_logfd, 1);
  dup2(g_logfd, 2);
  log("Logfile successfully reloaded.");
}

static void
sighup_handler(int /*signal*/) {
  g_logreload = true;
}

static void
log(const char * format, ... ) {
  if (g_logreload) reload_log();

  static const int TIMESTAMP_BUF_SIZE = 256;
  char timestamp_buf[TIMESTAMP_BUF_SIZE];
  time_t t;
  struct tm *tp;
  t = time(NULL);
  tp = localtime(&t);
  if (tp) {
    if (strftime(timestamp_buf, TIMESTAMP_BUF_SIZE, "%c", tp)) {
      printf("[%s] (pid:%d): ", timestamp_buf, getpid());
    }
  }
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  printf("\n");
  fflush(stdout);  // The embeded Java JVM seems to prevent flushing of printf unless we do this.
}

static std::string hdfs_file_name(const struct cvmcache_hash &id)
{
  char *human_hash = cvmcache_hash_print(const_cast<struct cvmcache_hash*>(&id));
  size_t length = strlen(human_hash);
  std::stringstream ss;
  char *tmp_path = static_cast<char*>(malloc(length + 2));
  memcpy(tmp_path, human_hash, 2);
  tmp_path[2] = '/';
  memcpy(tmp_path+3, human_hash+2, length-2);
  tmp_path[length+1] = '\0';
  ss << g_hdfs_base << "/" << tmp_path;
  free(tmp_path);
  free(human_hash);
  return ss.str();
}

static int hdfs_obj_info(struct cvmcache_hash *id, struct cvmcache_object_info *info)
{
  std::string fname = hdfs_file_name(*id);
  log("Getting object info for %s.", fname.c_str());


  hdfsFileInfo * hinfo = hdfsGetPathInfo(g_fs, fname.c_str());
  if (hinfo == nullptr)
  {
    return CVMCACHE_STATUS_NOENTRY;
  }
  info->size = hinfo->mSize;
  hdfsFreeFileInfo(hinfo, 1);
  return CVMCACHE_STATUS_OK;
}

static int hdfs_chrefcnt(struct cvmcache_hash *id, int32_t change_by)
{
  if (change_by == 0)
  {
    return CVMCACHE_STATUS_OK;
  }
  std::string fname = hdfs_file_name(*id);
  log("Changing ref count for %s by %d.", fname.c_str(), change_by);

  auto iter = open_files.find(*id);
  if (iter == open_files.end())
  {
    if (change_by < 0) {return CVMCACHE_STATUS_BADCOUNT;}

    hdfsFile fp = hdfsOpenFile(g_fs, fname.c_str(), O_RDONLY, 0, 0, 0);
    if (fp == nullptr)
    {
      return CVMCACHE_STATUS_NOENTRY;
    }
    open_files.emplace(*id, FileHandle(fp, change_by));
  }

  auto &handle = iter->second ;
  handle.ref_ += change_by;

  if (handle.ref_ < 0)
  {
    hdfsCloseFile(g_fs, handle.fp_);
    open_files.erase(iter);
    return CVMCACHE_STATUS_BADCOUNT;
  }
  return CVMCACHE_STATUS_OK;
}

static int hdfs_pread(struct cvmcache_hash *id,
                    uint64_t offset,
                    uint32_t *size,
                    unsigned char *buffer)
{
  auto iter = open_files.find(*id);
  if (iter == open_files.end())
  {
    return CVMCACHE_STATUS_BADCOUNT;
  }
  hdfsFile fp = iter->second.fp_;

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
  log("Starting transaction on cache file %s.", fname.c_str());
  std::string new_fname = fname + ".inprogress";

  hdfsFile fp = hdfsOpenFile(g_fs, fname.c_str(), O_RDONLY, 0, 0, 0);
  if (fp != nullptr)
  {
    hdfsCloseFile(g_fs, fp);
    return CVMCACHE_STATUS_OK;
  }
  fp = hdfsOpenFile(g_fs, new_fname.c_str(), O_WRONLY, 0, 1, 0);
  if (fp == nullptr)
  {
    log("Failed to open a new cache file (%s) for writing.", new_fname.c_str());
    return CVMCACHE_STATUS_IOERR;
  }

  TxnTransient txn(*id, fp);
  transactions[txn_id] = txn;
  return CVMCACHE_STATUS_OK;
}


static int hdfs_write_txn(uint64_t txn_id,
                 unsigned char *buffer,
                 uint32_t size)
{
  TxnTransient &txn = transactions[txn_id];

  log("Writing %" PRIu32 " bytes to txn ID %" PRIu64, size, txn_id);
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
  log("commit txn ID %" PRIu64 ", size %" PRIu64, txn_id, txn.size_);

  if (hdfsHFlush(g_fs, txn.fp_) == -1)
  {
    log("Commit txn ID %" PRIu64 " failed: %s (errno=%d)", txn_id, strerror(errno), errno);
    hdfsCloseFile(g_fs, txn.fp_);
    return CVMCACHE_STATUS_IOERR;
  }
  if (-1 == hdfsCloseFile(g_fs, txn.fp_))
  {
    log("Commit txn ID %" PRIu64 " failed: %s (errno=%d)", txn_id, strerror(errno), errno);
    return CVMCACHE_STATUS_IOERR;
  }
  std::string fname = hdfs_file_name(txn.id_);
  std::string new_fname = fname + ".inprogress";

  if ((-1 == hdfsRename(g_fs, new_fname.c_str(), fname.c_str())) &&  (-1 == hdfsExists(g_fs, fname.c_str())))
  {
    log("Commit of txn ID %" PRIu64 ", filename %s failed: %s (errno=%d)", txn_id, fname.c_str(), strerror(errno), errno);
    return CVMCACHE_STATUS_IOERR;
  }

  log("Commit of txn ID %" PRIu64 ", filename %s was successful.", txn_id, fname.c_str());
  transactions.erase(txn_id);

  auto iter = open_files.find(txn.id_);
  if (iter == open_files.end())
  {
    hdfsFile fp = hdfsOpenFile(g_fs, fname.c_str(), O_RDONLY, 0, 0, 0);
    if (fp == nullptr)
    {
      return CVMCACHE_STATUS_NOENTRY;
    }
    open_files.emplace(txn.id_, FileHandle(fp, 1));
  } else {
    // Race!  I think this can occur if multiple repos that contain the same data share
    // a cache plugin.
    iter->second.ref_ += 1;
  }

  return CVMCACHE_STATUS_OK;
}

static int hdfs_abort_txn(uint64_t txn_id) {
  log("Abort transaction %" PRIu64, txn_id);

  TxnTransient &txn = transactions[txn_id];

  if (-1 == hdfsCloseFile(g_fs, txn.fp_))
  {
    log("Abort txn ID %" PRIu64 " failed: %s (errno=%d)", txn_id, strerror(errno), errno);
    transactions.erase(txn_id);
    return CVMCACHE_STATUS_IOERR;
  }
  std::string fname = hdfs_file_name(txn.id_) + ".inprogress";
  if (-1 == hdfsDelete(g_fs, fname.c_str(), 0))
  {
    log("Delete failed for %s: %s (errno=%d)", fname.c_str(), strerror(errno), errno);
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

  int fd = open("/dev/zero", O_RDONLY);
  dup2(fd, 0);
  g_logfname = (argc > 3) ? argv[3] : "/dev/null";
  reload_log();
  struct sigaction sa;
  sa.sa_handler = &sighup_handler;
  sa.sa_flags = SA_RESTART;
  sigfillset(&sa.sa_mask);
  sigaction(SIGHUP, &sa, NULL);

  log("Starting HDFS cache plugin.");
  log("Will use HDFS directory %s", argv[1]);
  g_hdfs_base = argv[1];

  int euid = geteuid();
  if (euid == 0) {
    log("Cowardly refusing to run the cache plugin as root; will switch to user `cvmfs` if available.");
    struct passwd *cvmfs_user_info = getpwnam("cvmfs");
    if (cvmfs_user_info == nullptr)
      return 5;
    if (-1 == setgid(cvmfs_user_info->pw_gid)) {
       log("Failed to drop GID privileges: %s.", strerror(errno));
       return 6;
    }
    if (-1 == setuid(cvmfs_user_info->pw_uid)) {
       log("Failed to drop UID privileges: %s.", strerror(errno));
       return 7;
    }
    euid = cvmfs_user_info->pw_uid;
  }
  struct passwd * user_info = getpwuid(euid);
  if ((user_info == nullptr) || (user_info->pw_name == nullptr))
  {
    log("Failed to determine current username: %s (errno=%d).", strerror(errno), errno);
    return 2;
  }
  g_fs = hdfsConnectAsUser("default", 0, user_info->pw_name);
  if (g_fs == nullptr)
  {
    log("Failed to connect to HDFS: %s (errno=%d).", strerror(errno), errno);
    return 3;
  }

  // Re-install signal handler after Java starts up; Java stole it from us!
  sigaction(SIGHUP, &sa, NULL);

  struct cvmcache_callbacks callbacks;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.cvmcache_chrefcnt = hdfs_chrefcnt;
  callbacks.cvmcache_obj_info = hdfs_obj_info;
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

  int retval = cvmcache_listen(ctx, argv[2]);
  if (!retval) {
    hdfsDisconnect(g_fs);
    return 4;
  }
  log("Listening for cvmfs clients on %s.", argv[2]);
  cvmcache_process_requests(ctx, 0);

  cvmcache_wait_for(ctx);
  log("  ... good bye");
  cvmcache_cleanup_global();

  hdfsDisconnect(g_fs);
  return 0;
}
