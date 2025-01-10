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

#include <gtest/gtest.h>
#include <person.h>
#include <test1.h>
#include <array.h>
#include <variable.h>
#include <map.h>
#include <string>
#include <enum.h>

TEST(GeneratedTest, SerializeIn) {
    const std::string personstr {"{\"name\":\"Rohit Jairaj Singh\",\"ID\":322}"};
    auto fullstream = rohit::make_const_fullstream(personstr);

    test::test1::person person { };
    person.serialize_in<rohit::serializer::json>(fullstream);
    EXPECT_EQ(person.ID, 322);

    std::string valuesstr { "{\"ch\":\"a\",\"pi\":3.14,\"t1\":3.884563,\"t2\":TRUE}" };
    test::values values { };
    auto fullstream1 = rohit::make_const_fullstream(valuesstr);
    values.serialize_in<rohit::serializer::json>(fullstream1);
    EXPECT_EQ(values.ch, 'a');
}

TEST(GeneratedTest, SerializeOut) {
    test::test1::person person { "Rohit Jairaj Singh", 322 };
    rohit::FullStreamAutoAlloc fullstream { 256 };
    person.serialize_out<rohit::serializer::json>(fullstream);

    std::string result_person {reinterpret_cast<char *>(fullstream.begin()), fullstream.index()};
    std::string personstr {"{\"name\":\"Rohit Jairaj Singh\",\"ID\":322}"};
    EXPECT_TRUE(result_person == personstr);

    test::test1::personex personex { "Rohit Jairaj Singh", 322, 122 };
    fullstream.Reset();
    personex.serialize_out<rohit::serializer::json>(fullstream);
    std::string result_personex {reinterpret_cast<char *>(fullstream.begin()), fullstream.index()};

    std::string personexstr { "{\"person\":{\"name\":\"Rohit Jairaj Singh\",\"ID\":322},\"account\":122}" };

    EXPECT_TRUE(result_personex == personexstr);
    
    test::values values { 'a', 3.14, 3.884563, true};
    fullstream.Reset();
    values.serialize_out<rohit::serializer::json>(fullstream);
    std::string valuesstr {reinterpret_cast<char *>(fullstream.begin()), fullstream.index()};
    std::string result_valuesstr { "{\"ch\":\"a\",\"pi\":3.14,\"t1\":3.884563,\"t2\":TRUE}" };

    EXPECT_TRUE(result_valuesstr == valuesstr);

    fullstream.Reset();
    personex.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstream);
    test::test1::personex personexBinaryNone { };
    fullstream.Reset();
    personexBinaryNone.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstream);
    EXPECT_TRUE(personex.name == personexBinaryNone.name);
    EXPECT_TRUE(personex.ID == personexBinaryNone.ID);
    EXPECT_TRUE(personex.account == personexBinaryNone.account);

    fullstream.Reset();
    personex.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstream);
    test::test1::personex personexBinaryInteger { };
    fullstream.Reset();
    personexBinaryInteger.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstream);
    EXPECT_TRUE(personex.name == personexBinaryInteger.name);
    EXPECT_TRUE(personex.ID == personexBinaryInteger.ID);
    EXPECT_TRUE(personex.account == personexBinaryInteger.account);

    fullstream.Reset();
    personex.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstream);
    test::test1::personex personexBinaryString { };
    fullstream.Reset();
    personexBinaryString.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstream);
    EXPECT_TRUE(personex.name == personexBinaryString.name);
    EXPECT_TRUE(personex.ID == personexBinaryString.ID);
    EXPECT_TRUE(personex.account == personexBinaryString.account);
}

