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
from dataschema import schema2pyspark
import logging
import nudl.dataset
import pyspark
import pyspark.sql
import pyspark.sql.functions
import pyspark.sql.session
import pyspark.sql.types
import typing
import uuid


def pyspark_schema(step: nudl.dataset.DatasetStep):
    column_names = [column.name() for column in step.schema.columns]
    logging.info("Schema for step %s: %s. Columns: %s", step,
                 step.schema.name(), column_names)
    return schema2pyspark.ConvertTable(step.schema)


class SparkDataset:

    def __init__(self, step: nudl.dataset.DatasetStep,
                 dataframe: pyspark.sql.DataFrame):
        self.step = step
        self.dataframe = dataframe
        logging.info("SparkDataset created for %s", step)

    def rdd(self) -> pyspark.RDD:
        return self.dataframe.rdd

    @classmethod
    def from_rdd(cls, step: nudl.dataset.DatasetStep, rdd: pyspark.RDD):
        return cls(step, rdd.toDF(pyspark_schema(step)))  # type: ignore


def spark_init_session(name: str) -> pyspark.sql.session.SparkSession:
    return pyspark.sql.session.SparkSession.builder.appName(name).getOrCreate()


#################### In-module utilities:


def _aggregate_map(t: typing.Any, names: typing.List[str],
                   builder: collections.abc.Callable[[typing.Any],
                                                     typing.Tuple]):
    return pyspark.sql.Row(
        **{
            x: nudl.dataset.agg_value(y)
            for (x, y) in zip(names, nudl.dataset.agg_values(builder(t)))
        })


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


class _AggregateBuilder:
    """Builds structure, group by fields and aggregations."""

    def __init__(self, spec: typing.List[typing.Tuple[str, str]],
                 schema: pyspark.sql.types.StructType):
        self.spec = spec
        self.schema = schema
        self.field_names = []
        self.agg_spec = []
        self.group_names = []
        self.mapper_struct = schema

    def build(self):
        struct_fields = []
        for (agg_type, field_name) in self.spec:
            self.field_names.append(field_name)
            if agg_type == "group_by":
                self.group_names.append(field_name)
                struct_fields.append(self.schema[field_name])
            else:
                self.agg_spec.append(
                    self._agg_fun(agg_type)(field_name).alias(field_name))
                struct_fields.append(
                    self._get_aggregation_field(agg_type, field_name))
        self.mapper_struct = pyspark.sql.types.StructType(struct_fields)
        return self

    def _get_aggregation_field(self, agg_type: str, field_name: str):
        field_type = self.schema[field_name].dataType  # type: ignore
        if (agg_type in ("to_array", "to_set") and
                isinstance(field_type, pyspark.sql.types.ArrayType)):
            return pyspark.sql.types.StructField(field_name,
                                                 field_type.elementType, True)
        return self.schema[field_name]

    def _agg_fun(self, agg_type: str) -> collections.abc.Callable:
        if agg_type not in _AGGREGATE_MAP:
            raise ValueError(f"Unknown aggregate name ${agg_type}")
        return _AGGREGATE_MAP[agg_type]


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


########################################


