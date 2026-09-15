//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#include <array.hpp>
#include <enum.hpp>
#include <gtest/gtest.h>
#include <map.hpp>
#include <person.hpp>
#include <string>
#include <test1.hpp>
#include <variable.hpp>

TEST(generated_test, serialize_in) {
  const std::string personstr{"{\"fullname\":\"Rohit Jairaj Singh\",\"ID\":322}"};
  auto fullstream = rohit::make_constant_full_stream(personstr);

  test::test1::person person{};
  person.serialize_in<rohit::serializer::json>(fullstream);
  EXPECT_EQ(person.id, 322);

  std::string valuesstr{"{\"ch\":\"a\",\"pi\":3.14,\"t1\":3.884563,\"t2\":true}"};
  test::values values{};
  auto fullstream1 = rohit::make_constant_full_stream(valuesstr);
  values.serialize_in<rohit::serializer::json>(fullstream1);
  EXPECT_EQ(values.ch, 'a');
}

TEST(generated_test, serialize_out) {
  test::test1::person person{"Rohit Jairaj Singh", 322};
  rohit::full_stream_auto_alloc fullstream{256};
  person.serialize_out<rohit::serializer::json>(fullstream);

  std::string result_person{reinterpret_cast<char*>(fullstream.begin()),
                            fullstream.current_offset()};
  std::string personstr{"{\"fullname\":\"Rohit Jairaj Singh\",\"ID\":322}"};
  EXPECT_TRUE(result_person == personstr);

  test::test1::personex personex{"Rohit Jairaj Singh", 322, 122};
  fullstream.reset();
  personex.serialize_out<rohit::serializer::json>(fullstream);
  std::string result_personex{reinterpret_cast<char*>(fullstream.begin()),
                              fullstream.current_offset()};

  std::string personexstr{
      "{\"person\":{\"fullname\":\"Rohit Jairaj Singh\",\"ID\":322},\"account\":122}"};

  EXPECT_TRUE(result_personex == personexstr);

  test::values values{'a', 3.14f, 3.884563, true};
  fullstream.reset();
  values.serialize_out<rohit::serializer::json>(fullstream);
  std::string valuesstr{reinterpret_cast<char*>(fullstream.begin()), fullstream.current_offset()};
  std::string result_valuesstr{"{\"ch\":\"a\",\"pi\":3.14,\"t1\":3.884563,\"t2\":true}"};

  EXPECT_TRUE(result_valuesstr == valuesstr);

  fullstream.reset();
  personex.serialize_out<rohit::serializer::binary_none>(fullstream);
  test::test1::personex personex_binary_none{};
  fullstream.reset();
  personex_binary_none.serialize_in<rohit::serializer::binary_none>(fullstream);
  EXPECT_TRUE(personex.name == personex_binary_none.name);
  EXPECT_TRUE(personex.id == personex_binary_none.id);
  EXPECT_TRUE(personex.account == personex_binary_none.account);

  fullstream.reset();
  personex.serialize_out<rohit::serializer::binary_integer>(fullstream);
  test::test1::personex personex_binary_integer{};
  fullstream.reset();
  personex_binary_integer.serialize_in<rohit::serializer::binary_integer>(fullstream);
  EXPECT_TRUE(personex.name == personex_binary_integer.name);
  EXPECT_TRUE(personex.id == personex_binary_integer.id);
  EXPECT_TRUE(personex.account == personex_binary_integer.account);

  fullstream.reset();
  personex.serialize_out<rohit::serializer::binary_string>(fullstream);
  test::test1::personex personex_binary_string{};
  fullstream.reset();
  personex_binary_string.serialize_in<rohit::serializer::binary_string>(fullstream);
  EXPECT_TRUE(personex.name == personex_binary_string.name);
  EXPECT_TRUE(personex.id == personex_binary_string.id);
  EXPECT_TRUE(personex.account == personex_binary_string.account);
}

