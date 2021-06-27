#pragma once

#include <string>
#include <functional>

int dateFromRSS(const char* pubDate);
int durationFromRSS(const char* pubDate);

void rssparser(std::string const& data,
               std::function<void(const char*title,
                                  const char*desc,
                                  const char*thumbailImg)> const&infoCallback,
               std::function<void(const char* title,
                                  const char* desc,
                                  const char* url,
                                  int date,
                                  int duration,
                                  const char*thumbailImg)> const&itemCallback);
