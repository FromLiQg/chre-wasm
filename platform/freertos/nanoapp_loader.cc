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

#include "chre/platform/freertos/nanoapp_loader.h"

#include <cinttypes>
#include <cstring>

#include "chre.h"
#include "chre/platform/assert.h"
#include "chre/platform/freertos/loader_util.h"
#include "chre/platform/freertos/memory.h"
#include "chre/util/dynamic_vector.h"
#include "chre/util/macros.h"

#ifndef CHRE_LOADER_ARCH
#define CHRE_LOADER_ARCH EM_ARM
#endif  // CHRE_LOADER_ARCH

#ifndef CHRE_DL_VERBOSE
#define CHRE_DL_VERBOSE false
#endif  // CHRE_DL_VERBOSE

#if CHRE_DL_VERBOSE
#define LOGV(fmt, ...) LOGD(fmt, ##__VA_ARGS__)
#else
#define LOGV(fmt, ...)
#endif  // CHRE_DL_VERBOSE

namespace chre {
namespace {

using ElfHeader = ElfW(Ehdr);
using DynamicHeader = ElfW(Dyn);
using ProgramHeader = ElfW(Phdr);
using SectionHeader = ElfW(Shdr);
using ElfAddr = ElfW(Addr);
using ElfWord = ElfW(Word);
using ElfRel = ElfW(Rel);  // Relocation table entry,
                           // in section of type SHT_REL
using ElfRela = ElfW(Rela);
using ElfSym = ElfW(Sym);

constexpr char kSymTableName[] = ".symtab";
constexpr size_t kSymTableNameLen = sizeof(kSymTableName) - 1;
constexpr char kStrTableName[] = ".strtab";
constexpr size_t kStrTableNameLen = sizeof(kStrTableName) - 1;
constexpr char kInitArrayName[] = ".init_array";
constexpr size_t kInitArrayNameLen = sizeof(kInitArrayName) - 1;
// For now, assume all segments are 4K aligned.
// TODO(karthikmb/stange): See about reducing this.
constexpr size_t kBinaryAlignment = 4096;

struct LoadedBinaryData {
  // TODO(stange): Store this elsewhere since the location will be invalid after
  // loading is complete.
  union Data {
    uintptr_t location;
    void *rawLocation;
  } binary, mapping;

