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
#include <map.h>
#include <string>

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
}