TEST(GeneratedTest, SerializeArray) {
    arraytest::personlist personlist { 556, {{"Rohit Jairaj Singh", 1}, {"Ragini Rohit Singh", 2}}};
    rohit::FullStreamAutoAlloc fullstream { 256 };
    personlist.serialize_out<rohit::serializer::json>(fullstream);
    std::string personliststr {reinterpret_cast<char *>(fullstream.begin()), fullstream.index()};
    std::string result_personliststr { "{\"listid\":556,\"list\":[{\"name\":\"Rohit Jairaj Singh\",\"ID\":1},{\"name\":\"Ragini Rohit Singh\",\"ID\":2}]}" };
    EXPECT_TRUE(result_personliststr == personliststr);

    auto fullstream1 = rohit::make_const_fullstream(result_personliststr);
    arraytest::personlist personlist1 { };
    personlist1.serialize_in<rohit::serializer::json>(fullstream1);
    EXPECT_TRUE(personlist.listid == personlist1.listid);
    EXPECT_TRUE(personlist.list.size() == personlist1.list.size());
    EXPECT_TRUE(personlist.list[0].name == personlist1.list[0].name);

    rohit::FullStreamAutoAlloc fullstreamBinaryNone { 256 };
    personlist.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstreamBinaryNone);
    arraytest::personlist personlistBinaryNone { };
    fullstreamBinaryNone.Reset();
    personlistBinaryNone.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstreamBinaryNone);
    EXPECT_TRUE(personlist.listid == personlistBinaryNone.listid);
    EXPECT_TRUE(personlist.list.size() == personlistBinaryNone.list.size());
    EXPECT_TRUE(personlist.list[0].name == personlistBinaryNone.list[0].name);

    rohit::FullStreamAutoAlloc fullstreamBinaryInteger { 256 };
    personlist.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstreamBinaryInteger);
    arraytest::personlist personlistBinaryInteger { };
    fullstreamBinaryInteger.Reset();
    personlistBinaryInteger.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstreamBinaryInteger);
    EXPECT_TRUE(personlist.listid == personlistBinaryInteger.listid);
    EXPECT_TRUE(personlist.list.size() == personlistBinaryInteger.list.size());
    EXPECT_TRUE(personlist.list[0].name == personlistBinaryInteger.list[0].name);

    rohit::FullStreamAutoAlloc fullstreamBinaryString { 256 };
    personlist.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstreamBinaryString);
    arraytest::personlist personlistBinaryString { };
    fullstreamBinaryString.Reset();
    personlistBinaryString.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstreamBinaryString);
    EXPECT_TRUE(personlist.listid == personlistBinaryString.listid);
    EXPECT_TRUE(personlist.list.size() == personlistBinaryString.list.size());
    EXPECT_TRUE(personlist.list[0].name == personlistBinaryString.list[0].name);
}

TEST(GeneratedTest, SerializeMap) {
    maptest::personlist personlist { 556, {std::pair<uint64_t, maptest::person> {1, {"Rohit Jairaj Singh", 1}}, std::pair<uint64_t, maptest::person> {2, {"Ragini Rohit Singh", 2}}} };
    rohit::FullStreamAutoAlloc fullstream { 256 };
    personlist.serialize_out<rohit::serializer::json>(fullstream);
    std::string personliststr {reinterpret_cast<char *>(fullstream.begin()), fullstream.index()};
    std::string result_personliststr { "{\"listid\":556,\"list\":{1:{\"name\":\"Rohit Jairaj Singh\",\"ID\":1},2:{\"name\":\"Ragini Rohit Singh\",\"ID\":2}}}" };
    EXPECT_TRUE(result_personliststr == personliststr);

    auto fullstream1 = rohit::make_const_fullstream(result_personliststr);
    maptest::personlist personlist1 { };
    personlist1.serialize_in<rohit::serializer::json>(fullstream1);
    EXPECT_TRUE(personlist.listid == personlist1.listid);
    EXPECT_TRUE(personlist.list.size() == personlist1.list.size());
    EXPECT_TRUE(personlist.list[1].name == personlist1.list[1].name);

    fullstream.Reset();
    personlist.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstream);
    maptest::personlist personlistBinaryNone { };
    fullstream.Reset();
    personlistBinaryNone.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstream);
    EXPECT_TRUE(personlist.listid == personlistBinaryNone.listid);
    EXPECT_TRUE(personlist.list.size() == personlistBinaryNone.list.size());
    EXPECT_TRUE(personlist.list[1].name == personlistBinaryNone.list[1].name);

    fullstream.Reset();
    personlist.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstream);
    maptest::personlist personlistBinaryInteger { };
    fullstream.Reset();
    personlistBinaryInteger.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstream);
    EXPECT_TRUE(personlist.listid == personlistBinaryInteger.listid);
    EXPECT_TRUE(personlist.list.size() == personlistBinaryInteger.list.size());
    EXPECT_TRUE(personlist.list[1].name == personlistBinaryInteger.list[1].name);

    fullstream.Reset();
    personlist.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstream);
    maptest::personlist personlistBinaryString { };
    fullstream.Reset();
    personlistBinaryString.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstream);
    EXPECT_TRUE(personlist.listid == personlistBinaryString.listid);
    EXPECT_TRUE(personlist.list.size() == personlistBinaryString.list.size());
    EXPECT_TRUE(personlist.list[1].name == personlistBinaryString.list[1].name);
}