  // TODO(stange): Look into using inline functions rather than storing the
  // pointers directly.
  ElfHeader elfHeader;
  SectionHeader symbolTableHeader;
  SectionHeader stringTableHeader;
  DynamicHeader *dynamicHeaderPtr;
  ProgramHeader *programHeadersPtr;
  SectionHeader *sectionHeadersPtr;
  size_t numProgramHeaders;
  size_t numSectionHeaders;
  char *sectionNamesPtr;
  char *dynamicStringTablePtr;
  uint8_t *dynamicSymbolTablePtr;
  ElfAddr initArrayLoc;
  uint8_t *symbolTablePtr;
  size_t symbolTableSize;
  size_t dynamicSymbolTableSize;
  char *stringTablePtr;
  ElfAddr loadBias;
};

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
    {(void *)chreGetVersion, "chreGetVersion"},
    {(void *)chreGetApiVersion, "chreGetApiVersion"},
    {(void *)chreGetTime, "chreGetTime"},
    {(void *)chreLog, "chreLog"},
    {(void *)chreSendMessageToHostEndpoint, "chreSendMessageToHostEndpoint"},
    {(void *)chreSensorConfigure, "chreSensorConfigure"},
    {(void *)chreSensorFindDefault, "chreSensorFindDefault"},
    {(void *)memset, "memset"},
    {(void *)deleteOverride, "_ZdlPv"},
};

const char *getSectionHeaderName(LoadedBinaryData *data, size_t sh_name) {
  if (sh_name == 0) {
    return "";
  }

  return &data->sectionNamesPtr[sh_name];
}

bool verifyProgramHeaders(LoadedBinaryData *data) {
  // This is a minimal check for now -
  // there should be at least one load segment.
  bool success = false;
  for (size_t i = 0; i < data->numProgramHeaders; ++i) {
    if (data->programHeadersPtr[i].p_type == PT_LOAD) {
      success = true;
      break;
    }
  }
  return success;
}

bool verifySectionHeaders(LoadedBinaryData *data) {
  for (size_t i = 0; i < data->numSectionHeaders; ++i) {
    const char *name =
        getSectionHeaderName(data, data->sectionHeadersPtr[i].sh_name);

    if (strncmp(name, kSymTableName, kSymTableNameLen) == 0) {
      memcpy(&data->symbolTableHeader, &data->sectionHeadersPtr[i],
             sizeof(SectionHeader));
    } else if (strncmp(name, kStrTableName, kStrTableNameLen) == 0) {
      memcpy(&data->stringTableHeader, &data->sectionHeadersPtr[i],
             sizeof(SectionHeader));
    } else if (strncmp(name, kInitArrayName, kInitArrayNameLen) == 0) {
      data->initArrayLoc = data->sectionHeadersPtr[i].sh_addr;
    }
  }

  return true;
}

//! Verifies the ELF header and sets locations of program and section headers
//! in the provided data struct.
bool verifyElfHeader(LoadedBinaryData *data) {
  bool success = false;
  if ((data->elfHeader.e_ident[EI_MAG0] == ELFMAG0) &&
      (data->elfHeader.e_ident[EI_MAG1] == ELFMAG1) &&
      (data->elfHeader.e_ident[EI_MAG2] == ELFMAG2) &&
      (data->elfHeader.e_ident[EI_MAG3] == ELFMAG3) &&
      (data->elfHeader.e_ehsize == sizeof(ElfHeader)) &&
      (data->elfHeader.e_phentsize == sizeof(ProgramHeader)) &&
      (data->elfHeader.e_shentsize == sizeof(SectionHeader)) &&
      (data->elfHeader.e_shstrndx < data->elfHeader.e_shnum) &&
      (data->elfHeader.e_version == EV_CURRENT) &&
      (data->elfHeader.e_machine == CHRE_LOADER_ARCH) &&
      (data->elfHeader.e_type == ET_DYN)) {
    success = true;
  }
  return success;
}

bool copyAndVerifyHeaders(LoadedBinaryData *data) {
  size_t offset = 0;
  bool success = false;
  uint8_t *pDataBytes = static_cast<uint8_t *>(data->binary.rawLocation);

  // Verify the ELF Header
  memcpy(&data->elfHeader, pDataBytes, sizeof(ElfHeader));
  success = verifyElfHeader(data);

  LOGV("Verified ELF header %d", success);

  // Verify Program Headers
  if (success) {
    offset = data->elfHeader.e_phoff;
    size_t programHeadersSizeBytes =
        sizeof(ProgramHeader) * data->elfHeader.e_phnum;
    data->programHeadersPtr =
        static_cast<ProgramHeader *>(memoryAlloc(programHeadersSizeBytes));
    if (data->programHeadersPtr == nullptr) {
      success = false;
      LOG_OOM();
    } else {
      memcpy(data->programHeadersPtr, (pDataBytes + offset),
             programHeadersSizeBytes);
      data->numProgramHeaders = data->elfHeader.e_phnum;
      success = verifyProgramHeaders(data);
    }
  }

  LOGV("Verified Program headers %d", success);

  // Load Section Headers
  if (success) {
    offset = data->elfHeader.e_shoff;
    size_t sectionHeaderSizeBytes =
        sizeof(SectionHeader) * data->elfHeader.e_shnum;
    data->sectionHeadersPtr =
        static_cast<SectionHeader *>(memoryAlloc(sectionHeaderSizeBytes));
    if (data->sectionHeadersPtr == nullptr) {
      success = false;
      LOG_OOM();
    } else {
      memcpy(data->sectionHeadersPtr, (pDataBytes + offset),
             sectionHeaderSizeBytes);
      data->numSectionHeaders = data->elfHeader.e_shnum;
    }
  }

  LOGV("Loaded section headers %d", success);

  // Load section header names
  if (success) {
    SectionHeader &stringSection =
        data->sectionHeadersPtr[data->elfHeader.e_shstrndx];
    size_t sectionSize = stringSection.sh_size;
    data->sectionNamesPtr = static_cast<char *>(memoryAlloc(sectionSize));
    if (data->sectionNamesPtr == nullptr) {
      LOG_OOM();
      success = false;
    } else {
      memcpy(data->sectionNamesPtr,
             (void *)(data->binary.location + stringSection.sh_offset),
             sectionSize);
    }
  }

  LOGV("Loaded section header names %d", success);

  success = verifySectionHeaders(data);
  LOGV("Verified Section headers %d", success);

  // Load symbol table
  if (success) {
    data->symbolTableSize = data->symbolTableHeader.sh_size;
    if (data->symbolTableSize == 0) {
      LOGE("No symbols to resolve");
      success = false;
    } else {
      data->symbolTablePtr =
          static_cast<uint8_t *>(memoryAlloc(data->symbolTableSize));
      if (data->symbolTablePtr == nullptr) {
        LOG_OOM();
        success = false;
      } else {
        memcpy(
            data->symbolTablePtr,
            (void *)(data->binary.location + data->symbolTableHeader.sh_offset),
            data->symbolTableSize);
      }
    }
  }

  LOGV("Loaded symbol table %d", success);

  // Load string table
  if (success) {
    size_t stringTableSize = data->stringTableHeader.sh_size;
    if (data->symbolTableSize == 0) {
      LOGE("No string table corresponding to symbols");
      success = false;
    } else {
      data->stringTablePtr = static_cast<char *>(memoryAlloc(stringTableSize));
      if (data->stringTablePtr == nullptr) {
        LOG_OOM();
        success = false;
      } else {
        memcpy(
            data->stringTablePtr,
            (void *)(data->binary.location + data->stringTableHeader.sh_offset),
            stringTableSize);
      }
    }
  }

  LOGV("Loaded string table %d", success);

  return success;
}

uintptr_t roundDownToAlign(uintptr_t virtualAddr, const size_t alignment) {
  return virtualAddr & -alignment;
}

void mapBss(LoadedBinaryData *data, const ProgramHeader *hdr) {
  // if the memory size of this segment exceeds the file size zero fill the
  // difference.
  LOGV("Program Hdr mem sz: %zu file size: %zu", hdr->p_memsz, hdr->p_filesz);
  if (hdr->p_memsz > hdr->p_filesz) {
    ElfAddr endOfFile = hdr->p_vaddr + hdr->p_filesz + data->loadBias;
    ElfAddr endOfMem = hdr->p_vaddr + hdr->p_memsz + data->loadBias;
    if (endOfMem > endOfFile) {
      auto deltaMem = endOfMem - endOfFile;
      LOGV("Zeroing out %zu from page %p", deltaMem, endOfFile);
      memset((void *)endOfFile, 0, deltaMem);
    }
  }
}

bool createMappings(LoadedBinaryData *data) {
  if (data == nullptr) {
    LOGE("Null data, cannot create mapping!");
    return false;
  }
  // ELF needs pt_load segments to be in contiguous ascending order of
  // virtual addresses. So the first and last segs can be used to
  // calculate the entire address span of the image.
  const ProgramHeader *first = &data->programHeadersPtr[0];
  const ProgramHeader *last =
      &data->programHeadersPtr[data->elfHeader.e_phnum - 1];

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
        (first->p_offset < data->elfHeader.e_phoff) &&
        (first->p_filesz > (data->elfHeader.e_phoff +
                            (data->numProgramHeaders * sizeof(ProgramHeader))));
    if (!valid) {
      LOGE("Load segment program header validation failed");
    } else {
      // Get the last load segment
      while (last > first && last->p_type != PT_LOAD) --last;

      size_t memorySpan = last->p_vaddr + last->p_memsz - first->p_vaddr;
      LOGV("Nanoapp image Memory Span: %u", memorySpan);

      data->mapping.rawLocation =
          memoryAllocDramAligned(kBinaryAlignment, memorySpan);
      if (data->mapping.rawLocation == nullptr) {
        LOG_OOM();
      } else {
        LOGV("Starting location of mappings %p", data->mapping.rawLocation);

        // Calculate the load bias using the first load segment.
        uintptr_t adjustedFirstLoadSegAddr =
            roundDownToAlign(first->p_vaddr, kBinaryAlignment);
        data->loadBias = data->mapping.location - adjustedFirstLoadSegAddr;
        LOGV("Load bias is %" PRIu32, data->loadBias);

        success = true;
      }
    }
  }

