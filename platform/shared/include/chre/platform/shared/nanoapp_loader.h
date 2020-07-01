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

#ifndef CHRE_PLATFORM_SHARED_NANOAPP_LOADER_H_
#define CHRE_PLATFORM_SHARED_NANOAPP_LOADER_H_

#include <cinttypes>
#include <cstdlib>

#include "chre/platform/freertos/loader_util.h"

#include "chre/util/nested_data_ptr.h"

namespace chre {

/**
 * Provides dynamic loading support for nanoapps on FreeRTOS-based platforms.
 * At a high level, this class is responsible for mapping the provided binary
 * into CHRE's address space, relocating and resolving symbols, and initializing
 * and freeing static data.
 */
class NanoappLoader {
 public:
  NanoappLoader() = delete;

  explicit NanoappLoader(void *elfInput) {
    mBinary.dataPtr = elfInput;
  }

  /**
   * Factory method to create a NanoappLoader Instance after loading
   * the buffer containing the ELF binary.
   *
   * @return Class instance on successful load and verification,
   * nullptr otherwise.
   */
  static void *create(void *elfInput);

  /**
   * Attempts to locate the exported symbol specified by the given function
   * name.
   *
   * @param name A null-terminated char array that is the name of the function
   *     to be found.
   * @return The address of the function. nullptr if not found.
   */
  static void *findExportedSymbol(const char *name);

  /**
   * Closes the loader, freeing any state associated with the loaded ELF binary
   * and unmapping it from memory. Prior to unmapping from memory, any static
   * termination functions will be invoked.
   */
  void close();

  /**
   * Method for pointer lookup by symbol name. Only function pointers
   * are currently supported.
   *
   * @return function pointer on successful lookup, nullptr otherwise
   */
  void *findSymbolByName(const char *name);

 private:
  /**
   * Opens the ELF binary. This maps the binary into memory, resolves symbols,
   * and invokes any static initializers.
   *
   * @return true if all required opening steps were completed.
   */
  bool open();

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

  //! Name of various segments in the ELF that need to be looked up
  static constexpr const char *kSymTableName = ".symtab";
  static constexpr const char *kStrTableName = ".strtab";
  static constexpr const char *kInitArrayName = ".init_array";
  static constexpr const char *kFiniArrayName = ".fini_array";
  // For now, assume all segments are 4K aligned.
  // TODO(karthikmb/stange): See about reducing this.
  static constexpr size_t kBinaryAlignment = 4096;

  //! Pointer to the table of symbol names that need to be resolved.
  char *mDynamicStringTablePtr;
  //! Pointer to the table of all the section names.
  char *mSectionNamesPtr;
  //! Pointer to the table of symbol names of defined symbols.
  char *mStringTablePtr;
  //! Pointer to the table of symbol information for undefined symbols.
  uint8_t *mDynamicSymbolTablePtr;
  //! Pointer to the table of symbol information for defined symbols.
  uint8_t *mSymbolTablePtr;
  //! Size of the data pointed to by mDynamicSymbolTablePtr.
  size_t mDynamicSymbolTableSize;
  //! Number of ProgramHeaders pointed to by mProgramHeadersPtr.
  size_t mNumProgramHeaders;
  //! Number of SectionHeaders pointed to by mSectionHeadersPtr.
  size_t mNumSectionHeaders;
  //! Size of the data pointed to by mSymbolTablePtr.
  size_t mSymbolTableSize;

  // TODO(stange): Store this elsewhere since the location will be invalid after
  // loading is complete.
  //! The ELF that is being mapped into the system. This pointer will be invalid
  //! after open returns.
  NestedDataPtr<uintptr_t> mBinary;
  //! The starting location of the memory that has been mapped into the system.
  NestedDataPtr<uintptr_t> mMapping;
  //! Pointer to the array of dynamic header entries.
  DynamicHeader *mDynamicHeaderPtr;
  //! The difference between where the first load segment was mapped into
  //! virtual memory and what the virtual load offset was of that segment.
  ElfAddr mLoadBias;
  //! Copy of the ELF header from the mapped binary.
  ElfHeader mElfHeader;
  //! Pointer to the array of program header entries.
  ProgramHeader *mProgramHeadersPtr;
  //! Pointer to the array of section header entries.
  SectionHeader *mSectionHeadersPtr;
  //! Header containing size and offset information of the string table.
  SectionHeader mStringTableHeader;
  //! Header containing size and offset information of the symbol table.
  SectionHeader mSymbolTableHeader;

