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

#ifndef __IM_UTILS_H__
#define __IM_UTILS_H__


#include <string>

#include <im/IMTypes.h>



namespace im
{


typedef enum
{
    ROUND_AUTO,
    ROUND_DOWN,
    ROUND_UP,
    ROUND_NEAREST
} Rounding;


/* General rules for rounding
        Position: lower rate sample is at or after the higher rate sample
            ROUND_UP    : converting TO the lower edit rate
            ROUND_DOWN  : converting FROM the lower edit rate
        Duration: lower rate sample contain only complete sets of higher rate samples
            ROUND_DOWN  : converting TO the lower edit rate
            ROUND_UP    : converting FROM the lower edit rate
   ROUND_AUTO follows the above rules
*/

int64_t convert_position(int64_t in_position, int64_t factor_top, int64_t factor_bottom, Rounding rounding);
int64_t convert_position(Rational in_edit_rate, int64_t in_position, Rational out_edit_rate, Rounding rounding);
int64_t convert_duration(int64_t in_duration, int64_t factor_top, int64_t factor_bottom, Rounding rounding);
int64_t convert_duration(Rational in_edit_rate, int64_t in_duration, Rational out_edit_rate, Rounding rounding);

std::string strip_path(std::string filename);
std::string strip_name(std::string filename);
std::string strip_suffix(std::string filename);

std::string get_abs_cwd();
std::string get_abs_filename(std::string base_dir, std::string filename);

Timestamp generate_timestamp_now();

UUID generate_uuid();
UMID generate_umid();

uint16_t get_rounded_tc_base(Rational rate);

std::string get_generic_duration_string(int64_t count, Rational rate);

Rational convert_int_to_rational(int32_t value);


void decode_smpte_timecode(Rational frame_rate, const unsigned char *smpte_tc, unsigned int size, 
                           Timecode *timecode, bool *field_mark);
void encode_smpte_timecode(Timecode timecode, bool field_mark, unsigned char *smpte_tc, unsigned int size);


};



#endif
