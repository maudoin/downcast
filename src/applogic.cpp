#include "applogic.h"

#include "httpengine.h"

#include <codecvt>
#include <thread>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <assert.h>
#include <fmt/format.h>

AppLogic::AppLogic(std::string const& dbPath)
  : m_dbPath(dbPath)
  , m_storage(m_dbPath)
{
  auto storage = m_storage.lock();
  if(storage->isOpen())
  {
    m_podcasts = storage->readPodcasts();
  }
  else
  {
    Storage::initDefaultContent(dbPath);

    if(storage->open())
    {
      m_podcasts = storage->readPodcasts();
    }
  }
  setCurrentPodcastRowIndex(std::nullopt,
                            MediaStatus::New,
                            SetPodcastOption::FORCE_REFRESH);
}

//-----------------------------------------------------------------------------------
AppLogic::~AppLogic()
{
  if(m_work)
  {
    m_work->join();
  }
}
//-----------------------------------------------------------------------------------
void AppLogic::restart()
{
  std::optional<int> currentPodcastRowIndex;
  if(m_currentPodcastId)
  {
    auto it = std::find_if(m_podcasts.begin(), m_podcasts.end(), [&](PodcastCols const&p){return p.id==*m_currentPodcastId;});
    if(m_podcasts.end()!=it)
    {
      currentPodcastRowIndex = std::distance(m_podcasts.begin(), it);
    }
  }
  auto storage = m_storage.lock();
  m_podcasts = storage->readPodcasts();
  setCurrentPodcastRowIndex(currentPodcastRowIndex,
                            storage->getCurrentStatusFilter(),
                            SetPodcastOption::FORCE_REFRESH);
}
//-----------------------------------------------------------------------------------
void AppLogic::restartIf(int podcastId)
{
  if(m_currentPodcastId && *m_currentPodcastId!=podcastId)
  {
    return;
  }
  restart();;
}
//-----------------------------------------------------------------------------------
bool AppLogic::noPodcastFilter() const
{
  return !m_currentPodcastId;
}
//-----------------------------------------------------------------------------------
const char* AppLogic::podcastTitle(int i) const
{
  auto const& p{m_podcasts[i]};
  return p.title.empty()?p.link.c_str():p.title.c_str();
}
//-----------------------------------------------------------------------------------
bool AppLogic::setCurrentPodcastRowIndex(std::optional<std::optional<int>> const& i,
                                         std::optional<std::optional<MediaStatus>> const& status,
                                         SetPodcastOption option)
{
  bool changed = option == SetPodcastOption::FORCE_REFRESH;
  if(i)
  {
    std::optional<int> const& indexOrNone=*i;
    if(indexOrNone)
    {
      int index = *indexOrNone;
      if( index>=0 && index<m_podcasts.size() && m_currentPodcastId!=m_podcasts[index].id)
      {
        m_currentPodcastId = m_podcasts[index].id;
      }
    }
    else
    {
      m_currentPodcastId.reset();
    }

    changed = true;
  }
  if(status)
  {
    auto storage = m_storage.lock();
    storage->setFilter(*status);
    changed = true;
  }
  if(changed)
  {
    {
      auto storage = m_storage.lock();
      m_showCount = storage->queryEmissionCount(m_currentPodcastId);
    }
    //display cache
    m_displayedShow0to1RangeCache = ShowRangeDisplayCache();
    m_displayedShowRangeCache = ShowRangeDisplayCache();
    //selection
    m_selectedShowRanks.resize(m_showCount);
    m_lastSelectionRank = 0;
    std::fill(m_selectedShowRanks.begin(), m_selectedShowRanks.end(), false);
  }
  return false;
}
//-----------------------------------------------------------------------------------
bool AppLogic::isCurrentPodcast(int i) const
{
  return (i>=0 && i<m_podcasts.size() && m_currentPodcastId==m_podcasts[i].id);
}
//-----------------------------------------------------------------------------------
template <typename V>
void AppLogic::setShowSorting(V MediaViewCols::*structPointer, SortingOption sorting)
{
  {
    auto storage = m_storage.lock();
    storage->setSortingColumn(structPointer, sorting);
  }
  m_displayedShow0to1RangeCache = ShowRangeDisplayCache();
  m_displayedShowRangeCache = ShowRangeDisplayCache();
  m_lastSelectionRank = 0;
  std::fill(m_selectedShowRanks.begin(), m_selectedShowRanks.end(), false);
}
template void  AppLogic::setShowSorting(int MediaViewCols::*structPointer, SortingOption sorting);
template void  AppLogic::setShowSorting(std::string MediaViewCols::*structPointer, SortingOption sorting);
//-----------------------------------------------------------------------------------
std::vector<MediaViewCols>const& AppLogic::showsIn0to1RankRange()
{
  return showsInRankRange(m_displayedShow0to1RangeCache, 0, 1);
}
//-----------------------------------------------------------------------------------
std::vector<MediaViewCols>const& AppLogic::showsInRankRange(int iStart, int count)
{
  return showsInRankRange(m_displayedShowRangeCache, iStart, count);
}
//-----------------------------------------------------------------------------------
std::vector<MediaViewCols>const&
AppLogic::showsInRankRange(ShowRangeDisplayCache& cache, int iStart, int count)
{
  if(!m_busy &&
     (iStart!=cache.iStartLast || count != cache.countLast))
  {
    cache.iStartLast=iStart;
    cache.countLast=count;
    auto storage = m_storage.lock();
    cache.last = storage->emissions(m_currentPodcastId, iStart, count);
  }
  return cache.last;
}
/*
//-----------------------------------------------------------------------------------
std::pair<std::vector<MediaViewCols>::const_iterator,
          std::vector<MediaViewCols>::const_iterator>
AppLogic::showsInRankRange(int iStart, int count)
{

  auto& ref = m_displayedShowRangeCache.last;
  if(iStart==m_displayedShowRangeCache.iStartLast+m_displayedShowRangeCache.countLast)
  {
    //appended range case:
    m_displayedShowRangeCache.countLast+=count;
    auto const added = storage->emissions(m_currentPodcastId, iStart, count);
    std::copy(added.cbegin(), added.cend(), std::back_inserter(ref));
    return {ref.begin()+(m_displayedShowRangeCache.countLast-count), ref.end()};
  }
  int relativeStartFromCache = iStart - m_displayedShowRangeCache.iStartLast;
  int availCountFromStart = m_displayedShowRangeCache.countLast - relativeStartFromCache;
  if(relativeStartFromCache >= 0 && count <= availCountFromStart)
  {
    //subrange case
    return {ref.begin()+relativeStartFromCache, ref.end()};
  }
  else if(!m_busy)
  {
    //new range case
    m_displayedShowRangeCache.iStartLast=iStart;
    m_displayedShowRangeCache.countLast=count;
    ref = storage->emissions(m_currentPodcastId, iStart, count);
    return {ref.begin(), ref.end()};
  }
  return {ref.end(), ref.end()};
}*/
//-----------------------------------------------------------------------------------
bool AppLogic::isShowRankSelected(int iRank) const
{
  return m_selectedShowRanks[iRank];
}
//-----------------------------------------------------------------------------------
void AppLogic::showSelection(int rank, bool multiSelection, bool setRange)
{
  if(multiSelection && !setRange)
  {
    //toggle
    m_selectedShowRanks[rank]=!m_selectedShowRanks[rank];
  }
  if(!multiSelection && !setRange)
  {
    //single sel (can toggle anyway)
    bool const prev = m_selectedShowRanks[rank];
    std::fill(m_selectedShowRanks.begin(), m_selectedShowRanks.end(), false);
    m_selectedShowRanks[rank]=!prev;
  }
  if(setRange && m_lastSelectionRank != rank)
  {
    //range
    int const min = std::min(m_lastSelectionRank.load(), rank);
    int const max = std::max(m_lastSelectionRank.load(), rank);
    std::fill(m_selectedShowRanks.begin()+min, m_selectedShowRanks.begin()+max, true);
  }
  m_lastSelectionRank = rank;
}
//-----------------------------------------------------------------------------------
void AppLogic::selectShowRange(int rankFirst, int rankLast, bool addToSelection)
{
  if(!addToSelection)
  {
    std::fill(m_selectedShowRanks.begin(), m_selectedShowRanks.end(), false);
  }
  std::fill(m_selectedShowRanks.begin()+rankFirst, m_selectedShowRanks.begin()+rankLast+1, true);
}
//-----------------------------------------------------------------------------------
bool AppLogic::anySelection()const
{
  return std::find(m_selectedShowRanks.begin(), m_selectedShowRanks.end(), true)!=m_selectedShowRanks.end();
}
//-----------------------------------------------------------------------------------
void AppLogic::parse(std::string const& data,
                     int const podcastId, bool updatePodcast,
                     std::string const& url)
{
  auto nodeValueStr=[](const char* node)
  {
    return node?std::string(node):std::string();
  };
  auto storage = m_storage.lock();
  SqlInserter<MediaCols> inserter = storage->buildInserter<MediaCols>(SqlInserterMode::ADD);

  std::optional<PodcastCols> updatePodcastContent;
  rssparser(data, [&](const char* title, const char* description, const char* imageUrl)
  {
    if(updatePodcast)
    {
      updatePodcastContent.emplace();
      updatePodcastContent->id=podcastId;
      updatePodcastContent->link=url;
      updatePodcastContent->title=nodeValueStr(title);
      updatePodcastContent->summary=nodeValueStr(description);
      updatePodcastContent->image_url=nodeValueStr(imageUrl);
    }
  },
  [&](const char* itemTitle, const char* itemDesc, const char* itemEnclosureUrl,
      int publicationDate, int itemDuration, const char* itemThumbnailUrl)
  {
    MediaCols media;
    media.podcast_id = podcastId;
    media.title=nodeValueStr(itemTitle);
    media.summary=nodeValueStr(itemDesc);
    media.url = nodeValueStr(itemEnclosureUrl);
    media.publicationDate = publicationDate;
    media.duration = itemDuration;
    media.image_url = nodeValueStr(itemThumbnailUrl);
    media.status = Status::NEW;

    inserter.insert(media);
  }
  );
  setLastError(inserter.exec());
  if(updatePodcastContent)
  {
    SqlInserter<PodcastCols> inserter = storage->buildInserter<PodcastCols>(SqlInserterMode::UPDATE);
    inserter.insert(*updatePodcastContent);
    setLastError(inserter.exec());
  }
}
//-----------------------------------------------------------------------------------
void AppLogic::refresh(std::string const& url, int const podcastId, bool updatePodcast)
{
  if(auto body=HttpEngine::get(url))
  {
    parse(*body, podcastId, updatePodcast, url);
  }
}
//-----------------------------------------------------------------------------------
std::pair<std::optional<PodcastCols>, std::vector<std::string>>
AppLogic::queryPodcast(std::string const& url)const
{
  std::optional<PodcastCols> podcast;
  std::vector<std::string> shows;
  if(auto body=HttpEngine::get(url))
  {
    auto nodeValueStr=[](const char* node)
    {
      return node?std::string(node):std::string();
    };
    rssparser(*body, [&](const char* title, const char* description, const char* imageUrl)
    {
      podcast.emplace();
      podcast->id=0;
      podcast->link=url;
      podcast->title=nodeValueStr(title);
      podcast->summary=nodeValueStr(description);
      podcast->image_url=nodeValueStr(imageUrl);

      if(auto imageBody=HttpEngine::get(imageUrl))
      {
        podcast->image_blob.resize(imageBody->size());
        memcpy(podcast->image_blob.data(),
               imageBody->data(),
               imageBody->size());
      }
    },
    [&](const char* itemTitle, const char* itemDesc, const char* itemEnclosureUrl,
        int publicationDate, int itemDuration, const char* itemThumbnailUrl)
    {
      shows.emplace_back(nodeValueStr(itemTitle));
    });
  }
  return {std::move(podcast), std::move(shows)};
}
//-----------------------------------------------------------------------------------
template<typename OP>
void AppLogic::writeStorage(OP const& writeOp)
{
  m_busy=true;
  if(m_work)
  {
    m_work->join();
  }
  m_work.reset();
  m_work = std::make_unique<std::thread>([this, writeOp]{
    try {
      writeOp();
    }
    catch (...) {
      abort();
    }
    restart();
    m_busy = false;
  });

}
//-----------------------------------------------------------------------------------
void AppLogic::refreshAll()
{
  writeStorage([&]()
  {
    for(auto&& podcast:m_podcasts)
    {
      refresh(podcast.link, podcast.id, podcast.title.empty());
    }
  });

}
//-----------------------------------------------------------------------------------
bool AppLogic::insertPodcast(PodcastCols const& podcast, InsertPodcastMode mode)
{
  SqlInserterMode sqlMode;
  switch(mode)
  {
  case InsertPodcastMode::ADD:
    sqlMode=SqlInserterMode::ADD;
    break;
  case InsertPodcastMode::UPDATE:
    sqlMode=SqlInserterMode::UPDATE;
    break;
  }
  writeStorage([podcast, sqlMode, this]()
  {
    auto storage = m_storage.lock();
    SqlInserter<PodcastCols> inserter = storage->buildInserter<PodcastCols>(sqlMode);
    inserter.insert(podcast);
    setLastError(inserter.exec());
  });
  return true;
}
//-----------------------------------------------------------------------------------
void AppLogic::refreshCurrentPodcast()
{
  std::vector<PodcastCols> podcasts = retrieveCurrentPodcastsCopy();
  if(!podcasts.empty())
  {
    writeStorage([this, podcasts]()
    {
      for(auto const& podcast:podcasts)
      {
        refresh(podcast.link, podcast.id, podcast.title.empty());
      }
    });
  }
}
//-----------------------------------------------------------------------------------
void AppLogic::refreshPodcastAtIndex(int index)
{
  writeStorage([this, index]()
  {
    if(index>=0 && index<m_podcasts.size())
    {
      const PodcastCols& podcast = m_podcasts[index];
      refresh(podcast.link, podcast.id, podcast.title.empty());
    }
  });

}
//-----------------------------------------------------------------------------------
void AppLogic::deletePodcastAtIndex(int index)
{
  writeStorage([this, index]()
  {
    if(index>=0 && index<m_podcasts.size())
    {
      auto storage(m_storage.lock());
      storage->deletePodcast(m_podcasts[index].id);
    }
  });

}
//-----------------------------------------------------------------------------------
void AppLogic::setSelectedShowsStatus(Status status)
{
  auto ids = [&]{
    auto storage = m_storage.lock();
    return storage->getShowsIds(m_currentPodcastId, m_selectedShowRanks);
  }();
  writeStorage([this, ids, status](){
    auto storage = m_storage.lock();
    storage->setShowsStatus(ids, status);
  });
}
//-----------------------------------------------------------------------------------
bool AppLogic::isBusy()const{return m_busy;}
//-----------------------------------------------------------------------------------
int AppLogic::showCount()const{return m_showCount;}
//-----------------------------------------------------------------------------------
int AppLogic::podcastCount()const{return m_podcasts.size();}
//-----------------------------------------------------------------------------------
PodcastCols const& AppLogic::podcast(std::size_t const i)const{return m_podcasts[i];}
//-----------------------------------------------------------------------------------
bool AppLogic::isStatusActive(std::optional<AppLogic::MediaStatus> const& status)const
{
  auto storage = m_storage.lock();
  return status == storage->getCurrentStatusFilter();
}
//-----------------------------------------------------------------------------------
bool AppLogic::isDownloading()const
{
  return m_downloading;
}
//-----------------------------------------------------------------------------------
AppLogic::DownloadInfo AppLogic::getCurrentDownloadProgress()const
{
  auto locked=m_downloadInfo.readLock();
  return *locked;
}
//-----------------------------------------------------------------------------------
namespace
{
template <typename C>
std::basic_string<C> cleanupFilenameCharacters(std::basic_string<C> const& str)
{
  static const std::basic_string<C> illegalChars = "*[]\\/:?\"<>|";
  static const C replacementChar = '_';
  std::basic_string<C> cleaned(str);
  for (auto &c:cleaned)
  {
    if(illegalChars.find(c) != std::string::npos)
    {
      c = replacementChar;
    }
  }
  return cleaned;
}
template <typename C>
std::basic_string<C> ext(std::basic_string<C> const& path,
                         std::string const& defaultExt)
{
  static constexpr auto npos =std::basic_string<C>::npos;
  auto startPos = path.find_last_of(".");
  auto end = path.find_first_of("?", startPos);
  auto endPos = end == npos ? path.size() : end ;
  return (startPos == npos) ? defaultExt : path.substr(startPos, endPos-startPos);
}
template <typename C = std::filesystem::path::value_type>
std::filesystem::path toPath( std::string const& s )
{
  if constexpr(std::is_same_v<C, wchar_t>)
  {
    return {std::wstring_convert< std::codecvt_utf8_utf16<wchar_t> >().from_bytes( s.c_str() )};
  }
  else
  {
    return {s};
  }
}
inline auto to_time_t(int const publicationDate)
{
  const auto t =
      std::chrono::time_point<std::chrono::system_clock>{} +
      std::chrono::seconds(publicationDate);
  return std::chrono::system_clock::to_time_t(t);
}
void backupOrRemove(std::filesystem::path const& path, std::filesystem::path const& pathBackup)
{
  std::error_code ec;
  std::filesystem::rename(path, pathBackup, ec);
  if(ec)
  {
    std::filesystem::remove(path, ec);
  }
}
}
//-----------------------------------------------------------------------------------
std::string AppLogic::computeBaseFilename(const char* pattern,
                                          std::string const& title,
                                          int const publicationDate)
{

  auto in_time_t = to_time_t(publicationDate);
  std::ostringstream dateOss;
  dateOss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d");
  try
  {
    if(strlen(pattern))
    {
      return cleanupFilenameCharacters(
            fmt::format(fmt::runtime(pattern),
                        fmt::arg("date", dateOss.str()),
                        fmt::arg("title", title)));
    }
  }
  catch (...)
  {
  }
  dateOss<<"-"<<cleanupFilenameCharacters(title);
  return dateOss.str();
}
//-----------------------------------------------------------------------------------
std::vector<PodcastCols> AppLogic::retrieveCurrentPodcastsCopy()const
{
  if(m_currentPodcastId)
  {
    //not really efficient for a lot of podcasts
    //but storage query is overkill for few podcasts...
    //(storing current podcast rank would be state duplication over id)
    //(storing current podcast pointer would be tedious when data changes)
    auto it=std::find_if(m_podcasts.begin(), m_podcasts.end(), [&](auto const&p){return p.id==m_currentPodcastId;});
    if(it!=m_podcasts.end())
    {
      return {*it};
    }
    else
    {
      return {};
    }
  }
  return m_podcasts;

}