TEST(generated_test, serialize_array) {
  arraytest::personlist personlist{
      556, true, {{"Rohit Jairaj Singh", 1}, {"Ragini Rohit Singh", 2}}, {{1, 0}, {2, 1}}};
  rohit::full_stream_auto_alloc fullstream{256};
  personlist.serialize_out<rohit::serializer::json>(fullstream);
  std::string personliststr{reinterpret_cast<char*>(fullstream.begin()),
                            fullstream.current_offset()};
  std::string result_personliststr{
      R"({"listid":556,"check":true,"list":[{"name":"Rohit Jairaj Singh","ID":1},{"name":"Ragini Rohit Singh","ID":2}],"reverseListMap":[{"key":1,"value":0},{"key":2,"value":1}]})"};
  EXPECT_TRUE(result_personliststr == personliststr);

  auto fullstream1 = rohit::make_constant_full_stream(result_personliststr);
  arraytest::personlist personlist1{};
  personlist1.serialize_in<rohit::serializer::json>(fullstream1);
  EXPECT_TRUE(personlist.listid == personlist1.listid);
  EXPECT_TRUE(personlist.list.size() == personlist1.list.size());
  EXPECT_TRUE(personlist.list[0].name == personlist1.list[0].name);

  rohit::full_stream_auto_alloc fullstream_binary_none{256};
  personlist.serialize_out<rohit::serializer::binary_none>(fullstream_binary_none);
  arraytest::personlist personlist_binary_none{};
  fullstream_binary_none.reset();
  personlist_binary_none.serialize_in<rohit::serializer::binary_none>(fullstream_binary_none);
  EXPECT_TRUE(personlist.listid == personlist_binary_none.listid);
  EXPECT_TRUE(personlist.list.size() == personlist_binary_none.list.size());
  EXPECT_TRUE(personlist.list[0].name == personlist_binary_none.list[0].name);

  rohit::full_stream_auto_alloc fullstream_binary_integer{256};
  personlist.serialize_out<rohit::serializer::binary_integer>(fullstream_binary_integer);
  arraytest::personlist personlist_binary_integer{};
  fullstream_binary_integer.reset();
  personlist_binary_integer.serialize_in<rohit::serializer::binary_integer>(
      fullstream_binary_integer);
  EXPECT_TRUE(personlist.listid == personlist_binary_integer.listid);
  EXPECT_TRUE(personlist.list.size() == personlist_binary_integer.list.size());
  EXPECT_TRUE(personlist.list[0].name == personlist_binary_integer.list[0].name);

  rohit::full_stream_auto_alloc fullstream_binary_string{256};
  personlist.serialize_out<rohit::serializer::binary_string>(fullstream_binary_string);
  arraytest::personlist personlist_binary_string{};
  fullstream_binary_string.reset();
  personlist_binary_string.serialize_in<rohit::serializer::binary_string>(fullstream_binary_string);
  EXPECT_TRUE(personlist.listid == personlist_binary_string.listid);
  EXPECT_TRUE(personlist.list.size() == personlist_binary_string.list.size());
  EXPECT_TRUE(personlist.list[0].name == personlist_binary_string.list[0].name);
}