TEST(GeneratedTest, SerializeUnion) {
    test::cacheserver cacheserver {10, 10, 10, 10, 2010, 10240};
    test::server1 server {test::server1::e_entry::cache, {.cache = cacheserver}, test::test112::em2 };
    rohit::FullStreamAutoAlloc fullstream { 256 };
    server.serialize_out<rohit::serializer::json>(fullstream);
    std::string serverstr {reinterpret_cast<char *>(fullstream.begin()), fullstream.index()};
    std::string result_serverstr { "{\"entry:cache\":{\"serverbase\":{\"name\":{\"a\":10,\"b\":10,\"c\":10,\"d\":10},\"port\":2010},\"size\":10240},\"test12\":\"em2\"}"};
    EXPECT_TRUE(result_serverstr == serverstr);

    auto fullstream1 = rohit::make_const_fullstream(result_serverstr);
    test::server1 server1 { };
    server1.serialize_in<rohit::serializer::json>(fullstream1);
    EXPECT_TRUE(server.entry_type == server1.entry_type);
    EXPECT_TRUE(server.entry.cache.port == server1.entry.cache.port);
    EXPECT_TRUE(server.entry.cache.size == server1.entry.cache.size);
    

    rohit::FullStreamAutoAlloc fullstreamBinaryNone { 256 };
    server.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstreamBinaryNone);
    test::server1 serverBinaryNone { };
    fullstreamBinaryNone.Reset();
    serverBinaryNone.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstreamBinaryNone);
    EXPECT_TRUE(server.entry_type == serverBinaryNone.entry_type);
    EXPECT_TRUE(server.entry.cache.port == serverBinaryNone.entry.cache.port);
    EXPECT_TRUE(server.entry.cache.size == serverBinaryNone.entry.cache.size);

    rohit::FullStreamAutoAlloc fullstreamBinaryId { 256 };
    server.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstreamBinaryId);
    test::server1 serverBinaryId { };
    fullstreamBinaryId.Reset();
    serverBinaryId.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstreamBinaryId);
    EXPECT_TRUE(server.entry_type == serverBinaryId.entry_type);
    EXPECT_TRUE(server.entry.cache.port == serverBinaryId.entry.cache.port);
    EXPECT_TRUE(server.entry.cache.size == serverBinaryId.entry.cache.size);

    rohit::FullStreamAutoAlloc fullstreamBinaryName { 256 };
    server.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstreamBinaryName);
    test::server1 serverBinaryString { };
    fullstreamBinaryName.Reset();
    serverBinaryString.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstreamBinaryName);
    EXPECT_TRUE(server.entry_type == serverBinaryString.entry_type);
    EXPECT_TRUE(server.entry.cache.port == serverBinaryString.entry.cache.port);
    EXPECT_TRUE(server.entry.cache.size == serverBinaryString.entry.cache.size);
}


