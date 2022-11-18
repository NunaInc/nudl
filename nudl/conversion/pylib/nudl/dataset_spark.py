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

This is an implementation of dataset interface using spark.
"""

import collections.abc
import dataclasses
from dataschema import Schema, python2schema, schema2pyspark
import nudl.dataset
import pyspark
import pyspark.sql
import pyspark.sql.functions
import pyspark.sql.session
import pyspark.sql.types
import typing
import uuid

#
# TODO(catalin):
#  - the dataset should actually be a collection of data and operations.
#  - actual execution happens only on specific write / collect etc
#


def _seed_as_dict(seed: typing.Any) -> typing.Dict:
    if dataclasses.is_dataclass(seed):
        return dataclasses.asdict(seed)
    elif isinstance(seed, pyspark.sql.Row):
        return seed.asDict()
    else:
        raise ValueError(f"Invalid dataset seed: {seed} / {type(seed)}")


def _seed_as_dataclass(seed: typing.Any):
    if dataclasses.is_dataclass(seed):
        if isinstance(seed, type):
            return seed
        return type(seed)
    elif isinstance(seed, pyspark.sql.Row):
        fields = []
        for k, v in _seed_as_dict(seed).items():
            # TODO(catalin): Actually is a bit more complex than this..
            #   especially for rows and such
            fields.append((
                k,
                type(v),
            ))
        name = f"Row_{str(uuid.uuid4()).replace('-', '_')}"
        return dataclasses.make_dataclass(name, fields)
    else:
        raise ValueError(f"Invalid dataset seed: {seed} / {type(seed)}")


def _seed_as_dataschema(seed: typing.Any) -> Schema.Table:
    return python2schema.ConvertDataclass(_seed_as_dataclass(seed))


def _seed_as_pyspark_schema(seed: typing.Any) -> pyspark.sql.types.StructType:
    return schema2pyspark.ConvertTable(_seed_as_dataschema(seed))


@dataclasses.dataclass
class EmptyStruct:
    pass


# Just temporary - until we propagate proper schema from compiler.
def _safe_to_df(rdd: pyspark.RDD,
                result_seed: typing.Any) -> pyspark.sql.DataFrame:
    if rdd.isEmpty() and result_seed is None:
        return rdd.toDF(  # type: ignore
            schema2pyspark.ConvertTable(
                python2schema.ConvertDataclass(EmptyStruct)))
    return rdd.toDF(_seed_as_pyspark_schema(result_seed))  # type: ignore


class Dataset(nudl.dataset.Dataset):

    def __init__(self, engine: nudl.dataset.DatasetEngine, seed: typing.Any,
                 df: pyspark.sql.DataFrame):
        super().__init__(engine, seed)
        self.seed = seed
        self.df = df
        # Various lazy conversions.
        self._as_dict = None
        self._as_dataclass = None
        self._as_schema = None
        self._as_pyspark = None

    def seed_as_dict(self):
        if self._as_dict is None:
            self._as_dict = _seed_as_dict(self.seed)
        return self._as_dict

    def seed_as_dataclass(self):
        if self._as_dataclass is None:
            self._as_dataclass = _seed_as_dataclass(self.seed)
        return self._as_dataclass

    def seed_as_dataschema(self):
        if self._as_schema is None:
            self._as_schema = _seed_as_dataschema(self.seed)
        return self._as_schema

    def seed_as_pyspark_schema(self):
        if self._as_pyspark is None:
            self._as_pyspark = _seed_as_pyspark_schema(self.seed)
        return self._as_pyspark


def spark_init_session(name: str) -> pyspark.sql.session.SparkSession:
    return pyspark.sql.session.SparkSession.builder.appName(name).getOrCreate()


#################### In-module utilities:


def _agg_values(agg: typing.Tuple) -> collections.abc.Iterator[typing.Tuple]:
    return agg[1][1][1:]


def _agg_value(agg_value: typing.Tuple) -> typing.Any:
    return agg_value[1][0][1]


def _aggregate_map(t: typing.Any, names: typing.List[str],
                   builder: collections.abc.Callable[[typing.Any],
                                                     typing.Tuple]):
    return pyspark.sql.Row(
        **{x: _agg_value(y) for (x, y) in zip(names, _agg_values(builder(t)))})


def _check_full_agg_spec(agg):
    if (not isinstance(agg, typing.Tuple) or len(agg) != 2 or
            not isinstance(agg[1], typing.Tuple) or len(agg[1]) != 2 or
            not isinstance(agg[1][1], typing.Tuple) or len(agg[1][1]) < 2):
        raise ValueError(f"Bad aggregation specification: {agg}")
    pass


def _check_agg_spec(agg):
    if (not isinstance(agg, typing.Tuple) or len(agg) != 2 or
            not isinstance(agg[1], typing.Tuple) or not agg[1] or
            not isinstance(agg[1][0], typing.Tuple) or len(agg[1][0]) != 2):
        raise ValueError(f"Bad individual yaggregation specification: {agg}")
    pass


def _agg_fun(agg_type: str) -> collections.abc.Callable:
    if agg_type not in _AGGREGATE_MAP:
        raise ValueError(f"Unknown aggregate name ${agg_type}")
    return _AGGREGATE_MAP[agg_type]


_AGGREGATE_MAP = {
    "count": pyspark.sql.functions.count,
    "min": pyspark.sql.functions.min,
    "max": pyspark.sql.functions.max,
    "sum": pyspark.sql.functions.sum,
    "mean": pyspark.sql.functions.mean,
    "to_set": pyspark.sql.functions.collect_set,
    "to_array": pyspark.sql.functions.collect_list,
    "count_distinct": pyspark.sql.functions.countDistinct,
}


def _update_dict(d, k, v):
    d[k] = v
    return d


def _update_array_dict(d, k, v, index_key, index_val):
    if k in d:
        d[k].extend(v)
        d[index_key].extend([index_val] * len(v))
    else:
        d[k] = v
        d[index_key] = [index_val] * len(v)
    return d


def _join_builder_base(t):
    return t.asDict()


def _join_builder_next(t, field_name, builder):
    return _update_dict(builder(t[0]), field_name, t[1])


def _join_builder_next_multi(t, field_name, builder):
    return _update_dict(builder(t[0]), field_name, list(t[1]) if t[1] else [])


def _join_builder_next_multi_array(t, field_name, index_field_name, index,
                                   builder):
    return _update_array_dict(builder(t[0]), field_name,
                              list(t[1]) if t[1] else [], index_field_name,
                              index)


class _JoinSpec:

    def __init__(self, spec):
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
                if not isinstance(src, Dataset):
                    raise ValueError("Invalid element in join spec list - "
                                     "expecting Dataset. Got: {src}")
        elif not isinstance(self.spec_src, Dataset):
            raise ValueError(
                f"Invalid join spec - expecting Dataset. Got: {spec}")


class _NameKeeper:

    def __init__(self):
        self._known_names = set()
        self.names = []
        self._index = 0

    def _pick_next_name(self):
        j = self._index
        while f"arg_{j}" in self._known_names:
            j += 1
        return f"arg_{j}"

    def next_name(self, field_name: str) -> str:
        self._index += 1
        if not field_name or field_name == "_unnamed":
            field_name = self._pick_next_name()
        if field_name in self._known_names:
            raise ValueError(f"Field name already exists: {field_name}")
        self._known_names.add(field_name)
        self.names.append(field_name)
        return field_name


class SparkEngineImpl(nudl.dataset.DatasetEngine):

    def __init__(self, session: pyspark.sql.session.SparkSession):
        super().__init__("Spark")
        self.session = session

    def empty_dataset(self, seed: typing.Any) -> Dataset:
        return Dataset(self, seed,
                       self.session.sparkContext.emptyRDD().toDF(
                           _seed_as_pyspark_schema(seed)))  # type: ignore

    def read_csv(self, seed: typing.Any, file_spec: str) -> Dataset:
        return Dataset(
            self, seed,
            self.session.read.csv(file_spec,
                                  schema=_seed_as_pyspark_schema(seed)))

    def read_parquet(self, seed: typing.Any, file_spec: str) -> Dataset:
        # Get the schema for seed etc, add options and such.
        return Dataset(
            self,
            seed,
            # self.session.read.parquet(file_spec))
            self.session.read.format("parquet").schema(
                _seed_as_pyspark_schema(seed)).load(file_spec))

    # (file_spec)
    # schema=_seed_as_pyspark_schema(seed)))

    def _check_my_dataset(self, src: Dataset):
        if src.engine != self:
            raise ValueError(
                f"Dataset: {src} is not created by this engine: {self}")

    # For now we have the stupid filter.
    #  -- to be converted to proper dataframe filter w/ analysis.
    def dataset_filter(self, src: Dataset,
                       f: collections.abc.Callable[[typing.Any], bool]):
        self._check_my_dataset(src)
        return Dataset(self, src.seed,
                       _safe_to_df(src.df.rdd.filter(f), src.seed))

    # For now we have the stupid map.
    #  -- to be converted to proper arrow map w/ analysis.
    def dataset_map(self, src: Dataset, result_seed: typing.Any,
                    f: collections.abc.Callable[[typing.Any], typing.Any]):
        self._check_my_dataset(src)
        # For now preserve partitioning..
        return Dataset(self, result_seed,
                       _safe_to_df(src.df.rdd.map(f, True), result_seed))

    def dataset_flat_map(self, src: Dataset, result_seed: typing.Any,
                         f: collections.abc.Callable[[typing.Any],
                                                     typing.List[typing.Any]]):
        self._check_my_dataset(src)
        # For now preserve partitioning..
        return Dataset(self, result_seed,
                       _safe_to_df(src.df.rdd.flatMap(f, True), result_seed))

    def dataset_aggregate(self, src: Dataset, result_seed: typing.Any,
                          builder: collections.abc.Callable[[typing.Any],
                                                            typing.Tuple]):
        self._check_my_dataset(src)
        spec = builder(src.seed)
        _check_full_agg_spec(spec)
        known_fields = set()
        i = 0
        group_names = []
        agg_spec = []
        name_keeper = _NameKeeper()
        for agg in _agg_values(spec):
            _check_agg_spec(agg)
            agg_type = agg[0]
            field_name = name_keeper.next_name(agg[1][0][0])
            if agg_type == "group_by":
                group_names.append(field_name)
            else:
                agg_spec.append(
                    _agg_fun(agg_type)(field_name).alias(field_name))
        if not agg_spec:
            raise ValueError("No aggregations were specified")
        mapper = (lambda t, names=name_keeper.names, builder=builder:
                  _aggregate_map(t, names, builder))
        return Dataset(
            self, result_seed,
            _safe_to_df(src.df.rdd.map(mapper),
                        result_seed).groupBy(group_names).agg(*agg_spec))

    def dataset_join_left(self, src: Dataset, result_seed: typing.Any,
                          left_key: collections.abc.Callable[[typing.Any],
                                                             typing.Any],
                          right_spec: typing.Tuple):
        self._check_my_dataset(src)
        if not right_spec:
            return src
        left_join = src.df.rdd.map(lambda s, left_key=left_key:
                                   (left_key(s), s))
        builder = _join_builder_base  # lambda t: t.as_dict()
        for right in right_spec:
            spec = _JoinSpec(right)
            if spec.join_spec == "right_multi_array":
                if not spec.spec_src:
                    continue
                index_field_name = f"{spec.field_name}_index"
                index = 0
                for src in spec.spec_src:
                    right_join = src.df.rdd.groupBy(spec.spec_key)
                    builder = (
                        lambda t, field_name=spec.field_name, index_field_name=
                        index_field_name, index=index, builder=
                        builder: _join_builder_next_multi_array(
                            t, field_name, index_field_name, index, builder))
                    index += 1
                    left_join = left_join.leftOuterJoin(right_join)
            else:
                if spec.join_spec == "right_multi":
                    right_join = spec.spec_src.df.rdd.groupBy(spec.spec_key)
                    builder = (
                        lambda t, field_name=spec.field_name, builder=builder:
                        _join_builder_next_multi(t, field_name, builder))
                else:
                    right_join = spec.spec_src.df.rdd.map(
                        lambda s, right_key=spec.spec_key: (right_key(s), s))
                    builder = (
                        lambda t, field_name=spec.field_name, builder=builder:
                        _join_builder_next(t, field_name, builder))
                left_join = left_join.leftOuterJoin(right_join)
        final_builder = lambda t, builder=builder: builder(t[1])
        return Dataset(self, result_seed,
                       _safe_to_df(left_join.map(final_builder), result_seed))

    def dataset_limit(self, src: Dataset, size: int) -> Dataset:
        self._check_my_dataset(src)
        return Dataset(self, src.seed, src.df.limit(size))

    def dataset_collect(self, src: Dataset) -> typing.List[typing.Any]:
        self._check_my_dataset(src)
        return src.df.collect()
