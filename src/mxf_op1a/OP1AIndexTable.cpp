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

#include <algorithm>

#include <im/mxf_op1a/OP1AContentPackage.h>
#include <im/MXFUtils.h>
#include <im/IMException.h>
#include <im/Logging.h>

using namespace std;
using namespace im;
using namespace mxfpp;



// MAX_INDEX_SEGMENT_SIZE <
//      (65535 [2-byte max len]
//        - (80 [segment header] + 12 [delta entry array header] + 6 [delta entry] + 22 [index entry array header]))
#define MAX_INDEX_SEGMENT_SIZE      65000
#define INDEX_ENTRIES_INCREMENT     250

#define MAX_GOP_SIZE_GUESS          30

#define MAX_CACHE_ENTRIES           250



static bool compare_element(const OP1AIndexTableElement *left, const OP1AIndexTableElement *right)
{
    return left->is_picture && !right->is_picture;
}



OP1AIndexEntry::OP1AIndexEntry()
{
    temporal_offset = 0;
    key_frame_offset = 0;
    flags = 0;
    can_start_partition = true;
}

OP1AIndexEntry::OP1AIndexEntry(int8_t temporal_offset_, int8_t key_frame_offset_, uint8_t flags_,
                               bool can_start_partition_)
{
    temporal_offset = temporal_offset_;
    key_frame_offset = key_frame_offset_;
    flags = flags_;
    can_start_partition = can_start_partition_;
}

bool OP1AIndexEntry::IsDefault()
{
    return temporal_offset == 0 && key_frame_offset == 0 && flags == 0;
}

bool OP1AIndexEntry::IsCompatible(const OP1AIndexEntry &entry)
{
    // compatible if current entry is the default entry or the new entry equals the current entry
    return (temporal_offset == 0 && key_frame_offset == 0 && flags == 0) ||
           (temporal_offset == entry.temporal_offset && key_frame_offset == entry.key_frame_offset &&
               flags == entry.flags);
}



OP1ADeltaEntry::OP1ADeltaEntry()
{
    pos_table_index = 0;
    slice = 0;
    element_delta = 0;
}



OP1AIndexTableElement::OP1AIndexTableElement(uint32_t track_index_, bool is_picture_, bool is_cbe_,
                                             bool apply_temporal_reordering_)
{
    track_index = track_index_;
    is_picture = is_picture_;
    is_cbe = is_cbe_;
    apply_temporal_reordering = apply_temporal_reordering_;
    slice_offset = 0;
    element_size = 0;
}

void OP1AIndexTableElement::CacheIndexEntry(int64_t position, int8_t temporal_offset, int8_t key_frame_offset,
                                            uint8_t flags, bool can_start_partition)
{
    IM_CHECK(mIndexEntryCache.size() <= MAX_CACHE_ENTRIES);

    mIndexEntryCache[position] = OP1AIndexEntry(temporal_offset, key_frame_offset, flags, can_start_partition);
}

void OP1AIndexTableElement::UpdateIndexEntry(int64_t position, int8_t temporal_offset)
{
    IM_ASSERT(mIndexEntryCache.find(position) != mIndexEntryCache.end());

    mIndexEntryCache[position].temporal_offset = temporal_offset;
}

bool OP1AIndexTableElement::TakeIndexEntry(int64_t position, OP1AIndexEntry *entry)
{
    map<int64_t, OP1AIndexEntry>::iterator result = mIndexEntryCache.find(position);
    if (result == mIndexEntryCache.end())
        return false;

    *entry = result->second;
    mIndexEntryCache.erase(result);

    return true;
}

bool OP1AIndexTableElement::CanStartPartition(int64_t position)
{
    if (is_cbe)
        return true;

    IM_ASSERT(mIndexEntryCache.find(position) != mIndexEntryCache.end());

    return (mIndexEntryCache[position].can_start_partition);
}



OP1AIndexTableSegment::OP1AIndexTableSegment(uint32_t index_sid, uint32_t body_sid, mxfRational frame_rate,
                                             int64_t start_position, uint32_t index_entry_size, uint32_t slice_count)
{
    mIndexEntrySize = index_entry_size;

    mEntries.SetAllocBlockSize(INDEX_ENTRIES_INCREMENT * index_entry_size);

    mxfUUID uuid;
    mxf_generate_uuid(&uuid);

    mSegment.setInstanceUID(uuid);
    mSegment.setIndexEditRate(frame_rate);
    mSegment.setIndexStartPosition(start_position);
    mSegment.setIndexDuration(0);
    mSegment.setIndexSID(index_sid);
    mSegment.setBodySID(body_sid);
    mSegment.setEditUnitByteCount(0);
    mSegment.setSliceCount(slice_count);
}