TEST(GeneratedTest, SerializeUnion1) {
    constexpr auto enumval = test::to_test112("em3");
    test::server1 server {test::server1::e_entry::http, {.http = {10, 10, 10, 10, 2010, 10240, 5021}}, enumval };
    rohit::FullStreamAutoAlloc fullstream { 256 };
    server.serialize_out<rohit::serializer::json>(fullstream);
    std::string serverstr {reinterpret_cast<char *>(fullstream.begin()), fullstream.index()};
    std::string result_serverstr {"{\"entry:http\":{\"serverbase\":{\"name\":{\"a\":10,\"b\":10,\"c\":10,\"d\":10},\"port\":2010},\"size\":10240,\"mimesize\":5021},\"test12\":\"em3\"}"};
    EXPECT_TRUE(result_serverstr == serverstr);

    auto fullstream1 = rohit::make_const_fullstream(result_serverstr);
    test::server1 server1 { };
    server1.serialize_in<rohit::serializer::json>(fullstream1);
    EXPECT_TRUE(server.entry_type == server1.entry_type);
    EXPECT_TRUE(server.entry.http.port == server.entry.http.port);
    EXPECT_TRUE(server.entry.http.size == server.entry.http.size);
    EXPECT_TRUE(server.entry.http.mimesize == server.entry.http.mimesize);

    fullstream.Reset();
    server.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstream);
    test::server1 serverBinaryNone { };
    fullstream.Reset();
    serverBinaryNone.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstream);
    EXPECT_TRUE(server.entry_type == serverBinaryNone.entry_type);
    EXPECT_TRUE(server.entry.http.port == serverBinaryNone.entry.http.port);
    EXPECT_TRUE(server.entry.http.size == serverBinaryNone.entry.http.size);
    EXPECT_TRUE(server.entry.http.mimesize == serverBinaryNone.entry.http.mimesize);

    fullstream.Reset();
    server.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstream);
    test::server1 serverBinaryInteger { };
    fullstream.Reset();
    serverBinaryInteger.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstream);
    EXPECT_TRUE(server.entry_type == serverBinaryInteger.entry_type);
    EXPECT_TRUE(server.entry.http.port == serverBinaryInteger.entry.http.port);
    EXPECT_TRUE(server.entry.http.size == serverBinaryInteger.entry.http.size);
    EXPECT_TRUE(server.entry.http.mimesize == serverBinaryInteger.entry.http.mimesize);

    fullstream.Reset();
    server.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstream);
    test::server1 serverBinaryString { };
    fullstream.Reset();
    serverBinaryString.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstream);
    EXPECT_TRUE(server.entry_type == serverBinaryString.entry_type);
    EXPECT_TRUE(server.entry.http.port == serverBinaryString.entry.http.port);
    EXPECT_TRUE(server.entry.http.size == serverBinaryString.entry.http.size);
    EXPECT_TRUE(server.entry.http.mimesize == serverBinaryString.entry.http.mimesize);
}


TEST(GeneratedTest, SerializeEnum) {
    enumtest::test test1 { enumtest::to_testenum("test1") };
    rohit::FullStreamAutoAlloc fullstream { 256 };
    test1.serialize_out<rohit::serializer::json>(fullstream);
    std::string teststr {reinterpret_cast<char *>(fullstream.begin()), fullstream.index()};
    std::string result_teststr { "{\"te\":\"test1\"}" };
    EXPECT_TRUE(result_teststr == teststr);

    auto fullstream1 = rohit::make_const_fullstream(result_teststr);
    enumtest::test test11 { };
    test11.serialize_in<rohit::serializer::json>(fullstream1);
    EXPECT_TRUE(test1.te == test11.te);

    fullstream.Reset();
    test1.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstream);
    enumtest::test testBinaryNone { };
    fullstream.Reset();
    testBinaryNone.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::None>>(fullstream);
    EXPECT_TRUE(test1.te == testBinaryNone.te);

    fullstream.Reset();
    test1.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstream);
    enumtest::test testBinaryInteger { };
    fullstream.Reset();
    testBinaryInteger.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::Integer>>(fullstream);
    EXPECT_TRUE(test1.te == testBinaryInteger.te);

    fullstream.Reset();
    test1.serialize_out<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstream);
    enumtest::test testBinaryString { };
    fullstream.Reset();
    testBinaryString.serialize_in<rohit::serializer::binary<rohit::serializer::SerializeKeyType::String>>(fullstream);
    EXPECT_TRUE(test1.te == testBinaryString.te);
}