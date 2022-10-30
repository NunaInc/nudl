"""
#
# Copyright 2022 Nuna inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

Nudl builtins for performing operations on schemas / data.
"""

import collections.abc
import dataclass_csv
import os
import typing
from dataschema import schema_synth, data_writer


def read_csv(filename: str, seed: typing.Any,
             dialect: str) -> collections.abc.Iterable[typing.Any]:
    with open(filename) as file_csv:
        reader = dataclass_csv.DataclassReader(file_csv, type(seed))
        for row in reader:
            yield row


# Whatever - just for now - we cheat a bit:
DEFAULT_RE = [
    ('.*npi_id', ('str', (3, 'ABC'))),
    ('.*_id', ('str', (6, '0123456789ABCDEF'))),
    ('.*_id_syn', ('str', (4, '01234'))),
]


def generate_csv(gendir: str, seed: typing.Any, count: int) -> str:
    builder = schema_synth.Builder(re_generators=DEFAULT_RE)
    writer = data_writer.CsvWriter()
    gens = builder.schema_generator(output_type=writer.data_types()[0],
                                    data_classes=[type(seed)])
    for gen in gens:
        gen.pregenerate_keys(count)
    fileinfo = [
        schema_synth.FileGeneratorInfo(gen, count, None, None, 1)
        for gen in gens
    ]
    result = schema_synth.GenerateFiles(fileinfo, writer, gendir)
    return result[fileinfo[0].generator.name()][0]
