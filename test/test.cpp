#include <gtest/gtest.h>
#include "httplib.h"

TEST(HTTPLIB, TestParseLocation)
{
  static const std::string URL("https://stitcher2.acast.com/livestitches/4d4cc4fe72c9452bd0b0992a5c89e434.mp3?aid=63a4721c69c77e001126ad39&chid=a8879bdf-de58-4537-8dab-a3bb13948786&ci=oFpQlSRp3GFDZwrcZw5e3SEuFWtGfBXjcj6-mtxC8TJYKWNWTP3KWg%3D%3D&pf=rss&sv=sphinx%401.134.1&uid=6ec01abdba610f88f88e42ff560ecdef&Expires=1672109987714&Key-Pair-Id=K38CTQXUSD0VVB&Signature=TQXsBs7XluU~YRtPTcYe1EtVuvnkf542tbp1p7KUnvn24rm-tQjO8dYgLSbXlJCBwsiPtbnJc-YjLbGlaVLKDzzfABj2lCldE-KoeUSdnEQPWXdPK6FK5BR7kuN-CuY1MfQ-0sDa4MTGAErHZZB1p3~jiiZbbP7fYd9ttBfXwlZgjv5BtHOL4KQs7QY7q-~ZP5tXoGhtufPMruWRYOptrves991ax5lgKPwTvzhXSL6CEKpHWoAMi88shXnBBC~f2iOropB-yzcj5K-uaK6LPcObfHh9Akgl~uIAqbLka2Nrq-HQ-7QrMIUmFcA2nTEaAF66dGRj7AGtEkS2m2hB4A__");
  static const std::string Location("Location");
  static const std::string testLocation(Location+":"+URL);

  //test Location URL is preserved
  httplib::detail::parse_header(testLocation.data(), testLocation.data()+testLocation.size(),
               [&](std::string const& key, std::string const& value)
  {
    ASSERT_EQ(key, Location);
    ASSERT_EQ(value, URL);
    ASSERT_NE(value, httplib::detail::decode_url(URL, false));
  });

  //test non-Location URL is decoded
  static const std::string Other("Other");
  static const std::string testOther(Other+":"+URL);
  httplib::detail::parse_header(testOther.data(), testOther.data()+testOther.size(),
               [&](std::string const& key, std::string const& value)
  {
    ASSERT_EQ(key, Other);
    ASSERT_EQ(value, httplib::detail::decode_url(URL, false));
    ASSERT_NE(value, URL);
  });
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
