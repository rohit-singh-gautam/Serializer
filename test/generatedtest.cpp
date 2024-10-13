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
