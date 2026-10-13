"""Black-box regression tests. Pass the compiled binary as the first argument."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve()) if len(sys.argv) > 1 else str(Path('csvscope').resolve())


class CsvTests(unittest.TestCase):
    def run_csv(self, data, *options):
        return subprocess.run([BINARY, '-', '--json', *options], input=data.encode('utf-8'), capture_output=True)

    def test_stats_missing_and_mixed(self):
        result = self.run_csv('name,score\nAli,10\nSara,20\n,\nReza,n/a\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report['rows'], 4)
        score = report['columns'][1]
        self.assertEqual((score['type'], score['mean'], score['missing']), ('mixed', 15, 1))

    def test_quotes_multiline_and_crlf(self):
        result = self.run_csv('name,note\r\n"Ali, A","first\nsecond"\r\nSara,"says ""hi"""\r\n')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)['rows'], 2)

    def test_trailing_field_and_no_final_newline(self):
        result = self.run_csv('a,b,c\n1,2,')
        self.assertEqual(result.returncode, 0)
        self.assertEqual(json.loads(result.stdout)['columns'][2]['missing'], 1)

    def test_inconsistent_rows_do_not_distort_stats(self):
        result = self.run_csv('a,b\n1,2\n100\n3,4,5\n')
        self.assertEqual(result.returncode, 1)
        report = json.loads(result.stdout)
        self.assertEqual((report['bad_rows'], report['valid_rows']), (2, 1))
        self.assertEqual(report['columns'][0]['mean'], 1)

    def test_no_header_custom_separator_and_bom(self):
        result = self.run_csv('\ufeff1;2\n3;4', '--no-header', '--delimiter', ';')
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report['columns'][0]['name'], 'column_1')
        self.assertEqual(report['columns'][1]['mean'], 3)

    def test_malformed_quotes(self):
        for data in ['a\n"unfinished', 'a\n"closed"oops', 'a\nno"quote']:
            with self.subTest(data=data):
                result = self.run_csv(data)
                self.assertEqual(result.returncode, 2)
                self.assertIn(b'line', result.stderr)
                self.assertEqual(result.stdout, b'')

    def test_empty_and_header_only(self):
        self.assertEqual(self.run_csv('').returncode, 2)
        report = json.loads(self.run_csv('a,b\n').stdout)
        self.assertEqual(report['rows'], 0)
        self.assertEqual(report['columns'][0]['type'], 'empty')
        self.assertIsNone(report['columns'][0]['mean'])

    def test_unicode_header_json_escaping_and_nonfinite(self):
        result = self.run_csv('نام,"a""b",value\nعلی,x,nan\nسارا,y,inf\n')
        report = json.loads(result.stdout)
        self.assertEqual(report['columns'][0]['name'], 'نام')
        self.assertEqual(report['columns'][1]['name'], 'a"b')
        self.assertEqual(report['columns'][2]['type'], 'text')

    def test_numeric_policy(self):
        report = json.loads(self.run_csv('value\n1e2\n-2.5\n 5 \n').stdout)
        column = report['columns'][0]
        self.assertEqual((column['numeric'], column['text']), (2, 1))
        self.assertAlmostEqual(column['mean'], 48.75)

    def test_file_input_and_usage_errors(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'data.csv'
            path.write_bytes(b'a\n1\n')
            result = subprocess.run([BINARY, str(path), '--json'], capture_output=True)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(json.loads(result.stdout)['rows'], 1)
        for args in [[], ['--unknown'], ['-', '--delimiter', 'xx'], ['-', '--delimiter', '"']]:
            self.assertEqual(subprocess.run([BINARY, *args], capture_output=True).returncode, 2)


if __name__ == '__main__':
    unittest.main()
