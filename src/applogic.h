#pragma once

#include "storage.h"
#include "rssparser.h"
#include "MutexedData.h"
#include "SharedMutexedData.h"
#include "rssparser.h"

#include <vector>
#include <string>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <thread>

class AppLogic
{
  struct ShowRangeDisplayCache
  {
    int iStartLast=-1, countLast=0;
    std::vector<MediaViewCols> last;
  };
public:
  using MediaStatus = Storage::MediaStatus;

  AppLogic(std::string const& dbPath);
  ~AppLogic();

  bool noPodcastFilter() const;
  const char* podcastTitle(int i) const;
  enum class SetPodcastOption{NONE, FORCE_REFRESH};
  bool setCurrentPodcastRowIndex(std::optional<std::optional<int>> const& i,
                         std::optional<std::optional<MediaStatus>> const& status,
                         SetPodcastOption option = SetPodcastOption::NONE);
  bool isCurrentPodcast(int i) const;

  using SortingOption = Storage::SortingOption;
  template <typename V = int>
  void setShowSorting(V MediaViewCols::*structPointer, SortingOption sorting=SortingOption::ASCENDING);

  std::vector<MediaViewCols>const& showsInRankRange(int iStart, int count);
  std::vector<MediaViewCols>const& showsIn0to1RankRange();
  bool isShowRankSelected(int i) const;
  void showSelection(int rank, bool multiSelection, bool setRange);
  void selectShowRange(int rankFirst, int rankLast, bool addToSelection);
  bool anySelection()const;

  void setSelectedShowsStatus(Status status);

  void refreshAll();
  void refreshCurrentPodcast();
  void refreshPodcastAtIndex(int index);

  void deletePodcastAtIndex(int index);

  bool isBusy()const;
  int showCount()const;
  int podcastCount()const;
  PodcastCols const& podcast(std::size_t const i)const;
  bool isStatusActive(std::optional<AppLogic::MediaStatus> const& status)const;

  bool isDownloading()const;
  void abortDownload();
  void startDownload();
  struct DownloadInfo
  {
    int currentFile=0;
    int totalFiles=0;
    std::string currentLabel;
    int currentProgress=0;
    int currentTotal=0;
    int bytesPerSec=0;
  };
  //copy for thread safety
  DownloadInfo getCurrentDownloadProgress()const;

  std::pair<std::optional<PodcastCols>, std::vector<std::string>>
  queryPodcast(std::string const& url)const;

  enum class InsertPodcastMode{ADD, UPDATE};
  bool insertPodcast(PodcastCols const& podcast, InsertPodcastMode mode);

  std::string const* lastError()const;
private:
  void resetLastError();
  void setLastError(std::string const& msg);
  void setLastError(Storage::Result const& result);

  std::vector<MediaViewCols>const& showsInRankRange(ShowRangeDisplayCache& cache, int iStart, int count);
  const PodcastCols* retrieveCurrentPodcast()const;
  std::vector<PodcastCols> retrieveCurrentPodcastsCopy()const;
  template<typename OP>
  void writeStorage(OP const& writeOp);
  void restart();
  void restartIf(int podcastId);
  void parse(std::string const& data,
             int const podcastId, bool updatePodcast,
             std::string const& url);
  static std::pair<std::string, std::string> splitHostRessource(std::string const& url);
  void refresh(std::string const& url,
               int const podcastId, bool updatePodcast);


  std::string const m_dbPath;
  std::atomic_bool m_busy = false;
  MutexedData<Storage> m_storage;

  //display mutex?
  std::vector<PodcastCols> m_podcasts;

  //display mutex?
  std::optional<int> m_currentPodcastId = std::nullopt;
  std::atomic_int m_showCount = 0;
  ShowRangeDisplayCache m_displayedShow0to1RangeCache, m_displayedShowRangeCache;

  //selection mutex?
  std::atomic_int m_lastSelectionRank=-1;
  std::vector<bool> m_selectedShowRanks;

  std::atomic_bool m_downloading = false;
  SharedMutexedData<DownloadInfo> m_downloadInfo;
  SharedMutexedData<std::string> m_lastError;
  std::atomic_bool m_abortDownload = false;
  std::unique_ptr<std::thread> m_work;
};