class SparkPipeline:

    def __init__(self, session: pyspark.sql.session.SparkSession,
                 collect_options: nudl.dataset.CollectOptions):
        self.session = session
        self.steps = {}
        self.collect_options = collect_options

    def step_dataset(self, step: nudl.dataset.DatasetStep) -> SparkDataset:
        if step.step_id in self.steps:
            return self.steps[step.step_id]
        dataset = self._convert_step(step)
        self.steps[step.step_id] = dataset
        return dataset

    def _convert_step(self, step: nudl.dataset.DatasetStep) -> SparkDataset:
        if isinstance(step, nudl.dataset.EmptyStep):
            return SparkDataset.from_rdd(step,
                                         self.session.sparkContext.emptyRDD())
        elif isinstance(step, nudl.dataset.ReadCsvStep):
            return self._read_csv(step)
        elif isinstance(step, nudl.dataset.ReadParquetStep):
            return self._read_parquet(step)
        elif isinstance(step, nudl.dataset.FilterStep):
            return self._filter(step)
        elif isinstance(step, nudl.dataset.MapStep):
            assert step.source is not None
            return SparkDataset.from_rdd(
                step,
                self.step_dataset(step.source).rdd().map(step.fun, True))
        elif isinstance(step, nudl.dataset.FlatMapStep):
            assert step.source is not None
            return SparkDataset.from_rdd(
                step,
                self.step_dataset(step.source).rdd().flatMap(step.fun, True))
        elif isinstance(step, nudl.dataset.LimitStep):
            assert step.source is not None
            return SparkDataset(
                step,
                self.step_dataset(step.source).dataframe.limit(step.limit))
        elif isinstance(step, nudl.dataset.AggregateStep):
            return self._aggregate(step)
        elif isinstance(step, nudl.dataset.JoinLeftStep):
            return self._join_left(step)
        raise ValueError(f"Unknown dataset step kind: {step}")

    def _read_csv(self, step: nudl.dataset.ReadCsvStep) -> SparkDataset:
        df = self.session.read.csv(step.filespec, schema=pyspark_schema(step))
        if not self.collect_options.disable_column_prunning:
            fields = step.used_fields()
            if fields is not None:
                logging.info("Restricting CSV read %s columns to: %s", step,
                             fields)
                df = df.select(*fields)
        return SparkDataset(step, df)

    def _read_parquet(self, step: nudl.dataset.ReadParquetStep) -> SparkDataset:
        df = self.session.read.format("parquet").schema(
            pyspark_schema(step)).load(step.filespec)
        if not self.collect_options.disable_column_prunning:
            fields = step.used_fields()
            if fields is not None:
                logging.info("Restricting Parquet read %s columns to: %s", step,
                             fields)
                df = df.select(*fields)
        return SparkDataset(step, df)

    def _filter(self, step: nudl.dataset.FilterStep) -> SparkDataset:
        assert step.source is not None
        src = self.step_dataset(step.source)
        try:
            condition = step.fun(src.dataframe)
            logging.info("Using native Spark dataframe filter for %s", step)
            return SparkDataset(step, src.dataframe.filter(condition))
        except Exception:
            pass
        logging.info("Using RDD based function filter for %s", step)
        return SparkDataset.from_rdd(
            step,
            self.step_dataset(step.source).rdd().filter(step.fun))

    def _aggregate(self, step: nudl.dataset.AggregateStep):
        assert step.source is not None
        src = self.step_dataset(step.source)
        agg_builder = _AggregateBuilder(step.build_spec(),
                                        pyspark_schema(step)).build()
        mapper = (lambda t, names=agg_builder.field_names, builder=step.builder:
                  _aggregate_map(t, names, builder))
        return SparkDataset(
            step,
            src.dataframe.rdd.map(mapper).toDF(  # type: ignore
                agg_builder.mapper_struct).groupBy(
                    agg_builder.group_names).agg(*agg_builder.agg_spec))

    def _join_left(self, step: nudl.dataset.JoinLeftStep):
        assert step.source is not None
        left_src = self.step_dataset(step.source)
        right_join_fields = step.right_join_fields(
            self.collect_options.disable_column_prunning)
        if not step.right_spec or not right_join_fields:
            logging.info("Skiping join left for %s, per no joins needed", step)
            return left_src
        left_join = left_src.dataframe.rdd.map(lambda l, left_key=step.left_key:
                                               (left_key(l), l))
        builder = _join_builder_base
        for right in step.right_spec:
            spec = nudl.dataset.JoinSpecDecoder(right)
            if spec.field_name not in right_join_fields:
                logging.info("In %s, skiping join column: %s as it is unused",
                             step, spec.field_name)
                continue
            if spec.join_spec == "right_multi_array":
                if not spec.spec_src:
                    continue
                index_field_name = f"{spec.field_name}_index"
                index = 0
                for src in spec.spec_src:
                    right_src = self.step_dataset(src)
                    right_join = right_src.dataframe.rdd.groupBy(spec.spec_key)
                    builder = (
                        lambda t, field_name=spec.field_name, index_field_name=
                        index_field_name, index=index, builder=
                        builder: _join_builder_next_multi_array(
                            t, field_name, index_field_name, index, builder))
                    index += 1
                    left_join = left_join.leftOuterJoin(right_join)
            else:
                right_src = self.step_dataset(spec.spec_src)
                if spec.join_spec == "right_multi":
                    right_join = right_src.dataframe.rdd.groupBy(spec.spec_key)
                    builder = (
                        lambda t, field_name=spec.field_name, builder=builder:
                        _join_builder_next_multi(t, field_name, builder))
                else:
                    right_join = right_src.dataframe.rdd.map(
                        lambda s, right_key=spec.spec_key: (right_key(s), s))
                    builder = (
                        lambda t, field_name=spec.field_name, builder=builder:
                        _join_builder_next(t, field_name, builder))
                left_join = left_join.leftOuterJoin(right_join)  # type: ignore
        final_builder = lambda t, builder=builder: builder(t[1])
        return SparkDataset.from_rdd(step, left_join.map(final_builder))


class SparkEngineImpl(nudl.dataset.DatasetEngine):

    def __init__(self, session: pyspark.sql.session.SparkSession):
        super().__init__("Spark")
        self.session = session

    def collect(
            self, step: nudl.dataset.DatasetStep,
            options: nudl.dataset.CollectOptions) -> typing.List[typing.Any]:
        pipeline = SparkPipeline(self.session, options)
        if not options.disable_column_prunning:
            collector = nudl.dataset.FieldUsageCollector()
            step.update_field_usage(collector, True)
            updater = nudl.dataset.SchemaFieldsUpdater()
            step.update_schema_fields(updater)
        return pipeline.step_dataset(step).dataframe.collect()
