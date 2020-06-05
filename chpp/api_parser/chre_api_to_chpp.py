#!/usr/bin/python3
#
# Copyright (C) 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import argparse
import json
import os.path
import subprocess
from collections import defaultdict
from datetime import datetime

from pyclibrary import CParser

LICENSE_HEADER = """/*
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
"""

# Paths for output, relative to system/chre
CHPP_SERVICE_INCLUDE_PATH = "chpp/include/chpp/services"
CHPP_SERVICE_SOURCE_PATH = "chpp/services"


def system_chre_abs_path():
    """Gets the absolute path to the system/chre directory containing this script."""
    script_dir = os.path.dirname(os.path.realpath(__file__))
    # Assuming we're at system/chre/chpp/api_parser (i.e. up 2 to get to system/chre)
    chre_project_base_dir = os.path.normpath(script_dir + "/../..")
    return chre_project_base_dir


class CodeGenerator:
    """Given an ApiParser object, generates a header file with structure definitions in CHPP format.
    """

    def __init__(self, api):
        """
        :param api: ApiParser object
        """
        self.api = api
        self.json = api.json
        # Turn "chre_api/include/chre_api/chre/wwan.h" into "wwan"
        self.service_name = self.json['filename'].split('/')[-1].split('.')[0]
        self.capitalized_service_name = self.service_name[0].upper() + self.service_name[1:]
        self.commit_hash = subprocess.getoutput('git describe --always --long --dirty')

    # ----------------------------------------------------------------------------------------------
    # Header generation methods (plus some methods shared with encoder generation)
    # ----------------------------------------------------------------------------------------------

    def _autogen_notice(self):
        out = []
        out.append("// This file was automatically generated by {}\n".format(
            os.path.basename(__file__)))
        out.append("// Date: {} UTC\n".format(datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')))
        out.append("// Source: {} @ commit {}\n\n".format(self.json['filename'], self.commit_hash))
        out.append("// DO NOT modify this file directly, as those changes will be lost the next\n")
        out.append("// time the script is executed\n\n")
        return out

    def _dump_to_file(self, output_filename, contents, dry_run, skip_clang_fomat):
        """Outputs contents to output_filename, or prints contents if dry_run is True"""
        if dry_run:
            print("---- {} ----".format(output_filename))
            print(contents)
            print("---- end of {} ----\n".format(output_filename))
        else:
            with open(output_filename, 'w') as f:
                f.write(contents)

            if not skip_clang_fomat:
                clang_format_path = os.path.normpath(
                    "../../../../prebuilts/clang/host/linux-x86/clang-stable/bin/clang-format")
                args = [clang_format_path, '-i', output_filename]
                result = subprocess.run(args)
                result.check_returncode()

    def _is_array_type(self, type_info):
        # If this is an array type, declarators will be a tuple containing a list of
        # a single int element giving the size of the array
        return len(type_info.declarators) == 1 and isinstance(type_info.declarators[0], list)

    def _get_array_len(self, type_info):
        return type_info.declarators[0][0]

    def _get_chpp_type_from_chre(self, chre_type):
        """Given 'chreWwanCellInfo' returns 'struct ChppWwanCellInfo', etc."""
        prefix = self._get_struct_or_union_prefix(chre_type)

        # First see if we have an explicit name override (e.g. for anonymous types)
        for annotation in self.api.annotations[chre_type]["."]:
            if annotation['annotation'] == "rename_type":
                return prefix + annotation['type_override']

        # Otherwise, use the existing type name, just replace the "chre" prefix with "Chpp"
        if chre_type.startswith('chre'):
            return prefix + 'Chpp' + chre_type[4:]
        else:
            raise RuntimeError("Couldn't figure out new type name for {}".format(chre_type))

    def _get_member_comment(self, member_info):
        for annotation in member_info['annotations']:
            if annotation['annotation'] == "fixed_value":
                return "  // Input ignored; always set to {}".format(annotation['value'])
            elif annotation['annotation'] == "var_len_array":
                return "  // References {} instances of {}".format(
                    annotation['length_field'], self._get_member_type(member_info))
        return ""

    def _get_member_type(self, member_info, underlying_vla_type=False):
        """Gets the CHPP type specification prefix for a struct/union member.

        :param member_info: a dict element from self.api.structs_and_unions[struct]['members']
        :param underlying_vla_type: (used only for var-len array types) False to output
            'struct ChppOffset', and True to output the type that ChppOffset references
        :return: type specification string that prefixes the field name, e.g. 'uint8_t'
        """
        # 4 cases to handle:
        #   1) Annotation gives explicit type that we should use
        #   2) Annotation says this is a variable length array (so use ChppOffset if
        #      underlying_vla_type is False)
        #   3) This is a struct/union type, so use the renamed (CHPP) type name
        #   4) Regular type, e.g. uint32_t, so just use the type spec as-is
        for annotation in member_info['annotations']:
            if annotation['annotation'] == "rewrite_type":
                return annotation['type_override']
            elif not underlying_vla_type and annotation['annotation'] == "var_len_array":
                return "struct ChppOffset"

        if not underlying_vla_type and len(member_info['type'].declarators) > 0 and \
                member_info['type'].declarators[0] == "*":
            # This case should either be handled by rewrite_type (e.g. to uint32_t as
            # opaque/ignored), or var_len_array
            raise RuntimeError("Pointer types require annotation\n{}".format(
                member_info))

        if member_info['is_nested_type']:
            return self._get_chpp_type_from_chre(member_info['nested_type_name'])

        return member_info['type'].type_spec

    def _get_member_type_suffix(self, member_info):
        if self._is_array_type(member_info['type']):
            return "[{}]".format(self._get_array_len(member_info['type']))
        return ""

    def _get_struct_or_union_prefix(self, chre_type):
        return 'struct ' if not self.api.structs_and_unions[chre_type]['is_union'] else 'union '

    def _gen_header_includes(self):
        """Generates #include directives for use in <service>_types.h"""
        out = ["#include <stdbool.h>\n#include <stdint.h>\n\n"]

        includes = ["chpp/macros.h", "chre_api/chre/version.h"]
        includes.extend(self.json['output_includes'])
        for incl in sorted(includes):
            out.append("#include \"{}\"\n".format(incl))
        out.append("\n")
        return out

    def _gen_struct_or_union(self, name):
        """Generates the definition for a single struct/union type"""
        out = []
        if not name.startswith('anon'):
            out.append("//! See {{@link {}}} for details\n".format(name))
        out.append("{} {{\n".format(self._get_chpp_type_from_chre(name)))
        for member_info in self.api.structs_and_unions[name]['members']:
            out.append("  {} {}{};{}\n".format(self._get_member_type(member_info),
                                                 member_info['name'],
                                                 self._get_member_type_suffix(member_info),
                                                 self._get_member_comment(member_info)))

        out.append("} CHPP_PACKED_ATTR;\n\n")
        return out

    def _gen_structs_and_unions(self):
        """Generates definitions for all struct/union types required for the root structs."""
        out = []
        out.append("CHPP_PACKED_START\n\n")

        sorted_structs = self._sorted_structs(self.json['root_structs'])
        for type_name in sorted_structs:
            out.extend(self._gen_struct_or_union(type_name))

        out.append("CHPP_PACKED_END\n\n")
        return out

    def _sorted_structs(self, root_nodes):
        """Implements a topological sort on self.api.structs_and_unions.

        Elements are ordered by definition dependency, i.e. if A includes a field of type B,
        then B will appear before A in the returned list.
        :return: list of keys in self.api.structs_and_unions, sorted by dependency order
        """
        result = []
        visited = set()

        def sort_helper(collection, key):
            for dep in sorted(collection[key]['dependencies']):
                if dep not in visited:
                    visited.add(dep)
                    sort_helper(collection, dep)
            result.append(key)

        for node in sorted(root_nodes):
            sort_helper(self.api.structs_and_unions, node)
        return result

    # ----------------------------------------------------------------------------------------------
    # Encoder/decoder function generation methods
    # ----------------------------------------------------------------------------------------------

    def _get_chpp_member_sizeof_call(self, member_info):
        """Returns invocation used to determine the size of the provided member when encoded.

        Will be either sizeof(<type in CHPP struct>) or a function call if the member contains a VLA
        :param member_info: a dict element from self.api.structs_and_unions[struct]['members']
        :return: string
        """
        type_name = None
        if member_info['is_nested_type']:
            chre_type = member_info['nested_type_name']
            if self.api.structs_and_unions[chre_type]['has_vla_member']:
                return "{}(in->{})".format(self._get_chpp_sizeof_function_name(chre_type),
                                           member_info['name'])
            else:
                type_name = self._get_chpp_type_from_chre(chre_type)
        else:
            type_name = member_info['type'].type_spec
        return "sizeof({})".format(type_name)

    def _gen_chpp_sizeof_function(self, chre_type):
        """Generates a function to determine the encoded size of the CHRE struct, if necessary."""
        out = []

        # Note that this function *should* work with unions as well, but at the time of writing
        # it'll only be used with structs, so names, etc. are written with that in mind
        struct_info = self.api.structs_and_unions[chre_type]
        if not struct_info['has_vla_member']:
            # No codegen necessary, just sizeof on the CHPP structure name is sufficient
            return out

        core_type_name = self._strip_prefix_and_service_from_chre_struct_name(chre_type)
        parameter_name = core_type_name[0].lower() + core_type_name[1:]
        chpp_type_name = self._get_chpp_type_from_chre(chre_type)
        out.append("//! @return number of bytes required to represent the given\n//! {} as {}\n"
                   .format(chre_type, chpp_type_name))
        out.append("static size_t {}(\n        const {}{} *{}) {{\n"
                   .format(self._get_chpp_sizeof_function_name(chre_type),
                           self._get_struct_or_union_prefix(chre_type), chre_type,
                           parameter_name))

        # sizeof(this struct)
        out.append("  size_t encodedSize = sizeof({});\n".format(chpp_type_name))

        # Plus count * sizeof(type) for each var-len array included in this struct
        for member_info in self.api.structs_and_unions[chre_type]['members']:
            for annotation in member_info['annotations']:
                if annotation['annotation'] == "var_len_array":
                    # If the VLA field itself contains a VLA, then we'd need to generate a for
                    # loop to calculate the size of each element individually - I don't think we
                    # have any of these in the CHRE API today, so leaving this functionality out.
                    # Also note that to support that case we'd also want to recursively call this
                    # function to generate sizeof functions for nested fields.
                    if member_info['is_nested_type'] and \
                            self.api.structs_and_unions[member_info['nested_type_name']][
                                'has_vla_member']:
                        raise RuntimeError(
                            "Nested variable-length arrays is not currently supported ({} "
                            "in {})".format(member_info['name'], chre_type))

                    out.append("  encodedSize += {}->{} * sizeof({});\n".format(
                        parameter_name, annotation['length_field'],
                        self._get_member_type(member_info, True)))

        out.append("  return encodedSize;\n}\n\n")
        return out

    def _gen_chpp_sizeof_functions(self):
        """For each root struct, generate necessary functions to determine their encoded size."""
        out = []
        for struct in self.json['root_structs']:
            out.extend(self._gen_chpp_sizeof_function(struct))
        return out

    def _gen_conversion_includes(self):
        """Generates #include directives for the conversion source file"""
        out = ["#include \"chpp/macros.h\"\n"
               "#include \"chpp/memory.h\"\n"
               "#include \"chpp/services/{}_types.h\"\n\n".format(self.service_name)]
        out.append("#include <stddef.h>\n#include <stdint.h>\n#include <string.h>\n\n")
        return out

    def _get_chpp_sizeof_function_name(self, chre_struct):
        """Function name used to compute the encoded size of the given struct at runtime"""
        core_type_name = self._strip_prefix_and_service_from_chre_struct_name(chre_struct)
        return "chpp{}SizeOf{}FromChre".format(self.capitalized_service_name, core_type_name)

    def _get_encoding_function_name(self, chre_type):
        core_type_name = self._strip_prefix_and_service_from_chre_struct_name(chre_type)
        return "chpp{}Convert{}FromChre".format(self.capitalized_service_name, core_type_name)

    def _gen_encoding_function_signature(self, chre_type):
        out = []
        out.append("void {}(\n".format(self._get_encoding_function_name(chre_type)))
        out.append("    const {}{} *in,\n".format(
            self._get_struct_or_union_prefix(chre_type), chre_type))
        out.append("    {} *out".format(self._get_chpp_type_from_chre(chre_type)))
        if self.api.structs_and_unions[chre_type]['has_vla_member']:
            out.append(",\n")
            out.append("    uint8_t *payload,\n")
            out.append("    size_t payloadSize,\n")
            out.append("    uint16_t *vlaOffset")
        out.append(")")
        return out

    def _get_assignment_statement_for_field(self, member_info, in_vla_loop=False,
                                            containing_field_name=None):
        """Returns a statement to assign the provided member

        :param member_info:
        :param in_vla_loop: True if we're currently inside a loop and should append [i]
        :param containing_field_name: Additional member name to use to access the target field, or
        None; for example the normal case is "out->field = in->field", but if we're generating
        assignments in the parent conversion function (e.g. as used for union variants), we need to
        do "out->nested_field.field = in->nested_field.field"
        :return: assignment statement as a string
        """
        array_index = "[i]" if in_vla_loop else ""
        output_accessor = "" if in_vla_loop else "out->"
        containing_field = containing_field_name + "." if containing_field_name is not None else ""

        output_variable = "{}{}{}{}".format(output_accessor, containing_field, member_info['name'],
                                            array_index)
        input_variable = "in->{}{}{}".format(containing_field, member_info['name'], array_index)

        if member_info['is_nested_type']:
            # Use encoding function
            chre_type = member_info['nested_type_name']
            has_vla_member = self.api.structs_and_unions[chre_type]['has_vla_member']
            vla_params = ", payload, payloadSize, vlaOffset" if has_vla_member else ""
            return "{}(&{}, &{}{});\n".format(
                self._get_encoding_function_name(chre_type), input_variable, output_variable,
                vla_params)
        elif self._is_array_type(member_info['type']):
            # Array of primitive type (e.g. uint32_t[8]) - use memcpy
            return "memcpy({}, {}, sizeof({}));\n".format(output_variable, input_variable,
                                                          output_variable)
        else:
            # Regular assignment
            return "{} = {};\n".format(output_variable, input_variable)

    def _gen_vla_encoding(self, member_info, annotation):
        out = []

        variable_name = member_info['name']
        chpp_type = self._get_member_type(member_info, True)
        out.append("\n    {} *{} = ({} *) &payload[*vlaOffset];\n".format(
            chpp_type, variable_name, chpp_type))
        out.append("  out->{}.length = in->{} * {};\n".format(
            member_info['name'], annotation['length_field'],
            self._get_chpp_member_sizeof_call(member_info)))

        out.append("  CHPP_ASSERT(*vlaOffset + out->{}.length <= payloadSize);\n".format(
            member_info['name']))
        out.append("  if (out->{}.length > 0 &&\n"
                   "      *vlaOffset + out->{}.length <= payloadSize) {{\n".format(
            member_info['name'], member_info['name']))

        out.append("    out->{}.offset = *vlaOffset;\n".format(member_info['name']))
        out.append("    *vlaOffset += out->{}.length;\n".format(member_info['name']))

        out.append("    for (size_t i = 0; i < in->{}; i++) {{\n".format(
            annotation['length_field'], variable_name))
        out.append("      {}".format(
            self._get_assignment_statement_for_field(member_info, True)))
        out.append("    }\n")

        out.append("  } else {\n")
        out.append("    out->{}.offset = 0;\n".format(member_info['name']))
        out.append("  }\n");

        return out

    def _gen_union_variant_conversion_code(self, member_info, annotation):
        """Generates a switch statement to convert the "active"/"used" field within a union.

        Handles cases where a union has multiple types, but there's another peer/adjacent field
        which tells you which field in the union is to be used. Outputs code like this:
        switch (in->{discriminator field}) {
            case {first discriminator value associated with a fields}:
                {conversion code for the field associated with this discriminator value}
                ...
        :param chre_type: CHRE type of the union
        :param annotation: Reference to JSON annotation data with the discriminator mapping data
        :return: list of strings
        """
        out = []
        chre_type = member_info['nested_type_name']
        struct_info = self.api.structs_and_unions[chre_type]

        # Start off by zeroing out the union field so any padding is set to a consistent value
        out.append("  memset(&out->{}, 0, sizeof(out->{}));\n".format(member_info['name'],
                                                                        member_info['name']))

        # Next, generate the switch statement that will copy over the proper values
        out.append("  switch (in->{}) {{\n".format(annotation['discriminator']))
        for value, field_name in annotation['mapping']:
            out.append("    case {}:\n".format(value))

            found = False
            for nested_member_info in struct_info['members']:
                if nested_member_info['name'] == field_name:
                    out.append("      {}".format(
                        self._get_assignment_statement_for_field(
                            nested_member_info, containing_field_name=member_info['name'])))
                    found = True
                    break

            if not found:
                raise RuntimeError("Invalid mapping - couldn't find target field {} in struct {}"
                                   .format(field_name, chre_type))

            out.append("      break;\n")

        out.append("    default:\n"
                   "      CHPP_ASSERT(false);\n"
                   "  }\n")

        return out

    def _gen_encoding_function(self, chre_type, already_generated):
        out = []

        struct_info = self.api.structs_and_unions[chre_type]
        for dependency in sorted(struct_info['dependencies']):
            if dependency not in already_generated:
                out.extend(self._gen_encoding_function(dependency, already_generated))

        # Skip if we've already generated code for this type, or if it's a union (in which case we
        # handle the assignment in the parent structure to enable support for discrimination of
        # which field in the union to use)
        if chre_type in already_generated or struct_info['is_union']:
            return out
        already_generated.add(chre_type)

        out.append("static ")
        out.extend(self._gen_encoding_function_signature(chre_type))
        out.append(" {\n")

        for member_info in self.api.structs_and_unions[chre_type]['members']:
            generated_by_annotation = False
            for annotation in member_info['annotations']:
                if annotation['annotation'] == "fixed_value":
                    if self._is_array_type(member_info['type']):
                        out.append("  memset(&out->{}, {}, sizeof(out->{}));\n".format(
                            member_info['name'], annotation['value'], member_info['name']))
                    else:
                        out.append("  out->{} = {};\n".format(member_info['name'],
                                                                annotation['value']))
                    generated_by_annotation = True
                    break
                elif annotation['annotation'] == "enum":
                    # TODO: generate range verification code?
                    pass
                elif annotation['annotation'] == "var_len_array":
                    out.extend(self._gen_vla_encoding(member_info, annotation))
                    generated_by_annotation = True
                    break
                elif annotation['annotation'] == "union_variant":
                    out.extend(self._gen_union_variant_conversion_code(member_info, annotation))
                    generated_by_annotation = True
                    break

            if not generated_by_annotation:
                out.append("  {}".format(self._get_assignment_statement_for_field(member_info)))

        out.append("}\n\n")
        return out

    def _gen_encoding_functions(self):
        out = []
        already_generated = set()
        for struct in self.json['root_structs']:
            out.extend(self._gen_encoding_function(struct, already_generated))

        return out

    def _strip_prefix_and_service_from_chre_struct_name(self, struct):
        """Strip 'chre' and service prefix, e.g. 'chreWwanCellResultInfo' -> 'CellResultInfo'"""
        chre_stripped = struct[4:]
        upcased_service_name = self.service_name[0].upper() + self.service_name[1:]
        if not struct.startswith('chre') or not chre_stripped.startswith(upcased_service_name):
            # If this happens, we need to update the script to handle it. Right we assume struct
            # naming follows the pattern "chre<Service_name><Thing_name>"
            raise RuntimeError("Unexpected structure name {}".format(struct))

        return chre_stripped[len(self.service_name):]

    # ----------------------------------------------------------------------------------------------
    # Memory allocation generation methods
    # ----------------------------------------------------------------------------------------------

    def _get_chpp_sizeof_call(self, chre_type):
        """Returns invocation used to determine the size of the provided CHRE struct after encoding.

        Like _get_chpp_member_sizeof_call(), except for a top-level type assigned to the variable
        "in" rather than a member within a structure (e.g. a VLA field)
        :param chre_type: CHRE type name
        :return: string
        """
        if self.api.structs_and_unions[chre_type]['has_vla_member']:
            return "{}(in)".format(self._get_chpp_sizeof_function_name(chre_type))
        else:
            return "sizeof({})".format(self._get_chpp_type_from_chre(chre_type))

    def _get_encode_allocation_function_name(self, chre_type):
        core_type_name = self._strip_prefix_and_service_from_chre_struct_name(chre_type)
        return "chpp{}{}FromChre".format(self.capitalized_service_name, core_type_name)

    def _gen_encode_allocation_function_signature(self, chre_type, gen_docs=False):
        out = []
        if gen_docs:
            out.append("/**\n"
                       " * Converts from given CHRE structure to serialized CHPP type\n"
                       " *\n"
                       " * @param in Fully-formed CHRE structure\n"
                       " * @param out Upon success, will point to a buffer allocated with "
                       "chppMalloc().\n"
                       " * It is the responsibility of the caller to free this buffer via "
                       "chppFree().\n"
                       " * @param outSize Upon success, will be set to the size of the output "
                       "buffer,\n"
                       " * in bytes\n"
                       " * @return true on success, false if memory allocation failed\n"
                       " */\n")
        out.append("bool {}(\n".format(self._get_encode_allocation_function_name(chre_type)))
        out.append("    const {}{} *in,\n".format(
            self._get_struct_or_union_prefix(chre_type), chre_type))
        out.append("    {} **out,\n".format(self._get_chpp_type_from_chre(chre_type)))
        out.append("    size_t *outSize)")
        return out

    def _gen_encode_allocation_function(self, chre_type):
        out = []

        out.extend(self._gen_encode_allocation_function_signature(chre_type))
        out.append(" {\n")
        out.append("  CHPP_NOT_NULL(out);\n")
        out.append("  CHPP_NOT_NULL(outSize);\n\n")
        out.append("  size_t payloadSize = {};\n".format(self._get_chpp_sizeof_call(chre_type)))
        out.append("  *out = chppMalloc(payloadSize);\n".format(
            self._get_chpp_type_from_chre(chre_type)))

        out.append("  if (*out != NULL) {\n")

        struct_info = self.api.structs_and_unions[chre_type]
        if struct_info['has_vla_member']:
            out.append("    uint8_t *payload = (uint8_t *) *out;\n")
            out.append("    uint16_t vlaOffset = sizeof({});\n".format(
                self._get_chpp_type_from_chre(chre_type)))

        out.append("    {}(in, *out".format(self._get_encoding_function_name(chre_type)))
        if struct_info['has_vla_member']:
            out.append(", payload, payloadSize, &vlaOffset")
        out.append(");\n")
        out.append("    *outSize = payloadSize;\n")
        out.append("    return true;\n")
        out.append("  }\n")

        out.append("  return false;\n}\n")
        return out

    def _gen_encode_allocation_functions(self):
        out = []
        for chre_type in self.json['root_structs']:
            out.extend(self._gen_encode_allocation_function(chre_type))
        return out

    def _gen_encode_allocation_function_signatures(self):
        out = []
        for chre_type in self.json['root_structs']:
            out.extend(self._gen_encode_allocation_function_signature(chre_type, True))
            out.append(";\n\n")
        return out

    # ----------------------------------------------------------------------------------------------
    # Public methods
    # ----------------------------------------------------------------------------------------------

    def generate_header_file(self, dry_run=False, skip_clang_format=False):
        """Creates a C header file for this API and writes it to the file indicated in the JSON."""
        filename = self.service_name + "_types.h"
        if not dry_run:
            print("Generating {} ... ".format(filename), end='', flush=True)
        output_file = os.path.join(system_chre_abs_path(), CHPP_SERVICE_INCLUDE_PATH, filename)
        header = self.generate_header_string()
        self._dump_to_file(output_file, header, dry_run, skip_clang_format)
        if not dry_run:
            print("done")

    def generate_header_string(self):
        """Returns a C header with structure definitions for this API."""
        # To defer concatenation (speed things up), build the file as a list of strings then only
        # concatenate once at the end
        out = [LICENSE_HEADER]

        header_guard = "CHPP_"
        header_guard += self.json['output_file'].split("/")[-1].split(".")[0].upper()
        header_guard += "_H_"

        out.append("#ifndef {}\n#define {}\n\n".format(header_guard, header_guard))
        out.extend(self._autogen_notice())
        out.extend(self._gen_header_includes())
        out.append("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
        out.extend(self._gen_structs_and_unions())

        out.append("\n// Encoding functions (CHRE --> CHPP)\n\n")
        out.extend(self._gen_encode_allocation_function_signatures())

        # TODO: function prototypes for decoding

        out.append("#ifdef __cplusplus\n}\n#endif\n\n")
        out.append("#endif  // {}\n".format(header_guard))
        return ''.join(out)

    def generate_conversion_file(self, dry_run=False, skip_clang_format=False):
        """Generates a .c file with functions for encoding CHRE structs into CHPP and vice versa."""
        filename = self.service_name + "_convert.c"
        if not dry_run:
            print("Generating {} ... ".format(filename), end='', flush=True)
        contents = self.generate_conversion_string()
        output_file = os.path.join(system_chre_abs_path(), CHPP_SERVICE_SOURCE_PATH, filename)
        self._dump_to_file(output_file, contents, dry_run, skip_clang_format)
        if not dry_run:
            print("done")

    def generate_conversion_string(self):
        """Returns C code for encoding CHRE structs into CHPP and vice versa."""
        out = [LICENSE_HEADER, "\n"]

        out.extend(self._autogen_notice())
        out.extend(self._gen_conversion_includes())
        out.extend(self._gen_chpp_sizeof_functions())
        out.extend(self._gen_encoding_functions())
        out.extend(self._gen_encode_allocation_functions())

        # TODO: allocation + decoding functions (for CHPP to CHRE direction)

        return ''.join(out)


class ApiParser:
    """Given a file-specific set of annotations (extracted from JSON annotations file), parses a
    single API header file into data structures suitable for use with code generation.
    """

    def __init__(self, json_obj):
        """Initialize and parse the API file described in the provided JSON-derived object.

        :param json_obj: Extracted file-specific annotations from JSON
        """
        self.json = json_obj
        self.structs_and_unions = {}
        self._parse_annotations()
        self._parse_api()

    def _parse_annotations(self):
        # Convert annotations list to a more usable data structure: dict keyed by structure name,
        # containing a dict keyed by field name, containing a list of annotations (as they
        # appear in the JSON). In other words, we can easily get all of the annotations for the
        # "version" field in "chreWwanCellInfoResult" via
        # annotations['chreWwanCellInfoResult']['version']. This is also a defaultdict, so it's safe
        # to access if there are no annotations for this structure + field; it'll just give you
        # an empty list in that case.
        self.annotations = defaultdict(lambda: defaultdict(list))
        for struct_info in self.json['struct_info']:
            for annotation in struct_info['annotations']:
                self.annotations[struct_info['name']][annotation['field']].append(annotation)

    def _files_to_parse(self):
        """Returns a list of files to supply as input to CParser"""
        # Input paths for CParser are stored in JSON relative to <android_root>/system/chre
        # Reformulate these to absolute paths, and add in some default includes that we always
        # supply
        chre_project_base_dir = system_chre_abs_path()
        default_includes = ["chpp/api_parser/parser_defines.h",
                            "chre_api/include/chre_api/chre/version.h"]
        files = default_includes + self.json['includes'] + [self.json['filename']]
        return [os.path.join(chre_project_base_dir, file) for file in files]

    def _parse_structs_and_unions(self):
        # Starting with the root structures (i.e. those that will appear at the top-level in one
        # or more CHPP messages), build a data structure containing all of the information we'll
        # need to emit the CHPP structure definition and conversion code.
        structs_and_unions_to_parse = self.json['root_structs'].copy()
        while len(structs_and_unions_to_parse) > 0:
            type_name = structs_and_unions_to_parse.pop()
            if type_name in self.structs_and_unions:
                continue

            entry = {
                'appears_in': set(),  # Other types this type is nested within
                'dependencies': set(),  # Types that are nested in this type
                'has_vla_member': False,  # True if this type or any dependency has a VLA member
                'members': [],  # Info about each member of this type
            }
            if type_name in self.parser.defs['structs']:
                defs = self.parser.defs['structs'][type_name]
                entry['is_union'] = False
            elif type_name in self.parser.defs['unions']:
                defs = self.parser.defs['unions'][type_name]
                entry['is_union'] = True
            else:
                raise RuntimeError("Couldn't find {} in parsed structs/unions".format(type_name))

            for member_name, member_type, _ in defs['members']:
                member_info = {
                    'name': member_name,
                    'type': member_type,
                    'annotations': self.annotations[type_name][member_name],
                    'is_nested_type': False,
                }

                if member_type.type_spec.startswith('struct ') or \
                        member_type.type_spec.startswith('union '):
                    member_info['is_nested_type'] = True
                    member_type_name = member_type.type_spec.split(' ')[1]
                    member_info['nested_type_name'] = member_type_name
                    entry['dependencies'].add(member_type_name)
                    structs_and_unions_to_parse.append(member_type_name)

                entry['members'].append(member_info)

                # Flip a flag if this structure has at least one variable-length array member, which
                # means that the encoded size can only be computed at runtime
                if not entry['has_vla_member']:
                    for annotation in self.annotations[type_name][member_name]:
                        if annotation['annotation'] == "var_len_array":
                            entry['has_vla_member'] = True

            self.structs_and_unions[type_name] = entry

        # Build reverse linkage of dependency chain (i.e. lookup between a type and the other types
        # it appears in)
        for type_name, type_info in self.structs_and_unions.items():
            for dependency in type_info['dependencies']:
                self.structs_and_unions[dependency]['appears_in'].add(type_name)

        # Bubble up "has_vla_member" to types each type it appears in, i.e. if this flag is set to
        # True on a leaf node, then all its ancestors should also have the flag set to True
        for type_name, type_info in self.structs_and_unions.items():
            if type_info['has_vla_member']:
                types_to_mark = list(type_info['appears_in'])
                while len(types_to_mark) > 0:
                    type_to_mark = types_to_mark.pop()
                    self.structs_and_unions[type_to_mark]['has_vla_member'] = True
                    types_to_mark.extend(list(self.structs_and_unions[type_to_mark]['appears_in']))

    def _parse_api(self):
        file_to_parse = self._files_to_parse()
        self.parser = CParser(file_to_parse, cache='parser_cache')
        self._parse_structs_and_unions()


def run(args):
    print("Parsing ... ", end='', flush=True)
    with open('chre_api_annotations.json') as f:
        js = json.load(f)

    for file in js:
        api_parser = ApiParser(file)
        code_gen = CodeGenerator(api_parser)
        print("done")
        code_gen.generate_header_file(args.dry_run, args.skip_clang_format)
        code_gen.generate_conversion_file(args.dry_run, args.skip_clang_format)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Generate CHPP serialization code from CHRE APIs.')
    parser.add_argument('-n', dest='dry_run', action='store_true',
                        help='Print the output instead of writing to a file')
    parser.add_argument('--skip-clang-format', dest='skip_clang_format', action='store_true',
                        help='Skip running clang-format on the output files (doesn\'t apply to dry '
                             'runs)')
    args = parser.parse_args()
    run(args)
