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

This is an implementation of dataset interface using pandas.
"""

import collections.abc
import dataclasses
from dataschema import schema2pandas
import logging
import pandas
import nudl.dataset
import time
import typing


class PandasDataset:

    def __init__(self, step: nudl.dataset.DatasetStep,
                 dataframe: pandas.DataFrame):
        self.step = step
        self.dataframe = dataframe
        logging.info("PandasDataset created for %s", step)

    def collect(self):
        return list(
            map(lambda row, seed=self.step.seed_type(): seed(**row),
                self.dataframe.to_dict(orient="records")))


#################### In-module utilities:


def _aggregate_map(t: typing.Any, names: typing.List[str],
                   builder: collections.abc.Callable[[typing.Any],
                                                     typing.Tuple]):
    return {
        x: nudl.dataset.agg_value(y)
        for (x, y) in zip(names, nudl.dataset.agg_values(builder(t)))
    }


_AGGREGATE_MAP = {
    "count": pandas.Series.count,
    "min": pandas.Series.min,
    "max": pandas.Series.max,
    "sum": pandas.Series.sum,
    "mean": pandas.Series.mean,
    "to_set": set,
    "to_array": list,
    "count_distinct": pandas.Series.nunique,
}


class _AggregateBuilder:
    """Builds structure, group by fields and aggregations."""

    def __init__(self, spec: typing.List[typing.Tuple[str, str]]):
        self.spec = spec
        self.field_names = []
        self.agg_spec = {}
        self.group_names = []

    def build(self):
        for (agg_type, field_name) in self.spec:
            self.field_names.append(field_name)
            if agg_type == "group_by":
                self.group_names.append(field_name)
            else:
                self.agg_spec[field_name] = self._agg_fun(agg_type)
        return self

    def _agg_fun(self, agg_type: str) -> collections.abc.Callable:
        if agg_type not in _AGGREGATE_MAP:
            raise ValueError(f"Unknown aggregate name ${agg_type}")
        return _AGGREGATE_MAP[agg_type]


def _process_single_join(row, name):
    result = row.val_x.copy()
    val = row.val_y
    if isinstance(val, float):
        result[name] = None
    else:
        val.name = None
        result[name] = val.copy()
    return {'key': row.key, 'val': result}


def _process_multi_join(row, name):
    result = row.val_x.copy()
    val = row.val_y
    if isinstance(val, float):
        result[name] = []
    else:
        val.name = None
        result[name] = [val.copy()]
    return {'key': row.key, 'val': result}


def _agg_join(series, name):
    result = {}
    for elem in series:
        if not result:
            for index, value in elem.items():
                result[index] = value
        else:
            result[name].extend(elem[name])
    return result


def _process_index_multi_join(row, name, index_name, index):
    result = row.val_x.copy()
    val = row.val_y
    if isinstance(val, float):
        result[name] = []
        result[index_name] = []
    else:
        val.name = None
        result[name] = [val.copy()]
        result[index_name] = [index]
    return {'key': row.key, 'val': result}


def _agg_index_join(series, name, index_name):
    result = {}
    for elem in series:
        if not result:
            for index, value in elem.items():
                result[index] = value
        result[name].extend(elem[name])
        result[index_name].extend(elem[index_name])
    return result


def _expand_multi_join(row):
    return {"key": row.key, "val": pandas.Series(row.val)}


def _prepare_single_join(left_join, right_join, processor):
    return pandas.DataFrame.from_records(
        pandas.merge(left_join,
                     right_join,
                     how="left",
                     left_on='key',
                     right_on='key').apply(processor, axis=1))


def _prepare_multi_join(left_join, right_join, processor, aggregator):
    return (_prepare_single_join(left_join, right_join, processor).groupby(
        "key", as_index=False).agg(aggregator).apply(_expand_multi_join,
                                                     axis=1,
                                                     result_type="expand"))


def _prepare_for_join(src, src_key):
    # Note: the copy bellow is essential, else the last row would
    # be duplicated based on the internal reference passing w/ pandas
    return pandas.DataFrame.from_records(
        src.dataframe.apply(lambda row, key=src_key: {
            "key": key(row),
            "val": row.copy()
        },
                            axis=1))


def _to_record(result):
    if dataclasses.is_dataclass(result):
        return dataclasses.asdict(result)
    if isinstance(result, pandas.Series):
        return result.to_dict()
    raise ValueError(f"Unexpected result type for mapping function: {result}")


def _apply_map(row, fun):
    return _to_record(fun(row))


def _apply_flat_map(row, fun):
    return [_to_record(result) for result in fun(row)]


class PandasPipeline:

    def __init__(self):
        self.steps = {}

    def step_dataset(self, step: nudl.dataset.DatasetStep) -> PandasDataset:
        if step.step_id in self.steps:
            return self.steps[step.step_id]
        dataset = self._convert_step(step)
        self.steps[step.step_id] = dataset
        return dataset

    def _convert_step(self, step) -> PandasDataset:
        if isinstance(step, nudl.dataset.EmptyStep):
            return self._empty_dataset(step)
        elif isinstance(step, nudl.dataset.ReadCsvStep):
            return self._read_csv(step)
        elif isinstance(step, nudl.dataset.ReadParquetStep):
            return self._read_parquet(step)
        elif isinstance(step, nudl.dataset.FilterStep):
            return self._filter(step)
        elif isinstance(step, nudl.dataset.MapStep):
            return self._map(step)
        elif isinstance(step, nudl.dataset.FlatMapStep):
            return self._flat_map(step)
        elif isinstance(step, nudl.dataset.LimitStep):
            return self._limit(step)
        elif isinstance(step, nudl.dataset.AggregateStep):
            return self._aggregate(step)
        elif isinstance(step, nudl.dataset.JoinLeftStep):
            return self._join_left(step)
        raise ValueError(f"Unknown dataset step kind: {step}")

    def _empty_dataset(self, step: nudl.dataset.EmptyStep) -> PandasDataset:
        schema = schema2pandas.ConvertTable(step.schema)
        return PandasDataset(
            step,
            pandas.DataFrame(
                {key: pandas.Series(dtype=val) for key, val in schema.items()}))

    def _read_csv(self, step: nudl.dataset.ReadCsvStep) -> PandasDataset:
        schema = schema2pandas.ConvertTable(step.schema)
        return PandasDataset(  # type: ignore
            step,
            pandas.read_csv(
                step.filespec,
                # TODO(catalin): setup index based on field annotations.
                header=0,  # type: ignore
                names=schema.keys(),
                dtype=schema))

    def _read_parquet(self,
                      step: nudl.dataset.ReadParquetStep) -> PandasDataset:
        schema = schema2pandas.ConvertTable(step.schema)
        used_fields = step.used_fields()
        if used_fields is not None:
            logging.info("Restricting Parquet read %s columns to: %s", step,
                         used_fields)
        # TODO(catalin): add a parquet schema check
        return PandasDataset(
            step,
            pandas.read_parquet(step.filespec,
                                engine="pyarrow",
                                columns=used_fields,
                                use_nullable_dtypes=True))

    def _filter(self, step: nudl.dataset.FilterStep) -> PandasDataset:
        assert step.source is not None
        src = self.step_dataset(step.source)
        return PandasDataset(step,
                             pandas.DataFrame.from_records(
                                 src.dataframe[src.dataframe.apply(
                                     step.fun, axis=1)]))  # type: ignore

    def _map(self, step: nudl.dataset.MapStep) -> PandasDataset:
        assert step.source is not None
        src = self.step_dataset(step.source)
        return PandasDataset(
            step,
            pandas.DataFrame.from_records(
                src.dataframe.apply(
                    lambda row, fun=step.fun: _apply_map(row, fun), axis=1)))

    def _flat_map(self, step: nudl.dataset.FlatMapStep) -> PandasDataset:
        assert step.source is not None
        src = self.step_dataset(step.source)
        return PandasDataset(
            step,
            pandas.DataFrame.from_records(
                pandas.DataFrame.from_records(
                    src.dataframe.apply(
                        lambda row, fun=step.fun: _apply_flat_map(row, fun),
                        axis=1)).melt().apply(
                            lambda row: dataclasses.asdict(row.value), axis=1)))

    def _limit(self, step: nudl.dataset.LimitStep) -> PandasDataset:
        assert step.source is not None
        src = self.step_dataset(step.source)
        return PandasDataset(step, src.dataframe.iloc[:step.limit])

    def _aggregate(self, step: nudl.dataset.AggregateStep) -> PandasDataset:
        assert step.source is not None
        src = self.step_dataset(step.source)
        agg_builder = _AggregateBuilder(step.build_spec()).build()
        agg_source = pandas.DataFrame.from_records(  # type: ignore
            src.dataframe.apply(
                lambda t, names=agg_builder.field_names, builder=step.builder:
                _aggregate_map(t, names, builder),
                axis=1))

        if agg_builder.group_names:
            return PandasDataset(
                step,
                agg_source.groupby(  # type: ignore
                    agg_builder.group_names,
                    as_index=False).agg(agg_builder.agg_spec))
        return PandasDataset(  # type: ignore
            step,
            pandas.DataFrame.from_records(
                [agg_source.agg(agg_builder.agg_spec).to_dict()]))

    def _join_left(self, step: nudl.dataset.JoinLeftStep) -> PandasDataset:
        assert step.source is not None
        left_src = self.step_dataset(step.source)
        right_join_fields = step.right_join_fields()
        if not step.right_spec or not right_join_fields:
            logging.info("Skiping join left for %s, per no joins needed", step)
            return left_src

        left_join = _prepare_for_join(left_src, step.left_key)
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
                    right_join = _prepare_for_join(right_src, spec.spec_key)
                    processor = (
                        lambda row, name=spec.field_name, index_name=
                        index_field_name, index=index:
                        _process_index_multi_join(row, name, index_name, index))
                    aggregator = (lambda series, name=spec.field_name,
                                  index_name=index_field_name: _agg_index_join(
                                      series, name, index_name))
                    left_join = _prepare_multi_join(left_join, right_join,
                                                    processor, aggregator)
            else:
                right_src = self.step_dataset(spec.spec_src)
                right_join = _prepare_for_join(right_src, spec.spec_key)
                if spec.join_spec == "right_multi":
                    processor = (lambda row, name=spec.field_name:
                                 _process_multi_join(row, name))
                    aggregator = (lambda series, name=spec.field_name:
                                  _agg_join(series, name))
                    left_join = _prepare_multi_join(left_join, right_join,
                                                    processor, aggregator)
                else:
                    processor = (lambda row, name=spec.field_name:
                                 _process_single_join(row, name))
                    left_join = _prepare_single_join(left_join, right_join,
                                                     processor)
        return PandasDataset(
            step,
            pandas.DataFrame.from_records(
                left_join.apply(lambda row: row.val, axis=1)))


class PandasEngineImpl(nudl.dataset.DatasetEngine):

    def __init__(self):
        super().__init__("Pandas")

    def collect(self,
                step: nudl.dataset.DatasetStep) -> typing.List[typing.Any]:
        pipeline = PandasPipeline()
        collector = nudl.dataset.FieldUsageCollector()
        step.update_field_usage(collector, True)
        return pipeline.step_dataset(step).collect()