TEST(generated_test, serialize_map) {
  maptest::personlist personlist{
      556,
      {std::pair<std::uint64_t, maptest::person>{1, {"Rohit Jairaj Singh", 1}},
       std::pair<std::uint64_t, maptest::person>{2, {"Ragini Rohit Singh", 2}}}};
  rohit::full_stream_auto_alloc fullstream{256};
  personlist.serialize_out<rohit::serializer::json>(fullstream);
  std::string personliststr{reinterpret_cast<char*>(fullstream.begin()),
                            fullstream.current_offset()};
  std::string result_personliststr{
      R"({"listid":556,"list":[{"key":1,"value":{"name":"Rohit Jairaj Singh","ID":1}},{"key":2,"value":{"name":"Ragini Rohit Singh","ID":2}}]})"};
  EXPECT_TRUE(result_personliststr == personliststr);

  auto fullstream1 = rohit::make_constant_full_stream(result_personliststr);
  maptest::personlist personlist1{};
  personlist1.serialize_in<rohit::serializer::json>(fullstream1);
  EXPECT_TRUE(personlist.listid == personlist1.listid);
  EXPECT_TRUE(personlist.list.size() == personlist1.list.size());
  EXPECT_TRUE(personlist.list[1].name == personlist1.list[1].name);

  fullstream.reset();
  personlist.serialize_out<rohit::serializer::binary_none>(fullstream);
  maptest::personlist personlist_binary_none{};
  fullstream.reset();
  personlist_binary_none.serialize_in<rohit::serializer::binary_none>(fullstream);
  EXPECT_TRUE(personlist.listid == personlist_binary_none.listid);
  EXPECT_TRUE(personlist.list.size() == personlist_binary_none.list.size());
  EXPECT_TRUE(personlist.list[1].name == personlist_binary_none.list[1].name);

  fullstream.reset();
  personlist.serialize_out<rohit::serializer::binary_integer>(fullstream);
  maptest::personlist personlist_binary_integer{};
  fullstream.reset();
  personlist_binary_integer.serialize_in<rohit::serializer::binary_integer>(fullstream);
  EXPECT_TRUE(personlist.listid == personlist_binary_integer.listid);
  EXPECT_TRUE(personlist.list.size() == personlist_binary_integer.list.size());
  EXPECT_TRUE(personlist.list[1].name == personlist_binary_integer.list[1].name);

  fullstream.reset();
  personlist.serialize_out<rohit::serializer::binary_string>(fullstream);
  maptest::personlist personlist_binary_string{};
  fullstream.reset();
  personlist_binary_string.serialize_in<rohit::serializer::binary_string>(fullstream);
  EXPECT_TRUE(personlist.listid == personlist_binary_string.listid);
  EXPECT_TRUE(personlist.list.size() == personlist_binary_string.list.size());
  EXPECT_TRUE(personlist.list[1].name == personlist_binary_string.list[1].name);
}

TEST(generated_test, serialize_union) {
  test::cacheserver cacheserver{10, 10, 10, 10, 2010, 10240};
  test::server1 server{test::server1::e_entry::cache, {.cache = cacheserver}, test::test112::em2};
  rohit::full_stream_auto_alloc fullstream{256};
  server.serialize_out<rohit::serializer::json>(fullstream);
  std::string serverstr{reinterpret_cast<char*>(fullstream.begin()), fullstream.current_offset()};
  std::string result_serverstr{
      "{\"entry:cache\":{\"serverbase\":{\"name\":{\"a\":10,\"b\":10,\"c\":10,\"d\":10},\"port\":"
      "2010},\"size\":10240},\"test12\":\"em2\"}"};
  EXPECT_TRUE(result_serverstr == serverstr);

  auto fullstream1 = rohit::make_constant_full_stream(result_serverstr);
  test::server1 server1{};
  server1.serialize_in<rohit::serializer::json>(fullstream1);
  EXPECT_TRUE(server.entry_type == server1.entry_type);
  EXPECT_TRUE(server.entry.cache.port == server1.entry.cache.port);
  EXPECT_TRUE(server.entry.cache.size == server1.entry.cache.size);

  rohit::full_stream_auto_alloc fullstream_binary_none{256};
  server.serialize_out<rohit::serializer::binary_none>(fullstream_binary_none);
  test::server1 server_binary_none{};
  fullstream_binary_none.reset();
  server_binary_none.serialize_in<rohit::serializer::binary_none>(fullstream_binary_none);
  EXPECT_TRUE(server.entry_type == server_binary_none.entry_type);
  EXPECT_TRUE(server.entry.cache.port == server_binary_none.entry.cache.port);
  EXPECT_TRUE(server.entry.cache.size == server_binary_none.entry.cache.size);

  rohit::full_stream_auto_alloc fullstream_binary_id{256};
  server.serialize_out<rohit::serializer::binary_integer>(fullstream_binary_id);
  test::server1 server_binary_id{};
  fullstream_binary_id.reset();
  server_binary_id.serialize_in<rohit::serializer::binary_integer>(fullstream_binary_id);
  EXPECT_TRUE(server.entry_type == server_binary_id.entry_type);
  EXPECT_TRUE(server.entry.cache.port == server_binary_id.entry.cache.port);
  EXPECT_TRUE(server.entry.cache.size == server_binary_id.entry.cache.size);

  rohit::full_stream_auto_alloc fullstream_binary_name{256};
  server.serialize_out<rohit::serializer::binary_string>(fullstream_binary_name);
  test::server1 server_binary_string{};
  fullstream_binary_name.reset();
  server_binary_string.serialize_in<rohit::serializer::binary_string>(fullstream_binary_name);
  EXPECT_TRUE(server.entry_type == server_binary_string.entry_type);
  EXPECT_TRUE(server.entry.cache.port == server_binary_string.entry.cache.port);
  EXPECT_TRUE(server.entry.cache.size == server_binary_string.entry.cache.size);
}

