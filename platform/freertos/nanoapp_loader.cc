/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <cmath>
#include <cstring>

#include "chre/platform/freertos/nanoapp_loader.h"

#include "ash.h"
#include "chre.h"
#include "chre/platform/assert.h"
#include "chre/platform/freertos/memory.h"
#include "chre/util/dynamic_vector.h"
#include "chre/util/macros.h"

#ifndef CHRE_LOADER_ARCH
#define CHRE_LOADER_ARCH EM_ARM
#endif  // CHRE_LOADER_ARCH

namespace chre {
namespace {
struct ExportedData {
  void *data;
  const char *dataName;
};

// The new operator is used by singleton.h which causes the delete operator to
// be undefined in nanoapp binaries even though it's unused. Define this in case
// a nanoapp actually tries to use the operator.
void deleteOverride(void *ptr) {
  FATAL_ERROR("Nanoapp tried to free %p through delete operator", ptr);
}

// TODO(karthikmb/stange): While this array was hand-coded for simple
// "hello-world" prototyping, the list of exported symbols must be
// generated to minimize runtime errors and build breaks.
ExportedData gExportedData[] = {
    {(void *)ashLoadCalibrationParams, "ashLoadCalibrationParams"},
    {(void *)ashSaveCalibrationParams, "ashSaveCalibrationParams"},
    {(void *)ashSetCalibration, "ashSetCalibration"},
    {(void *)chreGetVersion, "chreGetVersion"},
    {(void *)chreGetApiVersion, "chreGetApiVersion"},
    {(void *)chreGetEstimatedHostTimeOffset, "chreGetEstimatedHostTimeOffset"},
    {(void *)chreGetPlatformId, "chreGetPlatformId"},
    {(void *)chreGetSensorSamplingStatus, "chreGetSensorSamplingStatus"},
    {(void *)chreGetTime, "chreGetTime"},
    {(void *)chreHeapAlloc, "chreHeapAlloc"},
    {(void *)chreHeapFree, "chreHeapFree"},
    {(void *)chreLog, "chreLog"},
    {(void *)chreSendMessageToHostEndpoint, "chreSendMessageToHostEndpoint"},
    {(void *)chreSensorConfigure, "chreSensorConfigure"},
    {(void *)chreSensorFindDefault, "chreSensorFindDefault"},
    {(void *)memcpy, "memcpy"},
    {(void *)memset, "memset"},
    {(void *)deleteOverride, "_ZdlPv"},
    {(void *)sqrtf, "sqrtf"},
};

}  // namespace

void *NanoappLoader::create(void *elfInput) {
  void *instance = nullptr;
  NanoappLoader *loader = memoryAlloc<NanoappLoader>(elfInput);
  if (loader != nullptr) {
    if (loader->init()) {
      instance = loader;
    } else {
      memoryFree(loader);
    }
  } else {
    LOG_OOM();
  }
  return instance;
}

bool NanoappLoader::init() {
  bool success = false;
  if (mBinary.rawLocation != nullptr) {
    if (!copyAndVerifyHeaders()) {
      LOGE("Failed to verify headers");
    } else if (!createMappings()) {
      LOGE("Failed to create mappings");
    } else if (!initDynamicStringTable()) {
      LOGE("Failed to initialize dynamic string table");
    } else if (!initDynamicSymbolTable()) {
      LOGE("Failed to initialize dynamic symbol table");
    } else if (!fixRelocations()) {
      LOGE("Failed to fix relocations");
    } else if (!resolveGot()) {
      LOGE("Failed to resolve GOT");
    } else {
      // TODO(karthikmb / stange): Initialize static variables
      success = true;
    }
  }

  if (!success) {
    freeAllocatedData();
  }

  return success;
}

void NanoappLoader::deinit() {
  freeAllocatedData();
}

void *NanoappLoader::findSymbolByName(const char *name) {
  void *symbol = nullptr;
  uint8_t *index = mSymbolTablePtr;
  while (index < (mSymbolTablePtr + mSymbolTableSize)) {
    ElfSym *currSym = reinterpret_cast<ElfSym *>(index);
    const char *symbolName = &mStringTablePtr[currSym->st_name];

    if (strncmp(symbolName, name, strlen(name)) == 0) {
      symbol = ((void *)(mMapping.location + currSym->st_value));
      break;
    }

    index += sizeof(ElfSym);
  }
  return symbol;
}

void NanoappLoader::mapBss(const ProgramHeader *hdr) {
  // if the memory size of this segment exceeds the file size zero fill the
  // difference.
  LOGV("Program Hdr mem sz: %zu file size: %zu", hdr->p_memsz, hdr->p_filesz);
  if (hdr->p_memsz > hdr->p_filesz) {
    ElfAddr endOfFile = hdr->p_vaddr + hdr->p_filesz + mLoadBias;
    ElfAddr endOfMem = hdr->p_vaddr + hdr->p_memsz + mLoadBias;
    if (endOfMem > endOfFile) {
      auto deltaMem = endOfMem - endOfFile;
      LOGV("Zeroing out %zu from page %p", deltaMem, endOfFile);
      memset((void *)endOfFile, 0, deltaMem);
    }
  }
}

uintptr_t NanoappLoader::roundDownToAlign(uintptr_t virtualAddr) {
  return virtualAddr & -kBinaryAlignment;
}

void NanoappLoader::freeAllocatedData() {
  memoryFree(mMapping.rawLocation);
  memoryFree(mProgramHeadersPtr);
  memoryFree(mSectionHeadersPtr);
  memoryFree(mSectionNamesPtr);
  memoryFree(mDynamicStringTablePtr);
  memoryFree(mDynamicSymbolTablePtr);
  memoryFree(mSymbolTablePtr);
  memoryFree(mStringTablePtr);
}

bool NanoappLoader::verifyElfHeader() {
  bool success = false;
  if ((mElfHeader.e_ident[EI_MAG0] == ELFMAG0) &&
      (mElfHeader.e_ident[EI_MAG1] == ELFMAG1) &&
      (mElfHeader.e_ident[EI_MAG2] == ELFMAG2) &&
      (mElfHeader.e_ident[EI_MAG3] == ELFMAG3) &&
      (mElfHeader.e_ehsize == sizeof(mElfHeader)) &&
      (mElfHeader.e_phentsize == sizeof(ProgramHeader)) &&
      (mElfHeader.e_shentsize == sizeof(SectionHeader)) &&
      (mElfHeader.e_shstrndx < mElfHeader.e_shnum) &&
      (mElfHeader.e_version == EV_CURRENT) &&
      (mElfHeader.e_machine == CHRE_LOADER_ARCH) &&
      (mElfHeader.e_type == ET_DYN)) {
    success = true;
  }
  return success;
}

bool NanoappLoader::verifyProgramHeaders() {
  // This is a minimal check for now -
  // there should be at least one load segment.
  bool success = false;
  for (size_t i = 0; i < mNumProgramHeaders; ++i) {
    if (mProgramHeadersPtr[i].p_type == PT_LOAD) {
      success = true;
      break;
    }
  }
  return success;
}

const char *NanoappLoader::getSectionHeaderName(size_t sh_name) {
  if (sh_name == 0) {
    return "";
  }

  return &mSectionNamesPtr[sh_name];
}

NanoappLoader::SectionHeader *NanoappLoader::getSectionHeader(
    const char *headerName) {
  SectionHeader *rv = nullptr;
  for (size_t i = 0; i < mNumSectionHeaders; ++i) {
    const char *name = getSectionHeaderName(mSectionHeadersPtr[i].sh_name);
    if (strncmp(name, headerName, strlen(headerName)) == 0) {
      rv = &mSectionHeadersPtr[i];
      break;
    }
  }
  return rv;
}

bool NanoappLoader::verifySectionHeaders() {
  for (size_t i = 0; i < mNumSectionHeaders; ++i) {
    const char *name = getSectionHeaderName(mSectionHeadersPtr[i].sh_name);

    if (strncmp(name, kSymTableName, strlen(kSymTableName)) == 0) {
      memcpy(&mSymbolTableHeader, &mSectionHeadersPtr[i],
             sizeof(SectionHeader));
    } else if (strncmp(name, kStrTableName, strlen(kStrTableName)) == 0) {
      memcpy(&mStringTableHeader, &mSectionHeadersPtr[i],
             sizeof(SectionHeader));
    } else if (strncmp(name, kInitArrayName, strlen(kInitArrayName)) == 0) {
      mInitArrayLoc = mSectionHeadersPtr[i].sh_addr;
    }
  }

  return true;
}

bool NanoappLoader::copyAndVerifyHeaders() {
  size_t offset = 0;
  bool success = false;
  uint8_t *pDataBytes = static_cast<uint8_t *>(mBinary.rawLocation);

  // Verify the ELF Header
  memcpy(&mElfHeader, pDataBytes, sizeof(mElfHeader));
  success = verifyElfHeader();

  LOGV("Verified ELF header %d", success);

  // Verify Program Headers
  if (success) {
    offset = mElfHeader.e_phoff;
    size_t programHeadersSizeBytes = sizeof(ProgramHeader) * mElfHeader.e_phnum;
    mProgramHeadersPtr =
        static_cast<ProgramHeader *>(memoryAlloc(programHeadersSizeBytes));
    if (mProgramHeadersPtr == nullptr) {
      success = false;
      LOG_OOM();
    } else {
      memcpy(mProgramHeadersPtr, (pDataBytes + offset),
             programHeadersSizeBytes);
      mNumProgramHeaders = mElfHeader.e_phnum;
      success = verifyProgramHeaders();
    }
  }

  LOGV("Verified Program headers %d", success);

  // Load Section Headers
  if (success) {
    offset = mElfHeader.e_shoff;
    size_t sectionHeaderSizeBytes = sizeof(SectionHeader) * mElfHeader.e_shnum;
    mSectionHeadersPtr =
        static_cast<SectionHeader *>(memoryAlloc(sectionHeaderSizeBytes));
    if (mSectionHeadersPtr == nullptr) {
      success = false;
      LOG_OOM();
    } else {
      memcpy(mSectionHeadersPtr, (pDataBytes + offset), sectionHeaderSizeBytes);
      mNumSectionHeaders = mElfHeader.e_shnum;
    }
  }

  LOGV("Loaded section headers %d", success);

  // Load section header names
  if (success) {
    SectionHeader &stringSection = mSectionHeadersPtr[mElfHeader.e_shstrndx];
    size_t sectionSize = stringSection.sh_size;
    mSectionNamesPtr = static_cast<char *>(memoryAlloc(sectionSize));
    if (mSectionNamesPtr == nullptr) {
      LOG_OOM();
      success = false;
    } else {
      memcpy(mSectionNamesPtr,
             (void *)(mBinary.location + stringSection.sh_offset), sectionSize);
    }
  }

  LOGV("Loaded section header names %d", success);

  success = verifySectionHeaders();
  LOGV("Verified Section headers %d", success);

  // Load symbol table
  if (success) {
    mSymbolTableSize = mSymbolTableHeader.sh_size;
    if (mSymbolTableSize == 0) {
      LOGE("No symbols to resolve");
      success = false;
    } else {
      mSymbolTablePtr = static_cast<uint8_t *>(memoryAlloc(mSymbolTableSize));
      if (mSymbolTablePtr == nullptr) {
        LOG_OOM();
        success = false;
      } else {
        memcpy(mSymbolTablePtr,
               (void *)(mBinary.location + mSymbolTableHeader.sh_offset),
               mSymbolTableSize);
      }
    }
  }

  LOGV("Loaded symbol table %d", success);

  // Load string table
  if (success) {
    size_t stringTableSize = mStringTableHeader.sh_size;
    if (mSymbolTableSize == 0) {
      LOGE("No string table corresponding to symbols");
      success = false;
    } else {
      mStringTablePtr = static_cast<char *>(memoryAlloc(stringTableSize));
      if (mStringTablePtr == nullptr) {
        LOG_OOM();
        success = false;
      } else {
        memcpy(mStringTablePtr,
               (void *)(mBinary.location + mStringTableHeader.sh_offset),
               stringTableSize);
      }
    }
  }

  LOGV("Loaded string table %d", success);

  return success;
}

bool NanoappLoader::createMappings() {
  // ELF needs pt_load segments to be in contiguous ascending order of
  // virtual addresses. So the first and last segs can be used to
  // calculate the entire address span of the image.
  const ProgramHeader *first = &mProgramHeadersPtr[0];
  const ProgramHeader *last = &mProgramHeadersPtr[mElfHeader.e_phnum - 1];

  // Find first load segment
  while (first->p_type != PT_LOAD && first <= last) {
    ++first;
  }

  bool success = false;
  if (first->p_type != PT_LOAD) {
    LOGE("Unable to find any load segments in the binary");
  } else {
    // Verify that the first load segment has a program header
    // first byte of a valid load segment can't be greater than the
    // program header offset
    bool valid =
        (first->p_offset < mElfHeader.e_phoff) &&
        (first->p_filesz >
         (mElfHeader.e_phoff + (mNumProgramHeaders * sizeof(ProgramHeader))));
    if (!valid) {
      LOGE("Load segment program header validation failed");
    } else {
      // Get the last load segment
      while (last > first && last->p_type != PT_LOAD) --last;

      size_t memorySpan = last->p_vaddr + last->p_memsz - first->p_vaddr;
      LOGV("Nanoapp image Memory Span: %u", memorySpan);

      mMapping.rawLocation =
          memoryAllocDramAligned(kBinaryAlignment, memorySpan);
      if (mMapping.rawLocation == nullptr) {
        LOG_OOM();
      } else {
        LOGV("Starting location of mappings %p", data->mMapping.rawLocation);

        // Calculate the load bias using the first load segment.
        uintptr_t adjustedFirstLoadSegAddr = roundDownToAlign(first->p_vaddr);
        mLoadBias = mMapping.location - adjustedFirstLoadSegAddr;
        LOGV("Load bias is %" PRIu32, mLoadBias);

        success = true;
      }
    }
  }

  if (success) {
    // Map the remaining segments
    for (const ProgramHeader *ph = first; ph <= last; ++ph) {
      if (ph->p_type == PT_LOAD) {
        ElfAddr segStart = ph->p_vaddr + mLoadBias;
        ElfAddr startPage = roundDownToAlign(segStart);
        ElfAddr phOffsetPage = roundDownToAlign(ph->p_offset);
        ElfAddr binaryStartPage = mBinary.location + phOffsetPage;
        size_t segmentLen = ph->p_filesz;

        LOGV("Mapping start page %p from %p with length %zu", startPage,
             (void *)binaryStartPage, segmentLen);
        memcpy((void *)startPage, (void *)binaryStartPage, segmentLen);
        mapBss(ph);
      } else {
        LOGE("Non-load segment found between load segments");
        success = false;
        break;
      }
    }
  }

  return success;
}

bool NanoappLoader::initDynamicStringTable() {
  bool success = false;

  SectionHeader *dynamicStringTablePtr = getSectionHeader(".dynstr");
  CHRE_ASSERT(dynamicStringTablePtr != nullptr);
  if (dynamicStringTablePtr != nullptr) {
    size_t stringTableSize = dynamicStringTablePtr->sh_size;
    mDynamicStringTablePtr = static_cast<char *>(memoryAlloc(stringTableSize));
    if (mDynamicStringTablePtr == nullptr) {
      LOG_OOM();
    } else {
      memcpy(mDynamicStringTablePtr,
             (void *)(mBinary.location + dynamicStringTablePtr->sh_offset),
             stringTableSize);
      success = true;
    }
  }

  return success;
}

bool NanoappLoader::initDynamicSymbolTable() {
  bool success = false;
  SectionHeader *dynamicSymbolTablePtr = getSectionHeader(".dynsym");
  CHRE_ASSERT(dynamicSymbolTablePtr != nullptr);
  if (dynamicSymbolTablePtr != nullptr) {
    size_t sectionSize = dynamicSymbolTablePtr->sh_size;
    mDynamicSymbolTablePtr = static_cast<uint8_t *>(memoryAlloc(sectionSize));
    if (mDynamicSymbolTablePtr == nullptr) {
      LOG_OOM();
    } else {
      memcpy(mDynamicSymbolTablePtr,
             (void *)(mBinary.location + dynamicSymbolTablePtr->sh_offset),
             sectionSize);
      mDynamicSymbolTableSize = sectionSize;
      success = true;
    }
  }

  return success;
}

const char *NanoappLoader::getDataName(size_t posInSymbolTable) {
  size_t sectionSize = mDynamicSymbolTableSize;
  size_t numElements = sectionSize / sizeof(ElfSym);
  CHRE_ASSERT(posInSymbolTable < numElements);
  char *dataName = nullptr;
  if (posInSymbolTable < numElements) {
    ElfSym *sym = reinterpret_cast<ElfSym *>(
        &mDynamicSymbolTablePtr[posInSymbolTable * sizeof(ElfSym)]);
    dataName = &mDynamicStringTablePtr[sym->st_name];
  }
  return dataName;
}

void *NanoappLoader::resolveData(size_t posInSymbolTable) {
  const char *dataName = getDataName(posInSymbolTable);
  LOGV("Resolving %s", dataName);

  if (dataName != nullptr) {
    for (size_t i = 0; i < ARRAY_SIZE(gExportedData); i++) {
      if (strncmp(dataName, gExportedData[i].dataName,
                  strlen(gExportedData[i].dataName)) == 0) {
        return gExportedData[i].data;
      }
    }
  }

  LOGV("%s not found!", dataName);

  return nullptr;
}

NanoappLoader::DynamicHeader *NanoappLoader::getDynamicHeader() {
  DynamicHeader *dyn = nullptr;
  for (size_t i = 0; i < mNumProgramHeaders; ++i) {
    if (mProgramHeadersPtr[i].p_type == PT_DYNAMIC) {
      dyn = (DynamicHeader *)(mProgramHeadersPtr[i].p_vaddr + mLoadBias);
      break;
    }
  }
  return dyn;
}

NanoappLoader::ProgramHeader *NanoappLoader::getFirstRoSegHeader() {
  // return the first read only segment found
  ProgramHeader *ro = nullptr;
  for (size_t i = 0; i < mNumProgramHeaders; ++i) {
    if (!(mProgramHeadersPtr[i].p_flags & PF_W)) {
      ro = &mProgramHeadersPtr[i];
      break;
    }
  }
  return ro;
}

NanoappLoader::ElfWord NanoappLoader::getDynEntry(DynamicHeader *dyn,
                                                  int field) {
  ElfWord rv = 0;

  while (dyn->d_tag != DT_NULL) {
    if (dyn->d_tag == field) {
      rv = dyn->d_un.d_val;
      break;
    }
    ++dyn;
  }

  return rv;
}

bool NanoappLoader::fixRelocations() {
  ElfAddr *addr;
  DynamicHeader *dyn = getDynamicHeader();
  ProgramHeader *roSeg = getFirstRoSegHeader();

  bool success = false;
  if ((dyn == nullptr) || (roSeg == nullptr)) {
    LOGE("Mandatory headers missing from shared object, aborting load");
  } else if (getDynEntry(dyn, DT_RELA) != 0) {
    LOGE("Elf binaries with a DT_RELA dynamic entry are unsupported");
  } else {
    ElfRel *reloc = (ElfRel *)(getDynEntry(dyn, DT_REL) + mBinary.location);
    size_t relocSize = getDynEntry(dyn, DT_RELSZ);
    size_t nRelocs = relocSize / sizeof(ElfRel);
    LOGV("Relocation %zu entries in DT_REL table", nRelocs);

    size_t i;
    for (i = 0; i < nRelocs; ++i) {
      ElfRel *curr = &reloc[i];
      int relocType = ELFW_R_TYPE(curr->r_info);
      switch (relocType) {
        case R_ARM_RELATIVE:
          LOGV("Resolving ARM_RELATIVE at offset %" PRIu32, curr->r_offset);
          addr =
              reinterpret_cast<ElfAddr *>(curr->r_offset + mMapping.location);
          // TODO: When we move to DRAM allocations, we need to check if the
          // above address is in a Read-Only section of memory, and give it
          // temporary write permission if that is the case.
          *addr += mMapping.location;
          break;

        case R_ARM_GLOB_DAT: {
          LOGV("Resolving R_ARM_GLOB_DAT at offset %" PRIu32, curr->r_offset);
          addr =
              reinterpret_cast<ElfAddr *>(curr->r_offset + mMapping.location);
          size_t posInSymbolTable = ELFW_R_SYM(curr->r_info);
          void *resolved = resolveData(posInSymbolTable);
          if (resolved == nullptr) {
            LOGE("Failed to resolve global symbol(%d) at offset 0x%x", i,
                 curr->r_offset);
            return false;
          }
          // TODO: When we move to DRAM allocations, we need to check if the
          // above address is in a Read-Only section of memory, and give it
          // temporary write permission if that is the case.
          *addr = reinterpret_cast<ElfAddr>(resolved);
          break;
        }

        case R_ARM_COPY:
          LOGE("R_ARM_COPY is an invalid relocation for shared libraries");
          break;
        default:
          LOGE("Invalid relocation type %u", relocType);
          break;
      }
    }

    if (i != nRelocs) {
      LOGE("Unable to resolve all symbols in the binary");
    } else {
      mDynamicHeaderPtr = dyn;
      success = true;
    }
  }

  return success;
}

bool NanoappLoader::resolveGot() {
  ElfAddr *addr;
  ElfRel *reloc =
      (ElfRel *)(getDynEntry(mDynamicHeaderPtr, DT_JMPREL) + mMapping.location);
  size_t relocSize = getDynEntry(mDynamicHeaderPtr, DT_PLTRELSZ);
  size_t nRelocs = relocSize / sizeof(ElfRel);
  LOGV("Resolving GOT with %zu relocations", nRelocs);

  for (size_t i = 0; i < nRelocs; ++i) {
    ElfRel *curr = &reloc[i];
    int relocType = ELFW_R_TYPE(curr->r_info);

    switch (relocType) {
      case R_ARM_JUMP_SLOT: {
        LOGV("Resolving ARM_JUMP_SLOT at offset %" PRIu32, curr->r_offset);
        addr = reinterpret_cast<ElfAddr *>(curr->r_offset + mMapping.location);
        size_t posInSymbolTable = ELFW_R_SYM(curr->r_info);
        void *resolved = resolveData(posInSymbolTable);
        if (resolved == nullptr) {
          LOGE("Failed to resolve symbol(%d) at offset 0x%x", i,
               curr->r_offset);
          return false;
        }
        *addr = reinterpret_cast<ElfAddr>(resolved);
        break;
      }

      default:
        LOGE("Unsupported relocation type: %u for symbol %s", relocType,
             getDataName(ELFW_R_SYM(curr->r_info)));
        return false;
    }
  }
  return true;
}

}  // namespace chre
