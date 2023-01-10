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
from dataschema import Schema, Schema_pb2, python2schema
import enum
import logging
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


@dataclasses.dataclass
class CollectOptions:
    disable_column_prunning: bool = False

    @classmethod
    def from_native_object(cls, obj: typing.Optional[typing.Any]):
        """This hides the native nudl structure."""
        if obj is None:
            return cls()
        return cls(getattr(obj, "disable_column_prunning", False))


_DATASET_STEP_ID = 0


def _next_step_id():
    global _DATASET_STEP_ID
    _DATASET_STEP_ID += 1
    return _DATASET_STEP_ID


_UsedFields = typing.Dict[type, typing.Set[str]]


class FieldUsageCollector:
    """Helper class that collects what fields are used from each schema class."""

    def __init__(self):
        self.field_usage = {}

    def update(self, field_usage: _UsedFields):
        for k, v in field_usage.items():
            if k not in self.field_usage:
                self.field_usage[k] = set()
            self.field_usage[k].update(v)

    def used_fields(self, seed_type) -> typing.Optional[typing.List[str]]:
        if seed_type not in self.field_usage:
            return None
        return self.field_usage[seed_type]


class SchemaFieldsUpdater:
    """Helper that changes schemas to restrict the fields defined, based
    on what is used in a dataset operation. This means removing unused
    fields in a schema and propagating these changes down a pipeline.
    """

    def __init__(self):
        self.updated_types: typing.Dict[str, typing.List[Schema.Column]] = {}

    def update_with_fields_used(self, schema: Schema.Table, fields_used):
        """Updated fields in a schema to keep only the ones that are used."""
        if not fields_used:
            return False
        field_set = set(fields_used)
        columns = []
        for column in schema.columns:
            if column.name() in field_set:
                columns.append(column)
        schema.columns = columns
        column_names = [column.name() for column in columns]
        logging.info("Restricting schema of: %s to columns: %s",
                     schema.full_name(), column_names)
        self.updated_types[schema.full_name()] = columns
        return True

    def update_column(self, column: Schema.Column):
        """Updates structured column fields to the schemas previously
        updated by this object. Performs recursively."""
        if column.info.column_type == Schema_pb2.ColumnInfo.TYPE_NESTED:  # type: ignore
            message_name = column.info.message_name  # type: ignore
            if message_name in self.updated_types:
                column.fields = self.updated_types[message_name]
                column_names = [column.name() for column in column.fields]
                logging.info(
                    "Updating column %s of type %s to include only columns %s",
                    column.name(), message_name, column_names)
                return True
        was_updated = False
        for field in column.fields:
            if self.update_column(field):
                was_updated = True
        return was_updated

    def update_fields(self, schema: Schema.Table):
        """Updates schemas to correspond to changes previously made
        in this object. Performs recursively."""
        if schema.full_name() in self.updated_types:
            schema.columns = self.updated_types[schema.full_name()]
            column_names = [column.name() for column in schema.columns]
            logging.info("Updating schema of: %s to columns: %s",
                         schema.full_name(), column_names)
            return True
        was_updated = False
        for column in schema.columns:
            if self.update_column(column):
                was_updated = True
        if was_updated:
            self.updated_types[schema.full_name()] = schema.columns
        return True


class DatasetStep:

    def __init__(self,
                 kind: StepKind,
                 source: typing.Optional['DatasetStep'],
                 seed: typing.Any,
                 field_usage: typing.Optional[_UsedFields] = None):
        if not dataclasses.is_dataclass(seed):
            raise ValueError("Expecting the dataset seed to be a dataclass."
                             f" Got: {seed}")
        self.step_id = _next_step_id()
        self.kind = kind
        self.source = source
        self.seed = seed
        self.schema = python2schema.ConvertDataclass(self.seed_type())
        if field_usage is None:
            self.field_usage = {}
        else:
            self.field_usage = field_usage
        self.direct_collect = False
        self.field_usage_collector = None
        self.direct_collect = False

    def __str__(self):
        return f"{self.__class__.__name__}[{self.seed_type().__qualname__}]"

    def seed_instance(self):
        if isinstance(self.seed, type):
            return self.seed()
        return self.seed

    def seed_type(self) -> typing.Type[object]:
        if isinstance(self.seed, type):
            return self.seed  # type: ignore
        return type(self.seed)

    def propagate_direct_collect(self):
        return True

    def update_field_usage(self, collector: FieldUsageCollector,
                           direct_collect: bool):
        """Records field usage reports in a global collector."""
        if not self.direct_collect:
            self.direct_collect = direct_collect
        self.field_usage_collector = collector
        self.field_usage_collector.update(self.field_usage)
        if self.source:
            self.source.update_field_usage(
                collector,
                direct_collect if self.propagate_direct_collect() else False)

    def used_fields(self):
        """Returns fields used for the schema type of this step."""
        if self.direct_collect or self.field_usage_collector is None:
            return None
        used_fields = self.field_usage_collector.used_fields(self.seed_type())
        if used_fields is None:
            return None
        result_fields = []  # Place them in the original column order
        for column in self.schema.columns:
            if column.name() in used_fields:
                result_fields.append(column.name())
        return result_fields

    def update_schema_fields(self, updater: SchemaFieldsUpdater):
        """Updates the schema of this step either according to the fields used,
        or to previous changes made to the schema for fields used."""
        if self.source:
            self.source.update_schema_fields(updater)
        updater.update_fields(self.schema)