TEST(generated_test, serialize_union1) {
  constexpr auto enumval = test::to_test112("em3");
  test::server1 server{
      test::server1::e_entry::http, {.http = {10, 10, 10, 10, 2010, 10240, 5021}}, enumval};
  rohit::full_stream_auto_alloc fullstream{256};
  server.serialize_out<rohit::serializer::json>(fullstream);
  std::string serverstr{reinterpret_cast<char*>(fullstream.begin()), fullstream.current_offset()};
  std::string result_serverstr{
      "{\"entry:http\":{\"serverbase\":{\"name\":{\"a\":10,\"b\":10,\"c\":10,\"d\":10},\"port\":"
      "2010},\"size\":10240,\"mimesize\":5021},\"test12\":\"em3\"}"};
  EXPECT_TRUE(result_serverstr == serverstr);

  auto fullstream1 = rohit::make_constant_full_stream(result_serverstr);
  test::server1 server1{};
  server1.serialize_in<rohit::serializer::json>(fullstream1);
  EXPECT_TRUE(server.entry_type == server1.entry_type);
  EXPECT_TRUE(server.entry.http.port == server.entry.http.port);
  EXPECT_TRUE(server.entry.http.size == server.entry.http.size);
  EXPECT_TRUE(server.entry.http.mimesize == server.entry.http.mimesize);

  fullstream.reset();
  server.serialize_out<rohit::serializer::binary_none>(fullstream);
  test::server1 server_binary_none{};
  fullstream.reset();
  server_binary_none.serialize_in<rohit::serializer::binary_none>(fullstream);
  EXPECT_TRUE(server.entry_type == server_binary_none.entry_type);
  EXPECT_TRUE(server.entry.http.port == server_binary_none.entry.http.port);
  EXPECT_TRUE(server.entry.http.size == server_binary_none.entry.http.size);
  EXPECT_TRUE(server.entry.http.mimesize == server_binary_none.entry.http.mimesize);

  fullstream.reset();
  server.serialize_out<rohit::serializer::binary_integer>(fullstream);
  test::server1 server_binary_integer{};
  fullstream.reset();
  server_binary_integer.serialize_in<rohit::serializer::binary_integer>(fullstream);
  EXPECT_TRUE(server.entry_type == server_binary_integer.entry_type);
  EXPECT_TRUE(server.entry.http.port == server_binary_integer.entry.http.port);
  EXPECT_TRUE(server.entry.http.size == server_binary_integer.entry.http.size);
  EXPECT_TRUE(server.entry.http.mimesize == server_binary_integer.entry.http.mimesize);

  fullstream.reset();
  server.serialize_out<rohit::serializer::binary_string>(fullstream);
  test::server1 server_binary_string{};
  fullstream.reset();
  server_binary_string.serialize_in<rohit::serializer::binary_string>(fullstream);
  EXPECT_TRUE(server.entry_type == server_binary_string.entry_type);
  EXPECT_TRUE(server.entry.http.port == server_binary_string.entry.http.port);
  EXPECT_TRUE(server.entry.http.size == server_binary_string.entry.http.size);
  EXPECT_TRUE(server.entry.http.mimesize == server_binary_string.entry.http.mimesize);
}