  /**
   * Invokes all initialization functions in .init_array segment.
   */
  void callInitArray();

  /**
   * Invokes all termination functions in the .fini_array segment.
   */
  void callTerminatorArray();

  /**
   * Allocates memory for all load segments that need to be mapped into virtual
   * memory and copies the load segments into the newly allocated memory.
   *
   * @return true if the memory for mapping was allocated and the load segments
   *     were formatted correctly.
   */
  bool createMappings();

  /**
   * Copies various sections and headers from the ELF while verifying that they
   * match the ELF format specficiation.
   *
   * @return true if all data was copied and verified.
   */
  bool copyAndVerifyHeaders();

  /**
   * Resolves all relocated symbols located in the DT_REL table.
   *
   * @return true if all relocated symbols were resolved.
   */
  bool fixRelocations();

  /**
   * Creates a copy of the dynamic string table.
   *
   * @return true if the table was created.
   */
  bool initDynamicStringTable();

  /**
   * Creates a copy of the dynamic symbol table.
   *
   * @return true if the table was created.
   */
  bool initDynamicSymbolTable();

  /**
   * Resolves entries in the Global Offset Table (GOT) to facility the ELF's
   * compiled using position independent code (PIC).
   *
   * @return true if all symbols were resolved.
   */
  bool resolveGot();

  /**
   * Verifies the ELF header has correct values based on the ELF spec.
   *
   * @return true if the header passed verification.
   */
  bool verifyElfHeader();

  /**
   * Verifies basic information about program headers.
   *
   * @return true if the headers passed verification.
   */
  bool verifyProgramHeaders();

  /**
   * Verifies basic information about section headers.
   *
   * @return true if the headers passed verification.
   */
  bool verifySectionHeaders();

  /**
   * Retrieves the symbol name of data located at the given position in the
   * symbol table.
   *
   * @param posInSymbolTable The position in the symbol table where information
   *     about the symbol can be found.
   * @return The symbol's name or nullptr if not found.
   */
  const char *getDataName(size_t posInSymbolTable);

  /**
   * Retrieves the name of the section header located at the given offset in the
   * section name table.
   *
   * @param headerOffset The offset in the section names table where the header
   *     is located.
   * @return The section's name or the empty string if the offset is 0.
   */
  const char *getSectionHeaderName(size_t headerOffset);

  /**
   * Rounds the given address down to the closest alignment boundary.
   *
   * @param virtualAddr The address to be rounded.
   * @return An address that is a multiple of the platform's alignment and is
   *     less than or equal to virtualAddr.
   */
  uintptr_t roundDownToAlign(uintptr_t virtualAddr);

  /**
   * Frees any data that was allocated as part of loading the ELF into memory.
   */
  void freeAllocatedData();

  /**
   * Ensures the BSS section is properly mapped into memory. If there is a
   * difference between the size of the BSS section in the ELF binary and the
   * size it needs to be in memory, the rest of the section is zeroed out.
   *
   * @param header The ProgramHeader of the BSS section that is being mapped in.
   */
  void mapBss(const ProgramHeader *header);

  /**
   * Resolves the address of an undefined symbol located at the given position
   * in the symbol table. This symbol must be defined and exposed by the given
   * platform in order for it to be resolved successfully.
   *
   * @param posInSymbolTable The position of the undefined symbol in the symbol
   *     table.
   * @return The address of the resolved symbol. nullptr if not found.
   */
  void *resolveData(size_t posInSymbolTable);

  /**
   * @return The address for the dynamic segment. nullptr if not found.
   */
  DynamicHeader *getDynamicHeader();

  /**
   * @return The address of the first read-only segment. nullptr if not found.
   */
  ProgramHeader *getFirstRoSegHeader();

  /**
   * Retrieves the section header with the given name.
   *
   * @param headerName The name of the section header to find.
   * @return The address of the section. nullptr if not found.
   */
  SectionHeader *getSectionHeader(const char *headerName);

  /**
   * Returns the first entry in the dynamic header that has a tag that matches
   * the given field.
   *
   * @param dyn The dynamic header for the binary.
   * @param field The field to be searched for.
   * @return The value found at the entry. 0 if the entry isn't found.
   */
  static ElfWord getDynEntry(DynamicHeader *dyn, int field);
};

}  // namespace chre

#endif  // CHRE_PLATFORM_SHARED_NANOAPP_LOADER_H_
