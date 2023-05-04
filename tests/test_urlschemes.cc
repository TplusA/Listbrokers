/*
 * Copyright (C) 2017, 2018, 2019, 2022  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of T+A List Brokers.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cppcutter.h>
#include <array>
#include <string>

#include "mock_messages.hh"

#include "strbo_url_usb.hh"

/*!
 * \addtogroup url_schemes_tests Unit tests
 * \ingroup url_schemes
 *
 * Unit tests for Streaming Board URLs.
 */
/*!@{*/

namespace url_schemes_tests
{

static MockMessages *mock_messages;

namespace usb_simple_key
{

void cut_setup()
{
    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;
}

void cut_teardown()
{
    mock_messages->check();
    mock_messages_singleton = nullptr;
    delete mock_messages;
    mock_messages = nullptr;
}

void test_default_ctor_creates_empty_url()
{
    USB::LocationKeySimple key;

    const std::string url(key.str());
    cut_assert_true(url.empty());
}

void test_construct_by_setting_device_and_partition_and_path()
{
    USB::LocationKeySimple key;

    key.set_device("my-device");
    key.set_partition("my-partition");
    key.set_path("path/to/some/location");

    const std::string url(key.str());
    cppcut_assert_equal("strbo-usb://my-device:my-partition/path%2Fto%2Fsome%2Flocation",
                        url.c_str());
}

void test_construct_by_setting_device_and_partition_and_path_fragments()
{
    USB::LocationKeySimple key;

    key.set_device("my-device");
    key.set_partition("my-partition");
    key.append_to_path("path");
    key.append_to_path("to");
    key.append_to_path("some");
    key.append_to_path("location");

    const std::string url(key.str());
    cppcut_assert_equal("strbo-usb://my-device:my-partition/path%2Fto%2Fsome%2Flocation",
                        url.c_str());
}

void test_construct_with_missing_path_yields_empty_string()
{
    USB::LocationKeySimple key;

    key.set_device("my-device");
    key.set_partition("my-partition");

    const std::string url(key.str());
    cut_assert_true(url.empty());
}

void test_construct_with_empty_path()
{
    USB::LocationKeySimple key;

    key.set_device("my-device");
    key.set_partition("my-partition");
    key.set_path("");

    const std::string url(key.str());
    cppcut_assert_equal("strbo-usb://my-device:my-partition/", url.c_str());
}

void test_construct_with_empty_partition_and_path()
{
    USB::LocationKeySimple key;

    key.set_device("my-device");
    key.set_partition("");
    key.set_path("");

    const std::string url(key.str());
    cppcut_assert_equal("strbo-usb://my-device:/", url.c_str());
}

void test_construct_with_missing_device_yields_empty_string()
{
    USB::LocationKeySimple key;

    key.set_partition("my-partition");
    key.set_path("path/to/some/location");

    const std::string url(key.str());
    cut_assert_true(url.empty());
}

void test_construct_with_missing_partition_yields_empty_string()
{
    USB::LocationKeySimple key;

    key.set_device("my-device");
    key.set_path("path/to/some/location");

    const std::string url(key.str());
    cut_assert_true(url.empty());
}

void test_construct_with_empty_partition_yields_empty_string()
{
    USB::LocationKeySimple key;

    key.set_partition("");
    key.set_path("path/to/some/location");

    const std::string url(key.str());
    cut_assert_true(url.empty());
}

void test_parse_valid_location_key()
{
    static const std::string expected_url("strbo-usb://dev-uuid:some-partition-id/this%2Fis%2Fmy%2Fstream.flac");

    USB::LocationKeySimple key;
    cut_assert_null(key.set_url(expected_url));

    const std::string url(key.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_empty_path_is_acceptable()
{
    static const std::string expected_url("strbo-usb://dev-uuid:partition/");

    USB::LocationKeySimple key;
    cut_assert_null(key.set_url(expected_url));

    const std::string url(key.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_empty_partition_and_path_is_acceptable()
{
    static const std::string expected_url("strbo-usb://dev-uuid:/");

    USB::LocationKeySimple key;
    cut_assert_null(key.set_url(expected_url));

    const std::string url(key.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_empty_device_is_rejected()
{
    static const std::string broken_url("strbo-usb://:part/my/path");

    USB::LocationKeySimple key;
    try
    {
        key.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Simple USB location key malformed: Component empty [Device]",
                            e.what());
    }

    static const std::string expected_url("strbo-usb://x:y/z");
    key.set_device("x");
    key.set_partition("y");
    key.set_path("z");

    std::string url(key.str());
    cppcut_assert_equal(expected_url, url);

    try
    {
        key.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Simple USB location key malformed: Component empty [Device]",
                            e.what());
    }

    cppcut_assert_equal(expected_url, url);
}

void test_url_with_missing_partition_is_rejected()
{
    static const std::string broken_url("strbo-usb://device/my/path");

    USB::LocationKeySimple key;
    try
    {
        key.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Simple USB location key malformed: No ':' found [Device]",
                            e.what());
    }

    static const std::string expected_url("strbo-usb://x:y/z");
    key.set_device("x");
    key.set_partition("y");
    key.set_path("z");

    std::string url(key.str());
    cppcut_assert_equal(expected_url, url);

    try
    {
        key.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Simple USB location key malformed: No ':' found [Device]",
                            e.what());
    }

    cppcut_assert_equal(expected_url, url);
}

void test_url_with_no_slash_is_rejected()
{
    static const std::string broken_url("strbo-usb://device:");

    USB::LocationKeySimple key;
    try
    {
        key.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Simple USB location key malformed: No '/' found [Partition]",
                            e.what());
    }

    static const std::string expected_url("strbo-usb://x:y/z");
    key.set_device("x");
    key.set_partition("y");
    key.set_path("z");

    std::string url(key.str());
    cppcut_assert_equal(expected_url, url);

    try
    {
        key.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Simple USB location key malformed: No '/' found [Partition]",
                            e.what());
    }

    cppcut_assert_equal(expected_url, url);
}

void test_unpack_url()
{
    static const std::array<std::pair<const std::string,
                                      const USB::LocationKeySimple::Components>, 5> test_data
    {
        std::make_pair("strbo-usb://dev:p1/y%2Fz",
                       USB::LocationKeySimple::Components("dev", "p1", "y/z")),
        std::make_pair("strbo-usb://d:part/a%2Fb%2Fcde%2Fit",
                       USB::LocationKeySimple::Components("d", "part", "a/b/cde/it")),
        std::make_pair("strbo-usb://dev:part1/",
                       USB::LocationKeySimple::Components("dev", "part1", "")),
        std::make_pair("strbo-usb://device:/",
                       USB::LocationKeySimple::Components("device", "", "")),
        std::make_pair("strbo-usb://usb-device:data/Metallica%2FHardwired%E2%80%A6To%20Self-Destruct%20%28Deluxe%29%2FCD1%2F03%20-%20Now%20That%20We%E2%80%99re%20Dead.flac",
                       USB::LocationKeySimple::Components("usb-device", "data",
                                "Metallica/Hardwired…To Self-Destruct (Deluxe)/CD1/"
                                "03 - Now That We’re Dead.flac")),
    };

    for(const auto &d : test_data)
    {
        USB::LocationKeySimple key;

        cut_assert_null(key.set_url(d.first));
        cut_assert_true(key.is_valid());

        const auto &components(key.unpack());

        cppcut_assert_equal(d.second.device_,    components.device_);
        cppcut_assert_equal(d.second.partition_, components.partition_);
        cppcut_assert_equal(d.second.path_,      components.path_);
    }
}

}

namespace usb_reference_key
{

void cut_setup()
{
    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;
}

void cut_teardown()
{
    mock_messages->check();
    mock_messages_singleton = nullptr;
    delete mock_messages;
    mock_messages = nullptr;
}

void test_default_ctor_creates_empty_url()
{
    USB::LocationKeyReference key;

    const std::string url(key.str());
    cut_assert_true(url.empty());
}

void test_construct_by_setting_components()
{
    USB::LocationKeyReference key;

    key.set_device("my-device");
    key.set_partition("my-partition");
    key.set_reference_point("path/to/some");
    key.set_item("location", StrBoUrl::ObjectIndex(4));

    const std::string url(key.str());
    cppcut_assert_equal("strbo-ref-usb://my-device:my-partition/path%2Fto%2Fsome/location:4",
                        url.c_str());
}

void test_construct_by_setting_components_with_path_fragments()
{
    USB::LocationKeyReference key;

    key.set_device("my-device");
    key.set_partition("my-partition");
    key.append_to_reference_point("path");
    key.append_to_reference_point("to");
    key.append_to_reference_point("some");
    key.set_item("location", StrBoUrl::ObjectIndex(4));

    const std::string url(key.str());
    cppcut_assert_equal("strbo-ref-usb://my-device:my-partition/path%2Fto%2Fsome/location:4",
                        url.c_str());
}

void test_construct_with_missing_location_yields_empty_string()
{
    USB::LocationKeyReference key;

    key.set_device("my-device");
    key.set_partition("my-partition");
    key.set_reference_point("path/to/somewhere");

    const std::string url(key.str());
    cut_assert_true(url.empty());
}

void test_construct_with_missing_reference_point_yields_empty_string()
{
    USB::LocationKeyReference key;

    key.set_device("my-device");
    key.set_partition("my-partition");
    key.set_item("root.mp3", StrBoUrl::ObjectIndex(2));

    const std::string url(key.str());
    cut_assert_true(url.empty());
}

void test_construct_with_path_as_item_yields_empty_string()
{
    USB::LocationKeyReference key;

    key.set_device("my-device");
    key.set_partition("my-partition");
    key.set_reference_point("path/to");
    key.set_item("somewhere/stream.mp3", StrBoUrl::ObjectIndex(7));

    const std::string url(key.str());
    cut_assert_true(url.empty());
}

void test_construct_with_empty_reference_point()
{
    USB::LocationKeyReference key;

    key.set_device("my-device");
    key.set_partition("my-partition");
    key.set_reference_point("");
    key.set_item("stream.flac", StrBoUrl::ObjectIndex(3));

    const std::string url(key.str());
    cppcut_assert_equal("strbo-ref-usb://my-device:my-partition//stream.flac:3", url.c_str());
}

void test_construct_reference_to_partition_on_device()
{
    USB::LocationKeyReference key;

    key.set_device("dev");
    key.set_partition("part");
    key.set_reference_point("");
    key.set_item("", StrBoUrl::ObjectIndex(0));

    const std::string url(key.str());
    cppcut_assert_equal("strbo-ref-usb://dev:part//:0", url.c_str());
}

void test_construct_reference_to_root_directory_of_partition()
{
    USB::LocationKeyReference key;

    key.set_device("dev");
    key.set_partition("part5");
    key.set_reference_point("");
    key.set_item("", StrBoUrl::ObjectIndex(5));

    const std::string url(key.str());
    cppcut_assert_equal("strbo-ref-usb://dev:part5//:5", url.c_str());
}

void test_construct_reference_to_fs_partition_on_device()
{
    USB::LocationKeyReference key;

    key.set_device("dev");
    key.set_partition("");
    key.set_reference_point("");
    key.set_item("", StrBoUrl::ObjectIndex(0));

    const std::string url(key.str());
    cppcut_assert_equal("strbo-ref-usb://dev://:0", url.c_str());
}

void test_construct_reference_to_root_directory_of_fs_on_device()
{
    USB::LocationKeyReference key;

    key.set_device("dev");
    key.set_partition("");
    key.set_reference_point("");
    key.set_item("", StrBoUrl::ObjectIndex(1));

    const std::string url(key.str());
    cppcut_assert_equal("strbo-ref-usb://dev://:1", url.c_str());
}

void test_parse_valid_location_key()
{
    static const std::string expected_url("strbo-ref-usb://dev-id:some-partition-id/this%2Fis%2Fmy/stream.flac:5");

    USB::LocationKeyReference key;
    cut_assert_null(key.set_url(expected_url));

    const std::string url(key.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_reference_to_partition_entry()
{
    static const std::string expected_url("strbo-ref-usb://dev-id:part-id//:0");

    USB::LocationKeyReference key;
    cut_assert_null(key.set_url(expected_url));

    const std::string url(key.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_reference_to_root_directory_of_partition()
{
    static const std::string expected_url("strbo-ref-usb://dev-id:part-id//:4");

    USB::LocationKeyReference key;
    cut_assert_null(key.set_url(expected_url));

    const std::string url(key.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_reference_to_fs_partition_on_device()
{
    static const std::string expected_url("strbo-ref-usb://dev-id://:0");

    USB::LocationKeyReference key;
    cut_assert_null(key.set_url(expected_url));

    const std::string url(key.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_reference_to_root_directory_of_fs_on_device()
{
    static const std::string expected_url("strbo-ref-usb://dev-id://:1");

    USB::LocationKeyReference key;
    cut_assert_null(key.set_url(expected_url));

    const std::string url(key.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_empty_partition_is_acceptable()
{
    static const std::string expected_url("strbo-ref-usb://dev://item:5");

    USB::LocationKeyReference key;
    cut_assert_null(key.set_url(expected_url));

    const std::string url(key.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_empty_path_is_acceptable()
{
    static const std::string expected_url("strbo-ref-usb://dev:partition//item:2");

    USB::LocationKeyReference key;
    cut_assert_null(key.set_url(expected_url));

    const std::string url(key.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_missing_partition_is_rejected()
{
    static const std::string broken_url("strbo-ref-usb://device/my/path:8");

    USB::LocationKeyReference key;
    try
    {
        key.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Reference USB location key malformed: Failed parsing device and partition [URL]",
                            e.what());
    }

    static const std::string expected_url("strbo-ref-usb://d:x/y/z:7");
    key.set_device("d");
    key.set_partition("x");
    key.set_reference_point("y");
    key.set_item("z", StrBoUrl::ObjectIndex(7));

    std::string url(key.str());
    cppcut_assert_equal(expected_url, url);

    try
    {
        key.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Reference USB location key malformed: Failed parsing device and partition [URL]",
                            e.what());
    }

    cppcut_assert_equal(expected_url, url);
}

void test_url_with_missing_item_name_is_rejected()
{
    static const std::string broken_url("strbo-ref-usb://device:part/my%2Fpath/:7");

    USB::LocationKeyReference key;
    try
    {
        key.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Reference USB location key malformed: Component empty [Item name]",
                            e.what());
    }
}

void test_url_with_path_in_item_field_is_rejected()
{
    static const std::string broken_url("strbo-ref-usb://dev-id:my-partition/this%2Fis/my%2Fstream.flac:3");

    USB::LocationKeyReference key;
    try
    {
        key.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Reference USB location key malformed: Component is a path [Item component]",
                            e.what());
    }
}

void test_url_with_no_slash_is_rejected()
{
    static const std::string broken_url_1("strbo-ref-usb://device:partition");
    static const std::string broken_url_2("strbo-ref-usb://device:partition/item:3");

    USB::LocationKeyReference key;

    try
    {
        key.set_url(broken_url_1);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Reference USB location key malformed: No '/' found [Partition]",
                            e.what());
    }

    try
    {
        key.set_url(broken_url_2);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Reference USB location key malformed: No '/' found [Reference point]",
                            e.what());
    }

    static const std::string expected_url("strbo-ref-usb://d:x/y/z:9");
    key.set_device("d");
    key.set_partition("x");
    key.set_reference_point("y");
    key.set_item("z", StrBoUrl::ObjectIndex(9));

    std::string url(key.str());
    cppcut_assert_equal(expected_url, url);

    try
    {
        key.set_url(broken_url_1);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("Reference USB location key malformed: No '/' found [Partition]",
                            e.what());
    }

    cppcut_assert_equal(expected_url, url);
}

void test_unpack_url()
{
    static const std::array<std::pair<const std::string,
                                      const USB::LocationKeyReference::Components>, 7> test_data
    {
        std::make_pair("strbo-ref-usb://dev:p1/y/z:8",
                       USB::LocationKeyReference::Components("dev", "p1", "y", "z", StrBoUrl::ObjectIndex(8))),
        std::make_pair("strbo-ref-usb://d:part/a%2Fb%2Fcde/it:6",
                       USB::LocationKeyReference::Components("d", "part", "a/b/cde", "it", StrBoUrl::ObjectIndex(6))),
        std::make_pair("strbo-ref-usb://dev:part1//:0",
                       USB::LocationKeyReference::Components("dev", "part1", "", "", StrBoUrl::ObjectIndex(0))),
        std::make_pair("strbo-ref-usb://d:p//:1",
                       USB::LocationKeyReference::Components("d", "p", "", "", StrBoUrl::ObjectIndex(1))),
        std::make_pair("strbo-ref-usb://device://:0",
                       USB::LocationKeyReference::Components("device", "", "", "", StrBoUrl::ObjectIndex(0))),
        std::make_pair("strbo-ref-usb://my-device://:1",
                       USB::LocationKeyReference::Components("my-device", "", "", "", StrBoUrl::ObjectIndex(1))),
        std::make_pair("strbo-ref-usb://usb-device:data/Metallica%2FHardwired%E2%80%A6To%20Self-Destruct%20%28Deluxe%29%2FCD1/03%20-%20Now%20That%20We%E2%80%99re%20Dead.flac:3",
                       USB::LocationKeyReference::Components("usb-device", "data",
                                "Metallica/Hardwired…To Self-Destruct (Deluxe)/CD1",
                                "03 - Now That We’re Dead.flac", StrBoUrl::ObjectIndex(3))),
    };

    for(const auto &d : test_data)
    {
        USB::LocationKeyReference key;

        cut_assert_null(key.set_url(d.first));
        cut_assert_true(key.is_valid());

        const auto &components(key.unpack());

        cppcut_assert_equal(d.second.device_,          components.device_);
        cppcut_assert_equal(d.second.partition_,       components.partition_);
        cppcut_assert_equal(d.second.reference_point_, components.reference_point_);
        cppcut_assert_equal(d.second.item_name_,       components.item_name_);
        cppcut_assert_equal(d.second.item_position_.get_object_index(),
                            components.item_position_.get_object_index());
    }
}

}

namespace usb_location_trace
{

void cut_setup()
{
    mock_messages = new MockMessages;
    cppcut_assert_not_null(mock_messages);
    mock_messages->init();
    mock_messages_singleton = mock_messages;
}

void cut_teardown()
{
    mock_messages->check();
    mock_messages_singleton = nullptr;
    delete mock_messages;
    mock_messages = nullptr;
}

void test_default_ctor_creates_empty_url()
{
    USB::LocationTrace trace;

    const std::string url(trace.str());
    cut_assert_true(url.empty());
}

void test_construct_by_setting_components()
{
    USB::LocationTrace trace;

    trace.set_device("my-device");
    trace.set_partition("my-partition");
    trace.set_reference_point("full/path/to/some");
    trace.set_item("deeply/nested/location", StrBoUrl::ObjectIndex(17));

    const std::string url(trace.str());
    cppcut_assert_equal("strbo-trace-usb://my-device:my-partition/full%2Fpath%2Fto%2Fsome/deeply%2Fnested%2Flocation:17",
                        url.c_str());
}

void test_construct_by_setting_components_with_path_fragments()
{
    USB::LocationTrace trace;

    trace.set_device("my-device");
    trace.set_partition("my-partition");
    trace.append_to_reference_point("full");
    trace.append_to_reference_point("path");
    trace.append_to_reference_point("to");
    trace.append_to_reference_point("some");
    trace.set_item("deeply/nested/location", StrBoUrl::ObjectIndex(17));

    const std::string url(trace.str());
    cppcut_assert_equal("strbo-trace-usb://my-device:my-partition/full%2Fpath%2Fto%2Fsome/deeply%2Fnested%2Flocation:17",
                        url.c_str());
}

void test_construct_by_setting_components_with_item_fragments()
{
    USB::LocationTrace trace;

    trace.set_device("my-device");
    trace.set_partition("my-partition");
    trace.set_reference_point("full/path/to/some");
    trace.append_to_item_path("deeply");
    trace.append_to_item_path("nested");
    trace.append_item("location", StrBoUrl::ObjectIndex(17));

    const std::string url(trace.str());
    cppcut_assert_equal("strbo-trace-usb://my-device:my-partition/full%2Fpath%2Fto%2Fsome/deeply%2Fnested%2Flocation:17",
                        url.c_str());
}

void test_construct_with_missing_location_yields_empty_string()
{
    USB::LocationTrace trace;

    trace.set_device("my-device");
    trace.set_partition("my-partition");
    trace.set_reference_point("path/to/somewhere");

    const std::string url(trace.str());
    cut_assert_true(url.empty());
}

void test_construct_with_missing_reference_point()
{
    USB::LocationTrace trace;

    trace.set_device("my-device");
    trace.set_partition("my-partition");
    trace.set_item("root.mp3", StrBoUrl::ObjectIndex(2));

    const std::string url(trace.str());
    cppcut_assert_equal("strbo-trace-usb://my-device:my-partition/root.mp3:2", url.c_str());
}

void test_construct_with_root_reference_point()
{
    USB::LocationTrace trace;

    trace.set_device("my-device");
    trace.set_partition("my-partition");
    trace.set_reference_point("/");
    trace.set_item("stream.flac", StrBoUrl::ObjectIndex(3));

    const std::string url(trace.str());
    cppcut_assert_equal("strbo-trace-usb://my-device:my-partition/stream.flac:3", url.c_str());
}

void test_construct_trace_to_partition_entry()
{
    USB::LocationTrace trace;

    trace.set_device("my-device");
    trace.set_partition("my-partition");
    trace.set_reference_point("");
    trace.set_item("", StrBoUrl::ObjectIndex(0));

    const std::string url(trace.str());
    cppcut_assert_equal("strbo-trace-usb://my-device:my-partition/:0", url.c_str());
}

void test_construct_trace_to_root_directory_of_partition()
{
    USB::LocationTrace trace;

    trace.set_device("my-device");
    trace.set_partition("my-partition-3");
    trace.set_reference_point("/");
    trace.set_item("", StrBoUrl::ObjectIndex(3));

    const std::string url(trace.str());
    cppcut_assert_equal("strbo-trace-usb://my-device:my-partition-3/:3", url.c_str());
}

void test_construct_trace_to_fs_partition_on_device()
{
    USB::LocationTrace trace;

    trace.set_device("my-device");
    trace.set_partition("");
    trace.set_reference_point("");
    trace.set_item("", StrBoUrl::ObjectIndex(0));

    const std::string url(trace.str());
    cppcut_assert_equal("strbo-trace-usb://my-device:/:0", url.c_str());
}

void test_construct_trace_to_root_directory_of_fs_on_device()
{
    USB::LocationTrace trace;

    trace.set_device("my-device");
    trace.set_partition("");
    trace.set_reference_point("");
    trace.set_item("", StrBoUrl::ObjectIndex(1));

    const std::string url(trace.str());
    cppcut_assert_equal("strbo-trace-usb://my-device:/:1", url.c_str());
}

void test_parse_valid_location_trace()
{
    static const std::string expected_url("strbo-trace-usb://the-device:some-partition-id/this%2Fis%2Fmy/traced%2Fstream.flac:5");

    USB::LocationTrace trace;
    cut_assert_null(trace.set_url(expected_url));

    const std::string url(trace.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_empty_partition_is_acceptable()
{
    static const std::string expected_url("strbo-trace-usb://dev:/item:6");

    USB::LocationTrace trace;
    cut_assert_null(trace.set_url(expected_url));

    const std::string url(trace.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_missing_reference_is_acceptable()
{
    static const std::string expected_url("strbo-trace-usb://dev:partition/item:2");

    USB::LocationTrace trace;
    cut_assert_null(trace.set_url(expected_url));

    const std::string url(trace.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_reference_to_partition_entry()
{
    static const std::string expected_url("strbo-trace-usb://dev-id:part-id/:0");

    USB::LocationTrace trace;
    cut_assert_null(trace.set_url(expected_url));

    const std::string url(trace.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_reference_to_root_directory_of_partition()
{
    static const std::string expected_url("strbo-trace-usb://dev-id:part-id/:4");

    USB::LocationTrace trace;
    cut_assert_null(trace.set_url(expected_url));

    const std::string url(trace.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_reference_to_fs_partition_on_device()
{
    static const std::string expected_url("strbo-trace-usb://dev-id:/:0");

    USB::LocationTrace trace;
    cut_assert_null(trace.set_url(expected_url));

    const std::string url(trace.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_reference_to_root_directory_of_fs_on_device()
{
    static const std::string expected_url("strbo-trace-usb://dev-id:/:1");

    USB::LocationTrace trace;
    cut_assert_null(trace.set_url(expected_url));

    const std::string url(trace.str());
    cppcut_assert_equal(expected_url, url);
}

void test_url_with_missing_partition_is_rejected()
{
    static const std::string broken_url("strbo-trace-usb://the-device/my/path:8");

    USB::LocationTrace trace;
    try
    {
        trace.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("USB location trace malformed: Failed parsing device and partition [URL]",
                            e.what());
    }

    static const std::string expected_url("strbo-trace-usb://d:x/y/z:7");
    trace.set_device("d");
    trace.set_partition("x");
    trace.set_reference_point("y");
    trace.set_item("z", StrBoUrl::ObjectIndex(7));

    std::string url(trace.str());
    cppcut_assert_equal(expected_url, url);

    try
    {
        trace.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("USB location trace malformed: Failed parsing device and partition [URL]",
                            e.what());
    }

    cppcut_assert_equal(expected_url, url);
}

void test_url_with_missing_item_name_is_rejected()
{
    static const std::string broken_url("strbo-trace-usb://device:part/path/:7");

    USB::LocationTrace trace;
    try
    {
        trace.set_url(broken_url);
        cut_fail("Expected StrBoUrl::Location::ParsingError");
    }
    catch(const StrBoUrl::Location::ParsingError &e)
    {
        cppcut_assert_equal("USB location trace malformed: Component empty [Item name]",
                            e.what());
    }
}

void test_url_with_explicit_root_reference_is_acceptable_but_yields_warning()
{
    static const std::string funny_url("strbo-trace-usb://d:partition/%2F/item:3");
    static const std::string expected_url("strbo-trace-usb://d:partition/item:3");

    USB::LocationTrace trace;

    cppcut_assert_equal("USB location trace contains unneeded explicit reference to root",
                        trace.set_url(funny_url));

    std::string url(trace.str());
    cppcut_assert_equal(expected_url, url);
}

void test_unpack_url()
{
    static const std::array<std::pair<const std::string,
                                      const USB::LocationTrace::Components>, 9> test_data
    {
        std::make_pair("strbo-trace-usb://dev:p1/y/z:8",
                       USB::LocationTrace::Components("dev", "p1", "y", "z", StrBoUrl::ObjectIndex(8))),
        std::make_pair("strbo-trace-usb://d:part/a%2Fb%2Fcde/it:6",
                       USB::LocationTrace::Components("d", "part", "a/b/cde", "it", StrBoUrl::ObjectIndex(6))),
        std::make_pair("strbo-trace-usb://dev:part1/:0",
                       USB::LocationTrace::Components("dev", "part1", "", "", StrBoUrl::ObjectIndex(0))),
        std::make_pair("strbo-trace-usb://d:p/:1",
                       USB::LocationTrace::Components("d", "p", "", "", StrBoUrl::ObjectIndex(1))),
        std::make_pair("strbo-trace-usb://device:/:0",
                       USB::LocationTrace::Components("device", "", "", "", StrBoUrl::ObjectIndex(0))),
        std::make_pair("strbo-trace-usb://my-device:/:1",
                       USB::LocationTrace::Components("my-device", "", "", "", StrBoUrl::ObjectIndex(1))),
        std::make_pair("strbo-trace-usb://usb-device:data/Metallica%2FHardwired%E2%80%A6To%20Self-Destruct%20%28Deluxe%29%2FCD1/03%20-%20Now%20That%20We%E2%80%99re%20Dead.flac:3",
                       USB::LocationTrace::Components("usb-device", "data",
                                "Metallica/Hardwired…To Self-Destruct (Deluxe)/CD1",
                                "03 - Now That We’re Dead.flac", StrBoUrl::ObjectIndex(3))),
        std::make_pair("strbo-trace-usb://usb-device:data/Metallica%2FHardwired%E2%80%A6To%20Self-Destruct%20%28Deluxe%29/CD1%2F03%20-%20Now%20That%20We%E2%80%99re%20Dead.flac:3",
                       USB::LocationTrace::Components("usb-device", "data",
                                "Metallica/Hardwired…To Self-Destruct (Deluxe)",
                                "CD1/03 - Now That We’re Dead.flac", StrBoUrl::ObjectIndex(3))),
        std::make_pair("strbo-trace-usb://usb-device:data/Metallica%2FHardwired%E2%80%A6To%20Self-Destruct%20%28Deluxe%29%2FCD1%2F03%20-%20Now%20That%20We%E2%80%99re%20Dead.flac:3",
                       USB::LocationTrace::Components("usb-device", "data", "",
                                "Metallica/Hardwired…To Self-Destruct (Deluxe)/CD1/03 - Now That We’re Dead.flac", StrBoUrl::ObjectIndex(3))),
    };

    for(const auto &d : test_data)
    {
        USB::LocationTrace trace;

        cut_assert_null(trace.set_url(d.first));
        cut_assert_true(trace.is_valid());

        const auto &components(trace.unpack());

        cppcut_assert_equal(d.second.device_,          components.device_);
        cppcut_assert_equal(d.second.partition_,       components.partition_);
        cppcut_assert_equal(d.second.reference_point_, components.reference_point_);
        cppcut_assert_equal(d.second.item_name_,       components.item_name_);
        cppcut_assert_equal(d.second.item_position_.get_object_index(),
                            components.item_position_.get_object_index());
    }
}

}

}

/*!@}*/