OP1AIndexTableSegment::~OP1AIndexTableSegment()
{
}

bool OP1AIndexTableSegment::RequireNewSegment(uint8_t can_start_partition)
{
    return mEntries.GetSize() >= MAX_INDEX_SEGMENT_SIZE ||
          (mEntries.GetSize() >= (MAX_INDEX_SEGMENT_SIZE - MAX_GOP_SIZE_GUESS * mIndexEntrySize) && can_start_partition);
}

void OP1AIndexTableSegment::AddIndexEntry(const OP1AIndexEntry *entry, int64_t stream_offset,
                                          vector<uint32_t> slice_cp_offsets)
{
    IM_ASSERT(mIndexEntrySize == 11 + slice_cp_offsets.size() * 4);
    mEntries.Grow(mIndexEntrySize);

    unsigned char *entry_bytes = mEntries.GetBytesAvailable();
    mxf_set_int8(entry->temporal_offset, &entry_bytes[0]);
    mxf_set_int8(entry->key_frame_offset, &entry_bytes[1]);
    mxf_set_uint8(entry->flags, &entry_bytes[2]);
    mxf_set_int64(stream_offset, &entry_bytes[3]);
    size_t i;
    for (i = 0; i < slice_cp_offsets.size(); i++)
        mxf_set_uint32(slice_cp_offsets[i], &entry_bytes[11 + i * 4]);

    mEntries.IncrementSize(mIndexEntrySize);

    mSegment.incrementIndexDuration();
}

void OP1AIndexTableSegment::UpdateIndexEntry(int64_t segment_position, int8_t temporal_offset)
{
    IM_ASSERT(segment_position * mIndexEntrySize < mEntries.GetSize());

    mxf_set_int8(temporal_offset, &mEntries.GetBytes()[segment_position * mIndexEntrySize]);
}

void OP1AIndexTableSegment::AddCBEIndexEntry(uint32_t edit_unit_byte_count)
{
    if (mSegment.getEditUnitByteCount() == 0) {
        mSegment.setEditUnitByteCount(edit_unit_byte_count);
    } else {
        IM_CHECK_M(mSegment.getEditUnitByteCount() == edit_unit_byte_count,
                   ("Failed to index variable content package size in CBE index table"));
    }

    mSegment.incrementIndexDuration();
}

uint32_t OP1AIndexTableSegment::GetDuration()
{
    return mSegment.getIndexDuration();
}



OP1AIndexTable::OP1AIndexTable(uint32_t index_sid, uint32_t body_sid, mxfRational frame_rate)
{
    mIndexSID = index_sid;
    mBodySID = body_sid;
    mFrameRate = frame_rate;
    mIsCBE = true;
    mHaveAVCI = false;
    mSliceCount = 0;
    mIndexEntrySize = 0;
    mAVCIFirstIndexSegment = 0;
    mDuration = 0;
    mStreamOffset = 0;
}

OP1AIndexTable::~OP1AIndexTable()
{
    size_t i;
    for (i = 0; i < mIndexElements.size(); i++)
        delete mIndexElements[i];

    delete mAVCIFirstIndexSegment;
    for (i = 0; i < mIndexSegments.size(); i++)
        delete mIndexSegments[i];
}

void OP1AIndexTable::RegisterPictureTrackElement(uint32_t track_index, bool is_cbe, bool apply_temporal_reordering)
{
    mIndexElements.push_back(new OP1AIndexTableElement(track_index, true, is_cbe, apply_temporal_reordering));
    mIndexElementsMap[track_index] = mIndexElements.back();

    mIsCBE &= is_cbe;
}

void OP1AIndexTable::RegisterAVCITrackElement(uint32_t track_index)
{
    mIndexElements.push_back(new OP1AIndexTableElement(track_index, true, true, false));
    mIndexElementsMap[track_index] = mIndexElements.back();

    mHaveAVCI = true;
}

void OP1AIndexTable::RegisterSoundTrackElement(uint32_t track_index)
{
    mIndexElements.push_back(new OP1AIndexTableElement(track_index, false, true, false));
    mIndexElementsMap[track_index] = mIndexElements.back();
}

void OP1AIndexTable::PrepareWrite()
{
    // order elements: picture elements followed by sound elements
    stable_sort(mIndexElements.begin(), mIndexElements.end(), compare_element);

    mIndexEntrySize = 11;
    mSliceCount = 0;
    size_t i;
    for (i = 0; i < mIndexElements.size(); i++) {
        if (i > 0 && !mIndexElements[i - 1]->is_cbe) {
            mSliceCount++;
            mIndexEntrySize += 4;
        }
        mIndexElements[i]->slice_offset = mSliceCount;
    }
    IM_ASSERT(!mIsCBE || mSliceCount == 0);

    mIndexSegments.push_back(new OP1AIndexTableSegment(mIndexSID, mBodySID, mFrameRate, 0, mIndexEntrySize,
                                                       mSliceCount));
    if (RequireIndexTableSegmentPair())
        mAVCIFirstIndexSegment = new OP1AIndexTableSegment(mIndexSID, mBodySID, mFrameRate, 0, mIndexEntrySize,
                                                           mSliceCount);
}

