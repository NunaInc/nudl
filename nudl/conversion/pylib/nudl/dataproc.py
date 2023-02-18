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
import smart_open
import typing
from dataschema import schema_synth, data_writer

def read_csv(seed: typing.Any, filename: str,
             dialect: str) -> collections.abc.Iterable[typing.Any]:
    with smart_open.open(filename) as file_csv:
        reader = dataclass_csv.DataclassReader(file_csv, type(seed))
        for row in reader:
            yield row


# A sensible sets of defaults
DEFAULT_RE = (
    ('.*_id', ('str', (12, '0123456789ABCDEF'))),
    ('(.*_|^)first_name', ('faker', 'first_name')),
    ('(.*_|^)last_name', ('faker', 'last_name')),
    ('(.*_|^)city', ('faker', 'city')),
    ('(.*_|^)state', ('faker', 'state')),
    ('(.*_|^)zip_code', ('faker', 'zipcode')),
    ('(.*_|^)sex', ('choice', (['M', 'F'],))),
    ('(.*_|^)phone', ('faker', 'phone_number')),
    ('(.*_|^)address', ('faker', 'street_address')),
    ('(.*_|^)ssn', ('faker', 'ssn')),
    ('(.*_|^)tin', ('faker', 'ssn')),
    ('(.*_|^)cents', ('toint', ('exp', 5000))),
    ('(.*_|^)language',
     ('choice', (['en', 'es', 'fr', 'zh', 'pt', 'vi', 'ro', 'it', 'de'],))),
)


def _generate_data(
        seeds: typing.Tuple, gendir: str, count: int,
        writer: data_writer.BaseWriter,
        generators: typing.Optional[typing.Tuple]) -> typing.List[str]:
    if not generators:
        generators = DEFAULT_RE
    builder = schema_synth.Builder(
        re_generators=list(generators))  # type:ignore
    gens = builder.schema_generator(
        output_type=writer.data_types()[0],  # type:ignore
        data_classes=[
            type(seed[1]) if isinstance(seed, typing.Tuple) else type(seed)
            for seed in seeds
        ])
    for gen in gens:
        gen.pregenerate_keys(count)
    fileinfo = [
        schema_synth.FileGeneratorInfo(gen, count, None, None, 1)
        for gen in gens
    ]
    result = schema_synth.GenerateFiles(fileinfo, writer, gendir)
    return [result[fi.generator.name()][0] for fi in fileinfo]


def generate_csv(seeds: typing.Tuple, gendir: str, count: int,
                 generators: typing.Optional[typing.Tuple]) -> typing.List[str]:
    return _generate_data(seeds, gendir, count, data_writer.CsvWriter(),
                          generators)


def generate_parquet(
        seeds: typing.Tuple, gendir: str, count: int,
        generators: typing.Optional[typing.Tuple]) -> typing.List[str]:
    return _generate_data(seeds, gendir, count,
                          data_writer.ParquetWriter(no_uint=True), generators)