//-----------------------------------------------------------------------------------
void AppLogic::abortDownload()
{
  m_abortDownload.store(true);
}

//-----------------------------------------------------------------------------------
void AppLogic::startDownload()
{
  std::vector<PodcastCols> podcasts = retrieveCurrentPodcastsCopy();
  if(!podcasts.empty())
  {
    resetLastError();
    if(m_work)
    {
      m_work->join();
    }
    m_work.reset();
    m_work = std::make_unique<std::thread>([podcasts, this]
    {
      m_abortDownload.store(false);
      try
      {
        m_downloading = true;
        for(auto const& podcast:podcasts)
        {
          if(m_abortDownload.load())
          {
            break;
          }

          std::filesystem::path const targetFolder(podcast.target);

          std::vector<MediaCols> downloads = [&]{
            auto storage = m_storage.lock();
            return storage->queryDownloads(podcast.id);
          }();
          {
            auto locked=m_downloadInfo.writeLock();
            locked->totalFiles = downloads.size();
          }
          int nFile=0;
          for(MediaCols const& show:downloads)
          {
            if(m_abortDownload.load())
            {
              break;
            }
            try
            {
              //output file path
              std::string filenameRaw = computeBaseFilename(podcast.pattern.c_str(),
                                                            show.title, show.publicationDate)
                                        + ext(show.url, ".mp3");
              std::filesystem::path tempPath=targetFolder
                                             / toPath<>(filenameRaw+".running");

              {
                auto locked=m_downloadInfo.writeLock();
                locked->currentLabel = filenameRaw;
                locked->currentProgress = 0;
                locked->currentTotal = 1;
                locked->currentFile = nFile++;
              };
              //output file and callbacks
              auto start = std::chrono::system_clock::now();
              auto out = std::make_unique<std::ofstream>(tempPath, std::ios_base::trunc|std::ios_base::binary|std::ios_base::out);
              if(!out)
              {
                setLastError("Cannot open target file " + tempPath.string());
                continue;
              }
              auto const receiver = [&](const char *data, size_t data_length)
              {
                out->write(data, data_length);
                return true;
              };
              auto const restart = [&]()
              {
                out.reset();
                out = std::make_unique<std::ofstream>(tempPath, std::ios_base::trunc|std::ios_base::binary|std::ios_base::out);
              };
              auto knownTotal=0;
              auto const progressHandler = [&](uint64_t current, uint64_t total)
              {
                if(total!=knownTotal && current!=total)
                {
                  auto prev = out->tellp();
                  out->seekp(total-1);
                  out->put(0);
                  out->seekp(prev);
                  knownTotal=total;
                }
                auto locked=m_downloadInfo.writeLock();
                locked->currentProgress = current;
                locked->currentTotal = total;
                auto const end=std::chrono::system_clock::now();
                auto const duration = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
                if(duration.count()!=0)
                {
                  locked->bytesPerSec =
                      (current * 1000) /
                      duration.count();
                }
                return true;
              };

              static constexpr int initialTryCount = 2;
              auto result = HttpEngine::get(show.url,
                                            receiver,
                                            restart,
                                            progressHandler,
                                            initialTryCount);
              if(result)
              {
                auto showId=show.id;
                {
                  auto storage = m_storage.lock();
                  storage->setShowsStatus({showId}, Status::DONE);
                }
                out->close();
                std::filesystem::path path=targetFolder
                                           / toPath<>(filenameRaw);
                backupOrRemove(path, targetFolder
                               / toPath<>(filenameRaw+".bak"));


                std::filesystem::rename(tempPath, path);
                resetLastError();
                restartIf(podcast.id);
              }
              else
              {
                out.reset();
                std::filesystem::remove(tempPath);
                setLastError(result.error);
              }
            }
            catch(std::exception &e)
            {
              setLastError(e.what());
            }
          }
        }

      }
      catch(std::exception &e)
      {
        setLastError(e.what());
      }
      m_abortDownload.store(false);
      m_downloading=false;
    });
  }
}

//-----------------------------------------------------------------------------------
void AppLogic::resetLastError()
{
  auto error = m_lastError.writeLock();
  error->clear();
}
//-----------------------------------------------------------------------------------
void AppLogic::setLastError(std::string const& msg)
{
  auto error = m_lastError.writeLock();
  *error = msg;
}
//-----------------------------------------------------------------------------------
void AppLogic::setLastError(Storage::Result const& result)
{
  if(result)
  {
    resetLastError();
  }
  else
  {
    setLastError(result.error);
  }
}
//-----------------------------------------------------------------------------------
std::string const* AppLogic::lastError()const
{
   auto error = m_lastError.readLock();
   return error->empty()?nullptr:&*error;
}