TEST(generated_test, serialize_enum) {
  enumtest::test test1{enumtest::to_testenum("test1")};
  rohit::full_stream_auto_alloc fullstream{256};
  test1.serialize_out<rohit::serializer::json>(fullstream);
  std::string teststr{reinterpret_cast<char*>(fullstream.begin()), fullstream.current_offset()};
  std::string result_teststr{"{\"te\":\"test1\"}"};
  EXPECT_TRUE(result_teststr == teststr);

  auto fullstream1 = rohit::make_constant_full_stream(result_teststr);
  enumtest::test test11{};
  test11.serialize_in<rohit::serializer::json>(fullstream1);
  EXPECT_TRUE(test1.te == test11.te);

  fullstream.reset();
  test1.serialize_out<rohit::serializer::binary_none>(fullstream);
  enumtest::test test_binary_none{};
  fullstream.reset();
  test_binary_none.serialize_in<rohit::serializer::binary_none>(fullstream);
  EXPECT_TRUE(test1.te == test_binary_none.te);

  fullstream.reset();
  test1.serialize_out<rohit::serializer::binary_integer>(fullstream);
  enumtest::test test_binary_integer{};
  fullstream.reset();
  test_binary_integer.serialize_in<rohit::serializer::binary_integer>(fullstream);
  EXPECT_TRUE(test1.te == test_binary_integer.te);

  fullstream.reset();
  test1.serialize_out<rohit::serializer::binary_string>(fullstream);
  enumtest::test test_binary_string{};
  fullstream.reset();
  test_binary_string.serialize_in<rohit::serializer::binary_string>(fullstream);
  EXPECT_TRUE(test1.te == test_binary_string.te);
}

static constexpr const char teststr[] =
    // Redundant spaces are added in below string for testing purposes only.
    R"(
{
    "name": "First Store",
    "sessionlist": [
        {
            "name": "First Session",
            "id": 22,
            "persons": {
                "listid": 55,
                "check": true,
                "list": [
                    {
                        "name": "Rohit Jairaj Singh",
                        "ID": 322
                    },
                    {
                        "name": "Ragini Rohit Singh",
                        "ID": 323
                    }
                ],
                "reverseListMap": [
                    {
                        "key": 322,
                        "value": 0
                    },
                    {
                        "key": 323,
                        "value": 1
                    }
                ]
            }
        },
        {
            "name": "Second Session",
            "id": 23,
            "persons": {
                "listid": 56,
                "check": false,
                "list": [
                    {
                        "name": "Rohit Jairaj Singh1",
                        "ID": 324
                    },
                    {
                        "name": "Ragini Rohit Singh2",
                        "ID": 325
                    }
                ],
                "reverseListMap": [
                    {
                        "key": 324,
                        "value": 0
                    },
                    {
                        "key": 325,
                        "value": 1
                    }
                ]
            }
        }
    ]
}
)";

