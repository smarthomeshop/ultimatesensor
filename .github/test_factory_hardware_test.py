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


class PirHardwareTest(unittest.TestCase):
    def run_pir(self, samples):
        package = yaml.load((V2 / "packages/factory_hardware_test.yaml").read_text(), Loader=yaml.BaseLoader)
        boot = next(x for x in package['esphome']['on_boot'] if x['priority'] == '566')
        clock = [0]
        messages = []
        def condition(value):
            code = value['lambda']
            self.assertIn('id(pir_motion).has_state()', code)
            state = samples(clock[0])
            return state is not None and (not state if '!id(pir_motion).state' in code else state)
        def actions(items):
            for action in items:
                if 'delay' in action:
                    clock[0] += int(action['delay'][:-1])
                elif 'wait_until' in action:
                    wait = action['wait_until']
                    end = clock[0] + int(wait['timeout'][:-1])
                    while not condition(wait['condition']) and clock[0] < end:
                        clock[0] += 1
                elif 'if' in action:
                    branch = action['if']
                    actions(branch['then'] if condition(branch['condition']) else branch.get('else', []))
                elif 'lambda' in action and 'SHS_TEST/1' in action['lambda']:
                    match = re.search(r'ESP_LOGI\("shs_test", "((?:\\.|[^"\\])*)"\)', action['lambda'])
                    payload = json.loads('"' + match.group(1) + '"')
                    messages.append(json.loads(payload.removeprefix('SHS_TEST/1 ')))
                    if messages[-1]['status'] == 'fail':
                        self.assertIn('id(shs_test_failed) = true;', action['lambda'])
        actions(boot['then'])
        self.assertLessEqual(clock[0], 33)
        self.assertEqual(len(messages), 1)
        self.assertEqual(boot['then'][-1]['lambda'], 'id(shs_test_pir_done) = true;')
        final = next(x for x in package['esphome']['on_boot'] if x['priority'] == '500')
        self.assertIn('id(shs_test_pir_done)', final['then'][0]['wait_until']['condition']['lambda'])
        self.assertIn('id(shs_test_pir_done)', final['then'][1]['lambda'])
        return messages[0]

    def test_real_motion_transition_passes(self):
        result = self.run_pir(lambda t: t >= 10)
        self.assertEqual((result['status'], result['value']), ('pass', 'Ja'))

    def test_no_motion_fails_with_retry_instruction(self):
        result = self.run_pir(lambda t: False)
        self.assertEqual((result['status'], result['value']), ('fail', 'Nee'))
        self.assertIn('test opnieuw', result['detail'])

    def test_constant_high_or_missing_input_never_passes(self):
        for state in (True, None):
            with self.subTest(state=state):
                self.assertEqual(self.run_pir(lambda t: state)['status'], 'fail')


if __name__ == "__main__":
    unittest.main()
