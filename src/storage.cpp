
#include "storage.h"
#include "sqliteengine_t.h"
#include "rssparser.h"

#include <chrono>
#include <time.h>
#include <sys/time.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdio.h>

namespace  {

static std::string date2Str(int date)
{
  const auto t = std::chrono::time_point<std::chrono::system_clock>{}+std::chrono::seconds(date);
  auto in_time_t = std::chrono::system_clock::to_time_t(t);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %Hh");
  return ss.str();
}
static std::string duration2Str(int totalSec)
{
  std::ostringstream oss;
  oss << std::setfill('0');

  int const s = totalSec % 60;
  if(totalSec != s)
  {
    int const totalMin = (totalSec - s)/60;
    int const min = totalMin % 60;
    if(totalMin != min)
    {
      int const totalH = (totalMin - min)/60;
      oss << totalH << ':';
      oss << std::setw(2) << min << ':' << std::setw(2) << s;
    }
    else
    {
      oss << min << ':' << std::setw(2) << s << "mn";
    }
  }
  else
  {
    oss << s <<"s";
  }
  return oss.str();
}
}
//-----------------------------------------------------------------------------------
std::string MediaViewCols::dateStr()const
{
  return date2Str(date);
}
//-----------------------------------------------------------------------------------
std::string MediaViewCols::durationStr()const
{
  return duration2Str(duration);
}
//cid	name	type	notnull	dflt_value	pk
//0	id	INTEGER	0	null	1
//1	title	TEXT	0	null	0
//2	link	TEXT	0	null	0
//3	summary	TEXT	0	null	0
//4	image_url	TEXT	0	null	0
//5	image_blob	BLOB	0	null	0
//6	target	TEXT	0	null	0
namespace SQLPodcastCols
{
static const char* table = "podcast";

static const SqlInteger id{"id", SqlColKind::AUTO_PRIMARY_KEY};
static const SqlText title{"title"};
static const SqlText link{"link", SqlColKind::UNIQUE};
static const SqlText summary{"summary"};
static const SqlText image_url{"image_url"};
static const SqlBlob image_blob{"image_blob"};
static const SqlText target{"target"};
};

//-----------------------------------------------------------------------------------
template<>
const char* SQLiteTable<PodcastCols>(){return SQLPodcastCols::table;}
template<>
auto SQLiteColumns<PodcastCols>(){return std::make_tuple(
        map(SQLPodcastCols::id, &PodcastCols::id),
        map(SQLPodcastCols::title, &PodcastCols::title),
        map(SQLPodcastCols::link, &PodcastCols::link),
        map(SQLPodcastCols::summary, &PodcastCols::summary),
        map(SQLPodcastCols::image_url, &PodcastCols::image_url),
        map(SQLPodcastCols::image_blob, &PodcastCols::image_blob),
        map(SQLPodcastCols::target, &PodcastCols::target));}
//-----------------------------------------------------------------------------------

//cid	name	type	notnull	dflt_value	pk
//0	podcast_id	INTEGER	0	null	0
//1	id	INTEGER	0	null	1
//2	title	TEXT	0	null	0
//3	link	TEXT	0	null	0
//4	summary	TEXT	0	null	0
//5	image_url	TEXT	0	null	0
//6	image_blob	INTEGER	0	null	0
//7	publicationDate	BLOB	0	null	0
//8	duration	TEXT	0	null	0
//9	status	INTEGER	1	null	0
namespace SQLMediaCols
{
static const char* table = "media";

static const SqlInteger podcast_id{"podcast_id"} ;
static const SqlInteger id{"id", SqlColKind::AUTO_PRIMARY_KEY} ;
static const SqlText title{"title"} ;
static const SqlText link{"link", SqlColKind::UNIQUE} ;
static const SqlText summary{"summary"} ;
static const SqlText image_url{"image_url"} ;
static const SqlBlob image_blob{"image_blob"} ;
static const SqlInteger publicationDate{"publicationDate"} ;
static const SqlInteger duration{"duration"} ;
static const SqlInteger status{"status"};
};
//-----------------------------------------------------------------------------------
template<>
const char* SQLiteTable<MediaCols>(){return SQLMediaCols::table;}
template<>
auto SQLiteColumns<MediaCols>(){return std::make_tuple(
        map(SQLMediaCols::podcast_id, &MediaCols::podcast_id),
        map(SQLMediaCols::id, &MediaCols::id),
        map(SQLMediaCols::title, &MediaCols::title),
        map(SQLMediaCols::link, &MediaCols::url),
        map(SQLMediaCols::summary, &MediaCols::summary),
        map(SQLMediaCols::image_url, &MediaCols::image_url),
        map(SQLMediaCols::image_blob, &MediaCols::image_blob),
        map(SQLMediaCols::publicationDate, &MediaCols::publicationDate),
        map(SQLMediaCols::duration, &MediaCols::duration),
        map(SQLMediaCols::status, &MediaCols::status));}