void OP1AIndexTable::AddIndexEntry(uint32_t track_index, int64_t position, int8_t temporal_offset,
                                   int8_t key_frame_offset, uint8_t flags, bool can_start_partition)
{
    IM_ASSERT(!mIsCBE);
    IM_ASSERT(position >= mDuration);
    IM_ASSERT(mIndexElementsMap.find(track_index) != mIndexElementsMap.end());

    mIndexElementsMap[track_index]->CacheIndexEntry(position, temporal_offset, key_frame_offset, flags,
                                                    can_start_partition);
}

void OP1AIndexTable::UpdateIndexEntry(uint32_t track_index, int64_t position, int8_t temporal_offset)
{
    IM_ASSERT(!mIsCBE);
    IM_ASSERT(position >= 0);
    IM_ASSERT(mIndexElementsMap.find(track_index) != mIndexElementsMap.end());

    if (position > mDuration) {
        mIndexElementsMap[track_index]->UpdateIndexEntry(position, temporal_offset);
    } else {
        int64_t end_offset = mDuration - position;
        size_t i = mIndexSegments.size() - 1;
        while (end_offset > mIndexSegments[i]->GetDuration()) {
            end_offset -= mIndexSegments[i]->GetDuration();
            i--;
        }
        mIndexSegments[i]->UpdateIndexEntry(mIndexSegments[i]->GetDuration() - end_offset, temporal_offset);
    }
}

bool OP1AIndexTable::CanStartPartition()
{
    if (mIsCBE)
        return true;

    size_t i;
    for (i = 0; i < mIndexElements.size(); i++) {
        if (!mIndexElements[i]->CanStartPartition(mDuration))
            return false;
    }

    return true;
}