  if (success) {
    // Map the remaining segments
    for (const ProgramHeader *ph = first; ph <= last; ++ph) {
      if (ph->p_type == PT_LOAD) {
        ElfAddr segStart = ph->p_vaddr + data->loadBias;
        ElfAddr startPage = roundDownToAlign(segStart, kBinaryAlignment);
        ElfAddr phOffsetPage = roundDownToAlign(ph->p_offset, kBinaryAlignment);
        ElfAddr binaryStartPage = data->binary.location + phOffsetPage;
        size_t segmentLen = ph->p_filesz;

        LOGV("Mapping start page %p from %p with length %zu", startPage,
             (void *)binaryStartPage, segmentLen);
        memcpy((void *)startPage, (void *)binaryStartPage, segmentLen);
        mapBss(data, ph);
      } else {
        LOGE("Non-load segment found between load segments");
        success = false;
        break;
      }
    }
  }

  return success;
}

DynamicHeader *getDynamicHeader(LoadedBinaryData *data) {
  DynamicHeader *dyn = nullptr;
  for (size_t i = 0; i < data->numProgramHeaders; ++i) {
    if (data->programHeadersPtr[i].p_type == PT_DYNAMIC) {
      dyn = (DynamicHeader *)(data->programHeadersPtr[i].p_vaddr +
                              data->loadBias);
      break;
    }
  }
  return dyn;
}

