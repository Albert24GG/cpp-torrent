#include "Bencode.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace Bencode;

TEST_CASE("Bencode: BDecode: Integers", "[Bencode][BDecode]") {
    SECTION("Positive integers") {
        REQUIRE(std::get<BencodeInt>(BDecode("i42e")) == 42);
        REQUIRE(std::get<BencodeInt>(BDecode("i1234567890e")) == 1'234'567'890);
        REQUIRE(std::get<BencodeInt>(BDecode("i0e")) == 0);
    }

    SECTION("Negative integers") {
        REQUIRE(std::get<BencodeInt>(BDecode("i-42e")) == -42);
        REQUIRE(std::get<BencodeInt>(BDecode("i-1234567890e")) == -1'234'567'890);
    }

    SECTION("Invalid integers") {
        REQUIRE_THROWS(BDecode("i42"));
        REQUIRE_THROWS(BDecode("i4-2"));
        REQUIRE_THROWS(BDecode("i-42"));
        REQUIRE_THROWS(BDecode("i-4-2"));
        REQUIRE_THROWS(BDecode("i-4s2e"));
        REQUIRE_THROWS(BDecode("i-4-2e"));
    }

    SECTION("Invalid integers with trailing characters") {
        REQUIRE_THROWS(BDecode("i42e0"));
        REQUIRE_THROWS(BDecode("i42e0e"));
    }

    SECTION("Offsets") {
        BencodeItem item = BDecode("i42123e");
        REQUIRE(item.start() == 0);
        REQUIRE(item.len() == 7);
    }
}

TEST_CASE("Bencode: BDecode: Strings", "[Bencode][BDecode]") {
    SECTION("Empty strings") {
        REQUIRE(std::get<BencodeString>(BDecode("0:")) == "");
    }

    SECTION("Non-empty strings") {
        REQUIRE(std::get<BencodeString>(BDecode("3:foo")) == "foo");
        REQUIRE(std::get<BencodeString>(BDecode("5:hello")) == "hello");
        REQUIRE(std::get<BencodeString>(BDecode("6:foobar")) == "foobar");
    }

    SECTION("Invalid strings") {
        REQUIRE_THROWS(BDecode("3:foobar"));
        REQUIRE_THROWS(BDecode("3:f"));
    }

    SECTION("Invalid strings with trailing characters") {
        REQUIRE_THROWS(BDecode("3:foo4:bar0"));
        REQUIRE_THROWS(BDecode("3:foo4:bar0e"));
    }

    SECTION("Offsets") {
        BencodeItem item = BDecode("3:foo");
        REQUIRE(item.start() == 0);
        REQUIRE(item.len() == 5);
    }
}

TEST_CASE("Bencode: BDecode: Lists", "[Bencode][BDecode]") {
    SECTION("Empty lists") {
        REQUIRE(std::get<BencodeList>(BDecode("le")).empty());
    }

    SECTION("Non-empty lists") {
        auto list = std::get<BencodeList>(BDecode("l3:foo3:bare"));
        REQUIRE(list.size() == 2);
        REQUIRE(std::get<BencodeString>(list[0]) == "foo");
        REQUIRE(std::get<BencodeString>(list[1]) == "bar");

        list = std::get<BencodeList>(BDecode("l3:foo3:bari42ee"));
        REQUIRE(list.size() == 3);
        REQUIRE(std::get<BencodeString>(list[0]) == "foo");
        REQUIRE(std::get<BencodeString>(list[1]) == "bar");
        REQUIRE(std::get<BencodeInt>(list[2]) == 42);
    }

    SECTION("Invalid lists") {
        REQUIRE_THROWS(BDecode("l3:foo3:bar"));
        REQUIRE_THROWS(BDecode("l3:foo3:bar0"));
        REQUIRE_THROWS(BDecode("l3:foo3:bar0e"));
    }

    SECTION("Invalid lists with trailing characters") {
        REQUIRE_THROWS(BDecode("l3:foo3:bar0e0"));
        REQUIRE_THROWS(BDecode("l3:foo3:bar0e0e"));
    }

    SECTION("Offsets") {
        BencodeItem list_item = BDecode("l3:foo3:bare");
        REQUIRE(list_item.start() == 0);
        REQUIRE(list_item.len() == 12);

        BencodeItem str_item = std::get<BencodeList>(list_item)[0];
        REQUIRE(str_item.start() == 1);
        REQUIRE(str_item.len() == 5);

        str_item = std::get<BencodeList>(list_item)[1];
        REQUIRE(str_item.start() == 6);
        REQUIRE(str_item.len() == 5);
    }
}

TEST_CASE("Bencode: BDecode: Dictionaries", "[Bencode][BDecode]") {
    SECTION("Empty dictionaries") {
        REQUIRE(std::get<BencodeDict>(BDecode("de")).empty());
    }

    SECTION("Non-empty dictionaries") {
        auto dict = std::get<BencodeDict>(BDecode("d3:foo3:bar3:baz3:quxe"));
        REQUIRE(dict.size() == 2);
        REQUIRE(std::get<BencodeString>(dict["foo"]) == "bar");
        REQUIRE(std::get<BencodeString>(dict["baz"]) == "qux");

        dict = std::get<BencodeDict>(BDecode("d3:foo3:bar3:baz3:qux6:foobari42ee"));
        REQUIRE(dict.size() == 3);
        REQUIRE(std::get<BencodeString>(dict["foo"]) == "bar");
        REQUIRE(std::get<BencodeString>(dict["baz"]) == "qux");
        REQUIRE(std::get<BencodeInt>(dict["foobar"]) == 42);
    }

    SECTION("Invalid dictionaries") {
        REQUIRE_THROWS(BDecode("d3:foo3:bar3:baz3:qux"));
        REQUIRE_THROWS(BDecode("d3:foo3:bar3:baz3:qux0"));
        REQUIRE_THROWS(BDecode("d3:foo3:bar3:baz3:qux0e"));
    }

    SECTION("Invalid dictionaries with trailing characters") {
        REQUIRE_THROWS(BDecode("d3:foo3:bar3:baz3:qux0e0"));
        REQUIRE_THROWS(BDecode("d3:foo3:bar3:baz3:qux0e0e"));
    }

    SECTION("Offsets") {
        BencodeItem dict_item = BDecode("d3:foo3:bar3:bazi123ee");
        REQUIRE(dict_item.start() == 0);
        REQUIRE(dict_item.len() == 22);

        BencodeItem str_item = std::get<BencodeDict>(dict_item)["foo"];
        REQUIRE(str_item.start() == 6);
        REQUIRE(str_item.len() == 5);

        BencodeItem int_item = std::get<BencodeDict>(dict_item)["baz"];
        REQUIRE(int_item.start() == 16);
        REQUIRE(int_item.len() == 5);
    }
}

TEST_CASE("Bencode: BDecode: Mixt", "[Bencode][BDecode]") {
    SECTION("List of dictionaries") {
        BencodeItem list_item = BDecode("ld3:foo3:bar3:baz3:quxed3:foo3:bar3:bazi123eee");
        BencodeList list      = std::get<BencodeList>(list_item);
        REQUIRE(list.size() == 2);
        REQUIRE(std::get<BencodeDict>(list[0]).size() == 2);
        REQUIRE(std::get<BencodeDict>(list[1]).size() == 2);

        SECTION("Offsets") {
            REQUIRE(list_item.start() == 0);
            REQUIRE(list_item.len() == 46);

            BencodeItem dict_item = std::get<BencodeList>(list_item)[0];
            REQUIRE(dict_item.start() == 1);
            REQUIRE(dict_item.len() == 22);

            BencodeItem str_item = std::get<BencodeDict>(dict_item)["foo"];
            REQUIRE(str_item.start() == 7);
            REQUIRE(str_item.len() == 5);

            BencodeItem int_item = std::get<BencodeDict>(dict_item)["baz"];
            REQUIRE(int_item.start() == 17);
            REQUIRE(int_item.len() == 5);
        }
    }

    SECTION("Dictionary of lists") {
        BencodeItem dict_item = BDecode("d3:fool3:foo3:bar3:baz3:quxe3:bazlee");
        BencodeDict dict      = std::get<BencodeDict>(dict_item);
        REQUIRE(dict.size() == 2);
        REQUIRE(std::get<BencodeList>(dict["foo"]).size() == 4);
        REQUIRE(std::get<BencodeList>(dict["baz"]).size() == 0);

        SECTION("Offsets") {
            REQUIRE(dict_item.start() == 0);
            REQUIRE(dict_item.len() == 36);

            BencodeItem list_item = std::get<BencodeDict>(dict_item)["foo"];
            REQUIRE(list_item.start() == 6);
            REQUIRE(list_item.len() == 22);

            BencodeItem str_item = std::get<BencodeList>(list_item)[0];
            REQUIRE(str_item.start() == 7);
            REQUIRE(str_item.len() == 5);

            str_item = std::get<BencodeList>(list_item)[1];
            REQUIRE(str_item.start() == 12);
            REQUIRE(str_item.len() == 5);

            str_item = std::get<BencodeList>(list_item)[2];
            REQUIRE(str_item.start() == 17);
            REQUIRE(str_item.len() == 5);

            str_item = std::get<BencodeList>(list_item)[3];
            REQUIRE(str_item.start() == 22);
            REQUIRE(str_item.len() == 5);
        }
    }
}