//-----------------------------------------------------------------------------------
template<>
const char* SQLiteTable<MediaViewCols>(){return SQLMediaCols::table;}
template<>
auto SQLiteColumns<MediaViewCols>(){return std::make_tuple(
        map(SQLMediaCols::id, &MediaViewCols::id),
        map(SQLMediaCols::title, &MediaViewCols::title),
        map(SQLMediaCols::link, &MediaViewCols::url),
        map(SQLMediaCols::summary, &MediaViewCols::summary),
        map(SQLMediaCols::image_blob, &MediaViewCols::image_blob),
        map(SQLMediaCols::publicationDate, &MediaViewCols::date),
        map(SQLMediaCols::duration, &MediaViewCols::duration));}
//-----------------------------------------------------------------------------------
template class SqlInserter<MediaCols>;
template class SqlInserter<PodcastCols>;
//-----------------------------------------------------------------------------------

Storage::Storage(std::string const& dbPath)
  :m_engine(dbPath)
{
}
//-----------------------------------------------------------------------------------
std::vector<PodcastCols> Storage::readPodcasts()const
{
  return m_engine.readTable<PodcastCols>();
}
//-----------------------------------------------------------------------------------
bool Storage::setFilter(std::optional<MediaStatus> const& status)
{
  bool changed = m_mediaStatus!=status;
  m_mediaStatus=status;
  if(changed)
  {
    if(m_mediaStatus)
    {
      m_statusFilter.emplace( Filter{SQLMediaCols::status.name,std::to_string(*m_mediaStatus)});
    }
    else
    {
      m_statusFilter = std::nullopt;
    }
  }
  return changed;
}
//-----------------------------------------------------------------------------------
std::optional<Storage::MediaStatus> Storage::getCurrentStatusFilter()const
{
  return m_mediaStatus;
}
//-----------------------------------------------------------------------------------
std::vector<Storage::Filter> Storage::filters(int podcastId)const
{
  std::vector<Filter> filters{Filter{SQLMediaCols::podcast_id.name, std::to_string(podcastId)}};
  if(m_statusFilter)
  {
    filters.emplace_back(*m_statusFilter);
  }
  return filters;
}
//-----------------------------------------------------------------------------------
std::vector<Storage::Filter> Storage::filters(std::optional<int> podcastId)const
{
  if(podcastId)
  {
    return filters(*podcastId);
  }
  if(m_statusFilter)
  {
    return {*m_statusFilter};
  }
  return {};
}
//-----------------------------------------------------------------------------------
template <typename V>
void Storage::setSortingColumn(V MediaViewCols::*structPointer, SortingOption sorting)
{
  m_engine.setSortingColumn<MediaViewCols>(structPointer, sorting);
}
template void Storage::setSortingColumn(int MediaViewCols::*structPointer, SortingOption sorting);
template void Storage::setSortingColumn(std::string MediaViewCols::*structPointer, SortingOption sorting);
//-----------------------------------------------------------------------------------
template <typename V>
std::optional<Storage::SortingOption>
Storage::isSortingColumn(V MediaViewCols::*structPointer)
{
  return m_engine.isSortingColumn(structPointer);
}
template std::optional<Storage::SortingOption> Storage::isSortingColumn(int MediaViewCols::*structPointer);
template std::optional<Storage::SortingOption> Storage::isSortingColumn(std::string MediaViewCols::*structPointer);
//-----------------------------------------------------------------------------------
int Storage::queryEmissionCount(std::optional<int> podcastId)
{
  return m_engine.countTable(SQLMediaCols::table, filters(podcastId));
}
//-----------------------------------------------------------------------------------
std::vector<MediaViewCols> Storage::emissions(std::optional<int> podcastId, int iStart, int count)
{
  return m_engine.readTable<MediaViewCols>(filters(podcastId), m_engine.sorting(), iStart, count);
}
//-----------------------------------------------------------------------------------
std::vector<MediaCols> Storage::queryDownloads(std::optional<int> podcastId) const
{
  std::vector<Filter> filters;
  filters.reserve(podcastId?2:1);
  if(podcastId)
  {
    filters.emplace_back(Filter{SQLMediaCols::podcast_id.name, std::to_string(*podcastId)});
  };
  filters.emplace_back(Filter{SQLMediaCols::status.name, std::to_string(Status::QUEUED)});
  return m_engine.readTable<MediaCols>(filters);
}
//-----------------------------------------------------------------------------------
bool Storage::isOpen()const
{
  return m_engine;
}
//-----------------------------------------------------------------------------------
bool Storage::open()
{
  return m_engine.open(SQLiteStorageCreationEngine::Mode::ReadWrite);
}
//-----------------------------------------------------------------------------------
void Storage::close()
{
  m_engine.close();
}
//-----------------------------------------------------------------------------------
std::vector<int>
Storage::getShowsIds(std::optional<int> const podcastId, std::vector<bool>const& mask) const
{
  auto allIds = m_engine.readTable<ShowIds>(SQLMediaCols::table, std::make_tuple(
                                    map(SQLMediaCols::id, &ShowIds::id)),
                                  filters(podcastId), m_engine.sorting());
  std::vector<int> ids;
  if(allIds.size()==mask.size())
  {
    ids.reserve(std::count(mask.begin(), mask.end(), true));
    for(int i=0;i<mask.size();++i)
    {
      if(mask[i])
      {
        ids.push_back(allIds[i].id);
      }
    }
  }
  return ids;
}