ProgramHeader *getFirstRoSegHeader(LoadedBinaryData *data) {
  // return the first read only segment found
  ProgramHeader *ro = nullptr;
  for (size_t i = 0; i < data->numProgramHeaders; ++i) {
    if (!(data->programHeadersPtr[i].p_flags & PF_W)) {
      ro = &data->programHeadersPtr[i];
      break;
    }
  }
  return ro;
}

ElfWord getDynEntry(DynamicHeader *dyn, int field) {
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

SectionHeader *getSectionHeader(LoadedBinaryData *data,
                                const char *headerName) {
  SectionHeader *rv = nullptr;
  for (size_t i = 0; i < data->numSectionHeaders; ++i) {
    const char *name =
        getSectionHeaderName(data, data->sectionHeadersPtr[i].sh_name);
    if (strncmp(name, headerName, strlen(headerName)) == 0) {
      rv = &data->sectionHeadersPtr[i];
      break;
    }
  }
  return rv;
}

bool initDynamicSymbolTable(LoadedBinaryData *data) {
  bool success = false;
  SectionHeader *dynamicSymbolTablePtr = getSectionHeader(data, ".dynsym");
  CHRE_ASSERT(dynamicSymbolTablePtr != nullptr);
  if (dynamicSymbolTablePtr != nullptr) {
    size_t sectionSize = dynamicSymbolTablePtr->sh_size;
    data->dynamicSymbolTablePtr =
        static_cast<uint8_t *>(memoryAlloc(sectionSize));
    if (data->dynamicSymbolTablePtr == nullptr) {
      LOG_OOM();
    } else {
      memcpy(data->dynamicSymbolTablePtr,
             (void *)(data->binary.location + dynamicSymbolTablePtr->sh_offset),
             sectionSize);
      data->dynamicSymbolTableSize = sectionSize;
      success = true;
    }
  }

  return success;
}

bool initDynamicStringTable(LoadedBinaryData *data) {
  bool success = false;

  SectionHeader *dynamicStringTablePtr = getSectionHeader(data, ".dynstr");
  CHRE_ASSERT(dynamicStringTablePtr != nullptr);
  if (dynamicStringTablePtr != nullptr) {
    size_t stringTableSize = dynamicStringTablePtr->sh_size;
    data->dynamicStringTablePtr =
        static_cast<char *>(memoryAlloc(stringTableSize));
    if (data->dynamicStringTablePtr == nullptr) {
      LOG_OOM();
    } else {
      memcpy(data->dynamicStringTablePtr,
             (void *)(data->binary.location + dynamicStringTablePtr->sh_offset),
             stringTableSize);
      success = true;
    }
  }

  return success;
}

const char *getDataName(size_t posInSymbolTable, LoadedBinaryData *data) {
  size_t sectionSize = data->dynamicSymbolTableSize;
  size_t numElements = sectionSize / sizeof(ElfSym);
  CHRE_ASSERT(posInSymbolTable < numElements);
  char *dataName = nullptr;
  if (posInSymbolTable < numElements) {
    ElfSym *sym = reinterpret_cast<ElfSym *>(
        &data->dynamicSymbolTablePtr[posInSymbolTable * sizeof(ElfSym)]);
    dataName = &data->dynamicStringTablePtr[sym->st_name];
  }
  return dataName;
}

void *resolveData(size_t posInSymbolTable, LoadedBinaryData *data) {
  const char *dataName = getDataName(posInSymbolTable, data);
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

bool fixRelocations(LoadedBinaryData *data) {
  ElfAddr *addr;
  DynamicHeader *dyn = getDynamicHeader(data);
  ProgramHeader *roSeg = getFirstRoSegHeader(data);

  bool success = false;
  if ((dyn == nullptr) || (roSeg == nullptr)) {
    LOGE("Mandatory headers missing from shared object, aborting load");
  } else if (getDynEntry(dyn, DT_RELA) != 0) {
    LOGE("Elf binaries with a DT_RELA dynamic entry are unsupported");
  } else {
    ElfRel *reloc =
        (ElfRel *)(getDynEntry(dyn, DT_REL) + data->binary.location);
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
          addr = reinterpret_cast<ElfAddr *>(curr->r_offset +
                                             data->mapping.location);
          // TODO: When we move to DRAM allocations, we need to check if the
          // above address is in a Read-Only section of memory, and give it
          // temporary write permission if that is the case.
          *addr += data->mapping.location;
          break;

        case R_ARM_GLOB_DAT: {
          LOGV("Resolving R_ARM_GLOB_DAT at offset %" PRIu32, curr->r_offset);
          addr = reinterpret_cast<ElfAddr *>(curr->r_offset +
                                             data->mapping.location);
          size_t posInSymbolTable = ELFW_R_SYM(curr->r_info);
          void *resolved = resolveData(posInSymbolTable, data);
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
      data->dynamicHeaderPtr = dyn;
      success = true;
    }
  }

  return success;
}

bool resolveGot(LoadedBinaryData *data) {
  ElfAddr *addr;
  ElfRel *reloc = (ElfRel *)(getDynEntry(data->dynamicHeaderPtr, DT_JMPREL) +
                             data->mapping.location);
  size_t relocSize = getDynEntry(data->dynamicHeaderPtr, DT_PLTRELSZ);
  size_t nRelocs = relocSize / sizeof(ElfRel);
  LOGV("Resolving GOT with %zu relocations", nRelocs);

  for (size_t i = 0; i < nRelocs; ++i) {
    ElfRel *curr = &reloc[i];
    int relocType = ELFW_R_TYPE(curr->r_info);

    switch (relocType) {
      case R_ARM_JUMP_SLOT: {
        LOGV("Resolving ARM_JUMP_SLOT at offset %" PRIu32, curr->r_offset);
        addr = reinterpret_cast<ElfAddr *>(curr->r_offset +
                                           data->mapping.location);
        size_t posInSymbolTable = ELFW_R_SYM(curr->r_info);
        void *resolved = resolveData(posInSymbolTable, data);
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
             getDataName(ELFW_R_SYM(curr->r_info), data));
        return false;
    }
  }
  return true;
}

void *findSymbolByName(LoadedBinaryData *data, const char *cName) {
  void *symbol = nullptr;
  uint8_t *index = data->symbolTablePtr;
  while (index < (data->symbolTablePtr + data->symbolTableSize)) {
    ElfSym *currSym = reinterpret_cast<ElfSym *>(index);
    const char *symbolName = &data->stringTablePtr[currSym->st_name];

    if (strncmp(symbolName, cName, strlen(cName)) == 0) {
      symbol = ((void *)(data->mapping.location + currSym->st_value));
      break;
    }

    index += sizeof(ElfSym);
  }
  return symbol;
}

void freeLoadedBinaryData(LoadedBinaryData *data) {
  if (data != nullptr) {
    memoryFreeDram(data->mapping.rawLocation);
    memoryFree(data->programHeadersPtr);
    memoryFree(data->sectionHeadersPtr);
    memoryFree(data->sectionNamesPtr);
    memoryFree(data->dynamicStringTablePtr);
    memoryFree(data->dynamicSymbolTablePtr);
    memoryFree(data->symbolTablePtr);
    memoryFree(data->stringTablePtr);
    memoryFree(data);
  }
}

}  // namespace

void *dlopenbuf(void *binary, size_t /*binarySize*/) {
  if (binary == nullptr) {
    return nullptr;
  }

  auto *data = memoryAlloc<LoadedBinaryData>();

  if (data != nullptr) {
    bool success = false;
    data->binary.rawLocation = binary;
    if (!copyAndVerifyHeaders(data)) {
      LOGE("Failed to verify headers");
    } else if (!createMappings(data)) {
      LOGE("Failed to create mappings");
    } else if (!initDynamicStringTable(data)) {
      LOGE("Failed to initialize dynamic string table");
    } else if (!initDynamicSymbolTable(data)) {
      LOGE("Failed to initialize dynamic symbol table");
    } else if (!fixRelocations(data)) {
      LOGE("Failed to fix relocations");
    } else if (!resolveGot(data)) {
      LOGE("Failed to resolve GOT");
    } else {
      // TODO(karthikmb / stange): Initialize static variables
      success = true;
    }
    if (!success) {
      freeLoadedBinaryData(data);
      data = nullptr;
    }
  } else {
    LOG_OOM();
  }

  return data;
}

void *dlsym(void *handle, const char *symbol) {
  LOGV("Attempting to find %s", symbol);

  auto *data = reinterpret_cast<LoadedBinaryData *>(handle);
  void *resolvedSymbol = nullptr;
  if (data != nullptr) {
    resolvedSymbol = findSymbolByName(data, symbol);
    LOGV("Found symbol at %p", resolvedSymbol);
  }
  return resolvedSymbol;
}

int dlclose(void *handle) {
  auto *data = static_cast<LoadedBinaryData *>(handle);

  // TODO(karthikmb / stange): Destroy static variables
  freeLoadedBinaryData(data);

  return 0;
}

const char *dlerror() {
  return "Shared library load failed";
}

}  // namespace chre
