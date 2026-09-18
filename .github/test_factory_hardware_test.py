import json
import re
import unittest
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[1]
V2 = ROOT / "ultimatesensor-v2"


class FactoryHardwareTest(unittest.TestCase):
    def test_start_messages_fit_in_logger_buffer(self):
        package = yaml.load(
            (V2 / "packages/factory_hardware_test.yaml").read_text(),
            Loader=yaml.BaseLoader,
        )
        buffer_size = int(package.get("logger", {}).get("tx_buffer_size", 512))
        start_lambda = next(
            action["lambda"]
            for boot in package["esphome"]["on_boot"]
            for action in boot["then"]
            if "lambda" in action and "SHS_TEST/1" in action["lambda"]
        )
        format_string = re.search(
            r'ESP_LOGI\("shs_test", "((?:\\.|[^"\\])*)"\)', start_lambda
        ).group(1)
        variants = sorted(V2.glob("*-factory-hardware-test.yaml"))
        self.assertEqual(len(variants), 16)

        for variant in variants:
            with self.subTest(variant=variant.name):
                config = yaml.load(variant.read_text(), Loader=yaml.BaseLoader)
                self.assertEqual(
                    config["packages"]["factory_hardware_test"],
                    "packages/factory_hardware_test.yaml",
                )
                substitutions = package["substitutions"] | config["substitutions"]
                resolved = re.sub(
                    r"\$\{([^}]+)\}",
                    lambda match: substitutions[match.group(1)],
                    format_string,
                )
                message = json.loads('"' + resolved + '"')
                payload = json.loads(message.removeprefix("SHS_TEST/1 "))
                self.assertEqual(payload["type"], "begin")
                self.assertEqual(
                    payload["profile"],
                    variant.stem.removesuffix("-factory-hardware-test"),
                )
                self.assertGreaterEqual(len(payload["tests"]), 10)
                self.assertLessEqual(
                    len(message.encode()) + 64,
                    buffer_size,
                    "Hardware-test JSON must fit with logger prefix, ANSI codes and terminator",
                )


if __name__ == "__main__":
    unittest.main()
