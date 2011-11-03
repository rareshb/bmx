/*
 * Copyright (C) 2010, British Broadcasting Corporation
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the British Broadcasting Corporation nor the names
 *       of its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define __STDC_FORMAT_MACROS

#include <cstring>
#include <cstdio>
#include <cerrno>

#if defined(_WIN32)
#include <sys/timeb.h>
#include <time.h>
#include <direct.h> // getcwd
#include <windows.h>
#else
#include <uuid/uuid.h>
#include <sys/time.h>
#endif

#include <im/Utils.h>
#include <im/URI.h>
#include <im/IMException.h>
#include <im/Logging.h>

using namespace std;


#define MAX_INT32   2147483647



int64_t im::convert_position(int64_t in_position, int64_t factor_top, int64_t factor_bottom, Rounding rounding)
{
    if (in_position == 0 || factor_top == factor_bottom)
        return in_position;

    if (in_position < 0)
        return -convert_position(-in_position, factor_top, factor_bottom, rounding);

    // don't expect factors to be > MAX_INT32. Expect to see numbers such as 25, 48000, 30000 (30000/1001)
    if (factor_top > MAX_INT32 || factor_bottom > MAX_INT32)
        log_warn("Convert position calculation may overflow\n");

    int64_t round_num = 0;
    if (rounding == ROUND_UP || (rounding == ROUND_AUTO && factor_top < factor_bottom))
        round_num = factor_bottom - 1;
    else if (rounding == ROUND_NEAREST)
        round_num = factor_bottom / 2;


    if (in_position <= MAX_INT32) {
        // no chance of overflow (assuming there exists a result that fits into int64_t)
        return (in_position * factor_top + round_num) / factor_bottom;
    } else {
        // separate the calculation into 2 parts so there is no chance of an overflow (assuming there exists
        // a result that fits into int64_t)
        // a*b/c = ((a/c)*c + a%c) * b) / c = (a/c)*b + (a%c)*b/c
        return (in_position / factor_bottom) * factor_top +
               ((in_position % factor_bottom) * factor_top + round_num) / factor_bottom;
    }
}

int64_t im::convert_position(Rational in_edit_rate, int64_t in_position, Rational out_edit_rate, Rounding rounding)
{
    return convert_position(in_position,
                            (int64_t)out_edit_rate.numerator * in_edit_rate.denominator,
                            (int64_t)out_edit_rate.denominator * in_edit_rate.numerator,
                            rounding);
}

int64_t im::convert_duration(int64_t in_duration, int64_t factor_top, int64_t factor_bottom, Rounding rounding)
{
    if (rounding == ROUND_AUTO) {
        return convert_position(in_duration, factor_top, factor_bottom,
                                (factor_top < factor_bottom ? ROUND_DOWN : ROUND_UP));
    } else {
        return convert_position(in_duration, factor_top, factor_bottom, rounding);
    }
}

int64_t im::convert_duration(Rational in_edit_rate, int64_t in_duration, Rational out_edit_rate, Rounding rounding)
{
    return convert_duration(in_duration,
                            (int64_t)out_edit_rate.numerator * in_edit_rate.denominator,
                            (int64_t)out_edit_rate.denominator * in_edit_rate.numerator,
                            rounding);
}

string im::strip_path(string filename)
{
    size_t sep_index;
    if ((sep_index = filename.rfind("/")) != string::npos)
        return filename.substr(sep_index + 1);
    else
        return filename;
}

string im::strip_name(string filename)
{
    size_t sep_index;
    if ((sep_index = filename.rfind("/")) != string::npos)
        return filename.substr(0, sep_index);
    else
        return "";
}

string im::strip_suffix(string filename)
{
    size_t suffix_index;
    if ((suffix_index = filename.rfind(".")) != string::npos)
        return filename.substr(0, suffix_index);
    else
        return filename;
}

string im::get_abs_cwd()
{
#define MAX_REASONABLE_PATH_SIZE    (10 * 1024)

    string abs_cwd;
    char *temp_path;
    size_t path_size = 1024;
    while (true) {
        temp_path = new char[path_size];
        if (getcwd(temp_path, path_size))
            break;
        if (errno == EEXIST)
            break;

        delete [] temp_path;
        if (errno != ERANGE) {
            throw IMException("Failed to get current working directory: %s", strerror(errno));
        } else if (path_size >= MAX_REASONABLE_PATH_SIZE) {
            throw IMException("Maximum path size (%d) for current working directory exceeded",
                               MAX_REASONABLE_PATH_SIZE, strerror(errno));
        }
        path_size += 1024;
    }
    abs_cwd = temp_path;
    delete [] temp_path;

    return abs_cwd;
}

string im::get_abs_filename(string base_dir, string filename)
{
    URI uri;
    uri.ParseFilename(filename);

    if (uri.IsRelative()) {
        URI base_uri;
        base_uri.ParseDirectory(base_dir);

        uri.MakeAbsolute(base_uri);
    }

    return uri.ToFilename();
}

im::Timestamp im::generate_timestamp_now()
{
    Timestamp now;

    struct tm gmt;
    time_t t = time(0);

#if defined(_MSC_VER)
    // TODO: need thread-safe (reentrant) version
    const struct tm *gmt_ptr = gmtime(&t);
    IM_CHECK(gmt_ptr);
    gmt = *gmt_ptr;
#else
    IM_CHECK(gmtime_r(&t, &gmt));
#endif

    now.year = gmt.tm_year + 1900;
    now.month = gmt.tm_mon + 1;
    now.day = gmt.tm_mday;
    now.hour = gmt.tm_hour;
    now.min = gmt.tm_min;
    now.sec = gmt.tm_sec;
    now.qmsec = 0;

    return now;
}

im::UUID im::generate_uuid()
{
    UUID im_uuid;

#if defined(_WIN32)
    GUID guid;
    CoCreateGuid(&guid);
    memcpy(&im_uuid, &guid, sizeof(im_uuid));
#else
    uuid_t uuid;
    uuid_generate(uuid);
    memcpy(&im_uuid, &uuid, sizeof(im_uuid));
#endif

    return im_uuid;
}

im::UMID im::generate_umid()
{
    // material type not identified, UUID material generation method, no instance method defined
    static const unsigned char umid_prefix[16] = {0x06, 0x0a, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x01, 0x01, 0x0f, 0x20,
                                                  0x13, 0x00, 0x00, 0x00};

    UMID umid;
    memcpy(&umid.octet0, umid_prefix, sizeof(umid_prefix));

    UUID material_number = generate_uuid();
    memcpy(&umid.octet16, &material_number, sizeof(material_number));

    return umid;
}

uint16_t im::get_rounded_tc_base(Rational rate)
{
    return (uint16_t)(rate.numerator / (double)rate.denominator + 0.5);
}

string im::get_generic_duration_string(int64_t count, Rational rate)
{
    if (count <= 0 || rate.numerator == 0 || rate.denominator == 0)
        return "00:00:00.00";

    Rational msec_rate = {1000, 1};
    int64_t msec = convert_position(rate, count, msec_rate, ROUND_DOWN);
    int64_t sec = msec / 1000;
    int64_t min = sec / 60;
    sec %= 60;
    int64_t hour = min / 60;
    min %= 60;
    int64_t sec_frac = 100 * (msec % 1000) / 1000;

    char buffer[64];
    sprintf(buffer, "%02"PRId64":%02d:%02d.%02d", hour, (int)min, (int)sec, (int)sec_frac);

    return buffer;
}

im::Rational im::convert_int_to_rational(int32_t value)
{
    Rational ret = {value, 1};
    return ret;
}

void im::decode_smpte_timecode(Rational frame_rate, const unsigned char *smpte_tc, unsigned int size,
                               Timecode *timecode, bool *field_mark)
{
    // see SMPTE 12M-1-2008 and SMPTE 331M-2004 section 8.2 for details

    IM_ASSERT(size >= 4);

    int hour, min, sec, frame;
    frame = ((smpte_tc[0] & 0x30) >> 4) * 10 + (smpte_tc[0] & 0x0f);
    sec   = ((smpte_tc[1] & 0x70) >> 4) * 10 + (smpte_tc[1] & 0x0f);
    min   = ((smpte_tc[2] & 0x70) >> 4) * 10 + (smpte_tc[2] & 0x0f);
    hour  = ((smpte_tc[3] & 0x30) >> 4) * 10 + (smpte_tc[3] & 0x0f);

    bool drop_frame = (smpte_tc[0] & 0x40);

    uint16_t tc_base = get_rounded_tc_base(frame_rate);
    if (tc_base > 30) {
        frame *= 2;

        // vitc field mark flag indicates first or second of frame pair
        // the preferred method is to have the flag set to 1 for the second frame
        if ((tc_base == 50 && (smpte_tc[3] & 0x80)) ||
            (tc_base == 60 && (smpte_tc[1] & 0x80)))
        {
            *field_mark = true;
        }
        else
        {
            *field_mark = false;
        }
    } else {
        *field_mark = false;
    }

    timecode->Init(frame_rate, drop_frame, hour, min, sec, frame);
}

void im::encode_smpte_timecode(Timecode timecode, bool field_mark, unsigned char *smpte_tc, unsigned int size)
{
    // see SMPTE 12M-1-2008 and SMPTE 331M-2004 section 8.2 for details

    IM_ASSERT(size >= 4);

    int frame = timecode.GetFrame();
    if (timecode.GetRoundedTCBase() > 30)
        frame /= 2;

    smpte_tc[0] = ((frame              % 10) & 0x0f) | (((frame              / 10) << 4) & 0x30);
    smpte_tc[1] = ((timecode.GetSec()  % 10) & 0x0f) | (((timecode.GetSec()  / 10) << 4) & 0x70);
    smpte_tc[2] = ((timecode.GetMin()  % 10) & 0x0f) | (((timecode.GetMin()  / 10) << 4) & 0x70);
    smpte_tc[3] = ((timecode.GetHour() % 10) & 0x0f) | (((timecode.GetHour() / 10) << 4) & 0x30);

    if (timecode.IsDropFrame())
        smpte_tc[0] |= 0x40;

    if (field_mark && timecode.GetRoundedTCBase() > 30 && timecode.GetFrame() % 2 == 1) {
        // indicate second frame of pair use the vitc field mark flag
        switch (timecode.GetRoundedTCBase())
        {
            case 50:
                smpte_tc[3] |= 0x80;
                break;
            case 60:
                smpte_tc[1] |= 0x80;
                break;
            default:
                break;
        }
    }

    if (size > 4)
        memset(&smpte_tc[4], 0, size - 4);
}
