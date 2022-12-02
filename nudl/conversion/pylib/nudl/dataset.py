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

This contains the abstract interface for a dataset and an engine
"""

import collections.abc
import dataclasses
from dataschema import python2schema
import enum
import typing


class StepKind(enum.Enum):
    EMPTY = 0
    READ_CSV = 1
    READ_PARQUET = 2
    FILTER = 3
    MAP = 4
    FLAT_MAP = 5
    AGGREGATE = 6
    JOIN_LEFT = 7
    LIMIT = 8


_DATASET_STEP_ID = 0


def _next_step_id():
    global _DATASET_STEP_ID
    _DATASET_STEP_ID += 1
    return _DATASET_STEP_ID


class DatasetStep:

    def __init__(self, kind: StepKind, source: typing.Optional['DatasetStep'],
                 seed: typing.Any):
        if not dataclasses.is_dataclass(seed):
            raise ValueError("Expecting the dataset seed to be a dataclass."
                             f" Got: {seed}")
        self.step_id = _next_step_id()
        self.kind = kind
        self.source = source
        self.seed = seed
        self.schema = python2schema.ConvertDataclass(self.seed_type())

    def seed_instance(self):
        if isinstance(self.seed, type):
            return self.seed()
        return self.seed

    def seed_type(self) -> typing.Type[object]:
        if isinstance(self.seed, type):
            return self.seed  # type: ignore
        return type(self.seed)


class EmptyStep(DatasetStep):

    def __init__(self, seed: typing.Any):
        super().__init__(StepKind.EMPTY, None, seed)


class ReadCsvStep(DatasetStep):

    def __init__(self, seed: typing.Any, filespec: str):
        super().__init__(StepKind.READ_CSV, None, seed)
        self.filespec = filespec


class ReadParquetStep(DatasetStep):

    def __init__(self, seed: typing.Any, filespec: str):
        super().__init__(StepKind.READ_PARQUET, None, seed)
        self.filespec = filespec


class FilterStep(DatasetStep):

    def __init__(self, source: DatasetStep,
                 fun: collections.abc.Callable[[typing.Any], bool]):
        super().__init__(StepKind.FILTER, source, source.seed)
        self.fun = fun


class MapStep(DatasetStep):

    def __init__(self, source: DatasetStep, seed: typing.Any,
                 fun: collections.abc.Callable[[typing.Any], typing.Any]):
        super().__init__(StepKind.MAP, source, seed)
        self.fun = fun


class FlatMapStep(DatasetStep):

    def __init__(
        self, source: DatasetStep, seed: typing.Any,
        fun: collections.abc.Callable[[typing.Any],
                                      collections.abc.Iterable[typing.Any]]):
        super().__init__(StepKind.FLAT_MAP, source, source.seed)
        self.fun = fun


def agg_value(agg_value: typing.Tuple) -> typing.Any:
    """Returns the actual value from an aggregation individual tuple"""
    return agg_value[1][0][1]


def agg_values(agg: typing.Tuple) -> collections.abc.Iterator[typing.Tuple]:
    """Returns the ("<aggregation_name>", ("<field_name>", <value>))*
    list of tuples returned by an aggregation function."""
    return agg[1][1][1:]


class AggregateStep(DatasetStep):
    """Builds an aggregator.
    The builder function returns values composed in this way:

    Aggregation values come in this composed tuples:
    ( <seed>,  # the input instance
      ( "agg",  # a marker
        ("_agg", <seed>),  # tombstone - needed for type building
        # The next elements in this inner tuple are the actual
        # set of aggregations:
        ("<aggregation_name>", ("<field_name>", <value>))*
        # E.g.
        ("group_by", ("name", p.name))
        ("sum", ("total", p.value))
      )
    )
    """

    def __init__(self, source: DatasetStep, seed: typing.Any,
                 builder: collections.abc.Callable[[typing.Any], typing.Tuple]):
        super().__init__(StepKind.AGGREGATE, source, seed)
        self.builder = builder

    def build_spec(self) -> typing.List[typing.Tuple[str, str]]:
        """Returns a list of (aggregation_type, field_name) for this step."""
        assert self.source is not None
        spec = self.builder(self.source.seed_instance())
        self._check_full_agg_spec(spec)
        field_names = [field.name for field in dataclasses.fields(self.seed)]
        aggregations = []
        index = 0
        for agg in agg_values(spec):
            self._check_agg_spec(agg)
            agg_type = agg[0]
            field_name = agg[1][0][0]
            if index > len(field_name):
                raise ValueError(
                    f"Too many fields in aggregation. At index: {index}, "
                    f"for field: {field_name}. "
                    f"Expected field names: {field_names}")
            if field_name == "_unnamed" or not field_name:
                field_name = field_names[index]
            elif field_name != field_names[index]:
                raise ValueError(
                    f"Unexpected field: {field_name} at index {index}. "
                    f"Expected field names: {field_names}")
            aggregations.append((agg_type, field_name))
            index += 1

        if not aggregations:
            raise ValueError("No aggregations were specified")
        return aggregations

    def _check_full_agg_spec(self, spec):
        """Checks that our specification looks as above description"""
        if (not isinstance(spec, typing.Tuple) or len(spec) != 2 or
                not isinstance(spec[1], typing.Tuple) or len(spec[1]) != 2 or
                not isinstance(spec[1][1], typing.Tuple) or
                len(spec[1][1]) < 2):
            raise ValueError(f"Bad aggregation specification: {spec}")
        pass

    def _check_agg_spec(self, agg):
        """Check that one of the inner tuples look as aggregation above."""
        if (not isinstance(agg, typing.Tuple) or len(agg) != 2 or
                not isinstance(agg[1], typing.Tuple) or not agg[1] or
                not isinstance(agg[1][0], typing.Tuple) or len(agg[1][0]) != 2):
            raise ValueError(
                f"Bad individual yaggregation specification: {agg}")
        pass


class JoinSpecDecoder:
    """Decodes a join specification which comes as a tuple of this form:
    ( <self.field_name>,    # - name of field in output structure
      ( <self.join_spec>,   # - type of join - e.g. 'right_multi'
        <self.spec_src> ),  # - join source DatasetStep
      ( "key",  # - just a name tombstone
        <self.spec_key> ),  # - join value function
    )
    """

    def __init__(self, spec: typing.Tuple):
        if (len(spec) != 2 or not isinstance(spec[0], str) or
                not isinstance(spec[1], tuple) or len(spec[1]) != 2 or
                not isinstance(spec[1][0], tuple) or len(spec[1][0]) != 2 or
                not isinstance(spec[1][0][0], str) or len(spec[1][1]) != 2 or
                not callable(spec[1][1][1])):
            raise ValueError(f"Invalid join spec: {spec}")
        self.field_name = spec[0]
        self.join_spec = spec[1][0][0]
        self.spec_src = spec[1][0][1]
        self.spec_key = spec[1][1][1]
        if self.join_spec == "right_multi_array":
            if not isinstance(self.spec_src, list):
                raise ValueError("Invalid join src - expecting list of "
                                 f"Dataset. Got: {self.join_spec}")
            for src in self.spec_src:
                if not isinstance(src, DatasetStep):
                    raise ValueError("Invalid element in join spec list - "
                                     "expecting Dataset. Got: {src}")
        elif not isinstance(self.spec_src, DatasetStep):
            raise ValueError(
                f"Invalid join spec - expecting Dataset. Got: {spec}")


class JoinLeftStep(DatasetStep):

    def __init__(self, source: DatasetStep, seed: typing.Any,
                 left_key: collections.abc.Callable[[typing.Any], typing.Any],
                 right_spec: typing.Tuple):
        super().__init__(StepKind.JOIN_LEFT, source, seed)
        self.left_key = left_key
        self.right_spec = right_spec


class LimitStep(DatasetStep):

    def __init__(self, source: DatasetStep, limit: int):
        super().__init__(StepKind.LIMIT, source, source.seed)
        self.limit = limit


class DatasetEngine:
    """Base 'abstract' class for a dataset engine - processes datasets
    and passes them through various processing stages."""

    def __init__(self, name):
        self.name = name

    def collect(self, step: DatasetStep) -> typing.List[typing.Any]:
        raise NotImplementedError(f"`collect` not implemented for {self.name}")
