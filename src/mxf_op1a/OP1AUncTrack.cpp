/*
 * Copyright (C) 2011, British Broadcasting Corporation
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

#include <im/mxf_op1a/OP1AUncTrack.h>
#include <im/IMException.h>
#include <im/Logging.h>

using namespace std;
using namespace im;
using namespace mxfpp;



static const mxfKey VIDEO_ELEMENT_KEY = MXF_UNC_EE_K(0x01, MXF_UNC_FRAME_WRAPPED_EE_TYPE, 0x00);



OP1AUncTrack::OP1AUncTrack(OP1AFile *file, uint32_t track_index, uint32_t track_id, uint8_t track_type_number,
                           mxfRational frame_rate, OP1AEssenceType essence_type)
: OP1APictureTrack(file, track_index, track_id, track_type_number, frame_rate, essence_type)
{
    mUncDescriptorHelper = dynamic_cast<UncMXFDescriptorHelper*>(mDescriptorHelper);
    IM_ASSERT(mUncDescriptorHelper);

    mUncDescriptorHelper->SetComponentDepth(8);

    mTrackNumber = MXF_UNC_TRACK_NUM(0x01, MXF_UNC_FRAME_WRAPPED_EE_TYPE, 0x00);
    mEssenceElementKey = VIDEO_ELEMENT_KEY;
}

OP1AUncTrack::~OP1AUncTrack()
{
}

void OP1AUncTrack::SetComponentDepth(uint32_t depth)
{
    mUncDescriptorHelper->SetComponentDepth(depth);
}
