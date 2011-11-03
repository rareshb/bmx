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

#ifndef __IM_AVID_CLIP_H__
#define __IM_AVID_CLIP_H__


#include <string>
#include <vector>
#include <map>

#include <libMXF++/MXF.h>

#include <im/avid_mxf/AvidTrack.h>
#include <im/IMTypes.h>



namespace im
{


typedef enum
{
    AVID_WHITE = 0,
    AVID_RED,
    AVID_YELLOW,
    AVID_GREEN,
    AVID_CYAN,
    AVID_BLUE,
    AVID_MAGENTA,
    AVID_BLACK
} AvidRGBColor;

typedef struct
{
    int64_t position;
    AvidRGBColor color;
    std::string comment;
} AvidLocator;


class AvidClip
{
public:
    friend class AvidTrack;

public:
    AvidClip(mxfRational frame_rate, std::string filename_prefix = "");
    ~AvidClip();

    void SetProjectName(std::string name);                              // default ""
    void SetClipName(std::string name);                                 // default ""
    void SetStartTimecode(Timecode start_timecode);                     // default 00:00:00:00, non-drop frame
    void SetProductInfo(std::string company_name, std::string product_name, mxfProductVersion product_version,
                        std::string version, mxfUUID product_uid);
    void SetCreationDate(mxfTimestamp creation_date);                   // default generated ('now')
    void SetGenerationUID(mxfUUID generation_uid);                      // default generated

public:
    void SetUserComment(std::string name, std::string value);
    void AddLocator(AvidLocator locator);

public:
    // default source package creation
    mxfpp::SourcePackage* CreateDefaultTapeSource(std::string name, uint32_t num_video_tracks, uint32_t num_audio_tracks);
    mxfpp::SourcePackage* CreateDefaultImportSource(std::string uri, std::string name,
                                                    uint32_t num_video_tracks, uint32_t num_audio_tracks);
    std::vector<std::pair<mxfUMID, uint32_t> > GetPictureSourceReferences(mxfpp::SourcePackage *source_package);
    std::vector<std::pair<mxfUMID, uint32_t> > GetSoundSourceReferences(mxfpp::SourcePackage *source_package);
    
    // custom source package creation
    mxfpp::DataModel* GetDataModel() const { return mDataModel; }
    mxfpp::AvidHeaderMetadata* GetHeaderMetadata() const { return mHeaderMetadata; }
    mxfpp::ContentStorage* GetContentStorage() const { return mContentStorage; }
    void RegisterTapeSource(mxfpp::SourcePackage *source_package);
    void RegisterImportSource(mxfpp::SourcePackage *source_package);

public:
    AvidTrack* CreateTrack(AvidEssenceType essence_type);
    AvidTrack* CreateTrack(AvidEssenceType essence_type, std::string filename);

public:
    void PrepareWrite();
    void WriteSamples(uint32_t track_index, const unsigned char *data, uint32_t size, uint32_t num_samples);
    void CompleteWrite();

    int64_t GetDuration() const;
    mxfRational GetFrameRate() const { return mClipFrameRate; }

private:
    void CreateMinimalHeaderMetadata();
    void CreateMaterialPackage();
    void SetTapeStartTimecode();

    void UpdateHeaderMetadata();
    void UpdateTrackDurations(AvidTrack *avid_track, mxfpp::Track *track, mxfRational edit_rate, int64_t duration);
    void UpdateTimecodeTrackDuration(AvidTrack *avid_track, mxfpp::GenericPackage *package, mxfRational package_edit_rate);
    bool GetStartTimecode(mxfpp::GenericPackage *package, Timecode *timecode);

    std::vector<std::pair<mxfUMID, uint32_t> > GetSourceReferences(mxfpp::SourcePackage *source_package, bool is_picture);

private:
    std::string mProjectName;
    std::string mClipName;
    mxfRational mClipFrameRate;
    std::string mFilenamePrefix;
    Timecode mStartTimecode;
    bool mStartTimecodeSet;
    std::string mCompanyName;
    std::string mProductName;
    mxfProductVersion mProductVersion;
    std::string mVersionString;
    mxfUUID mProductUID;
    std::map<std::string, std::string> mUserComments;
    std::vector<AvidLocator> mLocators;

    mxfTimestamp mCreationDate;
    mxfUUID mGenerationUID;
    mxfUMID mMaterialPackageUID;

    mxfpp::DataModel *mDataModel;
    mxfpp::AvidHeaderMetadata *mHeaderMetadata;
    mxfpp::ContentStorage *mContentStorage;
    mxfpp::MaterialPackage *mMaterialPackage;
    std::vector<mxfpp::SourcePackage*> mTapeSourcePackages;
    std::vector<mxfpp::SourcePackage*> mImportSourcePackages;
    mxfpp::TimecodeComponent *mMaterialTimecodeComponent;

    uint32_t mLocatorDescribedTrackId;

    std::vector<AvidTrack*> mTracks;
};


};



#endif