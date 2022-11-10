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

This is pretty spark specific - as the first system to test out.
"""
import collections.abc
import dataclasses
import pyspark
import pyspark.sql
import pyspark.sql.functions
import pyspark.sql.session
import typing

class Dataset:
    def __init__(self, seed: typing.Any, df: pyspark.sql.DataFrame):
        self.seed = seed
        self.df = df

    def seed_as_dict(self):
        if dataclasses.is_dataclass(self.seed):
            return dataclasses.asdict(self.seed)
        if isinstance(self.seed, pyspark.sql.Row):
            return self.seed.asDict()
        raise ValueError(
            "Expecting dataclass seed to be a pyspark Row "
            f"or a dataclass. Got: {self.seed} / {type(self.seed)}")

SPARK_SESSION = None

def spark_session() -> pyspark.sql.session.SparkSession:
    if SPARK_SESSION is None:
        return pyspark.sql.session.SparkSession.getActiveSession()
    return SPARK_SESSION

def set_spark_session(session: pyspark.sql.session.SparkSession):
    global SPARK_SESSION
    SPARK_SESSION = session

def spark_init_session(name: str) -> None:
    set_spark_session(pyspark.sql.session.SparkSession.builder.appName(name)
                      .getOrCreate())

def read_csv(seed: typing.Any, file_spec: str) -> Dataset:
    # Get the schema for seed etc, add options and such.
    return Dataset(seed, spark_session().read.csv(file_spec))

def read_parquet(seed: typing.Any, file_spec: str) -> Dataset:
    # Get the schema for seed etc, add options and such.
    return Dataset(seed, spark_session().read.parquet(file_spec))

# For now we have the stupid filter.
#  -- to be converted to proper dataframe filter w/ analysis.
def dataset_filter(src: Dataset,
                   f: collections.abc.Callable[[typing.Any], bool]):
    return Dataset(src.seed, src.df.rdd.filter(f).toDF())

# For now we have the stupid map.
#  -- to be converted to proper arrow map w/ analysis.
def dataset_map(src: Dataset,
                f: collections.abc.Callable[[typing.Any], typing.Any]):
    # For now preserve partitioning..
    return Dataset(src.seed, src.df.rdd.map(f, True).toDF())

def dataset_flat_map(src: Dataset,
                f: collections.abc.Callable[[typing.Any], typing.List[typing.Any]]):
    # For now preserve partitioning..
    return Dataset(src.seed, src.df.rdd.flatMap(f, True).toDF())

def _agg_values(agg: typing.Tuple) -> collections.abc.Iterator[typing.Tuple]:
    return agg[1][1][1:]

def _agg_value(agg_value: typing.Tuple) -> typing.Any:
    return agg_value[1][0][1]

def _aggregate_map(t: typing.Any,
                   names: typing.List[str],
                   builder: collections.abc.Callable[[typing.Any], typing.Tuple]):
    return pyspark.sql.Row(**{x : _agg_value(y) for (x, y) in zip(names, _agg_values(builder(t)))})

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

def _check_full_agg_spec(agg):
    if (not isinstance(agg, typing.Tuple) or len(agg) != 2
        or not isinstance(agg[1], typing.Tuple) or len(agg[1]) != 2
        or not isinstance(agg[1][1], typing.Tuple) or len(agg[1][1]) < 2):
        raise ValueError(f"Bad aggregation specification: {agg}")

def _check_agg_spec(agg):
    if (not isinstance(agg, typing.Tuple) or len(agg) != 2
        or not isinstance(agg[1], typing.Tuple) or not agg[1]
        or not isinstance(agg[1][0], typing.Tuple) or len(agg[1][0]) != 2):
        raise ValueError(f"Bad individual yaggregation specification: {agg}")
    pass

def _agg_fun(agg_type: str) -> collections.abc.Callable:
    if agg_type not in _AGGREGATE_MAP:
        raise ValueError(f"Unknown aggregate name ${agg_type}")
    return _AGGREGATE_MAP[agg_type]

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


def dataset_aggregate(
    src: Dataset,
    builder: collections.abc.Callable[[typing.Any], typing.Tuple]):
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
            agg_spec.append(_agg_fun(agg_type)(field_name).alias(field_name))
    if not agg_spec:
        raise ValueError("No aggregations were specified")
    mapper = (lambda t, names=name_keeper.names, builder=builder:
              _aggregate_map(t, names, builder))
    return Dataset(mapper(src.seed),
                   src.df.rdd.map(mapper).toDF()
                   .groupBy(group_names).agg(*agg_spec))

def dataset_collect(src: Dataset) -> typing.List[typing.Any]:
    return src.df.collect()

def _update_dict(d, k, v):
    d[k] = v
    return d

def _join_builder_base(t):
    return t.asDict()

def _join_builder_next(t, field_name, builder):
    return _update_dict(builder(t[0]), field_name, t[1])

def _join_builder_next_multi(t, field_name, builder):
    return _update_dict(builder(t[0]), field_name,
                        list(t[1]) if t[1] else [])

class _JoinSpec:
    def __init__(self, spec):
        if (len(spec) != 2
            or not isinstance(spec[0], str)
            or not isinstance(spec[1], tuple)
            or len(spec[1]) != 2
            or not isinstance(spec[1][0], tuple)
            or len(spec[1][0]) != 2
            or not isinstance(spec[1][0][0], str)
            or not isinstance(spec[1][0][1], Dataset)
            or len(spec[1][1]) != 2
            or not callable(spec[1][1][1])):
            raise ValueError(f"Invalid join spec: {spec}")
        self.field_name = spec[0]
        self.join_spec = spec[1][0][0]
        self.spec_src = spec[1][0][1]
        self.spec_key = spec[1][1][1]

def dataset_join_left(
    src: Dataset,
    left_key: collections.abc.Callable[[typing.Any], typing.Any],
    right_spec: typing.Tuple):
    if not right_spec:
        return src
    left_join = src.df.rdd.map(lambda s, left_key=left_key: (left_key(s), s))
    builder = _join_builder_base  # lambda t: t.as_dict()
    joint_seed = src.seed_as_dict()
    for right in right_spec:
        spec = _JoinSpec(right)
        if spec.join_spec == "right_multi":
            right_join = spec.spec_src.df.rdd.groupBy(spec.spec_key)
            joint_seed[spec.field_name] = [
                pyspark.sql.Row(**spec.spec_src.seed_as_dict())]
            builder = (
                lambda t, field_name=spec.field_name, builder=builder:
                _join_builder_next_multi(t, field_name, builder))
        else:
            right_join = spec.spec_src.df.rdd.map(
                lambda s, right_key=spec.spec_key: (right_key(s), s))
            joint_seed[spec.field_name] = pyspark.sql.Row(
                **spec.spec_src.seed_as_dict())
            builder = (
                lambda t, field_name=spec.field_name, builder=builder:
                _join_builder_next(t, field_name, builder))
        left_join = left_join.leftOuterJoin(right_join)
    final_builder = lambda t, builder=builder: builder(t[1])
        # _update_dict(builder(t[1][0]), field_name, t[1][1]))
    return Dataset(pyspark.sql.Row(**joint_seed),
                   left_join.map(final_builder).toDF())
