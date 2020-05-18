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

#include <cstring>

#include "chre.h"
#include "chre/platform/assert.h"
#include "chre/platform/freertos/loader_util.h"
#include "chre/util/dynamic_vector.h"
#include "chre/util/macros.h"

#ifndef CHRE_LOADER_ARCH
#define CHRE_LOADER_ARCH EM_ARM
#endif  // CHRE_LOADER_ARCH

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

namespace chre {
namespace {

constexpr char kSymTableName[] = ".symtab";
constexpr size_t kSymTableNameLen = sizeof(kSymTableName) - 1;
constexpr char kStrTableName[] = ".strtab";
constexpr size_t kStrTableNameLen = sizeof(kStrTableName) - 1;
constexpr char kInitArrayName[] = ".init_array";
constexpr size_t kInitArrayNameLen = sizeof(kInitArrayName) - 1;

struct LoadedBinaryData {
  // TODO(stange): Store this elsewhere since the location will be invalid after
  // loading is complete.
  union Data {
    uintptr_t location;
    void *rawLocation;
  } binary, mapping;

  // TODO(stange): Look into using inline functions rather than storing the
  // pointers directly.
  ElfHeader *elfHeader;
  ProgramHeader *programHeaders;
  DynamicHeader *dynamicHeader;
  SectionHeader *sectionHeaders;
  SectionHeader *symbolTableHeader;
  SectionHeader *stringTableHeader;
  size_t numSectionHeaders;
  size_t numProgramHeaders;
  char *sectionNames;
  uint8_t *dynamicSection;
  char *dynamicStringTable;
  uint8_t *dynamicSymbolTable;
  ElfAddr initArrayLoc;
  uint8_t *symbolTable;
  size_t symbolTableSize;
  char *stringTable;
  ElfAddr loadBias;
};

struct ExportedData {
  void *data;
  const char *dataName;
};

// TODO(karthikmb/stange): While this array was hand-coded for simple
// "hello-world" prototyping, the list of exported symbols must be
// generated to minimize runtime errors and build breaks.
ExportedData gExportedData[] = {
    {(void *)chreLog, "chreLog"},
    {(void *)chreGetVersion, "chreGetVersion"},
    {(void *)chreGetApiVersion, "chreGetApiVersion"},
    {(void *)chreGetTime, "chreGetTime"},
};

const char *getSectionHeaderName(LoadedBinaryData *data, size_t sh_name) {
  if (sh_name == 0) {
    return "";
  }

  if (data->sectionNames[0] == 0) {
    SectionHeader &stringSection =
        data->sectionHeaders[data->elfHeader->e_shstrndx];
    size_t sectionSize = stringSection.sh_size;
    memcpy(data->sectionNames,
           (void *)(data->binary.location + stringSection.sh_offset),
           sectionSize);
  }

  return &data->sectionNames[sh_name];
}

bool verifyProgramHeaders(LoadedBinaryData *data) {
  // This is a minimal check for now -
  // there should be at least one load segment.
  bool success = false;
  for (size_t i = 0; i < data->numProgramHeaders; ++i) {
    if (data->programHeaders[i].p_type == PT_LOAD) {
      success = true;
      break;
    }
  }
  return success;
}

bool verifySectionHeaders(LoadedBinaryData *data) {
  for (size_t i = 0; i < data->numSectionHeaders; ++i) {
    const char *name =
        getSectionHeaderName(data, data->sectionHeaders[i].sh_name);

    if (strncmp(name, kSymTableName, kSymTableNameLen) == 0) {
      data->symbolTableHeader = &data->sectionHeaders[i];
    } else if (strncmp(name, kStrTableName, kStrTableNameLen) == 0) {
      data->stringTableHeader = &data->sectionHeaders[i];
    } else if (strncmp(name, kInitArrayName, kInitArrayNameLen) == 0) {
      data->initArrayLoc = data->sectionHeaders[i].sh_addr;
    }
  }

  // While it's technically possible for a binary to have no symbol table, it
  // wouldn't make a useful nanoapp if it didn't invoke some CHRE APIs so
  // verify a symbol table exists.
  if (data->symbolTable == nullptr || data->stringTable == nullptr) {
    return false;
  }
  return true;
}

//! Verifies the ELF header and sets locations of program and section headers
//! in the provided data struct.
bool verifyElfHeader(LoadedBinaryData *data) {
  bool success = false;
  if ((data->elfHeader != nullptr) &&
      (data->elfHeader->e_ident[EI_MAG0] == ELFMAG0) &&
      (data->elfHeader->e_ident[EI_MAG1] == ELFMAG1) &&
      (data->elfHeader->e_ident[EI_MAG2] == ELFMAG2) &&
      (data->elfHeader->e_ident[EI_MAG3] == ELFMAG3) &&
      (data->elfHeader->e_ehsize == sizeof(ElfHeader)) &&
      (data->elfHeader->e_phentsize == sizeof(ProgramHeader)) &&
      (data->elfHeader->e_shentsize == sizeof(SectionHeader)) &&
      (data->elfHeader->e_shstrndx < data->elfHeader->e_shnum) &&
      (data->elfHeader->e_version == EV_CURRENT) &&
      (data->elfHeader->e_machine == CHRE_LOADER_ARCH) &&
      (data->elfHeader->e_type == ET_DYN)) {
    success = true;
  }
  return success;
}

bool copyAndVerifyHeaders(LoadedBinaryData *data) {
  size_t offset = 0;
  bool success = false;
  uint8_t *pDataBytes = static_cast<uint8_t *>(data->binary.rawLocation);

  // Verify the ELF Header
  memcpy(data->elfHeader, pDataBytes, sizeof(ElfHeader));
  success = verifyElfHeader(data);

  // Verify Program Headers
  if (success) {
    offset = data->elfHeader->e_phoff;
    size_t programHeadersSizeBytes =
        sizeof(ProgramHeader) * data->elfHeader->e_phnum;
    memcpy(data->programHeaders, (pDataBytes + offset),
           programHeadersSizeBytes);
    data->numProgramHeaders = data->elfHeader->e_phnum;
    success = verifyProgramHeaders(data);
  }

  // Verify Section Headers
  if (success) {
    offset = data->elfHeader->e_shoff;
    size_t sectionHeaderSizeBytes =
        sizeof(SectionHeader) * data->elfHeader->e_shnum;
    memcpy(data->sectionHeaders, (pDataBytes + offset), sectionHeaderSizeBytes);
    data->numSectionHeaders = data->elfHeader->e_shnum;
    success = verifySectionHeaders(data);
  }

  return success;
}

uintptr_t roundUpToAlign(uintptr_t virtualAddr, const size_t alignment) {
  return virtualAddr & -alignment;
}

uintptr_t roundDownToAlign(uintptr_t virtualAddr, const size_t alignment) {
  return (virtualAddr + alignment - 1) & -alignment;
}

bool createMappings(LoadedBinaryData *data) {
  if (data == nullptr) {
    LOGE("Null data, cannot create mapping!");
    return false;
  }
  // ELF needs pt_load segments to be in contiguous ascending order of
  // virtual addresses. So the first and last segs can be used to
  // calculate the entire address span of the image.
  const ProgramHeader *first = &data->programHeaders[0];
  const ProgramHeader *last =
      &data->programHeaders[data->elfHeader->e_phnum - 1];

  // Find first load segment
  // TODO fix this as it can crash because we blindly increment 'first'.
  while (first->p_type != PT_LOAD) {
    ++first;
  }

  // Verify that the first load segment has a program header
  // first byte of a valid load segment can't be greater than the
  // program header offset
  bool valid =
      (first->p_offset < data->elfHeader->e_phoff) &&
      (first->p_filesz > (data->elfHeader->e_phoff +
                          (data->numProgramHeaders * sizeof(ProgramHeader))));
  if (!valid) {
    LOGE("Load segment program header validation failed");
    return false;
  }

  // Get the last load segment
  while (last > first && last->p_type != PT_LOAD) --last;

  size_t memorySpan = last->p_vaddr + last->p_memsz - first->p_vaddr;
  LOGD("Nanoapp image Memory Span: %u", memorySpan);

  // map the first segment while accounting for alignment
  uintptr_t adjustedFirstLoadSegAddr = roundDownToAlign(first->p_vaddr, 4096);
  uintptr_t adjustedFirstLoadSegOffset =
      roundDownToAlign(first->p_offset, 4096);

  memcpy(data->mapping.rawLocation,
         (void *)(data->binary.location + adjustedFirstLoadSegOffset),
         memorySpan);
  // mapping might not be aligned, store a load bias
  data->loadBias = data->mapping.location - adjustedFirstLoadSegAddr;

  if (first->p_offset > data->elfHeader->e_phoff ||
      first->p_filesz < data->elfHeader->e_phoff + (data->elfHeader->e_phnum *
                                                    sizeof(ProgramHeader))) {
    LOGE("First load segment of ELF does not contain phdrs!");
    return false;
  }

  // maintain a variable to know where the previous segment load ended
  ElfAddr prevSegEnd = first->p_vaddr + first->p_memsz + data->loadBias;

  // Map the remaining segments
  for (const ProgramHeader *ph = first + 1; ph <= last; ++ph) {
    if (ph->p_type == PT_LOAD) {
      prevSegEnd = ph->p_vaddr + data->loadBias + ph->p_memsz;

      ElfAddr startPage = roundDownToAlign(ph->p_vaddr + data->loadBias, 4096);
      ElfAddr endPage = roundUpToAlign(prevSegEnd, 4096);
      ElfAddr phOffsetPage = roundDownToAlign(ph->p_offset, 4096);

      memcpy((void *)startPage, (void *)(data->binary.location + phOffsetPage),
             (endPage - startPage));
    }
  }

  // TODO (karthikmb/stange) - bss isn't mapped yet
  return true;
}

DynamicHeader *getDynamicHeader(LoadedBinaryData *data) {
  DynamicHeader *dyn = nullptr;
  for (size_t i = 0; i < data->numProgramHeaders; ++i) {
    if (data->programHeaders[i].p_type == PT_DYNAMIC) {
      dyn = (DynamicHeader *)(data->programHeaders[i].p_vaddr + data->loadBias);
      break;
    }
  }
  return dyn;
}

ProgramHeader *getFirstRoSegHeader(LoadedBinaryData *data) {
  // return the first read only segment found
  ProgramHeader *ro = nullptr;
  for (size_t i = 0; i < data->numProgramHeaders; ++i) {
    if (!(data->programHeaders[i].p_flags & PF_W)) {
      ro = &data->programHeaders[i];
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
        getSectionHeaderName(data, data->sectionHeaders[i].sh_name);
    if (strncmp(name, headerName, strlen(headerName)) == 0) {
      rv = &data->sectionHeaders[i];
      break;
    }
  }
  return rv;
}

const char *getDataName(size_t posInSymbolTable, LoadedBinaryData *data) {
  SectionHeader *dynamicSymbolTable = getSectionHeader(data, ".dynsym");
  CHRE_ASSERT(dynamicSymbolTable != nullptr);
  size_t sectionSize = dynamicSymbolTable->sh_size;
  size_t numElements = sectionSize / sizeof(ElfSym);
  CHRE_ASSERT(posInSymbolTable < numElements);

  // TODO remove size restrictions when moving to dynamic allocations
  CHRE_ASSERT_LOG(sectionSize < 256, "Section size > 256 = %u", sectionSize);
  memcpy(data->dynamicSymbolTable,
         (void *)(data->binary.location + dynamicSymbolTable->sh_offset),
         sectionSize);

  SectionHeader *dynamicStringTable = getSectionHeader(data, ".dynstr");
  CHRE_ASSERT(dynamicStringTable != nullptr);

  size_t stringTableSize = dynamicStringTable->sh_size;
  CHRE_ASSERT_LOG(stringTableSize < 512, "string table section size > 512 = %u",
                  stringTableSize);
  memcpy(data->dynamicStringTable,
         (void *)(data->binary.location + dynamicStringTable->sh_offset),
         stringTableSize);

  ElfSym *sym = reinterpret_cast<ElfSym *>(
      &data->dynamicSymbolTable[posInSymbolTable * sizeof(ElfSym)]);
  return &data->dynamicStringTable[sym->st_name];
}

void *resolveData(size_t posInSymbolTable, LoadedBinaryData *data) {
  const char *dataName = getDataName(posInSymbolTable, data);

  for (size_t i = 0; i < ARRAY_SIZE(gExportedData); i++) {
    if (strncmp(dataName, gExportedData[i].dataName,
                strlen(gExportedData[i].dataName)) == 0) {
      return gExportedData[i].data;
    }
  }

  return nullptr;
}

bool fixRelocations(LoadedBinaryData *data) {
  ElfAddr *addr;
  DynamicHeader *dyn = getDynamicHeader(data);
  ProgramHeader *roSeg = getFirstRoSegHeader(data);

  if ((dyn == nullptr) || (roSeg == nullptr)) {
    LOGE("Mandatory headers missing from shared object, aborting load");
    return false;
  }

  ElfWord relaCheck = getDynEntry(dyn, DT_RELA);
  if (relaCheck != 0) {
    LOGE("Elf binaries with a DT_RELA dynamic entry are unsupported");
    return false;
  }

  ElfRel *reloc = (ElfRel *)(getDynEntry(dyn, DT_REL) + data->binary.location);
  size_t relocSize = getDynEntry(dyn, DT_RELSZ);
  size_t nRelocs = relocSize / sizeof(ElfRel);

  for (size_t i = 0; i < nRelocs; ++i) {
    ElfRel *curr = &reloc[i];
    int relocType = ELFW_R_TYPE(curr->r_info);
    switch (relocType) {
      case R_ARM_RELATIVE:
        addr = reinterpret_cast<ElfAddr *>(curr->r_offset +
                                           data->mapping.location);
        // TODO: When we move to DRAM allocations, we need to check if the
        // above address is in a Read-Only section of memory, and give it
        // temporary write permission if that is the case.
        *addr += data->mapping.location;
        break;

      case R_ARM_GLOB_DAT: {
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
        return false;

      default:
        LOGE("Invalid relocation type %u", relocType);
        return false;
    }
  }

  data->dynamicHeader = dyn;
  return true;
}

bool resolveGot(LoadedBinaryData *data) {
  ElfAddr *addr;
  ElfRel *reloc = (ElfRel *)(getDynEntry(data->dynamicHeader, DT_JMPREL) +
                             data->mapping.location);
  size_t relocSize = getDynEntry(data->dynamicHeader, DT_PLTRELSZ);
  size_t nRelocs = relocSize / sizeof(ElfRel);

  for (size_t i = 0; i < nRelocs; ++i) {
    ElfRel *curr = &reloc[i];
    int relocType = ELFW_R_TYPE(curr->r_info);

    switch (relocType) {
      case R_ARM_JUMP_SLOT: {
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

bool readSymbolAndStringTables(LoadedBinaryData *data) {
  data->symbolTableSize = data->symbolTableHeader->sh_size;

  // TODO: Remove size restrictions when we move to dynamic allocations
  if (data->symbolTable[0] == 0) {
    CHRE_ASSERT_LOG(data->symbolTableSize < 512, "Symbol table size > 512 = %u",
                    data->symbolTableSize);
    memcpy(data->symbolTable,
           (void *)(data->binary.location + data->symbolTableHeader->sh_offset),
           data->symbolTableSize);
  }

  if (data->stringTable[0] == 0) {
    size_t stringTableSize = data->stringTableHeader->sh_size;
    CHRE_ASSERT_LOG(stringTableSize < 512, "string table size > 512 = %u",
                    stringTableSize);
    memcpy(data->stringTable,
           (void *)(data->binary.location + data->stringTableHeader->sh_offset),
           stringTableSize);
  }
  return true;
}

void *findSymbolByName(LoadedBinaryData *data, const char *cName) {
  void *symbol = nullptr;
  uint8_t *index = data->symbolTable;
  while (index < (data->symbolTable + data->symbolTableSize)) {
    ElfSym *currSym = reinterpret_cast<ElfSym *>(index);
    const char *symbolName = &data->stringTable[currSym->st_name];

    if (strncmp(symbolName, cName, strlen(cName)) == 0) {
      symbol = ((void *)(data->mapping.location + currSym->st_value));
      break;
    }

    index += sizeof(ElfSym);
  }
  return symbol;
}

LoadedBinaryData *initializeBuffersStatic() {
  // Temporary workaround function until we move to dynamic allocations
  // - go ham and assign static memory to everything
  static uint8_t dataBufferTemp[sizeof(LoadedBinaryData)];
  static ElfHeader elfHdrTemp;
  static ProgramHeader phdrTemp[8];
  static SectionHeader shdrTemp[37];
  static SectionHeader strtabTemp;
  static SectionHeader symtabTemp;
  static constexpr size_t kMaxMappingSize = 20000 + 4096;
  static uint8_t mapping[kMaxMappingSize];
  static char sectionNamesTemp[512];
  static uint8_t dynamicSymoblTableTemp[256];
  static char dynamicStringTableTemp[512];
  static uint8_t symbolTableTemp[512];
  static char stringTableTemp[512];

  auto *data = reinterpret_cast<LoadedBinaryData *>(&dataBufferTemp[0]);
  data->elfHeader = &elfHdrTemp;
  data->programHeaders = &phdrTemp[0];
  data->sectionHeaders = &shdrTemp[0];
  data->stringTableHeader = &strtabTemp;
  data->symbolTableHeader = &symtabTemp;
  data->sectionNames = sectionNamesTemp;
  data->dynamicSymbolTable = dynamicSymoblTableTemp;
  data->dynamicStringTable = dynamicStringTableTemp;
  data->symbolTable = symbolTableTemp;
  data->stringTable = stringTableTemp;
  data->mapping.location = roundDownToAlign((uintptr_t)mapping, 4096);

  return data;
}

}  // namespace

void *dlopenbuf(void *binary, size_t /* binarySize */) {
  if (binary == nullptr) {
    return nullptr;
  }

  bool success = false;
  LoadedBinaryData *data = initializeBuffersStatic();

  if (data != nullptr) {
    data->binary.rawLocation = binary;
    if (!copyAndVerifyHeaders(data)) {
      LOGE("Failed to verify headers");
    } else if (!createMappings(data)) {
      LOGE("Failed to create mappings");
    } else if (!fixRelocations(data)) {
      LOGE("Failed to fix relocations");
    } else if (!resolveGot(data)) {
      LOGE("Failed to resolve GOT");
    } else if (!readSymbolAndStringTables(data)) {
      LOGE("Failed to read symbol and string tables");
    } else {
      success = true;
    }
  } else {
    LOG_OOM();
  }

  return (success == true) ? data : nullptr;
}

void *dlsym(void *handle, const char *symbol) {
  auto *data = reinterpret_cast<LoadedBinaryData *>(handle);
  if (data != nullptr) {
    return findSymbolByName(data, symbol);
  }
  return nullptr;
}

int dlclose(void *handle) {
  // TODO(karthikmb/stange) - Free handle when it's allocated
  handle = nullptr;
  return 0;
}

const char *dlerror() {
  return "Shared library load failed";
}

}  // namespace chre
