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

#ifndef CHRE_PLATFORM_FREERTOS_NANOAPP_LOADER_H_
#define CHRE_PLATFORM_FREERTOS_NANOAPP_LOADER_H_

#include <cinttypes>
#include <cstdlib>

#include "chre/platform/freertos/loader_util.h"

namespace chre {

class NanoappLoader {
 public:
  NanoappLoader() = delete;

  explicit NanoappLoader(void *elfInput) {
    mBinary.rawLocation = elfInput;
  }

  /**
   * Factory method to create a NanoappLoader Instance after loading
   * the buffer containing the ELF binary.
   *
   * @return Class instance on successful load and verification,
   * nullptr otherwise.
   */
  static void *create(void *elfInput);

  bool init();
  void deinit();

  /**
   * Method for pointer lookup by symbol name. Only function pointers
   * are currently supported.
   *
   * @return function pointer on successful lookup, nullptr otherwise
   */
  void *findSymbolByName(const char *name);

 private:
  // TODO(stange): Store this elsewhere since the location will be invalid after
  // loading is complete.
  union Data {
    uintptr_t location;
    void *rawLocation;
  };

  using DynamicHeader = ElfW(Dyn);
  using ElfAddr = ElfW(Addr);
  using ElfHeader = ElfW(Ehdr);
  using ElfRel = ElfW(Rel);  // Relocation table entry,
                             // in section of type SHT_REL
  using ElfRela = ElfW(Rela);
  using ElfSym = ElfW(Sym);
  using ElfWord = ElfW(Word);
  using ProgramHeader = ElfW(Phdr);
  using SectionHeader = ElfW(Shdr);

  static constexpr const char *kSymTableName = ".symtab";
  static constexpr const char *kStrTableName = ".strtab";
  static constexpr const char *kInitArrayName = ".init_array";
  static constexpr const char *kFiniArrayName = ".fini_array";
  // For now, assume all segments are 4K aligned.
  // TODO(karthikmb/stange): See about reducing this.
  static constexpr size_t kBinaryAlignment = 4096;

  char *mDynamicStringTablePtr;
  char *mSectionNamesPtr;
  char *mStringTablePtr;
  uint8_t *mDynamicSymbolTablePtr;
  uint8_t *mSymbolTablePtr;
  size_t mDynamicSymbolTableSize;
  size_t mNumProgramHeaders;
  size_t mNumSectionHeaders;
  size_t mSymbolTableSize;

  Data mBinary;
  Data mMapping;
  DynamicHeader *mDynamicHeaderPtr;
  ElfAddr mLoadBias;
  ElfHeader mElfHeader;
  ProgramHeader *mProgramHeadersPtr;
  SectionHeader *mSectionHeadersPtr;
  SectionHeader mStringTableHeader;
  SectionHeader mSymbolTableHeader;

  void callInitArray();
  void callTerminatorArray();
  bool createMappings();
  bool copyAndVerifyHeaders();
  bool fixRelocations();
  bool initDynamicStringTable();
  bool initDynamicSymbolTable();
  bool resolveGot();
  bool verifyElfHeader();
  bool verifyProgramHeaders();
  bool verifySectionHeaders();
  const char *getDataName(size_t posInSymbolTable);
  const char *getSectionHeaderName(size_t sh_name);
  uintptr_t roundDownToAlign(uintptr_t virtualAddr);
  void freeAllocatedData();
  void mapBss(const ProgramHeader *header);
  void *resolveData(size_t posInSymbolTable);
  DynamicHeader *getDynamicHeader();
  ProgramHeader *getFirstRoSegHeader();
  SectionHeader *getSectionHeader(const char *headerName);
  ElfWord getDynEntry(DynamicHeader *dyn, int field);
};

}  // namespace chre

#endif  // CHRE_PLATFORM_FREERTOS_NANOAPP_LOADER_H_
