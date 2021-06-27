#include "applogic.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"//under windows, before windows.h

#include "rapidxml.hpp"

#include <type_traits>
#include <vector>
#include <sstream>
#include <optional>
#include <string>
#include <numeric>
#include <tuple>
#include <functional>
#include <chrono>
#include <iomanip>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

namespace  {
inline char* strptime(const char* s,
               const char* f,
               struct tm* tm)
{
  // Isn't the C++ standard lib nice? std::get_time is defined such that its
  // format parameters are the exact same as strptime. Of course, we have to
  // create a string stream first, and imbue it with the current C locale, and
  // we also have to make sure we return the right things if it fails, or
  // if it succeeds, but this is still far simpler an implementation than any
  // of the versions in any of the C standard libraries.
  std::istringstream input(s);
  input.imbue(std::locale(setlocale(LC_ALL, nullptr)));
  input >> std::get_time(tm, f);
  if (input.fail()) {
    return nullptr;
  }
  return (char*)(s + input.tellg());
}

}

//-----------------------------------------------------------------------------------
int dateFromRSS(const char* pubDate)
{
  struct tm tm;
  strptime(pubDate, "%a, %d %b %Y %H:%M:%S %z", &tm);
  auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
  int date = std::chrono::duration_cast<std::chrono::seconds>
             (tp-std::chrono::time_point<std::chrono::system_clock>{}).count();
  //auto i=date2Str(date);
  return date;
}

//-----------------------------------------------------------------------------------
int durationFromRSS(const char* duration)
{
  if(strlen(duration)==0)
  {
    return 0;
  }
  std::istringstream iss(duration);
  int seconds=0;
  static constexpr char SEP{':'};
  char c;
  do
  {
    int i;
    c=0;
    iss >> i >> c;
    seconds = seconds*60+i;
  }
  while(c==SEP);
  return seconds;
}

//-----------------------------------------------------------------------------------
void rssparser(std::string const& data,
               std::function<void(const char*, const char*, const char*)> const&infoCallback,
               std::function<void(const char*, const char*, const char*, int, int, const char*)> const&itemCallback)
{
  using namespace rapidxml;

  auto nodeValue=[](xml_base<>* node)
  {
    return node?node->value():nullptr;
  };
  static auto innerNodeIfEmptyValue = [](xml_node<>* node)
  {
    return node?(node->value_size()?node:node->first_node()):nullptr;
  };
  auto attributeOrContent = [](xml_node<>* node, const char* attrName)->xml_base<>*
  {
    if(!node)
      return nullptr;
    xml_base<>* attr = node?node->first_attribute(attrName):nullptr;
    if(attr)
      return attr;
    return innerNodeIfEmptyValue(node->first_node(attrName));
  };

  xml_document<> doc;    // character type defaults to char
  char* text = const_cast<char*>(data.data());
  doc.parse<0>(text);    // 0 means default parse flags

  //cout << "Name of my first node is: " << doc.first_node()->name() << "\n";
  xml_node<> *rssNode = doc.first_node("rss");
  if(rssNode)
  {
    xml_node<> *channelNode = rssNode->first_node("channel");
    if(channelNode)
    {
      xml_node<>* title = channelNode->first_node("title");
      xml_node<>* description = innerNodeIfEmptyValue(channelNode->first_node("description"));
      xml_node<>* image = channelNode->first_node("image");
      xml_base<>* imageUrl = image?attributeOrContent(image, "url"):nullptr;
      infoCallback(nodeValue(title), nodeValue(description), nodeValue(imageUrl));

      for(xml_node<>* item = channelNode->first_node("item");item;item = item->next_sibling())
      {
        xml_node<>* itemTitle = innerNodeIfEmptyValue(item->first_node("title"));
        xml_node<>* itemEnclosure = item->first_node("enclosure");
        if(itemTitle && itemEnclosure)
        {
          MediaCols media;

          xml_attribute<>* itemEnclosureUrl = itemEnclosure->first_attribute("url");
          xml_node<>* pubDate = item->first_node("pubDate");
          xml_node<>* itemDesc = innerNodeIfEmptyValue(item->first_node("description"));
          xml_node<>* itemDuration = innerNodeIfEmptyValue(item->first_node("itunes:duration"));
          xml_node<>* itemThumbnail = innerNodeIfEmptyValue(item->first_node("media:thumbnail"));
          xml_base<>* itemThumbnailUrl = itemThumbnail?attributeOrContent(itemThumbnail,"url"):nullptr;

          itemCallback(nodeValue(itemTitle),
                       nodeValue(itemDesc),
                       nodeValue(itemEnclosureUrl),
                       pubDate?dateFromRSS(pubDate->value()):0,
                       itemDuration?durationFromRSS(itemDuration->value()):0,
                       nodeValue(itemThumbnailUrl));

        }
      }
    }
  }
}
