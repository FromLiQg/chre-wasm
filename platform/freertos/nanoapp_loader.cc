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
#include "chre/platform/freertos/loader_util.h"
#include "chre/util/dynamic_vector.h"

#ifndef CHRE_LOADER_ARCH
#define CHRE_LOADER_ARCH EM_ARM
#endif  // CHRE_LOADER_ARCH

using ElfHeader = ElfW(Ehdr);
using DynamicHeader = ElfW(Dyn);
using ProgramHeader = ElfW(Phdr);
using SectionHeader = ElfW(Shdr);
using ElfAddr = ElfW(Addr);
using ElfWord = ElfW(Word);
using ElfRela = ElfW(Rela);  // Relocation table entry with addend,
                             // in section of type SHT_RELA
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
  uintptr_t binaryLocation;

  // TODO(stange): Look into using inline functions rather than storing the
  // pointers directly.
  ElfHeader *elfHeader;
  ProgramHeader *programHeaders;
  SectionHeader *sectionHeaders;
  SectionHeader *symbolTable;
  SectionHeader *stringTable;
  size_t numSectionHeaders;
  size_t numProgramHeaders;
  ElfAddr initArrayLoc;
};

bool verifyProgramHeaders(LoadedBinaryData *data) {
  // this is a minimal check for now -
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

const char *getSectionHeaderName(LoadedBinaryData *data, size_t sh_name) {
  if (sh_name == 0) {
    return "";
  }
  SectionHeader &stringSection =
      data->sectionHeaders[data->elfHeader->e_shstrndx];
  return reinterpret_cast<char *>(data->binaryLocation +
                                  stringSection.sh_offset);
}

bool verifySectionHeaders(LoadedBinaryData *data) {
  // this is a minimal check for now
  for (size_t i = 0; i < data->numSectionHeaders; ++i) {
    const char *name =
        getSectionHeaderName(data, data->sectionHeaders[i].sh_name);

    if (strncmp(name, kSymTableName, kSymTableNameLen) == 0) {
      data->symbolTable = &data->sectionHeaders[i];
    } else if (strncmp(name, kStrTableName, kStrTableNameLen) == 0) {
      data->stringTable = &data->sectionHeaders[i];
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
  // ELF header must start at the beginning of the binary.
  data->elfHeader = reinterpret_cast<ElfHeader *>(data->binaryLocation);

  bool success = false;
  if ((data->elfHeader->e_ident[EI_MAG0] == ELFMAG0) &&
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
    data->numProgramHeaders = data->elfHeader->e_phnum;
    data->programHeaders = reinterpret_cast<ProgramHeader *>(
        data->binaryLocation + data->elfHeader->e_phoff);

    data->numSectionHeaders = data->elfHeader->e_shnum;
    data->sectionHeaders = reinterpret_cast<SectionHeader *>(
        data->binaryLocation + data->elfHeader->e_shoff);

    success = true;
  }
  return success;
}

bool verifyAllHeaders(LoadedBinaryData *data) {
  return verifyElfHeader(data) && verifyProgramHeaders(data) &&
         verifySectionHeaders(data);
}

}  // namespace

void *dlopenbuf(void *binary, size_t /* binarySize */) {
  LoadedBinaryData *data =
      static_cast<LoadedBinaryData *>(memoryAlloc(sizeof(LoadedBinaryData)));
  if (data != nullptr) {
    memset(data, 0, sizeof(*data));
    data->binaryLocation = reinterpret_cast<uintptr_t>(binary);
    if (!(verifyAllHeaders(data))) {
      memoryFree(data);
      data = nullptr;
    }
  }

  return data;
}

void *dlsym(void * /*handle*/, const char * /*symbol*/) {
  return nullptr;
}

int dlclose(void *handle) {
  memoryFree(handle);
  return 0;
}

}  // namespace chre
