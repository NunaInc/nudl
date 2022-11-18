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
import typing


class Dataset:
    """Base 'abstract' class for a dataset
    - this represents an abstract flow of data"""

    def __init__(self, engine: 'DatasetEngine', seed: typing.Any):
        self.engine = engine
        self.seed = seed

    def seed_as_dict(self):
        """Returns the seed as a dictionary from field to default values."""
        raise NotImplementedError(f"`seed_as_dict` not implemented")

    def dataset_filter(self, f: collections.abc.Callable[[typing.Any], bool]):
        return self.engine.dataset_filter(self, f)

    def dataset_map(self, result_seed: typing.Any,
                    f: collections.abc.Callable[[typing.Any], typing.Any]):
        return self.engine.dataset_map(self, result_seed, f)

    def dataset_flat_map(self, result_seed: typing.Any,
                         f: collections.abc.Callable[[typing.Any],
                                                     typing.List[typing.Any]]):
        return self.engine.dataset_flat_map(self, result_seed, f)

    def dataset_aggregate(self, result_seed: typing.Any,
                          builder: collections.abc.Callable[[typing.Any],
                                                            typing.Tuple]):
        return self.engine.dataset_aggregate(self, result_seed, builder)

    def dataset_join_left(self, result_seed: typing.Any,
                          left_key: collections.abc.Callable[[typing.Any],
                                                             typing.Any],
                          right_spec: typing.Tuple):
        return self.engine.dataset_join_left(self, result_seed, left_key,
                                             right_spec)

    def dataset_limit(self, size: int):
        return self.engine.dataset_limit(self, size)

    def dataset_collect(self) -> typing.List[typing.Any]:
        return self.engine.dataset_collect(self)


class DatasetEngine:
    """Base 'abstract' class for a dataset engine - processes datasets
    and passes them through various processing stages."""

    def __init__(self, name):
        self.name = name

    def empty_dataset(self, seed: typing.Any) -> Dataset:
        return Dataset(self, seed)

    def read_csv(self, seed: typing.Any, file_spec: str) -> Dataset:
        raise NotImplementedError(f"`read_csv` not implemented for {self.name}")

    def read_parquet(self, seed: typing.Any, file_spec: str) -> Dataset:
        raise NotImplementedError(f"`read_csv` not implemented for {self.name}")

    def dataset_filter(self, src: Dataset,
                       f: collections.abc.Callable[[typing.Any], bool]):
        raise NotImplementedError(
            f"`dataset_filter` not implemented for {self.name}")

    def dataset_map(self, src: Dataset, result_seed: typing.Any,
                    f: collections.abc.Callable[[typing.Any], typing.Any]):
        raise NotImplementedError(
            f"`dataset_map` not implemented for {self.name}")

    def dataset_flat_map(self, src: Dataset, result_seed: typing.Any,
                         f: collections.abc.Callable[[typing.Any],
                                                     typing.List[typing.Any]]):
        raise NotImplementedError(
            f"`dataset_flat_map` not implemented for {self.name}")

    def dataset_aggregate(self, src: Dataset, result_seed: typing.Any,
                          builder: collections.abc.Callable[[typing.Any],
                                                            typing.Tuple]):
        raise NotImplementedError(
            f"`dataset_aggregate` not implemented for {self.name}")

    def dataset_join_left(self, src: Dataset, result_seed: typing.Any,
                          left_key: collections.abc.Callable[[typing.Any],
                                                             typing.Any],
                          right_spec: typing.Tuple):
        raise NotImplementedError(
            f"`dataset_join_left` not implemented for {self.name}")

    def dataset_limit(self, src: Dataset, size: int):
        raise NotImplementedError(
            f"`dataset_limit` not implemented for {self.name}")

    def dataset_collect(self, src: Dataset) -> typing.List[typing.Any]:
        raise NotImplementedError(
            f"`dataset_collect` not implemented for {self.name}")
