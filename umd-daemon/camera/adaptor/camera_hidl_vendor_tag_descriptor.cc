/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations
 * in the disclaimer below) provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of Qualcomm Innovation Center, Inc.
 *      nor the names of its contributors may be used to endorse
 *      or promote products derived from this software without specific
 *      prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
 * BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "camera_hidl_vendor_tag_descriptor.h"

#include "utils/camera_log.h"

using namespace android;

status_t CustomVendorTagDescriptor::createDescriptorFromHidl(
    const hardware::hidl_vec<VendorTagSection>& vts,
    sp<VendorTagDescriptor>& descriptor) {

    int tagCount = 0;

    for (size_t s = 0; s < vts.size(); s++) {
        tagCount += vts[s].tags.size();
    }

    if (tagCount < 0 || tagCount > INT32_MAX) {
        CAMERA_ERROR("%s: tag count %d from vendor tag sections is invalid.", __func__, tagCount);
        return BAD_VALUE;
    }

    Vector<uint32_t> tagArray;

    if (tagArray.resize(tagCount) != tagCount) {
      CAMERA_ERROR("%s: too many (%u) vendor tags defined.", __func__, tagCount);
      return BAD_VALUE;
    }

    sp<CustomVendorTagDescriptor> desc = new CustomVendorTagDescriptor();
    desc->mTagCount = tagCount;

    SortedVector<String8> sections;
    KeyedVector<uint32_t, String8> tagToSectionMap;

    int idx = 0;
    for (size_t s = 0; s < vts.size(); s++) {
        const VendorTagSection& section = vts[s];
        const char *sectionName = section.sectionName.c_str();
        if (sectionName == NULL) {
            CAMERA_ERROR("%s: no section name defined for vendor tag section %zu.", __func__, s);
            return BAD_VALUE;
        }
        String8 sectionString(sectionName);
        sections.add(sectionString);

        for (size_t j = 0; j < section.tags.size(); j++) {
            uint32_t tag = section.tags[j].tagId;
            if (tag < CAMERA_METADATA_VENDOR_TAG_BOUNDARY) {
                CAMERA_ERROR("%s: vendor tag %d not in vendor tag section.", __func__, tag);
                return BAD_VALUE;
            }
            tagArray.editItemAt(idx++) = section.tags[j].tagId;

            const char *tagName = section.tags[j].tagName.c_str();
            if (tagName == NULL) {
                CAMERA_ERROR("%s: no tag name defined for vendor tag %d.", __func__, tag);
                return BAD_VALUE;
            }
            desc->mTagToNameMap.add(tag, String8(tagName));
            tagToSectionMap.add(tag, sectionString);

            int tagType = (int) section.tags[j].tagType;
            if (tagType < 0) {
                CAMERA_ERROR("%s: tag type %d from vendor ops does not exist.", __func__, tagType);
                return BAD_VALUE;
            }
            desc->mTagToTypeMap.insert(std::make_pair(tag, tagType));
        }
    }

    desc->mSections = sections;

    for (size_t i = 0; i < tagArray.size(); ++i) {
        uint32_t tag = tagArray[i];
        String8 sectionString = tagToSectionMap.valueFor(tag);

        ssize_t index = sections.indexOf(sectionString);
        LOG_ALWAYS_FATAL_IF(index < 0, "index %zd must be non-negative", index);
        if (index < 0) {
          CAMERA_ERROR("%s: index %zd must be non-negative", __func__, index);
          return BAD_VALUE;
        }
        desc->mTagToSectionMap.add(tag, static_cast<uint32_t>(index));

        ssize_t reverseIndex = -1;
        if ((reverseIndex = desc->mReverseMapping.indexOfKey(sectionString)) < 0) {
            KeyedVector<String8, uint32_t>* nameMapper = new KeyedVector<String8, uint32_t>();
            reverseIndex = desc->mReverseMapping.add(sectionString, nameMapper);
        }
        desc->mReverseMapping[reverseIndex]->add(desc->mTagToNameMap.valueFor(tag), tag);
    }

    descriptor = std::move(desc);
    return OK;
}