void OP1AIndexTable::UpdateIndex(uint32_t size, vector<uint32_t> element_sizes)
{
    // create or check delta entries
    IM_ASSERT(element_sizes.size() == mIndexElements.size());
    if (mDuration == 0 || (mAVCIFirstIndexSegment && mDuration == 1)) {
        // create delta entries

        mDeltaEntries.clear();

        uint8_t prev_slice_offset = 0;
        uint32_t element_delta = 0;
        size_t i;
        for (i = 0; i < mIndexElements.size(); i++) {
            if (mIndexElements[i]->slice_offset != prev_slice_offset)
                element_delta = 0;

            OP1ADeltaEntry entry;
            entry.pos_table_index = (mIndexElements[i]->apply_temporal_reordering ? -1 : 0);
            entry.slice = mIndexElements[i]->slice_offset;
            entry.element_delta = element_delta;
            mDeltaEntries.push_back(entry);

            prev_slice_offset = mIndexElements[i]->slice_offset;
            element_delta += element_sizes[i];

            if (mIndexElements[i]->is_cbe)
                mIndexElements[i]->element_size = element_sizes[i];
        }
        if (mDeltaEntries.size() == 1 &&
            mDeltaEntries[0].pos_table_index == 0 &&
            mDeltaEntries[0].slice == 0 &&
            mDeltaEntries[0].element_delta == 0)
        {
            // no need for delta entry array
            mDeltaEntries.clear();
        }

        if (mIsCBE) {
            size_t i;
            for (i = 0; i < mDeltaEntries.size(); i++) {
                if (mAVCIFirstIndexSegment && mDuration == 0) {
                    mAVCIFirstIndexSegment->GetSegment()->appendDeltaEntry(mDeltaEntries[i].pos_table_index,
                                                                           mDeltaEntries[i].slice,
                                                                           mDeltaEntries[i].element_delta);
                } else {
                    mIndexSegments[0]->GetSegment()->appendDeltaEntry(mDeltaEntries[i].pos_table_index,
                                                                      mDeltaEntries[i].slice,
                                                                      mDeltaEntries[i].element_delta);
                }
            }
        }
    } else {
        // check delta entries remain constant

        size_t i;
        for (i = 0; i < mIndexElements.size(); i++) {
            if (mIndexElements[i]->is_cbe) {
                IM_CHECK_M(mIndexElements[i]->element_size == element_sizes[i],
                           ("Fixed size content package element data size changed"));
            }
        }
    }

    // update index table entries and/or duration
    if (mIsCBE) {
        if (mDuration == 0 && mAVCIFirstIndexSegment) {
            mAVCIFirstIndexSegment->AddCBEIndexEntry(size);
            mIndexSegments[0]->GetSegment()->setIndexStartPosition(1);
        } else {
            // delete first avci index segment if the edit unit size is the same as non-first edit units
            // e.g. the avci sequence and picture parameter sets are included in every frame
            if (mDuration == 1 && mAVCIFirstIndexSegment &&
                mAVCIFirstIndexSegment->GetSegment()->getEditUnitByteCount() == size)
            {
                size_t i;
                for (i = 0; i < mIndexElements.size(); i++) {
                    if (mIndexElements[i]->is_cbe &&
                        mIndexElements[i]->element_size != element_sizes[i])
                    {
                        break;
                    }
                }
                if (i >= mIndexElements.size()) {
                    delete mAVCIFirstIndexSegment;
                    mAVCIFirstIndexSegment = 0;

                    mIndexSegments[0]->GetSegment()->setIndexStartPosition(0);
                    mIndexSegments[0]->AddCBEIndexEntry(size);
                }
            }

            mIndexSegments[0]->AddCBEIndexEntry(size);
        }
    } else {
        bool can_start_partition = CanStartPartition(); // check before any TakeIndexEntry calls

        uint32_t slice_cp_offset = 0;
        vector<uint32_t> slice_cp_offsets;
        uint8_t prev_slice_offset = 0;
        OP1AIndexEntry entry;
        size_t i;
        for (i = 0; i < mIndexElements.size(); i++) {
            // get non-default entry if exists
            OP1AIndexEntry element_entry;
            if (mIndexElements[i]->TakeIndexEntry(mDuration, &element_entry) && !element_entry.IsDefault()) {
                IM_CHECK(entry.IsCompatible(element_entry));
                entry = element_entry;
            }

            if (mIndexElements[i]->slice_offset != prev_slice_offset) {
                slice_cp_offsets.push_back(slice_cp_offset);
                prev_slice_offset = mIndexElements[i]->slice_offset;
            }
            slice_cp_offset += element_sizes[i];
        }

        if (mIndexSegments.empty() || mIndexSegments.back()->RequireNewSegment(can_start_partition)) {
            mIndexSegments.push_back(new OP1AIndexTableSegment(mIndexSID, mBodySID, mFrameRate, mDuration,
                                                               mIndexEntrySize, mSliceCount));
        }

        mIndexSegments.back()->AddIndexEntry(&entry, mStreamOffset, slice_cp_offsets);
    }

    mDuration++;
    mStreamOffset += size;
}

bool OP1AIndexTable::HaveSegments()
{
    return mIsCBE || (!mIndexSegments.empty() && mIndexSegments[0]->GetDuration() > 0);
}

void OP1AIndexTable::WriteSegments(mxfpp::File *mxf_file, mxfpp::Partition *partition)
{
    IM_ASSERT(HaveSegments());
    IM_ASSERT(mDuration > 0);

    partition->markIndexStart(mxf_file);

    if (mIsCBE) {
        if (mAVCIFirstIndexSegment) {
            IM_CHECK(mxf_write_index_table_segment(mxf_file->getCFile(),
                                                   mAVCIFirstIndexSegment->GetSegment()->getCIndexTableSegment()));
        }
        if (!mAVCIFirstIndexSegment || mDuration > 1) {
            IM_CHECK(mxf_write_index_table_segment(mxf_file->getCFile(),
                                                   mIndexSegments[0]->GetSegment()->getCIndexTableSegment()));
        }
    } else {
        size_t i;
        for (i = 0; i < mIndexSegments.size(); i++) {
            IndexTableSegment *segment = mIndexSegments[i]->GetSegment();
            ByteArray *entries = mIndexSegments[i]->GetEntries();

            segment->writeHeader(mxf_file, mDeltaEntries.size(), segment->getIndexDuration());

            if (!mDeltaEntries.empty()) {
                segment->writeDeltaEntryArrayHeader(mxf_file, mDeltaEntries.size());
                size_t j;
                for (j = 0; j < mDeltaEntries.size(); j++) {
                    segment->writeDeltaEntry(mxf_file, mDeltaEntries[j].pos_table_index, mDeltaEntries[j].slice,
                                             mDeltaEntries[j].element_delta);
                }
            }

            segment->writeIndexEntryArrayHeader(mxf_file, mSliceCount, 0, segment->getIndexDuration());
            mxf_file->write(entries->GetBytes(), entries->GetSize());

            delete mIndexSegments[i];
        }
        mIndexSegments.clear();
    }

    partition->fillToKag(mxf_file);

    partition->markIndexEnd(mxf_file);
}
