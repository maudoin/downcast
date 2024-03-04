#include "httpengine.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"//under windows, before windows.h

#include <numeric>

//-----------------------------------------------------------------------------------
namespace
{
std::pair<std::string, std::string> splitHostRessource(std::string const& url)
{
  std::size_t skippedHttp = 0;
  for(const char* str:{"http://", "https://"})
  {
    if(0 == url.find_first_of(str))
    {
      skippedHttp = strlen(str)+1;
      break;
    }
  }
  size_t foundSlash =url.find_first_of("/", skippedHttp);
  if(foundSlash != std::string::npos)
  {
    auto hostStart = 0;//skippedHttp-1;
    auto hostSize = foundSlash;// - hostStart;
    return {url.substr(hostStart, hostSize), url.substr(foundSlash)};
  }
  return {url, "/"};
}
std::string join( std::list<std::string> const& fields, std::string const& delimiter=",")
{
  return std::accumulate(std::next(fields.begin()), fields.end(),
                         fields.front(),[&delimiter](auto&& a, auto&& b)
  {
    return a + delimiter + b;
  });
}
bool isRedirection(httplib::Result const& res,
                   std::string& locationUpdate,
                   std::list<std::string> cookiesUpdate,
                   httplib::Headers headersUpdate)
{

  if (res->status > 300 && res->status < 400 )
  {
    locationUpdate = res->location.empty()?
                       res->get_header_value("Location"):
                       res->location;
    //cookies transfer
    if(res->has_header("Set-Cookie"))
    {
      cookiesUpdate.emplace_back(res->get_header_value("Set-Cookie"));
      headersUpdate.erase ("Cookies");
      headersUpdate.insert({"Cookies", join(cookiesUpdate, "; ")});
    }
    return true;
  }
  return false;
}

}
//-----------------------------------------------------------------------------------
std::optional<std::string> HttpEngine::get(std::string const& url)
{
  auto [host, resource] = splitHostRessource(url);
  httplib::Client cli(host.c_str());
  httplib::Headers headers{
    { "user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:108.0) Gecko/20100101 Firefox/108.0" }
  };
  cli.set_follow_location(true);

  if (auto res = cli.Get(resource.c_str(), headers))
  {
    if (res->status == 200)
    {
      cli.stop();
      return std::make_optional(std::move(res->body));
    }
    else
    {
      //better log !
      std::cerr << url << ":" << res->reason << std::endl;
    }
  }
  else
  {
    auto err = res.error();
    std::cerr << err << std::endl;
  }

  cli.stop();
  return std::nullopt;
}
//-----------------------------------------------------------------------------------
HttpEngine::Result HttpEngine::get(
    std::string const& url,
    std::function<bool(const char *data, size_t data_length)> const& receiver,
    std::function<void()> const& restart,
    std::function<bool(uint64_t current, uint64_t total)> const& progressHandler,
    int const initialTryCount)
{
  Result result;
  std::string location  = url;
  std::list<std::string> cookies;
  httplib::Headers headers{
    { "user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:108.0) Gecko/20100101 Firefox/108.0" }
  };
  for(int remainingTries=initialTryCount;remainingTries>0;remainingTries--)
  {
    auto [host, resource] = splitHostRessource(location);
    httplib::Client cli(host.c_str());
    cli.set_follow_location(false);

    if (auto res = cli.Get(resource.c_str(), headers, receiver,progressHandler))
    {
      if (isRedirection(res, location, cookies, headers))
      {
        restart();
        remainingTries=initialTryCount+1;
      }
      else if (res->status == 200)
      {
        cli.stop();
        //remainingTries = 0;
        return result;
      }
      else if(remainingTries<=1)
      {
        //better log !
        result = {url+":"+res->reason};
      }
    }
    else
    {
      result = {url+":"+to_string(res.error())};
    }
    cli.stop();
  }
  return result;
}
