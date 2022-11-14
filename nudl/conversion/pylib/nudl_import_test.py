"""This simple binary just tests if the main nudl python modules
can be imported and does some simple test with them."""

import unittest

import nudl
import nudl.dataset
import nudl.dataproc
import nudl.dataset_spark


class NudlPyTest(unittest.TestCase):

    def test_base(self):
        self.assertEqual(nudl.safe_min([2, 3, 1]), 1)
        self.assertEqual(nudl.safe_min([]), None)
        self.assertEqual(nudl.safe_max([2, 3, 1]), 3)
        self.assertEqual(nudl.safe_max([]), None)

    def test_base_dataset(self):
        engine = nudl.dataset.DatasetEngine("Foo")
        self.assertEqual(engine.name, "Foo")


if __name__ == '__main__':
    unittest.main()