//-----------------------------------------------------------------------------------
template <typename T>
SqlInserter<T> Storage::buildInserter(SqlInserterMode const mode)
{
  return m_engine.buildInserter<T>(mode);
}
template SqlInserter<MediaCols> Storage::buildInserter(SqlInserterMode mode);
template SqlInserter<PodcastCols> Storage::buildInserter(SqlInserterMode mode);

//-----------------------------------------------------------------------------------
Storage::Result
Storage::setShowsStatus(std::vector<int>const& ids, Status const status)
{
  return m_engine.updateSelectedValues(SQLMediaCols::table,
                                       SQLMediaCols::id, ids,
                                       SQLMediaCols::status, (int)status);
}
//-----------------------------------------------------------------------------------
Storage::Result
Storage::deletePodcast(int const podcastId)
{
  return m_engine.deleteRecords(SQLPodcastCols::table,SQLPodcastCols::id, {podcastId});
}
//-----------------------------------------------------------------------------------
Storage::Result Storage::initDefaultContent(const std::string &dbPath)
{
  SQLiteStorageCreationEngine engine(dbPath);
  if(engine)
  {
    auto res = engine.createTable<PodcastCols>();
    if(!res)
    {
      return res;
    }
    res = engine.createTable<MediaCols>(/*std::string(" FOREIGN KEY (podcast_id) REFERENCES ")+
                                                                       SQLiteTable<PodcastCols>()+"(id)"*/);
    if(!res)
    {
      return res;
    }

    SqlInserter<PodcastCols> inserter = engine.buildInserter<PodcastCols>(SqlInserterMode::ADD);
    inserter.insert({0, "", "http://www.rtl.fr/podcast/les-grosses-tetes.xml"});
    inserter.insert({0, "", "http://radiofrance-podcast.net/podcast09/rss_13942.xml"});
    res = inserter.exec();
    if(!res)
    {
      return res;
    }
  }
  return {};
}
//-----------------------------------------------------------------------------------
struct CastapodMediaCols
{
  int podcast_id;
  int id;
  std::string title;
  std::string url;
  std::string summary;
  std::string image_url;
  Blob image_blob;
  int publicationDate;
  std::string duration;
  int status;
};
template<>
const char* SQLiteTable<CastapodMediaCols>(){return SQLMediaCols::table;}
template<>
auto SQLiteColumns<CastapodMediaCols>(){return std::make_tuple(
        map(SQLMediaCols::podcast_id, &CastapodMediaCols::podcast_id),
        map(SQLMediaCols::id, &CastapodMediaCols::id),
        map(SQLMediaCols::title, &CastapodMediaCols::title),
        map(SQLMediaCols::link, &CastapodMediaCols::url),
        map(SQLMediaCols::summary, &CastapodMediaCols::summary),
        map(SQLMediaCols::image_url, &CastapodMediaCols::image_url),
        map(SQLMediaCols::image_blob, &CastapodMediaCols::image_blob),
        map(SQLMediaCols::publicationDate, &CastapodMediaCols::publicationDate),
        map(SqlText{"duration"}, &CastapodMediaCols::duration),
        map(SQLMediaCols::status, &CastapodMediaCols::status));}
//-----------------------------------------------------------------------------------
void Storage::convertCastapodStorage(const std::string &srcPath,
                                                 const std::string &tgtPath)
{
  SQLiteStorageViewEngine source(srcPath);
  if(source)
  {
    SQLiteStorageCreationEngine target(tgtPath);
    if(target)
    {
      target.createTable<PodcastCols>();
      target.createTable<MediaCols>(/*std::string(" FOREIGN KEY (podcast_id) REFERENCES ")+
                                                                                                     SQLiteTable<PodcastCols>()+"(id)"*/);

      {
        SqlInserter<PodcastCols> inserter = target.buildInserter<PodcastCols>(SqlInserterMode::UPDATE);
        source.readTable<PodcastCols>([&](PodcastCols&&p){inserter.insert(p);});
        inserter.exec();
      }

      {
        SqlInserter<MediaCols> inserter = target.buildInserter<MediaCols>(SqlInserterMode::UPDATE);
        source.readTable<CastapodMediaCols>([&](CastapodMediaCols&&p){
          MediaCols converted;
          converted.podcast_id      = p.podcast_id      ;
          converted.id              = p.id              ;
          converted.title           = p.title           ;
          converted.url             = p.url             ;
          converted.summary         = p.summary         ;
          converted.image_url       = p.image_url       ;
          converted.image_blob      = p.image_blob      ;
          converted.publicationDate = p.publicationDate ;
          converted.duration        = durationFromRSS(p.duration.c_str());
          converted.status          = p.status          ;
          inserter.insert(converted);
        });
        inserter.exec();
      }
    }
  }
}