TEST(generated_test, serialize_array_complete) {
  std::string input{teststr};

  auto fullstream = rohit::make_constant_full_stream(input);
  arraytest::sessionstore sessionstore{};
  try {
    sessionstore.serialize_in<rohit::serializer::json>(fullstream);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  rohit::full_stream_auto_alloc fullstream1{256};
  sessionstore.serialize_out<rohit::serializer::binary_integer>(fullstream1);
  fullstream1.reset();
  arraytest::sessionstore sessionstore1{};
  sessionstore1.serialize_in<rohit::serializer::binary_integer>(fullstream1);
  EXPECT_TRUE(sessionstore.name == sessionstore1.name);
  EXPECT_TRUE(sessionstore.sessionlist.size() == sessionstore1.sessionlist.size());
  EXPECT_TRUE(sessionstore.sessionlist[0].name == sessionstore1.sessionlist[0].name);
  EXPECT_TRUE(sessionstore.sessionlist[0].persons.listid ==
              sessionstore1.sessionlist[0].persons.listid);
  EXPECT_TRUE(sessionstore.sessionlist[0].persons.list[0].name ==
              sessionstore1.sessionlist[0].persons.list[0].name);
  EXPECT_TRUE(sessionstore.sessionlist[0].persons.list[0].id ==
              sessionstore1.sessionlist[0].persons.list[0].id);
}

TEST(generated_test, serialize_json_beautification) {
  std::string input{teststr};

  auto fullstream = rohit::make_constant_full_stream(input);
  arraytest::sessionstore sessionstore{};
  try {
    sessionstore.serialize_in<rohit::serializer::json>(fullstream);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  rohit::full_stream_auto_alloc fullstream1{256};
  rohit::serializer::json_out<true> json_out_compressed{fullstream1,
                                                        rohit::serializer::format::compress};
  sessionstore.serialize_out(json_out_compressed);
  std::string result_compressed{reinterpret_cast<char*>(fullstream1.begin()),
                                fullstream1.current_offset()};
  std::string expected_compressed_output{
      R"({"name":"First Store","sessionlist":[{"name":"First Session","id":22,"persons":{"listid":55,"check":true,"list":[{"name":"Rohit Jairaj Singh","ID":322},{"name":"Ragini Rohit Singh","ID":323}],"reverseListMap":[{"key":322,"value":0},{"key":323,"value":1}]}},{"name":"Second Session","id":23,"persons":{"listid":56,"check":false,"list":[{"name":"Rohit Jairaj Singh1","ID":324},{"name":"Ragini Rohit Singh2","ID":325}],"reverseListMap":[{"key":324,"value":0},{"key":325,"value":1}]}}]})"};
  EXPECT_TRUE(result_compressed == expected_compressed_output);

  rohit::serializer::json_out<true> json_out_beautify{fullstream1,
                                                      rohit::serializer::format::beautify};
  fullstream1.reset();
  sessionstore.serialize_out(json_out_beautify);
  std::string result_beautify{reinterpret_cast<char*>(fullstream1.begin()),
                              fullstream1.current_offset()};
  std::string expected_beautify_output{
      R"({
  "name": "First Store",
  "sessionlist": [
    {
      "name": "First Session",
      "id": 22,
      "persons": {
        "listid": 55,
        "check": true,
        "list": [
          {
            "name": "Rohit Jairaj Singh",
            "ID": 322
          }, {
            "name": "Ragini Rohit Singh",
            "ID": 323
          }
        ],
        "reverseListMap": [
          {
            "key": 322,
            "value": 0
          }, {
            "key": 323,
            "value": 1
          }
        ]
      }
    }, {
      "name": "Second Session",
      "id": 23,
      "persons": {
        "listid": 56,
        "check": false,
        "list": [
          {
            "name": "Rohit Jairaj Singh1",
            "ID": 324
          }, {
            "name": "Ragini Rohit Singh2",
            "ID": 325
          }
        ],
        "reverseListMap": [
          {
            "key": 324,
            "value": 0
          }, {
            "key": 325,
            "value": 1
          }
        ]
      }
    }
  ]
})"};
  EXPECT_TRUE(result_beautify == expected_beautify_output);

  fullstream1.reset();
  rohit::serializer::json_out<true> json_out_vertical{fullstream1,
                                                      rohit::serializer::format::beautify_vertical};
  sessionstore.serialize_out(json_out_vertical);
  std::string result_vertical{reinterpret_cast<char*>(fullstream1.begin()),
                              fullstream1.current_offset()};
  std::string expected_beautify_vertical_output{
      R"({
  "name": "First Store",
  "sessionlist": 
  [
    {
      "name": "First Session",
      "id": 22,
      "persons": 
      {
        "listid": 55,
        "check": true,
        "list": 
        [
          {
            "name": "Rohit Jairaj Singh",
            "ID": 322
          },
          {
            "name": "Ragini Rohit Singh",
            "ID": 323
          }
        ],
        "reverseListMap": 
        [
          {
            "key": 322,
            "value": 0
          },
          {
            "key": 323,
            "value": 1
          }
        ]
      }
    },
    {
      "name": "Second Session",
      "id": 23,
      "persons": 
      {
        "listid": 56,
        "check": false,
        "list": 
        [
          {
            "name": "Rohit Jairaj Singh1",
            "ID": 324
          },
          {
            "name": "Ragini Rohit Singh2",
            "ID": 325
          }
        ],
        "reverseListMap": 
        [
          {
            "key": 324,
            "value": 0
          },
          {
            "key": 325,
            "value": 1
          }
        ]
      }
    }
  ]
})"};
  EXPECT_TRUE(result_vertical == expected_beautify_vertical_output);
}