class EmptyStep(DatasetStep):

    def __init__(self, seed: typing.Any):
        super().__init__(StepKind.EMPTY, None, seed)


class ReadCsvStep(DatasetStep):

    def __init__(self, seed: typing.Any, filespec: str):
        super().__init__(StepKind.READ_CSV, None, seed)
        self.filespec = filespec

    def update_schema_fields(self, updater: SchemaFieldsUpdater):
        updater.update_with_fields_used(self.schema, self.used_fields())


class ReadParquetStep(DatasetStep):

    def __init__(self, seed: typing.Any, filespec: str):
        super().__init__(StepKind.READ_PARQUET, None, seed)
        self.filespec = filespec

    def update_schema_fields(self, updater: SchemaFieldsUpdater):
        updater.update_with_fields_used(self.schema, self.used_fields())


class FilterStep(DatasetStep):

    def __init__(self, source: DatasetStep, field_usage: _UsedFields,
                 fun: collections.abc.Callable[[typing.Any], bool]):
        super().__init__(StepKind.FILTER, source, source.seed, field_usage)
        self.fun = fun


class MapStep(DatasetStep):

    def __init__(self, source: DatasetStep, seed: typing.Any,
                 field_usage: _UsedFields,
                 fun: collections.abc.Callable[[typing.Any], typing.Any]):
        super().__init__(StepKind.MAP, source, seed, field_usage)
        self.fun = fun

    def propagate_direct_collect(self):
        return False


class FlatMapStep(DatasetStep):

    def __init__(
        self, source: DatasetStep, seed: typing.Any, field_usage: _UsedFields,
        fun: collections.abc.Callable[[typing.Any],
                                      collections.abc.Iterable[typing.Any]]):
        super().__init__(StepKind.FLAT_MAP, source, source.seed, field_usage)
        self.fun = fun

    def propagate_direct_collect(self):
        return False


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
                 field_usage: _UsedFields,
                 builder: collections.abc.Callable[[typing.Any], typing.Tuple]):
        super().__init__(StepKind.AGGREGATE, source, seed, field_usage)
        self.builder = builder

    def propagate_direct_collect(self):
        return False

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
                 field_usage: _UsedFields,
                 left_key: collections.abc.Callable[[typing.Any], typing.Any],
                 right_spec: typing.Tuple):
        super().__init__(StepKind.JOIN_LEFT, source, seed, field_usage)
        self.left_key = left_key
        self.right_spec = right_spec

    def propagate_direct_collect(self):
        return False

    def update_field_usage(self, collector: FieldUsageCollector,
                           direct_collect: bool):
        super().update_field_usage(collector, direct_collect)
        for right in self.right_spec:
            spec = JoinSpecDecoder(right)
            if spec.join_spec == "right_multi_array":
                for src in spec.spec_src:
                    src.update_field_usage(collector, direct_collect)
            else:
                spec.spec_src.update_field_usage(collector, direct_collect)

    def right_join_fields(self, disable_prunning=False) -> typing.Set[str]:
        """Returns the right side fields to be joined based on the
        used fields."""
        right_join_fields = set()
        if not self.right_spec:
            return right_join_fields
        used_fields = self.used_fields()
        # Check if we need to do any joins at all based on used fields:
        for right in self.right_spec:
            spec = JoinSpecDecoder(right)
            if (disable_prunning or used_fields is None or
                    spec.field_name in used_fields):
                right_join_fields.add(spec.field_name)
        return right_join_fields

    def update_schema_fields(self, updater: SchemaFieldsUpdater):
        if self.source is None:
            return
        self.source.update_schema_fields(updater)
        source_fields = set(
            column.name() for column in self.source.schema.columns)
        right_join_fields = self.right_join_fields()
        for right in self.right_spec:
            spec = JoinSpecDecoder(right)
            if spec.field_name not in right_join_fields:
                continue
            source_fields.add(spec.field_name)
            if spec.join_spec == "right_multi_array":
                for src in spec.spec_src:
                    src.update_schema_fields(updater)
            else:
                spec.spec_src.update_schema_fields(updater)
        updater.update_fields(self.schema)
        updater.update_with_fields_used(self.schema, source_fields)


class LimitStep(DatasetStep):

    def __init__(self, source: DatasetStep, limit: int):
        super().__init__(StepKind.LIMIT, source, source.seed)
        self.limit = limit


class DatasetEngine:
    """Base 'abstract' class for a dataset engine - processes datasets
    and passes them through various processing stages."""

    def __init__(self, name):
        self.name = name

    def collect(self, step: DatasetStep,
                options: CollectOptions) -> typing.List[typing.Any]:
        raise NotImplementedError(f"`collect` not implemented for {self.name}")


_DEFAULT_ENGINE = DatasetEngine("Uninitialized Engine")


def default_engine() -> DatasetEngine:
    return _DEFAULT_ENGINE


def set_default_engine(engine: DatasetEngine):
    global _DEFAULT_ENGINE
    _DEFAULT_ENGINE = engine
