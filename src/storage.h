#pragma once

#include "sqliteengine.h"
#include <vector>
#include <optional>
#include <string>

//-----------------------------------------------------------------------------------
struct PodcastCols
{
  int id;
  std::string title;
  std::string link;
  std::string summary;
  std::string image_url;
  Blob image_blob;
  std::string target;
};
struct MediaViewCols
{
  int id;
  std::string title;
  std::string url;
  std::string summary;
  Blob image_blob;
  int date;
  int duration;
  std::string dateStr()const;
  std::string durationStr()const;

};
struct MediaCols
{
  int podcast_id;
  int id;
  std::string title;
  std::string url;
  std::string summary;
  std::string image_url;
  Blob image_blob;
  int publicationDate;
  int duration;
  int status;
};
struct ShowIds
{
  int id;
};
enum Status : int
{
    SKIPPED, NEW, QUEUED, DOWNLOADING, DONE
};
//-----------------------------------------------------------------------------------
class Storage
{
public:

  using Result = SQLiteStorageViewEngine::Result;
  using Filter = SQLiteStorageViewEngine::Filter;
  using SortingOption = SQLiteStorageViewEngine::SortingOption;

  Storage(std::string const& dbPath);

  std::vector<PodcastCols> readPodcasts()const;

  enum MediaStatus{Skipped=0, New=1, Queued=2, InProgress=3, Done=4};
  bool setFilter(std::optional<MediaStatus> const& status);
  std::optional<MediaStatus> getCurrentStatusFilter()const;

  template <typename V>
  void setSortingColumn(V MediaViewCols::*structPointer, SortingOption sorting);

  template <typename V>
  std::optional<SortingOption> isSortingColumn(V MediaViewCols::*structPointer);

  int queryEmissionCount(std::optional<int> podcastId);
  std::vector<MediaViewCols> emissions(std::optional<int> podcastId, int iStart, int count);

  std::vector<int> getShowsIds(std::optional<int> podcastId, std::vector<bool>const& mask) const;

  std::vector<MediaCols> queryDownloads(std::optional<int> podcastId) const;

  bool isOpen()const;
  bool open();
  void close();

  Result setShowsStatus(std::vector<int>const& ids, Status status);

  Result deletePodcast(int podcastId);

  static Result initDefaultContent(std::string const& dbPath);

  static void convertCastapodStorage(const std::string &srcPath,
                                     const std::string &tgtPath);

  template <typename T>
  SqlInserter<T> buildInserter(SqlInserterMode mode);
private:
  std::vector<Filter> filters(std::optional<int> podcastId)const;
  std::vector<Filter> filters(int podcastId)const;

  std::optional<MediaStatus> m_mediaStatus = std::nullopt;
  std::optional<Filter> m_statusFilter = std::nullopt;
  SQLiteStorageModificationEngine m_engine;
};
